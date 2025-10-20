#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <chrono>
#include <future>
#include <httplib.h>

struct BilibiliVideoInfo {
    std::string title;
    std::string bvid;
    std::string author;
    std::string description;
};

struct BilibiliPageInfo {
    int64_t cid;
    int page;
    std::string part;
    int duration;
};

struct BiliWbiKeys {
    std::string img_key;
    std::string sub_key;
    std::chrono::system_clock::time_point last_update;
};

class BilibiliNetworkInterface 
{
public:
    // Constructor/Destructor
    BilibiliNetworkInterface();
    ~BilibiliNetworkInterface();
    
    // Connection management
    bool connect(const std::string& base_url = "https://api.bilibili.com");
    void disconnect();
    bool isConnected() const;
    
    // Configuration loading/saving
    bool loadConfig(const std::string& config_file = "userinfo.json");
    bool saveConfig(const std::string& config_file = "userinfo.json");
    
    // Authentication and cookies
    bool initializeCookies();
    bool refreshWbiKeys();
    
    // Bilibili-specific API methods
    std::vector<BilibiliVideoInfo> searchByTitle(const std::string& title, int page = 1);
    std::vector<BilibiliPageInfo> getPagesCid(const std::string& bvid);
    std::string getAudioLink(const std::string& bvid, int64_t cid);
    
    // Streaming operations
    uint64_t getStreamBytesSize(const std::string& url);
    // Async streaming - returns immediately, streams in background thread
    std::future<bool> asyncDownloadStream(const std::string& url,
                                        std::function<bool(const char*, size_t)> content_receiver,
                                        std::function<bool(uint64_t, uint64_t)> progress_callback = nullptr);
    
    bool streamBilibiliAudio(const std::string& bvid, int64_t cid,
                           std::function<bool(const char*, size_t)> content_receiver);
    
    // Header management
    void setHeader(const std::string& key, const std::string& value);
    void removeHeader(const std::string& key);
    void clearHeaders();
    
    // Configuration
    void setTimeout(int timeout_sec);
    void setFollowLocation(bool follow);
    void setUserAgent(const std::string& user_agent);

    // Cookie management
    void syncCookiesFromHttplib();
    void syncCookiesToHttplib();
    void addCookie(const std::string& name, const std::string& value);
    void removeCookie(const std::string& name);
    void clearCookies();
    bool saveCookiesToFile(const std::string& filename) const;
    bool loadCookiesFromFile(const std::string& filename);
    
    // Debug
    std::unordered_map<std::string, std::string> getCurrentHeaders() const;
    std::unordered_map<std::string, std::string> getCurrentCookies() const;

private:
    // HTTP client instance
    std::unique_ptr<httplib::Client> http_client_;
    
    // Request headers and cookies
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> cookies_;
    
    // WBI signature keys
    BiliWbiKeys wbi_keys_;
    
    // Configuration
    std::string base_url_;
    std::string config_file_;
    int timeout_seconds_;
    bool follow_location_;
    bool connected_;
    
    // WBI signature methods
    std::string getMixinKey(const std::string& orig) const;
    std::unordered_map<std::string, std::string> encWbi(
        std::unordered_map<std::string, std::string> params) const;
    bool getWbiKeys();
    
    // HTTP helper methods
    httplib::Headers getHttplibHeaders() const;
    std::string getCookieString() const;
    bool makeSignedRequest(const std::string& path, 
                          const std::unordered_map<std::string, std::string>& params,
                          std::string& response);
    
    // Cookie helper methods
    struct ParsedCookie {
        std::string name;
        std::string value;
        std::string domain;
        std::string path;
        std::string expires;
        bool secure = false;
        bool httpOnly = false;
        std::string sameSite;
    };
    ParsedCookie parseCookieHeader(const std::string& cookie_header) const;
    void updateClientCookies();
    
    // Utility methods
    bool isValidUrl(const std::string& url) const;
    bool parseUrl(const std::string& url, std::string& host, std::string& path) const;
    void initializeDefaultHeaders();
    bool needsWbiRefresh() const;
    std::string urlEncode(const std::string& str) const;
    std::string md5Hash(const std::string& str) const;
    std::string getCurrentTimestamp() const;
};