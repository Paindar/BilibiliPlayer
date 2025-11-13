#include "network_manager.h"
#include "bili_network_interface.h"
#include <log/log_manager.h>
#include <future>
#include <thread>
#include <chrono>
#include <fstream>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <fmt/format.h>

namespace network
{
    NetworkManager::NetworkManager()
        : QObject(nullptr)
        , m_configured(false)
    {
        // Initialize BilibiliNetworkInterface
        m_biliInterface = std::make_unique<BilibiliNetworkInterface>();
        LOG_DEBUG("NetworkManager created");
    }
    
    NetworkManager::~NetworkManager()
    {
        // Signal cancel for searches
        cancelAllSearches();

        // Signal cancel for any active downloads and notify associated buffers
        std::vector<std::shared_ptr<RealtimePipe>> pipes;
        {
            std::lock_guard<std::mutex> lg(m_bgMutex);
            for (auto &entry : m_downloadCancelTokens) {
                if (entry.token) 
                    entry.token->store(true);
                if (entry.buffer)
                    pipes.push_back(entry.buffer);
            }
            m_downloadCancelTokens.clear();
        }
        for (auto& pipe : pipes) {
            pipe->close();
        }
        pipes.clear();
        
        // Join background watcher threads
        {
            std::lock_guard<std::mutex> lg(m_bgMutex);
            for (auto &t : m_bgThreads) {
                if (t.joinable()) {
                    t.join();
                }
            }
            m_bgThreads.clear();
            m_downloadCancelTokens.clear();
        }

        LOG_DEBUG("NetworkManager destroyed");
    }
    
    void NetworkManager::configure(const QString& platformDir, int timeoutMs, const QString& proxyUrl)
    {
        LOG_INFO("Configuring NetworkManager - timeout: {}ms, proxy: {}", timeoutMs, proxyUrl.toStdString());
        
        // Update timeout
        setRequestTimeout(timeoutMs);
        
        // Configure Bilibili interface
        if (m_biliInterface) {
            m_biliInterface->setPlatformDirectory(platformDir.toStdString());
            m_biliInterface->setTimeout(timeoutMs / 1000); // Convert to seconds
            if (!proxyUrl.isEmpty()) {
                // TODO: Implement proxy support
            }
            if (m_biliInterface->loadConfig()) {
                LOG_INFO("BilibiliNetworkInterface loaded configuration successfully.");
            } else if (m_biliInterface->initializeDefaultConfig()) {
                LOG_INFO("BilibiliNetworkInterface configured successfully.");
            } else {
                LOG_WARN("BilibiliNetworkInterface failed to initialize default configuration.");
                m_biliInterface.reset();
            }
        }
        
        // TODO: Configure API base URL and proxy if needed by underlying interfaces
        
        m_configured = true;
        LOG_INFO("NetworkManager configuration complete");
    }

    void NetworkManager::saveConfiguration()
    {
        if (m_biliInterface) {
            if (m_biliInterface->saveConfig()) {
                LOG_INFO("BilibiliNetworkInterface configuration saved successfully.");
            } else {
                LOG_WARN("BilibiliNetworkInterface failed to save configuration.");
            }
        }
        //TODO save other interfaces' configurations if exists.
    }

    // std::weak_ptr<BilibiliNetworkInterface> NetworkManager::getBiliNetworkInterface()
    // {
    //     return m_biliInterface;
    // }
    
