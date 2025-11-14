#pragma once

#include <QObject>
#include <memory>
#include <future>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <stream/realtime_pipe.hpp>
#include "platform/i_platform.h"
#include "platform/bili_network_interface.h"

// Forward declarations for platform-specific types
namespace network
{
    class BilibiliPlatform;
}
Q_DECLARE_METATYPE(network::SearchResult)

namespace network
{
    class NetworkManager : public QObject, public std::enable_shared_from_this<NetworkManager>
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
        std::future<uint64_t> getStreamSizeByParamsAsync(PlatformType platform, const QString& params);
        std::shared_future<std::shared_ptr<std::istream>> getAudioStreamAsync(PlatformType platform, const QString& params, const QString& savepath="");
        std::shared_future<void> downloadAsync(PlatformType platform, const QString& url, const QString& filepath);
        
        void setRequestTimeout(int timeoutMs) { m_requestTimeout = timeoutMs; }
        int getRequestTimeout() const { return m_requestTimeout; }
        
    signals:
        void searchCompleted(const QString& keyword);
        void searchFailed(const QString& keyword, const QString& errorMessage);
        void searchProgress(const QString& keyword, const QList<SearchResult>& results);
        void downloadCompleted(const QString& url, const QString& filepath);
        void downloadFailed(const QString& url, const QString& filepath, const QString& errorMessage);
    private:
        
        NetworkManager(const NetworkManager&) = delete;
        NetworkManager& operator=(const NetworkManager&) = delete;
        
        // Async search methods for different platforms
        std::future<QList<SearchResult>> performBilibiliSearchAsync(
            const QString& keyword, int maxResults, std::atomic<bool>& cancelFlag);
        
        // Helper methods
        std::future<void> monitorSearchFuturesWithCV(const QString& keyword, std::vector<std::future<void>>&& futures);
        std::string Seconds2HMS(int totalSeconds);
        bool cancelDownload(const std::shared_ptr<std::atomic<bool>>& token);
        bool checkPlatformStatus(PlatformType platform);
    
    private:    
        std::shared_ptr<BilibiliPlatform> m_biliInterface;
        std::atomic<bool> m_cancelFlag;
        std::atomic<bool> m_exitFlag{false};
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
        
        // Prevent duplicated downloads
        struct ActiveDownload {
            std::shared_ptr<std::promise<void>> promise;
            std::shared_future<void> future;
        };
        std::unordered_map<QString, ActiveDownload> m_activeDownloads;
        
        struct ActiveStream {
            std::shared_ptr<std::promise<std::shared_ptr<std::istream>>> promise;
            std::shared_future<std::shared_ptr<std::istream>> future;
        };
        std::unordered_map<QString, ActiveStream> m_activeStreams;
        mutable std::mutex m_downloadMapMutex;
        
        // Cancel a specific download by token: sets the token and notifies/destroys the
        // associated buffer so any blocked readers/writers wake. Returns true if an
        // entry was found and cancelled.
    };
}