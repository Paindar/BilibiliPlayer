#include "network_manager.h"
#include "../log/log_manager.h"
#include <future>
#include <thread>
#include <chrono>
#include <QUrl>

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
        cancelAllSearches();
        LOG_DEBUG("NetworkManager destroyed");
    }
    
    NetworkManager& NetworkManager::instance()
    {
        static NetworkManager instance;
        return instance;
    }
    
    void NetworkManager::configure(const QString& apiBaseUrl, int timeoutMs, const QString& proxyUrl)
    {
        LOG_INFO("Configuring NetworkManager - timeout: {}ms, proxy: {}", timeoutMs, proxyUrl.toStdString());
        
        // Update timeout
        setRequestTimeout(timeoutMs);
        
        // Configure Bilibili interface
        if (m_biliInterface) {
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
                        QList<SearchResult> convertedResults = convertBilibiliResults(biliResults);
                        emit searchProgress(keyword, convertedResults);
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
    
    std::future<std::vector<BilibiliVideoInfo>> NetworkManager::performBilibiliSearchAsync(
            const QString& keyword, int maxResults, std::atomic<bool>& cancelFlag)
    {
        return std::async(std::launch::async, [this, keyword, maxResults, &cancelFlag]() -> std::vector<BilibiliVideoInfo> {
            try {
                if (m_biliInterface->isConnected() == false) {
                    bool connected = m_biliInterface->connect();
                    if (!connected) {
                        throw std::runtime_error("Failed to connect to Bilibili API");
                    }
                }
                if (cancelFlag.load()) {
                    return {};
                }
                
                // This is the blocking network call - runs in worker thread
                auto searchResults = m_biliInterface->searchByTitle(keyword.toStdString(), maxResults);
                
                if (cancelFlag.load()) {
                    return {};
                }
                
                return searchResults;
                
            } catch (const std::exception& e) {
                LOG_ERROR("Bilibili search error: {}", e.what());
                throw; // Re-throw to be handled by caller
            }
        });
    }
    
    QList<SearchResult> NetworkManager::convertBilibiliResults(const std::vector<BilibiliVideoInfo>& results)
    {
        QList<SearchResult> convertedResults;
        
        for (const auto& biliResult : results) {
            SearchResult result;
            result.title = QUrl::fromPercentEncoding(QByteArray::fromStdString(biliResult.title));
            result.uploader = QUrl::fromPercentEncoding(QByteArray::fromStdString(biliResult.author));
            result.platform = "Bilibili";
            // result.duration = QString::fromStdString(biliResult.duration);
            // result.url = QString::fromStdString(biliResult.url);
            // result.coverUrl = QString::fromStdString(biliResult.cover_url);
            result.description = QUrl::fromPercentEncoding(QByteArray::fromStdString(biliResult.description));
            // result.viewCount = biliResult.view_count;
            // result.publishDate = QString::fromStdString(biliResult.publish_date);
            
            convertedResults.append(result);
        }
        
        return convertedResults;
    }
    
    void NetworkManager::monitorSearchFuturesWithCV(const QString& keyword, std::vector<std::future<void>>&& futures)
    {
        // Launch a monitoring thread that uses condition variables instead of polling
        std::async(std::launch::async, [this, keyword, futures = std::move(futures)]() mutable {
            
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