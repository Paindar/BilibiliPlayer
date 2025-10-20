#include "bili_network_interface.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <openssl/md5.h>
#include <json/json.h>

// Mixin key encoding table from Python script
static const std::vector<int> mixinKeyEncTab = {
    46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35, 27, 43, 5, 49,
    33, 9, 42, 19, 29, 28, 14, 39, 12, 38, 41, 13, 37, 48, 7, 16, 24, 55, 40,
    61, 26, 17, 0, 1, 60, 51, 30, 4, 22, 25, 54, 21, 56, 59, 6, 63, 57, 62, 11,
    36, 20, 34, 44, 52
};

BilibiliNetworkInterface::BilibiliNetworkInterface() 
    : timeout_seconds_(10)
    , follow_location_(true)
    , connected_(false)
    , config_file_("config.json") {
    initializeDefaultHeaders();
}

BilibiliNetworkInterface::~BilibiliNetworkInterface() {
    disconnect();
}

bool BilibiliNetworkInterface::connect(const std::string& base_url) {
    base_url_ = base_url;
    
    try {
        http_client_ = std::make_unique<httplib::Client>(base_url_);
        http_client_->set_connection_timeout(timeout_seconds_, 0);
        http_client_->set_read_timeout(timeout_seconds_, 0);
        // http_client_->set_follow_location(follow_location_);
        
        // Enable automatic cookie handling
        http_client_->set_keep_alive(true);
        http_client_->enable_server_certificate_verification(true);
        
        connected_ = true;
        
        // Load configuration if exists
        loadConfig();
        
        // Initialize cookies and WBI keys
        if (cookies_.empty()) {
            initializeCookies();
        }
        
        if (needsWbiRefresh()) {
            refreshWbiKeys();
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Connection failed: " << e.what() << std::endl;
        connected_ = false;
        return false;
    }
}

void BilibiliNetworkInterface::disconnect() {
    if (connected_) {
        saveConfig();
        http_client_.reset();
        connected_ = false;
    }
}

bool BilibiliNetworkInterface::isConnected() const {
    return connected_;
}

void BilibiliNetworkInterface::initializeDefaultHeaders() {
    headers_["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                            "(KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36";
    headers_["Accept"] = "application/json, text/plain, */*";
    headers_["Accept-Language"] = "zh-CN,zh;q=0.9";
    // headers_["Connection"] = "keep-alive";
    headers_["Cookies"] = "";
    headers_["Content-Type"] = "application/x-www-form-urlencoded";
    headers_["Referer"] = "https://www.bilibili.com/";
}

bool BilibiliNetworkInterface::initializeCookies() {
    if (!connected_) return false;
    
    // Create a dedicated client for cookie initialization
    httplib::Client main_client("https://www.bilibili.com");
    main_client.set_keep_alive(true);
    main_client.enable_server_certificate_verification(true);
    
    auto res = main_client.Head("/", getHttplibHeaders());
    
    if (res) {
        if (res->status == 200) {
            
            // Extract and parse Set-Cookie headers properly
            cookies_.clear();
            int cookie_count = 0;
            
            for (const auto& header : res->headers) {
                if (header.first == "Set-Cookie") {
                    cookie_count++;
                    
                    // Simple cookie parsing (name=value; attributes)
                    std::string cookie_str = header.second;
                    size_t eq_pos = cookie_str.find('=');
                    size_t semicolon_pos = cookie_str.find(';');
                    
                    if (eq_pos != std::string::npos) {
                        std::string name = cookie_str.substr(0, eq_pos);
                        std::string value = cookie_str.substr(eq_pos + 1, 
                            semicolon_pos != std::string::npos ? semicolon_pos - eq_pos - 1 : std::string::npos);
                        
                        // Trim whitespace
                        name.erase(0, name.find_first_not_of(" \t"));
                        name.erase(name.find_last_not_of(" \t") + 1);
                        value.erase(0, value.find_first_not_of(" \t"));
                        value.erase(value.find_last_not_of(" \t") + 1);
                        
                        if (!name.empty()) {
                            cookies_[name] = value;
                        }
                    }
                }
            }
            
            return true;
        }
    }
    
    return false;
    return false;
}

std::string BilibiliNetworkInterface::getMixinKey(const std::string& orig) const {
    std::string result;
    for (int i : mixinKeyEncTab) {
        if (i < orig.length()) {
            result += orig[i];
        }
    }
    return result.substr(0, 32);
}

std::unordered_map<std::string, std::string> BilibiliNetworkInterface::encWbi(
    std::unordered_map<std::string, std::string> params) const {
    
    std::string mixin_key = getMixinKey(wbi_keys_.img_key + wbi_keys_.sub_key);
    params["wts"] = getCurrentTimestamp();
    
    // Sort parameters
    std::vector<std::pair<std::string, std::string>> sorted_params(params.begin(), params.end());
    std::sort(sorted_params.begin(), sorted_params.end());
    
    // Filter and encode parameters
    std::string query;
    for (const auto& pair : sorted_params) {
        std::string value = pair.second;
        // Remove unwanted characters
        value.erase(std::remove_if(value.begin(), value.end(), 
            [](char c) { return c == '!' || c == '\'' || c == '(' || c == ')' || c == '*'; }), 
            value.end());
        
        if (!query.empty()) query += "&";
        query += urlEncode(pair.first) + "=" + urlEncode(value);
    }
    
    std::string wbi_sign = md5Hash(query + mixin_key);
    params["w_rid"] = wbi_sign;
    
    return params;
}

bool BilibiliNetworkInterface::getWbiKeys() {
    if (!connected_) return false;
    
    auto res = http_client_->Get("/x/web-interface/nav", getHttplibHeaders());
    
    if (res && res->status == 200) {
        Json::Value root;
        Json::Reader reader;
        
        if (reader.parse(res->body, root)) {
            std::string img_url = root["data"]["wbi_img"]["img_url"].asString();
            std::string sub_url = root["data"]["wbi_img"]["sub_url"].asString();
            
            // Extract keys from URLs
            size_t img_slash = img_url.find_last_of('/');
            size_t img_dot = img_url.find_last_of('.');
            if (img_slash != std::string::npos && img_dot != std::string::npos) {
                wbi_keys_.img_key = img_url.substr(img_slash + 1, img_dot - img_slash - 1);
            }
            
            size_t sub_slash = sub_url.find_last_of('/');
            size_t sub_dot = sub_url.find_last_of('.');
            if (sub_slash != std::string::npos && sub_dot != std::string::npos) {
                wbi_keys_.sub_key = sub_url.substr(sub_slash + 1, sub_dot - sub_slash - 1);
            }
            
            wbi_keys_.last_update = std::chrono::system_clock::now();
            
            return true;
        }
    }
    
    return false;
}

bool BilibiliNetworkInterface::refreshWbiKeys() {
    return getWbiKeys();
}

std::vector<BilibiliVideoInfo> BilibiliNetworkInterface::searchByTitle(const std::string& title, int page) {
    std::vector<BilibiliVideoInfo> results;
    
    if (!connected_) return results;
    
    std::unordered_map<std::string, std::string> params = {
        {"search_type", "video"},
        {"keyword", title},
        {"page", std::to_string(page)}
    };
    
    auto signed_params = encWbi(params);
    
    std::string response;
    if (makeSignedRequest("/x/web-interface/wbi/search/type", signed_params, response)) {
        Json::Value root;
        Json::Reader reader;
        
        if (reader.parse(response, root) && root["data"]["result"].isArray()) {
            for (const auto& item : root["data"]["result"]) {
                BilibiliVideoInfo info;
                info.title = item["title"].asString();
                info.bvid = item["bvid"].asString();
                info.author = item["author"].asString();
                info.description = item["description"].asString();
                
                // Clean up HTML tags from title
                std::regex html_tag("<[^>]*>");
                info.title = std::regex_replace(info.title, html_tag, "");
                
                results.push_back(info);
            }
        }
    }
    
    return results;
}

std::vector<BilibiliPageInfo> BilibiliNetworkInterface::getPagesCid(const std::string& bvid) {
    std::vector<BilibiliPageInfo> pages;
    
    if (!connected_) return pages;
    
    std::string query = "bvid=" + urlEncode(bvid);
    auto res = http_client_->Get("/x/player/pagelist?" + query, getHttplibHeaders());
    
    if (res && res->status == 200) {
        Json::Value root;
        Json::Reader reader;
        
        if (reader.parse(res->body, root) && root["code"].asInt() == 0) {
            for (const auto& page : root["data"]) {
                BilibiliPageInfo info;
                info.cid = page["cid"].asInt64();
                info.page = page["page"].asInt();
                info.part = page["part"].asString();
                info.duration = page["duration"].asInt();
                pages.push_back(info);
            }
        }
    }
    
    return pages;
}

std::string BilibiliNetworkInterface::getAudioLink(const std::string& bvid, int64_t cid) {
    if (!connected_) return "";
    
    std::unordered_map<std::string, std::string> params = {
        {"bvid", bvid},
        {"cid", std::to_string(cid)},
        {"fnval", "16"},
        {"qn", "64"}
    };
    
    auto signed_params = encWbi(params);
    
    std::string response;
    if (makeSignedRequest("/x/player/wbi/playurl", signed_params, response)) {
        Json::Value root;
        Json::Reader reader;
        
        if (reader.parse(response, root)) {
            const auto& audio_list = root["data"]["dash"]["audio"];
            if (audio_list.isArray() && !audio_list.empty()) {
                // Find highest bandwidth audio
                Json::Value best_audio;
                int max_bandwidth = 0;
                
                for (const auto& audio : audio_list) {
                    int bandwidth = audio["bandwidth"].asInt();
                    if (bandwidth > max_bandwidth) {
                        max_bandwidth = bandwidth;
                        best_audio = audio;
                    }
                }
                
                return best_audio["baseUrl"].asString();
            }
        }
    }
    
    return "";
}

uint64_t BilibiliNetworkInterface::getStreamBytesSize(const std::string &url)
{
    if (!connected_) return 0;

    try {
        // Parse the URL to extract host and path
        std::string host, path;
        if (!parseUrl(url, host, path)) {
            std::cerr << "Failed to parse URL: " << url << std::endl;
            return 0;
        }

        // Create a new client for this host
        httplib::Client size_client(host);
        size_client.set_follow_location(true);
        size_client.set_connection_timeout(timeout_seconds_, 0);
        size_client.set_read_timeout(timeout_seconds_, 0);

        // Use minimal headers with Range to get just the first byte
        httplib::Headers headers;
        headers.emplace("Referer", "https://www.bilibili.com");
        headers.emplace("User-Agent", headers_["User-Agent"]);
        headers.emplace("Range", "bytes=0-0");  // Request only first byte

        // Try HEAD request first
        auto res = size_client.Head(path, headers);
        if (res && res->status == 200) {
            auto it = res->headers.find("Content-Length");
            if (it != res->headers.end()) {
                return std::stoull(it->second);
            }
        }
        
        // If HEAD fails, try GET with Range header
                  
        res = size_client.Get(path, headers);
        if (res && (res->status == 206 || res->status == 200)) {  // 206 = Partial Content, 200 = OK
            // Try to get size from Content-Range header first (more reliable for partial content)
            auto range_it = res->headers.find("Content-Range");
            if (range_it != res->headers.end()) {
                // Content-Range format: "bytes 0-0/12345" where 12345 is total size
                std::string range_header = range_it->second;
                size_t slash_pos = range_header.find_last_of('/');
                if (slash_pos != std::string::npos) {
                    std::string total_size_str = range_header.substr(slash_pos + 1);
                    if (total_size_str != "*") {  // "*" means size unknown
                        try {
                            return std::stoull(total_size_str);
                        } catch (...) {
                            // Fall through to Content-Length
                        }
                    }
                }
            }
            
            // Fallback to Content-Length header
            auto length_it = res->headers.find("Content-Length");
            if (length_it != res->headers.end()) {
                return std::stoull(length_it->second);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception while getting stream size: " << e.what() << std::endl;
    }

    return 0;
}

std::future<bool> BilibiliNetworkInterface::asyncDownloadStream(const std::string &url,
                                                                std::function<bool(const char *, size_t)> content_receiver,
                                                                std::function<bool(uint64_t, uint64_t)> progress_callback)
{
    // Return a future that executes the download in a separate thread
    return std::async(std::launch::async, [this, url, content_receiver, progress_callback]() -> bool {
        if (!connected_) return false;
        
        try {
            // Parse the URL to extract host and path
            std::string host, path;
            if (!parseUrl(url, host, path)) {
                std::cerr << "Failed to parse URL: " << url << std::endl;
                return false;
            }
            
            // Create a new client for this host (each thread needs its own client)
            auto stream_client = std::make_shared<httplib::Client>(host);
            stream_client->set_follow_location(true);
            stream_client->set_connection_timeout(timeout_seconds_, 0);
            stream_client->set_read_timeout(timeout_seconds_, 0);
            
            // Use minimal headers for streaming
            httplib::Headers headers;
            headers.emplace("Referer", "https://www.bilibili.com");
            headers.emplace("User-Agent", headers_["User-Agent"]);
            
            auto res = stream_client->Get(path, headers, content_receiver, progress_callback);
            
            if (!res) {
                std::cerr << "Network error during async streaming from: " << url << std::endl;
                return false;
            }
            
            if (res->status != 200) {
                std::cerr << "HTTP error " << res->status << " when async streaming from: " << url << std::endl;
                return false;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Exception during async streaming: " << e.what() << std::endl;
            return false;
        }
    });
}

bool BilibiliNetworkInterface::streamBilibiliAudio(const std::string& bvid, int64_t cid,
                                                  std::function<bool(const char*, size_t)> content_receiver) {
    std::string audio_url = getAudioLink(bvid, cid);
    if (audio_url.empty()) return false;
    
    // Create new client for audio streaming
    httplib::Client audio_client(audio_url);
    audio_client.set_follow_location(true);
    
    httplib::Headers headers;
    headers.emplace("Referer", "https://www.bilibili.com");
    headers.emplace("User-Agent", headers_["User-Agent"]);
    
    auto res = audio_client.Get("", headers, content_receiver);
    return res && res->status == 200;
}

// Helper method implementations
httplib::Headers BilibiliNetworkInterface::getHttplibHeaders() const {
    httplib::Headers headers;
    for (const auto& pair : headers_) {
        headers.emplace(pair.first, pair.second);
    }
    
    // Note: When using httplib's built-in cookie management, 
    // we don't need to manually add Cookie header as httplib handles it automatically.
    // However, for backward compatibility and external clients, we still include it.
    if (!cookies_.empty()) {
        headers.emplace("Cookie", getCookieString());
    }
    
    return headers;
}

std::string BilibiliNetworkInterface::getCookieString() const {
    std::string cookie_str;
    for (const auto& pair : cookies_) {
        if (!cookie_str.empty()) cookie_str += "; ";
        cookie_str += pair.first + "=" + pair.second;
    }
    return cookie_str;
}

bool BilibiliNetworkInterface::makeSignedRequest(const std::string& path,
                                               const std::unordered_map<std::string, std::string>& params,
                                               std::string& response) {
    if (!connected_) return false;
    
    std::string query;
    for (const auto& pair : params) {
        if (!query.empty()) query += "&";
        query += urlEncode(pair.first) + "=" + urlEncode(pair.second);
    }
    
    auto res = http_client_->Get(path + "?" + query, getHttplibHeaders());
    
    if (res && res->status == 200) {
        response = res->body;
        return true;
    }
    
    return false;
}

// Utility method implementations
std::string BilibiliNetworkInterface::urlEncode(const std::string& str) const {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    
    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char)c);
            escaped << std::nouppercase;
        }
    }
    
    return escaped.str();
}

std::string BilibiliNetworkInterface::md5Hash(const std::string& str) const {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)str.c_str(), str.length(), digest);
    
    std::ostringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    
    return ss.str();
}