    void NetworkManager::executeMultiSourceSearch(const QString& keyword, uint selectedInterface, int maxResults)
    {
        if (keyword.trimmed().isEmpty()) {
            LOG_WARN("Search failed: keyword is empty");
            emit searchFailed(keyword, "Search keyword is empty");
            return;
        }
        
        LOG_INFO("Starting multi-source search for: '{}', interfaces: {}, maxResults: {}", 
                keyword.toStdString(), selectedInterface, maxResults);
        
        // Reset cancel flag for new search
        m_cancelFlag = false;
        
        // Vector to store futures for monitoring
        std::vector<std::future<void>> searchFutures;
        
        // Launch Bilibili search if requested
        if (selectedInterface & SupportInterface::Bilibili) {
            auto bilibiliFuture = std::async(std::launch::async, [this, keyword, maxResults]() {
                try {
                    auto biliResults = performBilibiliSearchAsync(keyword, maxResults, m_cancelFlag).get();
                    if (!m_cancelFlag) {
                        emit searchProgress(keyword, biliResults);
                    }
                } catch (const std::exception& e) {
                    if (!m_cancelFlag) {
                        emit searchFailed(keyword, QString("Bilibili search failed: %1").arg(e.what()));
                    }
                }
            });
            searchFutures.push_back(std::move(bilibiliFuture));
        }
        
        // Future: Add other platforms here
        // if (selectedInterface & SupportInterface::YouTube) { ... }
        // if (selectedInterface & SupportInterface::Local) { ... }
        
        // Monitor all futures in a separate thread using condition variables
        monitorSearchFuturesWithCV(keyword, std::move(searchFutures));
    }
    
    void NetworkManager::cancelAllSearches()
    {
        m_cancelFlag = true;
    }

    std::future<uint64_t> NetworkManager::getStreamSizeByParamsAsync(SupportInterface platform, const QString &params)
    {
        if (checkPlatformStatus(platform) == false) {
            return std::async(std::launch::async, [platform, params]() -> uint64_t {
                LOG_ERROR("Requested platform is not configured for stream size query, "
                    "platform enum: {}, params: {}", 
                    static_cast<int>(platform), 
                    params.toStdString());
                return 0;
            });
        }
        switch (platform) {
            case SupportInterface::Bilibili:
                return std::async(std::launch::async, [this, params]() -> uint64_t {
                    try {
                        std::string url = m_biliInterface->getAudioUrlByParams(params.toStdString());
                        if (url.empty()) {
                            LOG_ERROR("Failed to get audio URL from parameters for stream size query");
                            return 0;
                        }
                        return m_biliInterface->getStreamBytesSize(url);
                    } catch (const std::exception& e) {
                        LOG_ERROR("Bilibili stream size (by params) error: {}", e.what());
                        throw; // Re-throw to be handled by caller
                    }
                });

            default:
                return std::async(std::launch::async, []() -> uint64_t {
                    throw std::runtime_error("Unsupported platform for stream size query (by params)");
                });
        }
    }

