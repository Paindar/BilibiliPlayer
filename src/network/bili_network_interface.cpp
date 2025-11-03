#include "bili_network_interface.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <openssl/md5.h>
#include <json/json.h>
#include <log/log_manager.h>
#include <util/md5.h>
#include <util/urlencode.h>

// Mixin key encoding table from Python script
static const std::vector<int> mixinKeyEncTab = {
    46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35, 27, 43, 5, 49,
    33, 9, 42, 19, 29, 28, 14, 39, 12, 38, 41, 13, 37, 48, 7, 16, 24, 55, 40,
    61, 26, 17, 0, 1, 60, 51, 30, 4, 22, 25, 54, 21, 56, 59, 6, 63, 57, 62, 11,
    36, 20, 34, 44, 52
};

using namespace network;

BilibiliNetworkInterface::BilibiliNetworkInterface() 
    : timeout_seconds_(10)
    , follow_location_(true)
    , platform_dir_("./") {
}

BilibiliNetworkInterface::~BilibiliNetworkInterface() {
    for (auto &pair : http_clients) {
        for (auto &entry : pair.second) {
            if (entry.client) {
                try {
                    entry.client->stop();
                } catch (...) {
                    // ignore
                }
            }
        }
    }
}

// Configuration methods with cookie persistence
bool BilibiliNetworkInterface::loadConfig(const std::string& config_file) {
    std::string config = platform_dir_ + '/' + (config_file.empty() ? "bilibili.json" : config_file);
    
    std::ifstream file(config);
    if (file.is_open()) {
        Json::Value root;
        Json::Reader reader;
        
        if (reader.parse(file, root)) {
            // We need to update both configuration state and client headers/cookies.
            // loadCookies_unsafe() calls updateClientCookies_unsafe(), which requires
            // the client mutex to be held. Acquire both mutexes to satisfy callers
            // of the internal _unsafe methods.
            std::scoped_lock lock(m_configureMutex_, m_clientMutex_);
            // Load headers
            if (root.isMember("headers")) {
                const auto& headersObj = root["headers"];
                if (!loadHeaders_unsafe(headersObj)) {
                    LOG_ERROR("Failed to load headers from config.");
                }
            }
            // Load cookies
            if (root.isMember("cookies")) {
                const auto& cookiesObj = root["cookies"];
                cookies_.clear();
                if (!loadCookies_unsafe(cookiesObj)) {
                    LOG_ERROR("Failed to load cookies from config.");
                }
            }
            // Load img_key, sub_key, last_update if present
            if (root.isMember("wbi_keys")) {
                const auto& wbiObj = root["wbi_keys"];
                if (!loadWbiKeys_unsafe(wbiObj)) {
                    LOG_ERROR("Failed to load WBI keys from config.");
                }
            }
            if (needsWbiRefresh_unsafe()) {
                refreshWbiKeys_unsafe();
            }
            return true;
        } else {
            LOG_ERROR("Failed to parse config file: {}", config);
        }
    }
    return false;
}

bool BilibiliNetworkInterface::saveConfig(const std::string& config_file) {
    std::string config = platform_dir_ + '/' + (config_file.empty() ? "bilibili.json" : config_file);
    std::ofstream file(config);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file for writing: {}", config);
        return false;
    }
    Json::Value root;
    std::scoped_lock lock(m_configureMutex_);
    // Save headers
    if (!saveHeaders_unsafe(root["headers"])) {
        LOG_ERROR("Failed to save headers to config.");
    }
    // Save cookies
    if (!saveCookies_unsafe(root["cookies"])) {
        LOG_ERROR("Failed to save cookies to config.");
    }
    // Save Wbi keys
    if (!saveWbiKeys_unsafe(root["wbi_keys"])) {
        LOG_ERROR("Failed to save WBI keys to config.");
    }
    file << root.toStyledString();
    return true;
}

bool BilibiliNetworkInterface::setPlatformDirectory(const std::string &platform_dir)
{
    std::scoped_lock lock(m_clientMutex_);
    if (std::filesystem::exists(platform_dir) && std::filesystem::is_directory(platform_dir)) {
        this->platform_dir_ = platform_dir;
        return true;
    }
    return false;
}