std::string BilibiliNetworkInterface::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return std::to_string(timestamp);
}

bool BilibiliNetworkInterface::needsWbiRefresh() const {
    if (wbi_keys_.img_key.empty() || wbi_keys_.sub_key.empty()) {
        return true;
    }
    
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::hours>(now - wbi_keys_.last_update);
    return diff.count() >= 24; // Refresh every 24 hours
}

// Configuration methods with cookie persistence
bool BilibiliNetworkInterface::loadConfig(const std::string& config_file) {
    std::string cookie_file = config_file.empty() ? config_file_ : config_file;
    if (cookie_file.find(".json") == std::string::npos) {
        cookie_file = cookie_file.substr(0, cookie_file.find_last_of('.')) + "_cookies.json";
    }
    
    return loadCookiesFromFile(cookie_file);
}

bool BilibiliNetworkInterface::saveConfig(const std::string& config_file) {
    std::string cookie_file = config_file.empty() ? config_file_ : config_file;
    if (cookie_file.find(".json") == std::string::npos) {
        cookie_file = cookie_file.substr(0, cookie_file.find_last_of('.')) + "_cookies.json";
    }
    
    return saveCookiesToFile(cookie_file);
}

void BilibiliNetworkInterface::setHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void BilibiliNetworkInterface::removeHeader(const std::string& key) {
    headers_.erase(key);
}

