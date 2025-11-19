# Audio Source Component Contract

**Feature**: 001-unified-stream-player  
**Component**: Audio Source and Network Management  
**Date**: 2025-11-13 (Updated from actual implementation)

## Purpose

Defines the interface contract for managing audio sources across different platforms (Bilibili, YouTube, Local files) using the actual implementation from `playlist::SongInfo` and `network::NetworkManager`.

## Recent Implementation Updates (2025-11-15)

The network module was refactored to improve lifetime safety, avoid UI-blocking searches, and consolidate platform implementations:

- Moved the Bilibili platform implementation into `src/network/platform/` (canonical files: `i_platform.h`, `bili_network_interface.h`, `bili_network_interface.cpp`). A lightweight forwarder remains under `src/network/` to preserve legacy includes.
- Introduced a canonical platform interface in `src/network/platform/i_platform.h` defining `PlatformType`, `SearchResult`, and the `IPlatform` (alias-compatible) interface used by `NetworkManager`.
- `NetworkManager` now owns platform instances via `std::shared_ptr` (no external factory ownership) and inherits `std::enable_shared_from_this<NetworkManager>` to safely pass ownership into background tasks.
- `BilibiliPlatform` (implementation) also inherits `std::enable_shared_from_this<BilibiliPlatform>` and exposes a `std::atomic<bool> exitFlag_` used to request graceful shutdown of background operations.
- All asynchronous/background lambdas were updated to prefer capturing a `shared_ptr` (`self = shared_from_this()`) where available and to access members via `self->...` to avoid use-after-free. Lambdas gracefully handle cases where `shared_from_this()` is not available (unit tests or non-shared usage) by falling back to `this` for short-lived synchronous work.
- Search orchestration was hardened: `executeMultiSourceSearch` launches platform-specific searches asynchronously and uses a watcher helper (`monitorSearchFuturesWithCV`) to wait on futures using condition-variable-friendly monitoring (avoid busy-waiting). Incremental progress is delivered via Qt signals.
- Signal emission from worker threads is now queued to the object's thread using `QMetaObject::invokeMethod(self.get(), ...)` (QueuedConnection) to ensure thread-safety with Qt objects.
- CMake and test targets updated to reference `src/network/platform/bili_network_interface.cpp` to avoid duplicate symbol definitions; unit tests were adjusted to instantiate `BilibiliPlatform` using `std::make_shared` where tests rely on `shared_from_this()`.

Rationale: these updates prevent UI-blocking searches, remove fragile ownership patterns, and harden asynchronous lifetime handling while keeping the public API and the contract described in this document intact.

---

## Data Structure: SongInfo

### Purpose
Represents a single audio source/song with platform-specific metadata.

### Class Definition

```cpp
namespace playlist {
    struct SongInfo {
        QString title;          // Song title
        QString uploader;       // Artist or uploader name
        int platform;           // Boxing for network::SupportInterface enum
        int duration;           // Duration in seconds
        QString filepath;       // Local cache path or empty for streaming-only
        QString coverName;      // Cover image filename
        QString args;           // Platform-specific arguments (e.g., JSON with bvid/cid for Bilibili)
        
        bool operator==(const SongInfo& other) const {
            return title == other.title &&
                   uploader == other.uploader &&
                   platform == other.platform;
        }
    };
}

Q_DECLARE_METATYPE(playlist::SongInfo)
```

---

## Component Interface: NetworkManager

### Purpose
Orchestrates multi-platform searches and audio stream retrieval for different platforms.

### Class Definition