void BilibiliNetworkInterface::setTimeout(int timeout_sec) {
    std::scoped_lock lock(m_clientMutex_);
    timeout_seconds_ = timeout_sec;
}

void BilibiliNetworkInterface::setUserAgent(const std::string& user_agent) {
    std::scoped_lock lock(m_clientMutex_);
    headers_["User-Agent"] = user_agent;
}

std::vector<BilibiliPageInfo> BilibiliNetworkInterface::getPagesCid(const std::string& bvid) {
    std::scoped_lock lock(m_clientMutex_);
    return getPagesCid_unsafe(bvid);
}

/*********** Interface method *****************/
std::vector<BilibiliVideoInfo> BilibiliNetworkInterface::searchByTitle(const std::string& title, int page) {
    std::unordered_map<std::string, std::string> params = {
        {"search_type", "video"},
        {"keyword", title},
        // {"order", "totalrank"},
        {"page", std::to_string(page)}
    };
    
    {
        std::scoped_lock lock(m_wbiMutex_);
        // Todo: add wbi updating check here
        if (!encWbi_unsafe(params)) {
            LOG_ERROR("Failed to encode WBI parameters for search.");
            return {};
        }
    }
    std::string response;
    {
        std::scoped_lock lock(m_clientMutex_);

        if (!sendGetRequest_unsafe("https://api.bilibili.com", "/x/web-interface/wbi/search/type", params, response)) {
            LOG_ERROR("Search request failed for title: {}", title);
            return {};
        }
    }
    std::vector<BilibiliVideoInfo> results;
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
            LOG_DEBUG("Found video: {} by {}", info.title, info.author);
            results.push_back(info);
        }
    }
    return results;
}

std::string network::BilibiliNetworkInterface::getAudioUrlByParams(const std::string &params)
{
    // Phase 1: Extract bvid and cid from params
    std::string bvid;
    int64_t cid = 0;
    auto bvid_pos = params.find("bvid=");
    if (bvid_pos != std::string::npos) {
        size_t bvid_end = params.find('&', bvid_pos);
        bvid = params.substr(bvid_pos + 5, bvid_end - (bvid_pos + 5));
    }
    auto cid_pos = params.find("cid=");
    if (cid_pos != std::string::npos) {
        size_t cid_end = params.size();
        cid = std::stoll(params.substr(cid_pos + 4, cid_end - (cid_pos + 4)));
    }
    if (bvid.empty() || cid == 0) {
        return std::string();
    }
    // Phase 2: construct request and decorate with WBI
    std::unordered_map<std::string, std::string> req_params = {
        {"bvid", bvid},
        {"cid",  std::to_string(cid)},
        {"fnval", "16"},
        {"qn", "64"}
    };
    {
        std::scoped_lock lock(m_wbiMutex_);
        // Todo: add wbi updating check here
        if (!encWbi_unsafe(req_params)) {
            LOG_ERROR("Failed to encode WBI parameters for audio URL.");
            return std::string();
        }
    }
    std::string response;
    {
        std::scoped_lock lock(m_clientMutex_);
        if (!sendGetRequest_unsafe("https://api.bilibili.com", "/x/player/wbi/playurl", req_params, response)) {
            LOG_ERROR("Failed to get audio link for bvid: {}, cid: {}", bvid, cid);
            return "";
        }
    }

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
    return std::string();
}