void BilibiliNetworkInterface::clearHeaders() {
    headers_.clear();
    initializeDefaultHeaders();
}

void BilibiliNetworkInterface::setTimeout(int timeout_sec) {
    timeout_seconds_ = timeout_sec;
    if (http_client_) {
        http_client_->set_connection_timeout(timeout_sec, 0);
        http_client_->set_read_timeout(timeout_sec, 0);
    }
}

void BilibiliNetworkInterface::setFollowLocation(bool follow) {
    follow_location_ = follow;
    if (http_client_) {
        http_client_->set_follow_location(follow);
    }
}

void BilibiliNetworkInterface::setUserAgent(const std::string& user_agent) {
    headers_["User-Agent"] = user_agent;
}

std::unordered_map<std::string, std::string> BilibiliNetworkInterface::getCurrentHeaders() const
{
    return headers_;
}

std::unordered_map<std::string, std::string> BilibiliNetworkInterface::getCurrentCookies() const
{
    return cookies_;
}

// Improved cookie management methods
BilibiliNetworkInterface::ParsedCookie BilibiliNetworkInterface::parseCookieHeader(const std::string& cookie_header) const {
    ParsedCookie cookie;
    
    // Split cookie header by semicolon to get all attributes
    std::vector<std::string> parts;
    std::stringstream ss(cookie_header);
    std::string part;
    
    while (std::getline(ss, part, ';')) {
        // Trim whitespace
        part.erase(0, part.find_first_not_of(" \t"));
        part.erase(part.find_last_not_of(" \t") + 1);
        parts.push_back(part);
    }
    
    if (parts.empty()) return cookie;
    
    // First part is always name=value
    size_t eq_pos = parts[0].find('=');
    if (eq_pos != std::string::npos) {
        cookie.name = parts[0].substr(0, eq_pos);
        cookie.value = parts[0].substr(eq_pos + 1);
    }
    
    // Parse additional attributes
    for (size_t i = 1; i < parts.size(); i++) {
        std::string attr = parts[i];
        std::transform(attr.begin(), attr.end(), attr.begin(), ::tolower);
        
        if (attr.find("domain=") == 0) {
            cookie.domain = parts[i].substr(7);
        } else if (attr.find("path=") == 0) {
            cookie.path = parts[i].substr(5);
        } else if (attr.find("expires=") == 0) {
            cookie.expires = parts[i].substr(8);
        } else if (attr == "secure") {
            cookie.secure = true;
        } else if (attr == "httponly") {
            cookie.httpOnly = true;
        } else if (attr.find("samesite=") == 0) {
            cookie.sameSite = parts[i].substr(9);
        }
    }
    
    return cookie;
}