```cpp
namespace network {
    enum SupportInterface {
        Bilibili = 0x1,
        // Future platforms can be added here (YouTube, etc.)
        All = 0xFFFFFFFF
    };

    struct SearchResult {
        QString title;
        QString uploader;
        SupportInterface platform;
        int duration;           // seconds
        QString coverUrl;       // Network URL for cover image
        QString coverImg;       // Filename for cover (stored in tmp/cover/ folder, e.g., "BV1x7411F744.jpg")
        QString description;
        QString interfaceData;  // Raw platform-specific data (JSON)
    };

    class NetworkManager : public QObject {
        Q_OBJECT
        
    public:
        NetworkManager();
        ~NetworkManager();
        
        // Configuration
        void configure(const QString& platformDir, int timeoutMs = 10000, const QString& proxyUrl = "");
        void saveConfiguration();
        bool isConfigured() const;
        
        // Multi-source search orchestration
        void executeMultiSourceSearch(const QString& keyword, uint selectedInterface, int maxResults = 20);
        void cancelAllSearches();
        
        // Stream retrieval
        std::shared_future<std::shared_ptr<std::istream>> getAudioStreamAsync(
            SupportInterface platform, 
            const QString& params,  // Platform-specific params (e.g., JSON with bvid/cid)
            const QString& savepath = ""
        );
        
        std::future<uint64_t> getStreamSizeByParamsAsync(SupportInterface platform, const QString& params);
        std::shared_future<void> downloadAsync(SupportInterface platform, const QString& url, const QString& filepath);
        
        void setRequestTimeout(int timeoutMs);
        int getRequestTimeout() const;
        
    signals:
        void searchCompleted(const QString& keyword);
        void searchFailed(const QString& keyword, const QString& errorMessage);
        void searchProgress(const QString& keyword, const QList<SearchResult>& results);
        void downloadCompleted(const QString& url, const QString& filepath);
        void downloadFailed(const QString& url, const QString& filepath, const QString& errorMessage);
        
    private:
        std::unique_ptr<BilibiliNetworkInterface> m_biliInterface;
        std::atomic<bool> m_cancelFlag;
        int m_requestTimeout = 10000;
        bool m_configured = false;
    };
}

Q_DECLARE_METATYPE(network::SearchResult)
```

---

## Method Contracts

### NetworkManager Configuration

#### `configure(platformDir, timeoutMs, proxyUrl)`

**Preconditions**:
- `platformDir` must be a valid directory path for platform-specific configurations
- `timeoutMs` must be positive (default: 10000ms)

**Postconditions**:
- Platform interfaces (BilibiliNetworkInterface) initialized
- Configuration loaded from platform directory
- `m_configured` set to true
- Ready to execute searches and stream requests

**Error Handling**:
- Logs error if configuration fails
- `isConfigured()` returns false on failure

**Thread Safety**:
- Should be called before any network operations
- Not thread-safe (call from main thread during initialization)

**Example**:
```cpp
NetworkManager netMgr;
netMgr.configure("./platforms", 15000);
if (netMgr.isConfigured()) {
    // Ready to use
}
```

---

### Search Operations

#### `executeMultiSourceSearch(keyword, selectedInterface, maxResults)`

**Preconditions**:
- NetworkManager must be configured (`isConfigured() == true`)
- `keyword` must be non-empty
- `selectedInterface` is bitmask of `SupportInterface` values
- `maxResults` > 0 (default: 20)

**Postconditions**:
- Async search tasks launched for selected platforms
- `searchProgress` signal emitted as results arrive (incremental)
- `searchCompleted` signal emitted when all platforms finish
- `searchFailed` signal emitted if any platform fails

**Error Handling**:
- Individual platform failures don't cancel entire search
- Failed platforms emit `searchFailed` with error message
- Successful platforms still emit `searchProgress` and `searchCompleted`

**Threading**:
- Non-blocking (returns immediately)
- Results delivered via Qt signals on main thread
- Background threads handle API calls

**Cover Image Handling**:
- `coverUrl`: Network URL to download the cover image from
- `coverImg`: Generated filename for storing the cover (e.g., "BV1x7411F744.jpg" for Bilibili)
- Cover images are cached in `tmp/cover/` directory
- UI components check if cover exists before displaying
- If cover doesn't exist, download is triggered via `NetworkManager::downloadAsync()`

**Example**:
```cpp
connect(&netMgr, &NetworkManager::searchProgress, 
    [](const QString& kw, const QList<SearchResult>& results) {
        for (const auto& result : results) {
            qDebug() << result.title << result.uploader;
        }
    });
netMgr.executeMultiSourceSearch("夜曲", network::SupportInterface::Bilibili, 10);
```

---

#### `cancelAllSearches()`

**Preconditions**:
- None (can be called anytime)

**Postconditions**:
- Sets cancel flag for all active searches
- Background threads terminate gracefully
- No more `searchProgress` signals emitted
- `searchFailed` may be emitted with "Cancelled" message

**Thread Safety**:
- Thread-safe (uses atomic flag)

---

### Stream Retrieval

#### `getAudioStreamAsync(platform, params, savepath)`

**Preconditions**:
- `platform` must be valid configured platform (e.g., `SupportInterface::Bilibili`)
- `params` contains platform-specific parameters:
  - Bilibili: JSON with `{"bvid":"...", "cid":..., "qn":...}`
  - YouTube: URL or video ID (future implementation)