uint64_t BilibiliNetworkInterface::getStreamBytesSize(const std::string &url)
{
    // Parse the URL to extract host and path
    std::string host, path;
    if (!parseUrl(url, host, path)) {
        LOG_ERROR("Failed to parse URL: {}", url);
        return 0;
    }
    
    try {
        std::scoped_lock lock(m_clientMutex_);
        // Borrow a client from the pool for this host
        std::shared_ptr<httplib::Client> size_client = borrowHttpClient_unsafe(host);
        if (!size_client) {
            LOG_ERROR("Failed to borrow HTTP client for host: {}", host);
            return 0;
        }

        // Build headers (include cookies and configured headers) while holding the client mutex
        httplib::Headers headers = getHttplibHeaders_unsafe(host, path);
        // Ensure Range is set for byte range request
        headers.emplace("Range", "bytes=0-0");  // Request only first byte

        // Try HEAD request first
        auto res = size_client->Head(path, headers);
        if (res && res->status == 200) {
            auto it = res->headers.find("Content-Length");
            if (it != res->headers.end()) {
                returnHttpClient_unsafe(size_client);
                return std::stoull(it->second);
            }
        }

        // If HEAD fails, try GET with Range header
        res = size_client->Get(path, headers);
        if (!res) {
            returnHttpClient_unsafe(size_client);
            return 0;
        }
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
                            auto result = std::stoull(total_size_str);
                            returnHttpClient_unsafe(size_client);
                            return result;
                        } catch (...) {
                            // Fall through to Content-Length
                        }
                    }
                }
            }

            // Fallback to Content-Length header
            auto length_it = res->headers.find("Content-Length");
            if (length_it != res->headers.end()) {
                auto result = std::stoull(length_it->second);
                returnHttpClient_unsafe(size_client);
                return result;
            }

        }
        returnHttpClient_unsafe(size_client);
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while getting stream size: {}", e.what());
    }

    return 0;
}

std::future<bool> BilibiliNetworkInterface::asyncDownloadStream(const std::string &url,
                                                                std::function<bool(const char *, size_t)> content_receiver,
                                                                std::function<bool(uint64_t, uint64_t)> progress_callback)
{
    // Return a future that executes the download in a separate thread
    std::string host, path;
    if (!parseUrl(url, host, path)) {
        LOG_ERROR("Failed to parse URL: {}", url);
        return std::async(std::launch::deferred, []() { return false; });
    }
    return std::async(std::launch::async, [this, host, path, content_receiver, progress_callback]() -> bool {
        try {
            // Create a new client for this host (each thread needs its own client)
            std::shared_ptr<httplib::Client> stream_client;
            httplib::Headers headers;

            // Borrow client and build headers while holding the client mutex
            {
                std::scoped_lock lock(m_clientMutex_);
                stream_client = borrowHttpClient_unsafe(host);
                if (!stream_client) {
                    throw std::runtime_error("Failed to borrow HTTP client for streaming.");
                }
                headers = getHttplibHeaders_unsafe(host, path);
            }

            auto res = stream_client->Get(path, headers, content_receiver, progress_callback);
            returnHttpClient_unsafe(stream_client);

            if (!res) {
                LOG_ERROR("Failed to perform async streaming from: {}/{}.", host, path);
                return false;
            }

            if (res->status != 200) {
                LOG_ERROR("HTTP error {} when async streaming from: {}/{}.", res->status, host, path);
                return false;
            }

            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("Exception during async streaming: {}", e.what());
            return false;
        }
    });
}

/********* Private method *********/
std::string BilibiliNetworkInterface::getMixinKey(const std::string& orig) const {
    std::string result;
    for (int i : mixinKeyEncTab) {
        if (i < orig.length()) {
            result += orig[i];
        }
    }
    return result.substr(0, 32);
}

std::string BilibiliNetworkInterface::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return std::to_string(timestamp);
}

bool BilibiliNetworkInterface::needsWbiRefresh_unsafe() const {
    if (wbi_keys_.img_key.empty() || wbi_keys_.sub_key.empty()) {
        return true;
    }
    
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::hours>(now - wbi_keys_.last_update);
    return diff.count() >= 24; // Refresh every 24 hours
}

bool BilibiliNetworkInterface::encWbi_unsafe(std::unordered_map<std::string, std::string> &params) const
{
    BiliWbiKeys wbi_keys_copy = wbi_keys_;
    if (wbi_keys_copy.img_key.empty() || wbi_keys_copy.sub_key.empty()) {
        LOG_INFO("WBI keys are not initialized");
        return false;
    }
    std::string mixin_key = getMixinKey(wbi_keys_copy.img_key + wbi_keys_copy.sub_key);
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
        query += util::urlEncode(pair.first) + "=" + util::urlEncode(value);
    }
    params["w_rid"] = util::md5Hash(query + mixin_key);
    return true;
}

