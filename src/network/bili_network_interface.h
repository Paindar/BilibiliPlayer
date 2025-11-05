#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <chrono>
#include <future>
#include <httplib.h>
#include <json/json.h>
#include <mutex>
#include <optional>

namespace network
{
    struct BilibiliVideoInfo {
        std::string title;
        std::string bvid;
        std::string author;
        std::string description;
    };

    struct BilibiliPageInfo {
        int64_t cid; // cid
        int page; // page number
        std::string part; // part title
        int duration; // duration in seconds
        std::string first_frame; // page cover
    };

    

    class BilibiliNetworkInterface 
    {
    public:
        // Constructor/Destructor
        BilibiliNetworkInterface();
        ~BilibiliNetworkInterface();
        bool initializeDefaultConfig();
        // Configuration loading/saving
        bool loadConfig(const std::string& config_file = "bilibili.json");
        bool saveConfig(const std::string& config_file = "bilibili.json");
        
        // Configuration
        bool setPlatformDirectory(const std::string& platform_dir);
        void setTimeout(int timeout_sec);
        void setUserAgent(const std::string& user_agent);

        // Bilibili API
        [[deprecated]] std::vector<BilibiliPageInfo> getPagesCid(const std::string& bvid);
    public: // interface implement
        // Search videos by title
        std::vector<BilibiliVideoInfo> searchByTitle(const std::string& title, int page = 1);
        // Get audio URL by interface parameters (e.g., bvid/cid)
        std::string getAudioUrlByParams(const std::string& params);
        // Get stream size by URL
        uint64_t getStreamBytesSize(const std::string& url);
        // Async streaming - returns immediately, streams in background thread
        std::future<bool> asyncDownloadStream(const std::string& url,
                                            std::function<bool(const char*, size_t)> content_receiver,
                                            std::function<bool(uint64_t, uint64_t)> progress_callback = nullptr);

#ifdef UNIT_TEST
    // Test helpers (only present when UNIT_TEST is defined at compile time)
    // Create `n` clients for the given host by borrowing/returning under the
    // client mutex. Useful to populate the pool for tests.
    void test_create_clients(const std::string& host, size_t n);
    // Return the total number of clients recorded for a host
    size_t test_pool_size(const std::string& host) const;
    // Run concurrent borrow/return operations across threads; returns true if no exceptions
    bool test_concurrent_borrow_return(const std::string& host, size_t thread_count, size_t iter_count);
        // Cookie test helpers
        // Parse a single Set-Cookie header (as received from origin_host) and add to the jar
        void test_add_cookie_from_set_cookie(const std::string& set_cookie_line, const std::string& origin_host);
        // Build headers for a full URL (scheme+host+path) for testing cookie selection
        httplib::Headers test_get_headers_for_url(const std::string& url);
#endif
    private: // Struct
        struct BiliWbiKeys {
            std::string img_key;
            std::string sub_key;
            std::chrono::system_clock::time_point last_update;
        };
    private:
        /************* Client request related *************/
        // WBI signature methods, this method is thread-safe
        std::string getMixinKey(const std::string& orig) const;
        std::string getCurrentTimestamp() const;
        // Require wbi mutex
        bool encWbi_unsafe(std::unordered_map<std::string, std::string>& params) const;
        bool needsWbiRefresh_unsafe() const;
        // Require wbi mutex AND client mutex (caller must hold both when invoking)
        bool refreshWbiKeys_unsafe();

        // HTTP helper methods
        const httplib::Headers& getDefaultHeaders() const;
        // Require client mutex
        // Build request headers (including Cookie) appropriate for the target host/path.
        // Caller must hold m_clientMutex_. The host should include scheme (e.g. https://api.bilibili.com)
        httplib::Headers getHttplibHeaders_unsafe(const std::string& host, const std::string& path) const;
        // Require client mutex
        bool initializeCookies_unsafe();
        // Require client mutex
        std::shared_ptr<httplib::Client> borrowHttpClient_unsafe(const std::string& host);
        // Require client mutex
        void returnHttpClient_unsafe(std::shared_ptr<httplib::Client> client);
        // Require client mutex
        bool sendGetRequest_unsafe(const std::string& host, 
                            const std::string& path,
                            const std::unordered_map<std::string, std::string>& params,
                            std::string& response);
        std::vector<BilibiliPageInfo> getPagesCid_unsafe(const std::string& bvid);
        // Require client mutex
        void updateClientCookies_unsafe();
        /************* Configuration related *************/
        bool loadHeaders_unsafe(const Json::Value& jsonObj);
        bool saveHeaders_unsafe(Json::Value& jsonObj) const;
        bool loadCookies_unsafe(const Json::Value& jsonObj);
        bool saveCookies_unsafe(Json::Value& jsonObj) const;
        bool loadWbiKeys_unsafe(const Json::Value& jsonObj);
        bool saveWbiKeys_unsafe(Json::Value& jsonObj) const;
        // Utility methods
        bool parseUrl(const std::string& url, std::string& host, std::string& path) const;
    private:
        // HTTP client instance
        mutable std::mutex m_clientMutex_;
        std::unordered_map<std::string, std::string> headers_;
        // Structured cookie jar. Use a simple vector of Cookie entries keyed by
        // domain/path for selection. Access must be guarded by m_clientMutex_.
        struct Cookie {
            std::string name;
            std::string value;
            std::string domain; // empty = host-only
            std::string cookie_path;
            std::optional<std::chrono::system_clock::time_point> expires;
            bool secure = false;
            bool httpOnly = false;
            std::string sameSite;
        };
        std::vector<Cookie> cookies_;
        struct ClientEntry {
            std::string host;
            std::shared_ptr<httplib::Client> client;
            bool available = true;
        };
        // Map from host -> list of client entries for that host. Using a hashmap
        // makes lookups for a given host O(1) average and groups clients by host.
        std::unordered_map<std::string, std::vector<ClientEntry>> http_clients;
        
        // WBI signature keys
        mutable std::mutex m_wbiMutex_;
        BiliWbiKeys wbi_keys_;
        
        // Configuration
        mutable std::mutex m_configureMutex_;
        std::string platform_dir_; // where to load/save config files
        int timeout_seconds_; // request timeout in seconds
        bool follow_location_; 
    };
} // namespace network