#pragma once

#include <QObject>
#include <memory>
#include <future>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <stream/realtime_pipe.hpp>
#include "bili_network_interface.h"

namespace network
{
    enum SupportInterface {
        Bilibili = 0x1,
        // Future platforms can be added here
        All = 0xFFFFFFFF
    };

    // Search result structure
    struct SearchResult {
        QString title;
        QString uploader;
        SupportInterface platform;
        QString duration;
        QString coverUrl;
        QString description;

        // Reserved for raw interface data storage
        QString interfaceData;
    };
}

Q_DECLARE_METATYPE(network::SearchResult)

namespace network
{
    class NetworkManager : public QObject
    {
        Q_OBJECT
        
    public:
        NetworkManager();
        ~NetworkManager();
        // Configuration
        void configure(const QString& platformDir, int timeoutMs = 10000, const QString& proxyUrl = "");
        void saveConfiguration();
        bool isConfigured() const { return m_configured; }
        
        // Main search orchestrator method - coordinates multiple async searches
        void executeMultiSourceSearch(const QString& keyword, uint selectedInterface, int maxResults = 20);
        void cancelAllSearches();
        // Convenience: get size by interface params (e.g., bvid/cid) instead of direct URL
        std::future<uint64_t> getStreamSizeByParamsAsync(SupportInterface platform, const QString& params);
        std::future<std::shared_ptr<std::istream>> getAudioStreamAsync(SupportInterface platform, const QString& params, const QString& savepath="");
        std::future<void> downloadAsync(SupportInterface platform, const QString& url, const QString& filepath);
        
        void setRequestTimeout(int timeoutMs) { m_requestTimeout = timeoutMs; }
        int getRequestTimeout() const { return m_requestTimeout; }
        
    signals:
        void searchCompleted(const QString& keyword);
        void searchFailed(const QString& keyword, const QString& errorMessage);
        void searchProgress(const QString& keyword, const QList<SearchResult>& results);
        
    private:
        
        NetworkManager(const NetworkManager&) = delete;
        NetworkManager& operator=(const NetworkManager&) = delete;
        
        // Async search methods for different platforms
        std::future<QList<SearchResult>> performBilibiliSearchAsync(
            const QString& keyword, int maxResults, std::atomic<bool>& cancelFlag);
        
        // Helper methods
        std::future<void> monitorSearchFuturesWithCV(const QString& keyword, std::vector<std::future<void>>&& futures);
        QString Seconds2HMS(int totalSeconds);
        bool cancelDownload(const std::shared_ptr<std::atomic<bool>>& token);
        bool checkPlatformStatus(SupportInterface platform);
    
    private:    
        std::unique_ptr<BilibiliNetworkInterface> m_biliInterface;
        std::atomic<bool> m_cancelFlag;
        std::mutex m_completionMutex;
        std::condition_variable m_completionCV;
        int m_requestTimeout = 10000; // 10 seconds
        bool m_configured = false;
        // Background thread management for streaming/watchers
        mutable std::mutex m_bgMutex;
        std::vector<std::thread> m_bgThreads;
        struct DownloadCancelEntry {
            std::shared_ptr<std::atomic<bool>> token;
            std::shared_ptr<RealtimePipe> buffer;
        };
        std::vector<DownloadCancelEntry> m_downloadCancelTokens;
        // Cancel a specific download by token: sets the token and notifies/destroys the
        // associated buffer so any blocked readers/writers wake. Returns true if an
        // entry was found and cancelled.
    };
}