void BilibiliNetworkInterface::updateClientCookies() {
    // Since httplib 0.15.3 doesn't have built-in cookie store management,
    // we manually add cookies to each request via headers
    // This method ensures our main http_client stays updated with current cookies
}

void BilibiliNetworkInterface::syncCookiesFromHttplib() {
    // In cpp-httplib 0.15.3, there's no built-in cookie store to sync from
    // This is a placeholder for potential future httplib versions
}

void BilibiliNetworkInterface::syncCookiesToHttplib() {
    // In cpp-httplib 0.15.3, we handle cookies manually via headers
    updateClientCookies();
}

void BilibiliNetworkInterface::addCookie(const std::string& name, const std::string& value) {
    cookies_[name] = value;
    updateClientCookies();
}

void BilibiliNetworkInterface::removeCookie(const std::string& name) {
    cookies_.erase(name);
    updateClientCookies();
}

void BilibiliNetworkInterface::clearCookies() {
    cookies_.clear();
    updateClientCookies();
}

bool BilibiliNetworkInterface::saveCookiesToFile(const std::string& filename) const {
    try {
        Json::Value root;
        Json::Value cookiesObj;
        
        // Create object format: { "cookies": { "key1": "val1", "key2": "val2" } }
        for (const auto& cookie : cookies_) {
            cookiesObj[cookie.first] = cookie.second;
        }
        root["cookies"] = cookiesObj;
        
        std::ofstream file(filename);
        if (file.is_open()) {
            Json::StreamWriterBuilder builder;
            builder["indentation"] = "  ";
            std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            writer->write(root, &file);
            file.close();
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error saving cookies: " << e.what() << std::endl;
    }
    return false;
}

bool BilibiliNetworkInterface::loadCookiesFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) return false;
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(file, root)) return false;
        
        cookies_.clear();
        // Format: { "cookies": { "key1": "val1", "key2": "val2", "key3": "val3" }, ... }
        if (root["cookies"].isObject()) {
            for (const auto& cookie : root["cookies"].getMemberNames()) {
                std::string name = cookie;
                std::string value = root["cookies"][cookie].asString();
                
                // Check if cookie is still valid (optional expiration check)
                if (!name.empty()) {
                    cookies_[name] = value;
                }
            }
        }
        
        updateClientCookies();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading cookies: " << e.what() << std::endl;
    }
    return false;
}

bool BilibiliNetworkInterface::isValidUrl(const std::string& url) const {
    return url.find("http") == 0;
}

bool BilibiliNetworkInterface::parseUrl(const std::string& url, std::string& host, std::string& path) const {
    // Find the scheme (http:// or https://)
    size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        return false;
    }
    
    std::string scheme = url.substr(0, scheme_end);
    size_t host_start = scheme_end + 3;
    
    // Find the path (first '/' after the host)
    size_t path_start = url.find('/', host_start);
    if (path_start == std::string::npos) {
        // No path, just host
        host = url.substr(0);
        path = "/";
    } else {
        // Extract host and path
        host = url.substr(0, path_start);
        path = url.substr(path_start);
    }
    
    return true;
}