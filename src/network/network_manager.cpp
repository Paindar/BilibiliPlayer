#include "network_manager.h"
#include "../log/log_manager.h"
#include <future>
#include <thread>
#include <chrono>
#include <fstream>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>

namespace network
{
    NetworkManager::NetworkManager()
        : QObject(nullptr)
        , m_configured(false)
    {
        // Initialize BilibiliNetworkInterface
        m_biliInterface = std::make_unique<BilibiliNetworkInterface>();
        m_biliInterface->setTimeout(m_requestTimeout / 1000); // Convert to seconds
        LOG_DEBUG("NetworkManager created");
    }
    
    NetworkManager::~NetworkManager()
    {
        // Signal cancel for searches
        cancelAllSearches();

        // Signal cancel for any active downloads and notify associated buffers
        {
            std::lock_guard<std::mutex> lg(m_bgMutex);
            for (auto &entry : m_downloadCancelTokens) {
                if (entry.token) entry.token->store(true);
                if (auto buf = entry.buffer.lock()) {
                    buf->destroy();
                }
            }
        }

        // Optionally disconnect the underlying interface to accelerate socket shutdown
        if (m_biliInterface) {
            m_biliInterface->disconnect();
        }

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
    
    NetworkManager& NetworkManager::instance()
    {
        static NetworkManager instance;
        return instance;
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
        }
        
        // TODO: Configure API base URL and proxy if needed by underlying interfaces
        
        m_configured = true;
        LOG_INFO("NetworkManager configuration complete");
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

    std::future<uint64_t> NetworkManager::getStreamSizeAsync(SupportInterface platform, const QString &url)
    {
        switch (platform) {
            case SupportInterface::Bilibili:
                return std::async(std::launch::async, [this, url]() -> uint64_t {
                    try {
                        if (!m_biliInterface->isConnected()) {
                            bool connected = m_biliInterface->connect();
                            if (!connected) {
                                throw std::runtime_error("Failed to connect to Bilibili API");
                            }
                        }
                        return m_biliInterface->getStreamBytesSize(url.toStdString());
                    } catch (const std::exception& e) {
                        LOG_ERROR("Bilibili stream size query error: {}", e.what());
                        throw; // Re-throw to be handled by caller
                    }
                });
                
            default:
                // Return a future that immediately throws an exception
                return std::async(std::launch::async, []() -> uint64_t {
                    throw std::runtime_error("Unsupported platform for stream size query");
                });
        }
    }

    std::future<uint64_t> NetworkManager::getStreamSizeByParamsAsync(SupportInterface platform, const QString &params)
    {
        switch (platform) {
            case SupportInterface::Bilibili:
                return std::async(std::launch::async, [this, params]() -> uint64_t {
                    try {
                        if (!m_biliInterface->isConnected()) {
                            bool connected = m_biliInterface->connect();
                            if (!connected) {
                                throw std::runtime_error("Failed to connect to Bilibili API");
                            }
                        }
                        std::string url = m_biliInterface->getUrlByParams(params.toStdString());
                        if (url.empty()) {
                            throw std::runtime_error("Failed to construct URL from params");
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

    std::future<void> NetworkManager::downloadAsync(SupportInterface platform, const QString &url, const QString &filepath)
    {
        switch (platform) {
            case SupportInterface::Bilibili:
                return std::async(std::launch::async, [this, url, filepath]() -> void {
                    try {
                        if (!m_biliInterface->isConnected()) {
                            bool connected = m_biliInterface->connect();
                            if (!connected) {
                                throw std::runtime_error("Failed to connect to Bilibili API");
                            }
                        }
                        
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
                            throw std::runtime_error("Download failed");
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
                                throw std::runtime_error("Failed to finalize download file move");
                            }
                        }
                        
                        LOG_INFO("Download completed successfully: {}", filepath.toStdString());
                        
                    } catch (const std::exception& e) {
                        LOG_ERROR("Bilibili download error: {}", e.what());
                        throw; // Re-throw to be handled by caller
                    }
                });
                
            default:
                // Return a future that immediately throws an exception
                return std::async(std::launch::async, []() -> void {
                    throw std::runtime_error("Unsupported platform for download");
                });
        }
    }

    std::future<std::shared_ptr<StreamingInputStream>> NetworkManager::getAudioStreamAsync(SupportInterface platform, const QString& params, const QString& savepath)
    {
        switch (platform) {
            case SupportInterface::Bilibili:

                return std::async(std::launch::async, [this, params, savepath]() -> std::shared_ptr<StreamingInputStream> {
                    try {
                        if (!m_biliInterface->isConnected()) {
                            bool connected = m_biliInterface->connect();
                            if (!connected) {
                                throw std::runtime_error("Failed to connect to Bilibili API");
                            }
                        }
                        
                        // Create streaming buffer (5MB buffer for smooth streaming)
                        const size_t buffer_size = 5 * 1024 * 1024;
                        // Create streaming input that wraps the buffer
                        auto streaming_buffer = std::make_shared<StreamingAudioBuffer>(buffer_size);
                        auto streaming_input = std::make_shared<StreamingInputStream>(streaming_buffer);
                        std::shared_ptr<std::ofstream> file_stream;
                        QString temp_savepath;
                        if (!savepath.isEmpty()) {
                            temp_savepath = savepath + ".part";
                            // Ensure parent directory exists
                            QFileInfo fi(savepath);
                            QDir().mkpath(fi.absolutePath());
                            file_stream = std::make_shared<std::ofstream>(temp_savepath.toStdString(), std::ios::binary);
                            if (!file_stream->is_open()) {
                                throw std::runtime_error("Failed to open file for writing: " + temp_savepath.toStdString());
                            }
                        }
                        // Get actual URL from params
                        std::string url = m_biliInterface->getUrlByParams(params.toStdString());
                        if (url.empty()) {
                            throw std::runtime_error("Failed to get audio URL from parameters");
                        }
                        // Start async download to the streaming buffer with a cancel token
                        auto cancel_token = std::make_shared<std::atomic<bool>>(false);
                        {
                            std::lock_guard<std::mutex> lg(m_bgMutex);
                            m_downloadCancelTokens.push_back({ cancel_token, streaming_buffer });
                        }

                        auto download_future = m_biliInterface->asyncDownloadStream(url,
                            [streaming_buffer, file_stream, cancel_token](const char* data, size_t size) -> bool {
                                // Check cancellation request
                                if (cancel_token && cancel_token->load()) return false;
                                if (data && size > 0) {
                                    // Write to streaming buffer (blocking until space available or destroyed)
                                    streaming_buffer->write(data, size);
                                    if (file_stream && file_stream->is_open()) {
                                        file_stream->write(data, size);
                                    }
                                }
                                return true; // Continue download
                            }
                        );

                        // Spawn a lightweight watcher to finalize file and mark buffer complete
                        std::thread watcher_thread([this, streaming_buffer, file_stream, savepath, temp_savepath, cancel_token, df = std::move(download_future)]() mutable {
                            try {
                                bool ok = df.get();
                                if (file_stream && file_stream->is_open()) {
                                    file_stream->flush();
                                    file_stream->close();
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
                            // In any case, mark buffer completion to unblock readers
                            streaming_buffer->setDownloadComplete();

                            // Remove cancel token from active list (best-effort)
                            try {
                                std::lock_guard<std::mutex> lg(m_bgMutex);
                                auto it = std::find_if(m_downloadCancelTokens.begin(), m_downloadCancelTokens.end(),
                                    [&cancel_token](const DownloadCancelEntry& e) { return e.token == cancel_token; });
                                if (it != m_downloadCancelTokens.end()) m_downloadCancelTokens.erase(it);
                            } catch (...) {
                                // ignore
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
                        return streaming_input;
                        
                    } catch (const std::exception& e) {
                        LOG_ERROR("Bilibili streaming error: {}", e.what());
                        throw; // Re-throw to be handled by caller
                    }
                });
                
            default:
                // Return a future that immediately throws an exception
                return std::async(std::launch::async, []() -> std::shared_ptr<StreamingInputStream> {
                    throw std::runtime_error("Unsupported platform for streaming");
                });
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
        if (auto buf = it->buffer.lock()) {
            // wake blocked readers/writers
            buf->destroy();
        }

        // Erase the entry from the active list; caller may still hold token
        m_downloadCancelTokens.erase(it);
        return true;
    }

    std::future<QList<SearchResult>> NetworkManager::performBilibiliSearchAsync(
        const QString &keyword, int maxResults, std::atomic<bool> &cancelFlag)
    {
        if (!m_biliInterface->isConnected()) {
            bool connected = m_biliInterface->connect();
            if (!connected) {
                throw std::runtime_error("Failed to connect to Bilibili API");
            }
        }
        if (cancelFlag.load()) {
            return {};
        }

        auto fut = std::async(std::launch::async, [this, keyword, maxResults, &cancelFlag]() -> QList<SearchResult> {
            try {
                // Phase 1: searching by title
                auto searchResults = m_biliInterface->searchByTitle(keyword.toStdString(), 1); // Blocking call
                // Phase 2: for each result, get additional page info
                std::vector<std::future<QList<SearchResult>>> pageFutures;
                std::mutex resultMutex;
                QList<SearchResult> result;
                for (auto& bv: searchResults) {
                    if (cancelFlag.load()) {
                        return {};
                    }
                    auto pageFut = std::async([this, bv]() -> QList<SearchResult>{
                        auto pages = m_biliInterface->getPagesCid(bv.bvid); // Blocking call
                        QList<SearchResult> pageResults;
                        for (const auto& page : pages) {
                            SearchResult pageResult {
                                .title = QString::fromStdString(bv.title),
                                .uploader = QString::fromStdString(bv.author),
                                .platform = SupportInterface::Bilibili,
                                .duration = Seconds2HMS(page.duration),
                                .coverUrl = QString::fromStdString(page.first_frame),
                                .description = QString::fromStdString(bv.description.substr(0, 200)),
                                .interfaceData = QString("bvid=%1&cid=%2")
                                    .arg(QString::fromStdString(bv.bvid))
                                    .arg(QString::number(page.cid)),
                            };
                            LOG_DEBUG("Bilibili search result - Title: {}, Uploader: {}, CID: {}, Duration: {}",
                                      pageResult.title.toStdString(),
                                      pageResult.uploader.toStdString(),
                                      page.cid,
                                      pageResult.duration.toStdString());
                            pageResults.append(pageResult);
                        }
                        return pageResults;
                    });
                    pageFutures.push_back(std::move(pageFut));
                }

                // Wait for all page futures to complete and collect results
                for (auto& pageFut : pageFutures) {
                    if (cancelFlag.load()) {
                        return {};
                    }
                    try
                    {
                        auto pageResults = pageFut.get(); // Blocking call
                        if (pageResults.size() == 0) {
                            continue;
                        } else if (pageResults.size() >= static_cast<size_t>(maxResults)) {
                            pageResults.resize(maxResults); // Trim to maxResults
                        }
                        result.append(pageResults);
                    }
                    catch(const std::exception& e)
                    {
                        LOG_ERROR("Error retrieving page results: {}", e.what());
                        throw;
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
    QString NetworkManager::Seconds2HMS(int totalSeconds)
    {
        int hours = totalSeconds / 3600;
        int minutes = (totalSeconds % 3600) / 60;
        int seconds = totalSeconds % 60;

        if (hours > 0) {
            return QString("%1:%2:%3").arg(hours).arg(minutes).arg(seconds);
        }
        return QString("%1:%2").arg(minutes).arg(seconds);
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