bool BilibiliNetworkInterface::refreshWbiKeys_unsafe() {
    std::shared_ptr<httplib::Client> http_client;
    httplib::Headers headers;
    {
        // Caller is expected to hold m_clientMutex_ before calling this _unsafe method.
        http_client = borrowHttpClient_unsafe("https://api.bilibili.com");
        if (!http_client) {
            throw std::runtime_error("Failed to borrow HTTP client for WBI key refresh.");
        }
        headers = getHttplibHeaders_unsafe("https://api.bilibili.com", "/x/web-interface/nav");
    }

    auto res = http_client->Get("/x/web-interface/nav", headers);
    returnHttpClient_unsafe(http_client);

    if (res && res->status == 200) {
        Json::Value root;
        Json::Reader reader;
        
        if (reader.parse(res->body, root)) {
            std::string img_url = root["data"]["wbi_img"]["img_url"].asString();
            std::string sub_url = root["data"]["wbi_img"]["sub_url"].asString();
            
            // Extract keys from URLs
            size_t img_slash = img_url.find_last_of('/');
            size_t img_dot = img_url.find_last_of('.');
            BiliWbiKeys new_keys;
            if (img_slash != std::string::npos && img_dot != std::string::npos) {
                new_keys.img_key = img_url.substr(img_slash + 1, img_dot - img_slash - 1);
            }
            
            size_t sub_slash = sub_url.find_last_of('/');
            size_t sub_dot = sub_url.find_last_of('.');
            if (sub_slash != std::string::npos && sub_dot != std::string::npos) {
                new_keys.sub_key = sub_url.substr(sub_slash + 1, sub_dot - sub_slash - 1);
            }

            new_keys.last_update = std::chrono::system_clock::now();
            wbi_keys_ = new_keys;
            return true;
        }
    }
    
    return false;
}

const httplib::Headers& network::BilibiliNetworkInterface::getDefaultHeaders() const
{
    const static httplib::Headers headers({
        {"Accept", "application/json, text/plain, */*"},
        {"Accept-Language", "zh-CN,zh;q=0.9"},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                        "(KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36"},
        {"Cookie", ""},
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Referer", "https://www.bilibili.com/"}
    });
    return headers;
}

// Helper method implementations
httplib::Headers BilibiliNetworkInterface::getHttplibHeaders_unsafe(const std::string& host, const std::string& path) const {
    httplib::Headers headers;
    for (const auto& pair : headers_) {
        headers.emplace(pair.first, pair.second);
    }

    // Build cookie string from structured cookie jar, selecting cookies that
    // match the request host/path, are not expired, and satisfy Secure flag.
    std::string cookie_str;
    auto now = std::chrono::system_clock::now();
    for (const auto& c : cookies_) {
        // Domain matching: if cookie.domain is empty -> host-only, else cookie.domain must be a suffix of host
        bool domain_ok = false;
        if (c.domain.empty()) {
            domain_ok = (host == c.domain || c.domain.empty()); // host-only entries stored with empty domain; allow by default
            // For host-only cookie (domain empty) we assume it's acceptable for the requested host.
            domain_ok = true;
        } else {
            // Simple suffix match, case-insensitive
            if (host.size() >= c.domain.size()) {
                auto host_tail = host.substr(host.size() - c.domain.size());
                // naive case-sensitive compare (hosts here include scheme), so allow exact substring match
                if (host_tail == c.domain) domain_ok = true;
            }
        }
        if (!domain_ok) continue;

    // Path check: request path must start with cookie path
    if (!c.cookie_path.empty() && path.rfind(c.cookie_path, 0) != 0) continue;

        // Expiry check
        if (c.expires.has_value() && now > c.expires.value()) continue;

        // Secure: if cookie is secure, ensure host uses https://
        if (c.secure) {
            if (host.rfind("https://", 0) != 0) continue;
        }

        if (!cookie_str.empty()) cookie_str += "; ";
        cookie_str += c.name + "=" + c.value;
    }

    // Ensure we don't leave a stale Cookie entry copied from headers_
    headers.erase("Cookie");
    if (!cookie_str.empty()) {
        headers.emplace("Cookie", cookie_str);
    }

    return headers;
}