    std::shared_future<void> NetworkManager::downloadAsync(SupportInterface platform, const QString &url, const QString &filepath)
    {
        if (checkPlatformStatus(platform) == false) {
            return std::async(std::launch::async, [platform, url, filepath]() -> void {
                LOG_ERROR("Requested platform is not configured for download, "
                    "platform enum: {}, url: {}, filepath: {}", 
                    static_cast<int>(platform), 
                    url.toStdString(), 
                    filepath.toStdString());
                throw std::runtime_error("Platform not configured for download");
            }).share();
        }
        
        // Create a unique key for this download (combination of URL and filepath)
        QString downloadKey = QString("%1::%2").arg(url).arg(filepath);
        
        // Check if this download is already in progress
        {
            std::lock_guard<std::mutex> lock(m_downloadMapMutex);
            auto it = m_activeDownloads.find(downloadKey);
            if (it != m_activeDownloads.end()) {
                LOG_INFO("Download already in progress for: {}, returning existing future", filepath.toStdString());
                return it->second.future;
            }
        }
        
        // Create a new download entry
        auto promisePtr = std::make_shared<std::promise<void>>();
        std::shared_future<void> sharedFuture = promisePtr->get_future().share();
        
        {
            std::lock_guard<std::mutex> lock(m_downloadMapMutex);
            m_activeDownloads[downloadKey] = ActiveDownload{ promisePtr, sharedFuture };
        }
        
        switch (platform) {
            case SupportInterface::Bilibili:
                return std::async(std::launch::async, [this, url, filepath, downloadKey, promisePtr]() -> void {
                    try {
                        // Write to temporary .part file first
                        const QString tempPath = filepath + ".part";
                        // Ensure target directory exists
                        QFileInfo fi(filepath);
                        QDir().mkpath(fi.absolutePath());
                        std::ofstream file(tempPath.toStdString(), std::ios::binary);
                        if (!file.is_open()) {
                            throw std::runtime_error("Failed to open file for writing: " + tempPath.toStdString());
                        }
                        
                        // Use asyncDownloadStream with a callback that writes to file
                        auto download_future = m_biliInterface->asyncDownloadStream(url.toStdString(), 
                            [&file](const char* data, size_t size) -> bool {
                                if (data && size > 0) {
                                    file.write(data, size);
                                    if (file.fail()) {
                                        LOG_ERROR("Failed to write data to file");
                                        return false; // Stop download on write error
                                    }
                                }
                                return true; // Continue download
                            }
                        );
                        
                        // Wait for download to complete
                        bool success = download_future.get();
                        file.close();
                        
                        if (!success) {
                            // Remove partial file on failure
                            QFile::remove(tempPath);
                            
                            // Clean up from active downloads map
                            {
                                std::lock_guard<std::mutex> lock(m_downloadMapMutex);
                                m_activeDownloads.erase(downloadKey);
                            }
                            promisePtr->set_exception(std::make_exception_ptr(std::runtime_error("Download failed")));
                            emit downloadFailed(url, filepath, "Download failed");
                            return;
                        }
                        // Move .part to final path (best-effort atomic on same volume)
                        if (QFile::exists(filepath)) {
                            QFile::remove(filepath);
                        }
                        if (!QFile::rename(tempPath, filepath)) {
                            // Fallback: copy then remove
                            if (QFile::copy(tempPath, filepath)) {
                                QFile::remove(tempPath);
                            } else {
                                QFile::remove(tempPath);
                                
                                // Clean up from active downloads map
                                {
                                    std::lock_guard<std::mutex> lock(m_downloadMapMutex);
                                    m_activeDownloads.erase(downloadKey);
                                }
                                promisePtr->set_exception(std::make_exception_ptr(std::runtime_error("Failed to finalize download file move")));
                                emit downloadFailed(url, filepath, "Failed to finalize download file move");
                                return;
                            }
                        }
                        
                        LOG_INFO("Download completed successfully: {}", filepath.toStdString());
                        
                        // Clean up from active downloads map
                        {
                            std::lock_guard<std::mutex> lock(m_downloadMapMutex);
                            m_activeDownloads.erase(downloadKey);
                        }
                        promisePtr->set_value();
                        emit downloadCompleted(url, filepath);
                        
                    } catch (const std::exception& e) {
                        LOG_ERROR("Bilibili download error: {}", e.what());
                        
                        // Clean up from active downloads map
                        {
                            std::lock_guard<std::mutex> lock(m_downloadMapMutex);
                            m_activeDownloads.erase(downloadKey);
                        }
                        promisePtr->set_exception(std::current_exception());
                        emit downloadFailed(url, filepath, e.what());
                        throw; // Re-throw to be handled by caller
                    }
                }).share();
                
            default:
                // Clean up and return error future
                {
                    std::lock_guard<std::mutex> lock(m_downloadMapMutex);
                    m_activeDownloads.erase(downloadKey);
                }
                return std::async(std::launch::async, [promisePtr]() -> void {
                    promisePtr->set_exception(std::make_exception_ptr(std::runtime_error("Unsupported platform for download")));
                }).share();
        }
    }