- `savepath` is optional local cache path

**Postconditions**:
- Returns `shared_future<shared_ptr<istream>>` for async retrieval
- Stream uses `RealtimePipe` for producer-consumer pattern
- If `savepath` provided, stream data saved to file while streaming
- Future resolves when stream is ready to read
- Stream remains open until consumer closes it

**Error Handling**:
- Future throws exception if stream retrieval fails
- Caller should catch exceptions when calling `.get()` on future
- Stream may be null if platform interface fails

**Threading**:
- Non-blocking (returns future immediately)
- Background thread handles streaming and optional file writing
- Multiple concurrent streams supported

**Example**:
```cpp
QString params = R"({"bvid":"BV1x7411F744","cid":7631925,"qn":80})";
auto streamFuture = netMgr.getAudioStreamAsync(network::SupportInterface::Bilibili, params);
// ... do other work ...
auto stream = streamFuture.get();  // Blocks until stream ready
if (stream) {
    // Feed to FFmpegStreamDecoder
    decoder->open(stream);
}
```

---

#### `getStreamSizeByParamsAsync(platform, params)`

**Preconditions**:
- Same as `getAudioStreamAsync`

**Postconditions**:
- Returns `future<uint64_t>` with stream size in bytes
- Uses HTTP HEAD request to get Content-Length
- Returns 0 if size unavailable (chunked encoding)

**Error Handling**:
- Future throws exception on network errors

**Example**:
```cpp
auto sizeFuture = netMgr.getStreamSizeByParamsAsync(network::SupportInterface::Bilibili, params);
uint64_t size = sizeFuture.get();
qDebug() << "Stream size:" << size << "bytes";
```

---

#### `downloadAsync(platform, url, filepath)`

**Preconditions**:
- `url` must be valid HTTP/HTTPS URL
- `filepath` must be writable location
- `platform` specifies which interface to use

**Postconditions**:
- Returns `shared_future<void>` for async download
- Downloads entire file to `filepath`
- `downloadCompleted` signal emitted on success
- `downloadFailed` signal emitted on failure

**Error Handling**:
- Future throws exception on download failure
- Partial file may exist on failure (not cleaned up automatically)

**Threading**:
- Non-blocking (returns future immediately)
- Background thread handles download

---

## Component Interface: BilibiliNetworkInterface

### Purpose
Platform-specific implementation for Bilibili API with HTTP connection pooling and cookie management.

### Class Definition

```cpp
namespace network {
    struct BilibiliVideoInfo {
        std::string title;
        std::string bvid;
        std::string author;
        std::string description;
        std::string coverImg;
    };

    struct BilibiliPageInfo {
        int64_t cid;
        int page;
        std::string part;
        int duration;
        std::string firstFrame;
    };

    struct BilibiliSearchResult {
        std::string title;
        std::string bvid;
        std::string author;
        std::string description;
        std::string cover;
        int64_t cid;
        std::string partTitle;
        int duration;
    };

    class BilibiliNetworkInterface {
    public:
        BilibiliNetworkInterface();
        ~BilibiliNetworkInterface();
        
        // Configuration
        bool loadConfig(const std::string& config_file = "bilibili.json");
        bool saveConfig(const std::string& config_file = "bilibili.json");
        bool setPlatformDirectory(const std::string& platform_dir);
        void setTimeout(int timeout_sec);
        void setUserAgent(const std::string& user_agent);
        
        // Search and metadata
        std::vector<BilibiliSearchResult> searchByTitle(const std::string& title, int page = 1);
        std::string getAudioUrlByParams(const std::string& params);  // JSON params
        uint64_t getStreamBytesSize(const std::string& url);
        
        // Async streaming
        std::future<bool> asyncDownloadStream(
            const std::string& url,
            std::function<bool(const char*, size_t)> content_receiver,
            std::function<bool(uint64_t, uint64_t)> progress_callback = nullptr
        );
        
    private:
        // HTTP client pool (borrow/return pattern per host)
        std::shared_ptr<httplib::Client> borrowHttpClient_unsafe(const std::string& host);
        void returnHttpClient_unsafe(std::shared_ptr<httplib::Client> client);
        
        // Cookie jar with domain/path/expiry validation
        struct Cookie {
            std::string name;
            std::string value;
            std::string domain;
            std::string cookie_path;
            std::optional<std::chrono::system_clock::time_point> expires;
            bool secure;
            bool httpOnly;
            std::string sameSite;
        };
        std::vector<Cookie> cookies_;
        
        // WBI signature support (Bilibili anti-automation)
        bool encWbi_unsafe(std::unordered_map<std::string, std::string>& params) const;
        bool refreshWbiKeys_unsafe();
        
        std::mutex m_clientMutex_;
        std::unordered_map<std::string, std::string> headers_;
    };
}
```