bool BilibiliNetworkInterface::initializeCookies_unsafe() {
    // Create a dedicated client for cookie initialization
    std::shared_ptr<httplib::Client> http_client = borrowHttpClient_unsafe("https://www.bilibili.com");
    if (!http_client) {
        throw std::runtime_error("Failed to borrow HTTP client for cookie initialization.");
    }

    // Caller must hold m_clientMutex_ when invoking this _unsafe method.
    httplib::Headers headers = getHttplibHeaders_unsafe("https://www.bilibili.com", "/");
    auto res = http_client->Head("/", headers);
    returnHttpClient_unsafe(http_client);

    if (!res) {
        LOG_ERROR("Failed to initialize cookies: No response from server.");
        return false;
    }
    if (res->status != 200) {
        LOG_ERROR("Failed to initialize cookies: HTTP status {}", res->status);
        return false;
    }
    
    // Extract and parse Set-Cookie headers properly. There can be multiple Set-Cookie headers,
    // and each header contains one cookie followed by attributes separated by ';'.
    cookies_.clear();
    for (const auto& header : res->headers) {
        if (header.first == "Set-Cookie") {
            std::string cookie_line = header.second;
            // Split into tokens by ';'
            std::istringstream iss(cookie_line);
            std::string token;
            std::vector<std::string> parts;
            while (std::getline(iss, token, ';')) {
                // Trim
                size_t a = token.find_first_not_of(" \t");
                size_t b = token.find_last_not_of(" \t");
                if (a == std::string::npos) continue;
                parts.push_back(token.substr(a, b - a + 1));
            }

            if (parts.empty()) continue;

            // First part is name=value
            auto nv = parts[0];
            size_t eq_pos = nv.find('=');
            if (eq_pos == std::string::npos) continue;
            Cookie c;
            c.name = nv.substr(0, eq_pos);
            c.value = nv.substr(eq_pos + 1);
            // Defaults
            c.cookie_path = "/";
            c.domain = ""; // host-only unless Domain attribute present

            // Parse attributes
            for (size_t i = 1; i < parts.size(); ++i) {
                std::string attr = parts[i];
                // Find '=' if present
                size_t pos = attr.find('=');
                std::string key = attr.substr(0, pos == std::string::npos ? attr.size() : pos);
                std::string val;
                if (pos != std::string::npos) val = attr.substr(pos + 1);
                // Lowercase key for comparison
                std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch){ return std::tolower(ch); });
                if (key == "domain") {
                    c.domain = val;
                } else if (key == "path") {
                    c.cookie_path = val;
                } else if (key == "expires") {
                    // Try parse RFC1123-ish time; fallback to none
                    // Very permissive parsing: try to parse as time_t via std::get_time
                    std::tm tm{};
                    std::istringstream ss(val);
                    ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S %Z");
                    if (!ss.fail()) {
                        auto tt = std::mktime(&tm);
                        if (tt != -1) c.expires = std::chrono::system_clock::from_time_t(tt);
                    }
                } else if (key == "max-age") {
                    try {
                        long seconds = std::stol(val);
                        c.expires = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
                    } catch (...) {}
                } else if (key == "secure") {
                    c.secure = true;
                } else if (key == "httponly") {
                    c.httpOnly = true;
                } else if (key == "samesite") {
                    c.sameSite = val;
                }
            }

            // store cookie
            cookies_.push_back(std::move(c));
        }
    }
    return true;
}

std::shared_ptr<httplib::Client> network::BilibiliNetworkInterface::borrowHttpClient_unsafe(const std::string &host)
{
    // Find or create vector for this host
    auto &vec = http_clients[host];
    for (auto &entry : vec) {
        if (entry.available) {
            entry.available = false;
            return entry.client;
        }
    }

    // No available client for this host -> create and append
    auto new_client = std::make_shared<httplib::Client>(host);
    new_client->set_keep_alive(true);
    new_client->enable_server_certificate_verification(true);
    new_client->set_follow_location(true);
    new_client->set_connection_timeout(timeout_seconds_, 0);
    new_client->set_read_timeout(timeout_seconds_, 0);
    vec.push_back({host, new_client, false});
    return new_client;
}