    std::shared_future<std::shared_ptr<std::istream>> NetworkManager::getAudioStreamAsync(SupportInterface platform, const QString& params, const QString& savepath)
    {
        if (checkPlatformStatus(platform) == false) {
            return std::async(std::launch::async, [platform, params, savepath]() -> std::shared_ptr<std::istream> {
                LOG_ERROR("Requested platform is not configured for streaming, "
                    "platform enum: {}, params: {}, savepath: {}", 
                    static_cast<int>(platform), 
                    params.toStdString(),
                    savepath.toStdString());
                throw std::runtime_error("Platform not configured for streaming");
            }).share();
        }
        
        // Create a unique key for this stream (combination of params and savepath)
        QString streamKey = QString("%1::%2").arg(params).arg(savepath);
        
        // Check if this stream is already being fetched
        {
            std::lock_guard<std::mutex> lock(m_downloadMapMutex);
            auto it = m_activeStreams.find(streamKey);
            if (it != m_activeStreams.end()) {
                LOG_INFO("Audio stream already in progress for params: {}, savepath: {}, returning existing future", 
                         params.toStdString(), savepath.toStdString());
                return it->second.future;
            }
        }
        
        // Create a new stream entry
        auto promisePtr = std::make_shared<std::promise<std::shared_ptr<std::istream>>>();
        std::shared_future<std::shared_ptr<std::istream>> sharedFuture = promisePtr->get_future().share();
        
        {
            std::lock_guard<std::mutex> lock(m_downloadMapMutex);
            m_activeStreams[streamKey] = ActiveStream{ promisePtr, sharedFuture };
        }
        
        switch (platform) {
            case SupportInterface::Bilibili:
                return std::async(std::launch::async, [this, params, savepath, streamKey, promisePtr]() -> std::shared_ptr<std::istream> {
                    try {
                        // Create streaming buffer (5MB buffer for smooth streaming)
                        const size_t buffer_size = 5 * 1024 * 1024;
                        // Create streaming input that wraps the buffer
                        auto pipe = std::make_shared<RealtimePipe>(buffer_size);
                        std::shared_ptr<std::ofstream> file_stream;
                        QString temp_savepath;
                        if (!savepath.isEmpty()) {
                            temp_savepath = savepath + ".part";
                            // Ensure parent directory exists
                            QFileInfo fi(savepath);
                            QDir().mkpath(fi.absolutePath());
                            file_stream = std::make_shared<std::ofstream>(temp_savepath.toStdString(), std::ios::binary);
                            if (!file_stream->is_open()) {
                                // Clean up from active streams map
                                {
                                    std::lock_guard<std::mutex> lock(m_downloadMapMutex);
                                    m_activeStreams.erase(streamKey);
                                }
                                promisePtr->set_exception(std::make_exception_ptr(
                                    std::runtime_error("Failed to open file for writing: " + temp_savepath.toStdString())));
                                throw std::runtime_error("Failed to open file for writing: " + temp_savepath.toStdString());
                            }
                        }
                        // Get actual URL from params
                        std::string url = m_biliInterface->getAudioUrlByParams(params.toStdString());
                        if (url.empty()) {
                            // Clean up from active streams map
                            {
                                std::lock_guard<std::mutex> lock(m_downloadMapMutex);
                                m_activeStreams.erase(streamKey);
                            }
                            promisePtr->set_exception(std::make_exception_ptr(
                                std::runtime_error("Failed to get audio URL from parameters")));
                            throw std::runtime_error("Failed to get audio URL from parameters");
                        }
                        // Start async download to the streaming buffer with a cancel token
                        auto cancel_token = std::make_shared<std::atomic<bool>>(false);
                        {
                            std::lock_guard<std::mutex> lg(m_bgMutex);
                            m_downloadCancelTokens.push_back({ cancel_token, pipe });
                        }
                        auto os = pipe->getOutputStream();
                        auto is = pipe->getInputStream();
                        auto download_future = m_biliInterface->asyncDownloadStream(
                            url,
                            [os, file_stream, cancel_token](const char* data, size_t size) -> bool {
                                // Check cancellation request
                                if (cancel_token && cancel_token->load()) return false;
                                if (data && size > 0) {
                                    try {
                                        os->write(data, size);
                                        if (file_stream && file_stream->is_open())
                                            file_stream->write(data, size);
                                    } catch (...) {
                                        return false; // stop on any exception
                                    }
                                }
                                return true; // Continue download
                            }
                        );

                        // Spawn a lightweight watcher to finalize file and mark buffer complete
                        std::thread watcher_thread([this, os, file_stream, savepath, temp_savepath, streamKey, cancel_token, df = std::move(download_future)]() mutable {
                            try {
                                bool ok = df.get();
                                os.reset(); // Always close output stream (even on error)
                                if (ok) {
                                    if (file_stream && file_stream->is_open()) {
                                        file_stream->flush();
                                        file_stream->close();
                                    }
                                } 
                                if (!savepath.isEmpty()) {
                                    if (ok) {
                                        // Move .part to final
                                        if (QFile::exists(savepath)) {
                                            QFile::remove(savepath);
                                        }
                                        if (!QFile::rename(temp_savepath, savepath)) {
                                            if (QFile::copy(temp_savepath, savepath)) {
                                                QFile::remove(temp_savepath);
                                            } else {
                                                QFile::remove(temp_savepath);
                                            }
                                        }
                                    } else {
                                        // Remove partial on failure
                                        QFile::remove(temp_savepath);
                                    }
                                }
                            } catch (const std::exception& ex) {
                                LOG_ERROR("Streaming finalize error: {}", ex.what());
                            }

                            // Remove cancel token from active list (best-effort)
                            std::lock_guard<std::mutex> lg(m_bgMutex);
                            auto it = std::find_if(m_downloadCancelTokens.begin(), m_downloadCancelTokens.end(),
                                [&cancel_token](const DownloadCancelEntry& e) { return e.token == cancel_token; });
                            if (it != m_downloadCancelTokens.end()) {
                                m_downloadCancelTokens.erase(it);
                                LOG_INFO("Download canceled: {}", cancel_token->load());
                            }
                            
                            // Remove from active streams map
                            {
                                std::lock_guard<std::mutex> lock(m_downloadMapMutex);
                                m_activeStreams.erase(streamKey);
                            }
                        });

                        // Track watcher thread so we can join on shutdown
                        {
                            std::lock_guard<std::mutex> lg(m_bgMutex);
                            m_bgThreads.emplace_back(std::move(watcher_thread));
                        }
                        // Note: The download continues in background, the StreamingInputStream will
                        // block on read() calls until data is available. A watcher thread ensures
                        // buffer completion is signaled and optional file is finalized.
                        
                        LOG_INFO("Streaming started for URL: {}", url);
                        
                        // Set the promise with the input stream and return it
                        promisePtr->set_value(is);
                        return is;
                        
                    } catch (const std::exception& e) {
                        LOG_ERROR("Bilibili streaming error: {}", e.what());
                        
                        // Clean up from active streams map
                        {
                            std::lock_guard<std::mutex> lock(m_downloadMapMutex);
                            m_activeStreams.erase(streamKey);
                        }
                        promisePtr->set_exception(std::current_exception());
                        throw; // Re-throw to be handled by caller
                    }
                }).share();
                
            default:
                // Clean up and return error future
                {
                    std::lock_guard<std::mutex> lock(m_downloadMapMutex);
                    m_activeStreams.erase(streamKey);
                }
                return std::async(std::launch::async, [promisePtr]() -> std::shared_ptr<std::istream> {
                    promisePtr->set_exception(std::make_exception_ptr(std::runtime_error("Unsupported platform for streaming")));
                    throw std::runtime_error("Unsupported platform for streaming");
                }).share();
        }
    }