---

### Method Contracts

#### `searchByTitle(title, page)`

**Preconditions**:
- `title` must be non-empty
- `page` >= 1

**Postconditions**:
- Returns vector of `BilibiliSearchResult`
- Each result includes title, bvid, author, cid, duration
- Results sorted by relevance (Bilibili API order)
- Maximum 20 results per page

**Error Handling**:
- Returns empty vector on API failure
- Logs error message internally

**Threading**:
- Blocking call (may take 1-3 seconds)
- Thread-safe (mutex-protected)

---

#### `getAudioUrlByParams(params)`

**Preconditions**:
- `params` is JSON string with: `{"bvid":"...", "cid":..., "qn":...}`
- `qn` is quality number (80=high, 64=medium, etc.)

**Postconditions**:
- Returns direct audio stream URL from Bilibili CDN
- URL valid for 1-2 hours (includes expiry token)
- WBI signature applied automatically

**Error Handling**:
- Returns empty string on failure
- May fail if video unavailable/restricted

**Threading**:
- Blocking call
- Thread-safe (mutex-protected)

---

#### `asyncDownloadStream(url, content_receiver, progress_callback)`

**Preconditions**:
- `url` must be valid Bilibili CDN URL (from `getAudioUrlByParams`)
- `content_receiver` is callback: `bool(const char* data, size_t len)` returning true to continue
- `progress_callback` is optional: `bool(uint64_t current, uint64_t total)`

**Postconditions**:
- Returns `future<bool>` resolving to true on complete download
- `content_receiver` called repeatedly with chunks
- `progress_callback` called periodically with progress
- Download runs in background thread

**Error Handling**:
- Future resolves to false on network error
- If `content_receiver` returns false, download cancelled

**Threading**:
- Non-blocking (returns future immediately)
- Background thread handles HTTP streaming

**Example**:
```cpp
RealtimePipe pipe;
auto future = biliInterface.asyncDownloadStream(
    audioUrl,
    [&](const char* data, size_t len) {
        pipe.write(data, len);
        return true;  // Continue
    },
    [](uint64_t current, uint64_t total) {
        qDebug() << "Progress:" << (current * 100 / total) << "%";
        return true;
    }
);
// When future completes, close pipe input
future.get();
pipe.closeInput();  // Signal end to consumer
```

---

## Integration with Playlist

### Creating SongInfo from Search Results

```cpp
playlist::SongInfo createSongFromSearchResult(const network::SearchResult& result) {
    playlist::SongInfo song;
    song.title = result.title;
    song.uploader = result.uploader;
    song.platform = static_cast<int>(result.platform);
    song.duration = result.duration;
    song.filepath = "";  // Empty for streaming-only
    song.coverName = result.coverImg;  // Use coverImg filename directly
    song.args = result.interfaceData;  // Platform-specific params
    
    // Download cover if it doesn't exist
    QString coverPath = configManager->getCoverCacheDirectory() + "/" + result.coverImg;
    if (!QFile::exists(coverPath)) {
        networkManager->downloadAsync(result.platform, result.coverUrl, coverPath);
    }
    
    return song;
}
```

### Creating SongInfo from Local File

```cpp
playlist::SongInfo createSongFromFile(const QString& filepath) {
    playlist::SongInfo song;
    song.title = QFileInfo(filepath).baseName();
    song.uploader = "Unknown";
    song.platform = -1;  // Local file indicator
    song.duration = 0;  // To be extracted by FFmpeg
    song.filepath = filepath;
    song.coverName = "";
    song.args = "";
    return song;
}
```

---

## Validation Rules

### SongInfo Validation

1. **Title**: Must be non-empty, max 256 chars
2. **Uploader**: Can be empty (defaults to "Unknown")
3. **Platform**: Valid `SupportInterface` enum value or -1 for local
4. **Duration**: >= 0 (0 means unknown, extracted later)
5. **Filepath**: Either valid local path OR empty for streaming
6. **Args**: Platform-specific validation:
   - Bilibili: JSON with `bvid`/`cid`
   - Local: empty
   - YouTube: URL or video ID (future)

### Network URL Validation

