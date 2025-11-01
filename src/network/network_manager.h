#pragma once

#include <QObject>
#include <memory>
#include <future>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <stream/streaming_audio_buffer.h>
#include "bili_network_interface.h"

namespace network
{
    enum SupportInterface {
        Bilibili = 0x1,
        // Future platforms can be added here
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
        static NetworkManager& instance();
        
        // Configuration
        void configure(const QString& platformDir, int timeoutMs = 10000, const QString& proxyUrl = "");
        bool isConfigured() const { return m_configured; }
        
        // Main search orchestrator method - coordinates multiple async searches
        void executeMultiSourceSearch(const QString& keyword, uint selectedInterface, int maxResults = 20);
        void cancelAllSearches();
        std::future<uint64_t> getStreamSizeAsync(SupportInterface platform, const QString& url);
        std::future<std::shared_ptr<StreamingInputStream>> getAudioStreamAsync(SupportInterface platform, const QString& params, const QString& savepath="");
        std::future<void> downloadAsync(SupportInterface platform, const QString& url, const QString& filepath);
        
        void setRequestTimeout(int timeoutMs) { m_requestTimeout = timeoutMs; }
        int getRequestTimeout() const { return m_requestTimeout; }
        
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
        std::future<QList<SearchResult>> performBilibiliSearchAsync(
            const QString& keyword, int maxResults, std::atomic<bool>& cancelFlag);
        
        // Helper methods
        std::future<void> monitorSearchFuturesWithCV(const QString& keyword, std::vector<std::future<void>>&& futures);
        QString Seconds2HMS(int totalSeconds);
        
        std::unique_ptr<BilibiliNetworkInterface> m_biliInterface;
        std::atomic<bool> m_cancelFlag;
        std::mutex m_completionMutex;
        std::condition_variable m_completionCV;
        int m_requestTimeout = 10000; // 10 seconds
        bool m_configured = false;
    };
}