    bool NetworkManager::cancelDownload(const std::shared_ptr<std::atomic<bool>>& token)
    {
        if (!token) return false;
        std::lock_guard<std::mutex> lg(m_bgMutex);
        auto it = std::find_if(m_downloadCancelTokens.begin(), m_downloadCancelTokens.end(),
            [&token](const DownloadCancelEntry& e) { return e.token == token; });
        if (it == m_downloadCancelTokens.end()) {
            // Not found; still set token to be safe
            token->store(true);
            return false;
        }

        // Atomically request cancel
        it->token->store(true);
        if (std::shared_ptr<RealtimePipe> buf = it->buffer) {
            // wake blocked readers/writers
            buf->close();
        }

        // Erase the entry from the active list; caller may still hold token
        m_downloadCancelTokens.erase(it);
        return true;
    }

    bool NetworkManager::checkPlatformStatus(SupportInterface platform)
    {
        switch(platform)
        {
            case SupportInterface::Bilibili:
                return m_biliInterface != nullptr;
            default:
                return false;
        }
    }

    std::future<QList<SearchResult>> NetworkManager::performBilibiliSearchAsync(
        const QString &keyword, int maxResults, std::atomic<bool> &cancelFlag)
    {
        if (cancelFlag.load()) {
            return {};
        }
        if (checkPlatformStatus(SupportInterface::Bilibili) == false) {
            return std::async(std::launch::async, [keyword]() -> QList<SearchResult> {
                LOG_ERROR("Bilibili interface not configured for search, keyword: {}", keyword.toStdString());
                throw std::runtime_error("Bilibili interface not configured for search");
            });
        }

        auto fut = std::async(std::launch::async, [this, keyword, maxResults, &cancelFlag]() -> QList<SearchResult> {
            try {
                // Cast to concrete type to access platform-specific searchByTitle
                auto* biliInterface = dynamic_cast<BilibiliNetworkInterface*>(m_biliInterface.get());
                if (!biliInterface) {
                    throw std::runtime_error("Invalid Bilibili interface");
                }
                
                // New searchByTitle already combines searching and getting page info
                auto pages = biliInterface->searchByTitle(keyword.toStdString()); // Returns BilibiliPageInfo with enriched metadata
                
                QList<SearchResult> result;
                for (const auto& page : pages) {
                    if (cancelFlag.load()) {
                        return {};
                    }
                    // Generate cover filename from bvid (unique identifier)
                    QString coverFilename = QString("%1.jpg").arg(QString::fromStdString(page.bvid));
                    
                    SearchResult pageResult {
                        .title = QString::fromStdString(fmt::format("{} - {}", page.title, page.partTitle)),
                        .uploader = QString::fromStdString(page.author),
                        .platform = SupportInterface::Bilibili,
                        .duration = page.duration,
                        .coverUrl = QString::fromStdString(page.cover),
                        .coverImg = coverFilename,
                        .description = QString::fromStdString(page.description.substr(0, std::min<size_t>(200, page.description.length()))),
                        .interfaceData = QString("bvid=%1&cid=%2")
                            .arg(QString::fromStdString(page.bvid))
                            .arg(QString::number(page.cid)),
                    };
                    
                    LOG_DEBUG("Bilibili search result - Title: {}, Uploader: {}, CID: {}, Duration: {}",
                              pageResult.title.toStdString(),
                              pageResult.uploader.toStdString(),
                              page.cid,
                              Seconds2HMS(pageResult.duration));
                    
                    result.append(pageResult);
                    
                    if (result.size() >= maxResults) {
                        break; // Reached max results
                    }
                }

                return result;
            } catch (const std::exception& e) {
                LOG_ERROR("Bilibili search error: {}", e.what());
                throw; // Re-throw to be handled by caller
            }
        });
        return fut;
    }