1. **HTTP/HTTPS only**: Must start with `http://` or `https://`
2. **Reachable**: DNS resolution succeeds
3. **Valid response**: HTTP 200 or 206 (partial content)
4. **Content-Type**: Should be audio/* or video/* (for video→audio extraction)

---

## Error Handling Patterns

### Network Errors

```cpp
try {
    auto streamFuture = netMgr.getAudioStreamAsync(platform, params);
    auto stream = streamFuture.get();  // May throw
    if (!stream) {
        qDebug() << "Stream is null";
        return;
    }
    // Use stream
} catch (const std::exception& e) {
    qDebug() << "Stream retrieval failed:" << e.what();
    // Fallback or retry logic
}
```

### Search Cancellation

```cpp
connect(&netMgr, &NetworkManager::searchProgress, this, &Widget::onSearchProgress);
connect(&netMgr, &NetworkManager::searchFailed, this, &Widget::onSearchFailed);

// User clicks cancel button
netMgr.cancelAllSearches();
```

---

## Performance Considerations

1. **Connection Pooling**: BilibiliNetworkInterface maintains per-host connection pools
   - Borrow/return pattern prevents connection exhaustion
   - Typical pool size: 1-2 clients per host
   - Tested in `test/test_http_pool.cpp`
   
2. **Streaming Buffer**: RealtimePipe provides circular buffer
   - Producer (network) writes chunks asynchronously
   - Consumer (FFmpeg decoder) reads on-demand
   - Prevents memory bloat for large files
   - Tested in `test/realtime_pipe_test.cpp`

3. **Search Debouncing**: UI should debounce search input (300ms)
   - Prevents excessive API calls during typing
   - Implemented in SearchPage

4. **Cookie Persistence**: Cookies saved to JSON on config save
   - Reduces login/auth requests
   - WBI keys cached with refresh logic
   - Tested in `test/test_cookie_jar.cpp`

---

## Testing Considerations

See test files:
- `test/test_http_pool.cpp`: HTTP connection pooling validation
- `test/test_cookie_jar.cpp`: Cookie domain/path/expiry matching
- `test/streaming_integration_test.cpp`: End-to-end streaming with RealtimePipe
- `test/fake_network_producer_test.cpp`: Mock network producer patterns
- `test/streaming_cancellation_test.cpp`: Cancellation token validation

---

## Platform Interface Architecture

### Design Pattern: Abstract Base Class

The network layer uses an abstract base class pattern to support multiple platforms while maintaining type safety and compile-time interface checking.

#### INetworkPlatformInterface

```cpp
namespace network {
    class INetworkPlatformInterface {
    public:
        virtual ~INetworkPlatformInterface() = default;

        // Configuration lifecycle
        virtual bool initializeDefaultConfig() = 0;
        virtual bool loadConfig(const std::string& config_file) = 0;
        virtual bool saveConfig(const std::string& config_file) = 0;
        virtual bool setPlatformDirectory(const std::string& platform_dir) = 0;
        virtual void setTimeout(int timeout_sec) = 0;
        virtual void setUserAgent(const std::string& user_agent) = 0;

        // Stream URL retrieval
        virtual std::string getAudioUrlByParams(const std::string& params) = 0;
        virtual uint64_t getStreamBytesSize(const std::string& url) = 0;

        // Asynchronous streaming
        virtual std::future<bool> asyncDownloadStream(
            const std::string& url,
            std::function<bool(const char*, size_t)> content_receiver,
            std::function<bool(uint64_t, uint64_t)> progress_callback = nullptr) = 0;
    };
}
```

**Design Rationale**:
- **searchByTitle excluded**: Returns platform-specific types (`BilibiliSearchResult`, future `YouTubeSearchResult`). Each platform implements its own version, and NetworkManager converts results to `network::SearchResult`.
- **Pure virtual methods**: Enforces implementation contract for all platforms.
- **Common operations**: Configuration, URL retrieval, and streaming are standardized across platforms.

#### BilibiliNetworkInterface Implementation

```cpp
class BilibiliNetworkInterface : public INetworkPlatformInterface {
public:
    // INetworkPlatformInterface implementation
    bool initializeDefaultConfig() override;
    bool loadConfig(const std::string& config_file = "bilibili.json") override;
    bool saveConfig(const std::string& config_file = "bilibili.json") override;
    bool setPlatformDirectory(const std::string& platform_dir) override;
    void setTimeout(int timeout_sec) override;
    void setUserAgent(const std::string& user_agent) override;
    
    std::string getAudioUrlByParams(const std::string& params) override;
    uint64_t getStreamBytesSize(const std::string& url) override;
    std::future<bool> asyncDownloadStream(
        const std::string& url,
        std::function<bool(const char*, size_t)> content_receiver,
        std::function<bool(uint64_t, uint64_t)> progress_callback = nullptr) override;

public: // Platform-specific methods
    // Platform-specific search - not in base interface
    std::vector<BilibiliSearchResult> searchByTitle(const std::string& title, int page = 1);
};
```

#### NetworkManager Usage Pattern

```cpp
class NetworkManager : public QObject {
private:
    std::unique_ptr<INetworkPlatformInterface> m_biliInterface;  // Base class pointer
    
    std::future<QList<SearchResult>> performBilibiliSearchAsync(
        const QString& keyword, int maxResults, std::atomic<bool>& cancelFlag) {
        
        return std::async(std::launch::async, [this, keyword, maxResults, &cancelFlag]() {
            // Cast to concrete type for platform-specific operations
            auto* biliInterface = dynamic_cast<BilibiliNetworkInterface*>(m_biliInterface.get());
            if (!biliInterface) {
                throw std::runtime_error("Invalid Bilibili interface");
            }
            
            // Call platform-specific searchByTitle
            auto pages = biliInterface->searchByTitle(keyword.toStdString());
            
            // Convert to generic SearchResult
            QList<SearchResult> results;
            for (const auto& page : pages) {
                results.append(SearchResult{
                    .title = QString::fromStdString(page.title),
                    .platform = SupportInterface::Bilibili,
                    .coverImg = QString("%1.jpg").arg(QString::fromStdString(page.bvid)),
                    .interfaceData = QString("bvid=%1&cid=%2")
                        .arg(QString::fromStdString(page.bvid))
                        .arg(page.cid)
                });
            }
            return results;
        });
    }
};
```

**Advantages**:
- **Type safety**: Compile-time interface checking via pure virtual methods.
- **Extensibility**: Easy to add new platforms (YouTube, SoundCloud) by inheriting from `INetworkPlatformInterface`.
- **Clean separation**: Platform-specific types and methods isolated from generic interface.
- **Polymorphism**: Base class pointer enables future platform switching at runtime.

---

## Future Extensions

### YouTube Support

```cpp
class YouTubeNetworkInterface : public INetworkPlatformInterface {
public:
    // INetworkPlatformInterface implementation
    bool initializeDefaultConfig() override;
    bool loadConfig(const std::string& config_file = "youtube.json") override;
    bool saveConfig(const std::string& config_file = "youtube.json") override;
    bool setPlatformDirectory(const std::string& platform_dir) override;
    void setTimeout(int timeout_sec) override;
    void setUserAgent(const std::string& user_agent) override;
    
    std::string getAudioUrlByParams(const std::string& params) override;  // Uses yt-dlp
    uint64_t getStreamBytesSize(const std::string& url) override;
    std::future<bool> asyncDownloadStream(/* ... */) override;

