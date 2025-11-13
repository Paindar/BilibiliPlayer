#pragma once

#include <string>
#include <vector>
#include <future>
#include <functional>
#include <cstdint>

namespace network
{
    /**
     * @brief Abstract base interface for platform-specific network operations
     * 
     * This interface defines the contract that all platform implementations
     * (Bilibili, YouTube, etc.) must follow. It provides methods for:
     * - Configuration management
     * - Content search
     * - Audio stream URL retrieval
     * - Stream size queries
     * - Asynchronous streaming downloads
     */
    class INetworkPlatformInterface
    {
    public:
        virtual ~INetworkPlatformInterface() = default;

        // ========== Configuration Methods ==========
        
        /**
         * @brief Initialize with default configuration
         * @return true if initialization successful, false otherwise
         */
        virtual bool initializeDefaultConfig() = 0;

        /**
         * @brief Load configuration from file
         * @param config_file Path to configuration file (relative to platform directory)
         * @return true if loaded successfully, false otherwise
         */
        virtual bool loadConfig(const std::string& config_file) = 0;

        /**
         * @brief Save current configuration to file
         * @param config_file Path to configuration file (relative to platform directory)
         * @return true if saved successfully, false otherwise
         */
        virtual bool saveConfig(const std::string& config_file) = 0;

        /**
         * @brief Set the platform-specific configuration directory
         * @param platform_dir Absolute path to platform directory
         * @return true if directory is valid and set, false otherwise
         */
        virtual bool setPlatformDirectory(const std::string& platform_dir) = 0;

        /**
         * @brief Set timeout for network requests
         * @param timeout_sec Timeout in seconds
         */
        virtual void setTimeout(int timeout_sec) = 0;

        /**
         * @brief Set User-Agent header for HTTP requests
         * @param user_agent User-Agent string
         */
        virtual void setUserAgent(const std::string& user_agent) = 0;

        // ========== Search & Query Methods ==========
        // Note: searchByTitle is intentionally NOT in the interface because it returns
        // platform-specific types (BilibiliPageInfo, YouTubeVideoInfo, etc.).
        // Each concrete implementation provides its own searchByTitle method,
        // and NetworkManager converts results to network::SearchResult.

        // ========== Stream URL Methods ==========

        /**
         * @brief Get audio stream URL from platform-specific parameters
         * @param params Platform-specific parameters (e.g., JSON string with bvid/cid for Bilibili)
         * @return Direct audio stream URL, or empty string on failure
         */
        virtual std::string getAudioUrlByParams(const std::string& params) = 0;

        /**
         * @brief Get byte size of stream from URL
         * @param url Direct stream URL
         * @return Size in bytes, or 0 if unavailable
         */
        virtual uint64_t getStreamBytesSize(const std::string& url) = 0;

        // ========== Async Streaming Methods ==========

        /**
         * @brief Asynchronously download and stream content
         * @param url Direct stream URL to download
         * @param content_receiver Callback invoked with chunks: bool(const char* data, size_t size)
         *                         Return true to continue, false to cancel
         * @param progress_callback Optional progress callback: bool(uint64_t current, uint64_t total)
         *                          Return true to continue, false to cancel
         * @return Future that resolves to true on complete download, false on error/cancellation
         */
        virtual std::future<bool> asyncDownloadStream(
            const std::string& url,
            std::function<bool(const char*, size_t)> content_receiver,
            std::function<bool(uint64_t, uint64_t)> progress_callback = nullptr) = 0;
    };
}