    // Helper method to convert seconds to hh:mm:ss format, hide hours if zero
    std::string NetworkManager::Seconds2HMS(int totalSeconds)
    {
        int hours = totalSeconds / 3600;
        int minutes = (totalSeconds % 3600) / 60;
        int seconds = totalSeconds % 60;

        if (hours > 0) {
            return fmt::format("{}:{:02}:{:02}", hours, minutes, seconds);
        }
        return fmt::format("{}:{:02}", minutes, seconds);
    }

    std::future<void> NetworkManager::monitorSearchFuturesWithCV(const QString& keyword, std::vector<std::future<void>>&& futures)
    {
        // Launch a monitoring thread that uses condition variables instead of polling
        return std::async(std::launch::async, [this, keyword, futures = std::move(futures)]() mutable {
            
            for (auto& future : futures) {
                if (m_cancelFlag.load()) {
                    break;
                }
                
                try {
                    // This blocks until the future completes (no CPU spinning!)
                    future.get(); // This will re-throw any exception from the future
                    
                } catch (const std::exception& e) {
                    // Exception handling: convert and emit error signal
                    QMetaObject::invokeMethod(this, [this, keyword, e]() {
                        emit searchFailed(keyword, QString("Search error: %1").arg(e.what()));
                    }, Qt::QueuedConnection);
                    
                } catch (...) {
                    // Handle unknown exceptions
                    QMetaObject::invokeMethod(this, [this, keyword]() {
                        emit searchFailed(keyword, "Unknown search error occurred");
                    }, Qt::QueuedConnection);
                }
            }
            
            // All futures completed successfully
            if (!m_cancelFlag.load()) {
                QMetaObject::invokeMethod(this, [this, keyword]() {
                    emit searchCompleted(keyword);
                }, Qt::QueuedConnection);
            }
        });
    }
}