void network::BilibiliNetworkInterface::returnHttpClient_unsafe(std::shared_ptr<httplib::Client> client)
{
    // Search all host buckets for the client
    for (auto &pair : http_clients) {
        for (auto &entry : pair.second) {
            if (entry.client == client) {
                entry.available = true;
                return;
            }
        }
    }
    // Not found: add to default empty-host bucket as available
    LOG_WARN("returnHttpClient_unsafe: returned client not found in pool; adding to orphan bucket");
    http_clients[std::string()].push_back({std::string(), client, true});
}

bool BilibiliNetworkInterface::sendGetRequest_unsafe(const std::string& host, 
                                                const std::string& path,
                                               const std::unordered_map<std::string, std::string>& params,
                                               std::string& response) 
{
    std::shared_ptr<httplib::Client> http_client = borrowHttpClient_unsafe(host);
    if (!http_client) {
        throw std::runtime_error("Failed to borrow HTTP client.");
    }

    httplib::Params httplib_params;
    for (const auto& pair : params) {
        httplib_params.emplace(pair.first, pair.second);
    }
    auto res = http_client->Get(path, httplib_params, getHttplibHeaders_unsafe(host, path));
    returnHttpClient_unsafe(http_client);
    if (res && res->status == 200) {
        response = res->body;
        return true;
    }
    return false;
}

std::vector<BilibiliPageInfo> BilibiliNetworkInterface::getPagesCid_unsafe(const std::string& bvid) {
    std::vector<BilibiliPageInfo> pages;

    httplib::Params params;
    params.emplace("bvid", util::urlEncode(bvid));
    std::shared_ptr<httplib::Client> http_client = borrowHttpClient_unsafe("https://api.bilibili.com");
    if (!http_client) {
        throw std::runtime_error("Failed to borrow HTTP client for page CID retrieval.");
    }
    auto res = http_client->Get("/x/player/pagelist", params, getHttplibHeaders_unsafe("https://api.bilibili.com", "/x/player/pagelist"));
    returnHttpClient_unsafe(http_client);
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
                info.first_frame = page["first_frame"].asString();
                pages.push_back(info);
            }
        }
    }
    
    return pages;
}

void BilibiliNetworkInterface::updateClientCookies_unsafe() {
    // Since httplib 0.15.3 doesn't have built-in cookie store management,
    // we manually add cookies to each request via headers
    // Update the Cookie header in our headers map for consistency
    // Build cookie string under cookies mutex, then update headers under headers mutex
    // With structured cookie jar we build Cookie header per-request. Clear any
    // previously-stored Cookie header in headers_ to avoid duplication.
    headers_.erase("Cookie");
}

