#pragma once

#include <QObject>
#include <memory>
#include <future>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "bili_network_interface.h"

// Search result structure
struct SearchResult {
    QString title;
    QString uploader;
    QString platform;
    QString duration;
    QString url;
    QString coverUrl;
    QString description;
    int viewCount = 0;
    QString publishDate;
};

Q_DECLARE_METATYPE(SearchResult)

namespace network
{
    enum SupportInterface {
        Bilibili = 0x1,
        // Future platforms can be added here
    };
    class NetworkManager : public QObject
    {
        Q_OBJECT
        
    public:
        static NetworkManager& instance();
        
        // Configuration
        void configure(const QString& apiBaseUrl = "", int timeoutMs = 10000, const QString& proxyUrl = "");
        bool isConfigured() const { return m_configured; }
        
        // Main search orchestrator method - coordinates multiple async searches
        void executeMultiSourceSearch(const QString& keyword, uint selectedInterface, int maxResults = 20);
        void cancelAllSearches();

        void setRequestTimeout(int timeoutMs) { m_requestTimeout = timeoutMs; }
        int getRequestTimeout() const { return m_requestTimeout; }
        
        // Get network interface for direct access if needed
        // std::weak_ptr<BilibiliNetworkInterface> getBiliNetworkInterface();
        
    signals:
        void searchCompleted(const QString& keyword);
        void searchFailed(const QString& keyword, const QString& errorMessage);
        void searchProgress(const QString& keyword, const QList<SearchResult>& results);
        
    private:
        NetworkManager();
        ~NetworkManager();
        
        NetworkManager(const NetworkManager&) = delete;
        NetworkManager& operator=(const NetworkManager&) = delete;
        
        // Async search methods for different platforms
        std::future<std::vector<BilibiliVideoInfo>> performBilibiliSearchAsync(
            const QString& keyword, int maxResults, std::atomic<bool>& cancelFlag);
        
        // Future method placeholders for other platforms
        // std::future<std::vector<YouTubeVideoInfo>> performYouTubeSearchAsync(...);
        // std::future<std::vector<LocalVideoInfo>> performLocalSearchAsync(...);
        
        // Helper methods
        QList<SearchResult> convertBilibiliResults(const std::vector<BilibiliVideoInfo>& results);
        void monitorSearchFuturesWithCV(const QString& keyword, std::vector<std::future<void>>&& futures);
        
        std::unique_ptr<BilibiliNetworkInterface> m_biliInterface;
        std::atomic<bool> m_cancelFlag;
        std::mutex m_completionMutex;
        std::condition_variable m_completionCV;
        int m_requestTimeout = 10000; // 10 seconds
        bool m_configured = false;
    };
}