public: // Platform-specific methods
    std::vector<YouTubeSearchResult> searchByTitle(const std::string& title, int page = 1);
};
```

Integration into NetworkManager:
- Add `std::unique_ptr<INetworkPlatformInterface> m_youtubeInterface` member
- Update `executeMultiSourceSearch` to query YouTube interface
- Update `performYouTubeSearchAsync` (new method) to convert `YouTubeSearchResult` to `SearchResult`
- Update `getAudioStreamAsync` to handle YouTube params
- Implement yt-dlp subprocess execution in `YouTubeNetworkInterface::getAudioUrlByParams`

**Task Reference**: T026 (YouTube client implementation) - PRIORITY

---

## Thread Safety Summary

| Component | Thread Safety | Protection Mechanism |
|-----------|---------------|---------------------|
| NetworkManager | Thread-safe | Qt signals, atomic flags |
| BilibiliNetworkInterface | Thread-safe | `m_clientMutex_` protects all operations |
| SearchResult | Immutable | Qt value type, safe to copy |
| SongInfo | Immutable | Qt value type, safe to copy |

---

## Error Codes

| Error | Description |
|-------|-------------|
| `INVALID_PARAMS` | Platform params invalid or missing required fields |
| `NETWORK_UNREACHABLE` | Network error (DNS, connection, timeout) |
| `API_ERROR` | Platform API returned error response |
| `STREAM_FAILED` | Failed to retrieve audio stream |
| `CANCELLED` | Operation cancelled by user |

---