/******* Configuration related *************/
bool BilibiliNetworkInterface::loadHeaders_unsafe(const Json::Value &jsonObj)
{
    try {
        if (jsonObj.isObject()) {
            for (const auto& key : jsonObj.getMemberNames()) {
                headers_[key] = jsonObj[key].asString();
            }
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error loading headers: {}", e.what());
    }
    return false;
}

bool BilibiliNetworkInterface::saveHeaders_unsafe(Json::Value &jsonObj) const
{
    try {
        // Ensure the jsonObj is an object type
        if (!jsonObj.isObject()) {
            jsonObj = Json::Value(Json::objectValue);
        }
        
        for (const auto& header : headers_) {
            jsonObj[header.first] = header.second;
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving headers: {}", e.what());
    }
    return false;
}

bool BilibiliNetworkInterface::loadCookies_unsafe(const Json::Value& jsonObj) {
    try {
        // Support legacy format (object of name->value) and new structured format
        // (array of cookie objects).
        cookies_.clear();
        if (jsonObj.isObject()) {
            // Legacy: each member is cookie-name -> value
            for (const auto& key : jsonObj.getMemberNames()) {
                Cookie c;
                c.name = key;
                c.value = jsonObj[key].asString();
                c.cookie_path = "/";
                c.domain = "";
                cookies_.push_back(std::move(c));
            }
            return true;
        } else if (jsonObj.isArray()) {
            for (const auto& item : jsonObj) {
                if (!item.isObject()) continue;
                Cookie c;
                c.name = item.get("name", "").asString();
                c.value = item.get("value", "").asString();
                c.domain = item.get("domain", "").asString();
                c.cookie_path = item.get("path", "/").asString();
                if (item.isMember("expires")) {
                    std::string ex = item["expires"].asString();
                    if (!ex.empty()) {
                        try {
                            std::time_t t = std::stoll(ex);
                            c.expires = std::chrono::system_clock::from_time_t(t);
                        } catch (...) {}
                    }
                }
                c.secure = item.get("secure", false).asBool();
                c.httpOnly = item.get("httpOnly", false).asBool();
                c.sameSite = item.get("sameSite", "").asString();
                cookies_.push_back(std::move(c));
            }
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error loading cookies: {}", e.what());
    }
    return false;
}

bool BilibiliNetworkInterface::saveCookies_unsafe(Json::Value& jsonObj) const {
    try {
        // Ensure the jsonObj is an object type
        if (!jsonObj.isObject()) {
            jsonObj = Json::Value(Json::objectValue);
        }
        // Save structured cookies as an array of cookie objects for clarity
        Json::Value arr(Json::arrayValue);
        for (const auto& c : cookies_) {
            Json::Value obj(Json::objectValue);
            obj["name"] = c.name;
            obj["value"] = c.value;
            obj["domain"] = c.domain;
            obj["path"] = c.cookie_path;
            if (c.expires.has_value()) {
                std::time_t t = std::chrono::system_clock::to_time_t(c.expires.value());
                obj["expires"] = std::to_string(t);
            }
            obj["secure"] = c.secure;
            obj["httpOnly"] = c.httpOnly;
            obj["sameSite"] = c.sameSite;
            arr.append(obj);
        }
        jsonObj = arr;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving cookies: {}", e.what());
    }
    return false;
}

bool network::BilibiliNetworkInterface::loadWbiKeys_unsafe(const Json::Value &jsonObj)
{
    if (jsonObj.isObject()) {
        wbi_keys_.img_key = jsonObj.get("img_key", "").asString();
        wbi_keys_.sub_key = jsonObj.get("sub_key", "").asString();
        std::string last_update_str = jsonObj.get("last_update", "").asString();
        if (!last_update_str.empty()) {
            try {
                std::time_t t = std::stoll(last_update_str);
                wbi_keys_.last_update = std::chrono::system_clock::from_time_t(t);
            } catch (...) {
                wbi_keys_.last_update = std::chrono::system_clock::now();
            }
        }
        return true;
    }
    return false;
}

bool network::BilibiliNetworkInterface::saveWbiKeys_unsafe(Json::Value &jsonObj) const
{
    // Ensure the jsonObj is an object type
    if (!jsonObj.isObject()) {
        jsonObj = Json::Value(Json::objectValue);
    }
    
    jsonObj["img_key"] = wbi_keys_.img_key;
    jsonObj["sub_key"] = wbi_keys_.sub_key;
    std::time_t t = std::chrono::system_clock::to_time_t(wbi_keys_.last_update);
    jsonObj["last_update"] = std::to_string(t);
    return true;
}

#ifdef UNIT_TEST
void BilibiliNetworkInterface::test_create_clients(const std::string& host, size_t n) {
    std::scoped_lock lock(m_clientMutex_);
    for (size_t i = 0; i < n; ++i) {
        auto c = borrowHttpClient_unsafe(host);
        // Immediately return to mark as available
        returnHttpClient_unsafe(c);
    }
}

size_t BilibiliNetworkInterface::test_pool_size(const std::string& host) const {
    std::scoped_lock lock(m_clientMutex_);
    auto it = http_clients.find(host);
    if (it == http_clients.end()) return 0;
    return it->second.size();
}

bool BilibiliNetworkInterface::test_concurrent_borrow_return(const std::string& host, size_t thread_count, size_t iter_count) {
    try {
        // Ensure at least one client exists
        test_create_clients(host, 1);

        std::vector<std::thread> threads;
        for (size_t t = 0; t < thread_count; ++t) {
            threads.emplace_back([this, &host, iter_count]() {
                for (size_t i = 0; i < iter_count; ++i) {
                    std::shared_ptr<httplib::Client> c;
                    {
                        std::scoped_lock lock(m_clientMutex_);
                        c = borrowHttpClient_unsafe(host);
                    }
                    // Simulate short work
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    returnHttpClient_unsafe(c);
                }
            });
        }

        for (auto &th : threads) th.join();
        return true;
    } catch (...) {
        return false;
    }
}
#endif

// Helper: canonicalize host (strip scheme, optional port) and lowercase
static std::string canonicalize_host(const std::string &host_with_scheme) {
    std::string s = host_with_scheme;
    // Strip scheme
    auto pos = s.find("://");
    if (pos != std::string::npos) s = s.substr(pos + 3);
    // Strip path if present
    pos = s.find('/');
    if (pos != std::string::npos) s = s.substr(0, pos);
    // Strip port if present
    pos = s.find(':');
    if (pos != std::string::npos) s = s.substr(0, pos);
    // Lowercase
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

#ifdef UNIT_TEST
void BilibiliNetworkInterface::test_add_cookie_from_set_cookie(const std::string& set_cookie_line, const std::string& origin_host) {
    std::scoped_lock lock(m_clientMutex_);
    // Reuse parsing logic by constructing a fake response header
    std::string header = set_cookie_line;
    // parse similarly to initializeCookies_unsafe
    std::istringstream iss(header);
    std::string token;
    std::vector<std::string> parts;
    while (std::getline(iss, token, ';')) {
        size_t a = token.find_first_not_of(" \t");
        size_t b = token.find_last_not_of(" \t");
        if (a == std::string::npos) continue;
        parts.push_back(token.substr(a, b - a + 1));
    }
    if (parts.empty()) return;
    auto nv = parts[0];
    size_t eq_pos = nv.find('=');
    if (eq_pos == std::string::npos) return;
    Cookie c;
    c.name = nv.substr(0, eq_pos);
    c.value = nv.substr(eq_pos + 1);
    c.cookie_path = "/";
    c.domain = "";
    for (size_t i = 1; i < parts.size(); ++i) {
        std::string attr = parts[i];
        size_t pos = attr.find('=');
        std::string key = attr.substr(0, pos == std::string::npos ? attr.size() : pos);
        std::string val;
        if (pos != std::string::npos) val = attr.substr(pos + 1);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch){ return std::tolower(ch); });
        if (key == "domain") {
            c.domain = val;
        } else if (key == "path") {
            c.cookie_path = val;
        } else if (key == "expires") {
            std::tm tm{};
            std::istringstream ss(val);
            ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S %Z");
            if (!ss.fail()) {
                auto tt = std::mktime(&tm);
                if (tt != -1) c.expires = std::chrono::system_clock::from_time_t(tt);
            }
        } else if (key == "max-age") {
            try { long seconds = std::stol(val); c.expires = std::chrono::system_clock::now() + std::chrono::seconds(seconds); } catch(...) {}
        } else if (key == "secure") { c.secure = true; }
        else if (key == "httponly") { c.httpOnly = true; }
        else if (key == "samesite") { c.sameSite = val; }
    }
    // If domain is empty (host-only), set to origin host's canonical hostname
    if (c.domain.empty()) {
        c.domain = canonicalize_host(origin_host);
    }
    // Normalize domain: strip leading dot and lowercase
    if (!c.domain.empty() && c.domain[0] == '.') c.domain.erase(0,1);
    std::transform(c.domain.begin(), c.domain.end(), c.domain.begin(), [](unsigned char ch){ return std::tolower(ch); });
    cookies_.push_back(std::move(c));
}

httplib::Headers BilibiliNetworkInterface::test_get_headers_for_url(const std::string& url) {
    std::string host, path;
    if (!parseUrl(url, host, path)) return httplib::Headers();
    std::scoped_lock lock(m_clientMutex_);
    return getHttplibHeaders_unsafe(host, path);
}
#endif

// Utility method implementations
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