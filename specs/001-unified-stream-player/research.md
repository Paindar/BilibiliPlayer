# Research: Unified Multi-Source Audio Stream Player

**Feature**: 001-unified-stream-player  
**Date**: 2025-11-13  
**Status**: Complete

## Purpose

This document consolidates research findings for technical decisions, platform integration approaches, and best practices for building a multi-source audio streaming application with Qt6, FFmpeg, and WASAPI.

---

## 1. Audio Decoding with FFmpeg

### Decision

Use FFmpeg's libavformat, libavcodec, and libswresample libraries for decoding audio from multiple formats and extracting audio from video sources.

### Rationale

- **Universal Format Support**: FFmpeg supports virtually all audio and video formats (MP3, FLAC, AAC, OGG, WAV, and video containers like MP4, MKV, WEBM)
- **Audio Extraction**: Can extract audio streams from video files without decoding video, saving CPU/memory
- **Proven Stability**: Industry-standard library used by VLC, MPV, and commercial media players
- **Conan Integration**: Available as Conan package, simplifying dependency management
- **Stream Protocol Support**: Handles HTTP/HTTPS streams natively, can pipe network data

### Implementation Approach

```
1. Use AVFormatContext to open local files or network streams
2. Find audio stream index with av_find_best_stream()
3. Open decoder with avcodec_open2()
4. Decode frames with avcodec_receive_frame()
5. Resample to target format (PCM, 48kHz, stereo) with swr_convert()
6. Push decoded PCM to circular buffer for playback
```

### Key APIs

- `avformat_open_input()` - Open audio/video file or stream
- `avformat_find_stream_info()` - Probe stream metadata
- `avcodec_find_decoder()` - Find appropriate decoder
- `avcodec_receive_frame()` - Decode audio frame
- `swr_convert()` - Resample to target format

### Alternatives Considered

- **Qt Multimedia**: Limited format support, less control over decoding pipeline
- **libsndfile**: Audio-only, doesn't handle video or network streams
- **Native OS codecs (Windows Media Foundation)**: Platform-specific, limited format coverage

---

## 2. Low-Latency Audio Output with WASAPI

### Decision

Use Windows Audio Session API (WASAPI) in exclusive or shared mode for audio output on Windows, with Qt Multimedia as fallback for cross-platform.

### Rationale

- **Low Latency**: WASAPI exclusive mode achieves <10ms latency, shared mode <50ms
- **Direct Hardware Access**: Bypasses Windows audio stack in exclusive mode
- **Sample Rate Control**: Can request native hardware sample rate to avoid resampling
- **High-Quality Audio**: Supports 24-bit, 32-bit, and high sample rates (96kHz, 192kHz)
- **Windows Primary Target**: Matches project's Windows-first deployment strategy

### Implementation Approach

```
1. Initialize COM with CoInitializeEx(COINIT_MULTITHREADED)
2. Get IMMDeviceEnumerator to enumerate audio devices
3. Get default audio endpoint with GetDefaultAudioEndpoint()
4. Activate IAudioClient interface
5. Initialize audio client with desired format (PCM, 48kHz, 16-bit stereo)
6. Get IAudioRenderClient for buffer access
7. Run playback thread that:
   - Waits for buffer availability
   - Copies PCM data from circular buffer to WASAPI buffer
   - Releases buffer for rendering
```

### Key APIs

- `IAudioClient::Initialize()` - Set up audio session
- `IAudioClient::GetBufferSize()` - Get buffer size in frames
- `IAudioClient::Start()/Stop()` - Control playback
- `IAudioRenderClient::GetBuffer()` - Get writable buffer
- `IAudioRenderClient::ReleaseBuffer()` - Submit buffer for playback

### Performance Targets

- Exclusive mode: 5-10ms latency, 256-512 frame buffer
- Shared mode: 30-50ms latency, 1024-2048 frame buffer
- Target: <100ms total latency (network buffering + decoding + output)

### Alternatives Considered

- **Qt Multimedia (QAudioSink)**: Simpler API but higher latency (~100-200ms), less control
- **PortAudio**: Cross-platform but adds extra dependency, WASAPI backend less optimized
- **DirectSound**: Deprecated, higher latency than WASAPI

---

## 3. Network Streaming with cpp-httplib

### Decision

Use cpp-httplib for HTTP/HTTPS requests to platform APIs and audio streaming, with connection pooling and async support.

### Rationale

- **Header-Only Option**: Can be integrated as header-only library, simplifying builds
- **HTTPS Support**: Works with OpenSSL for secure connections (required for Bilibili, YouTube)
- **Connection Pooling**: Reuses TCP connections, reducing latency for API calls
- **Streaming Support**: Can stream response bodies for progressive download
- **Simple API**: Modern C++11-style interface, easier than libcurl
- **Conan Available**: Managed via Conan package manager

### Implementation Approach

```
1. Create httplib::Client with connection pooling enabled
2. For Bilibili/YouTube API calls:
   - Set headers (User-Agent, cookies, API keys)
   - GET/POST requests with JSON payloads
   - Parse responses with jsoncpp
3. For audio streaming:
   - GET request with Range headers for partial content
   - Stream response body in chunks (8KB-64KB)
   - Push chunks to StreamingBuffer for decoder
   - Handle 206 Partial Content, 429 Rate Limit, 503 errors
4. Implement retry logic with exponential backoff
```

### Best Practices

- **Connection Pooling**: Reuse httplib::Client instances per domain
- **Timeout Configuration**: Set read timeout (5-10s), connection timeout (3s)
- **Error Handling**: Check status codes, log errors, retry transient failures (503, timeout)
- **Rate Limiting**: Track request timestamps, respect 429 responses
- **User-Agent**: Set realistic User-Agent to avoid blocking
- **Cookie Management**: Persist cookies for session continuity

### Alternatives Considered

- **libcurl**: More complex API, C-style interface, wider platform support
- **Qt Network (QNetworkAccessManager)**: Integrated with Qt but less control over connections
- **Boost.Beast**: Too heavy, requires Boost dependency

---

## 4. Bilibili Platform Integration

### Decision

Use Bilibili's public API endpoints for search and stream URL extraction, with web scraping fallback if API changes.

### Rationale

- **No Official SDK**: Bilibili doesn't provide official C++ SDK
- **Public APIs Available**: Community-documented APIs for search, video info, stream URLs
- **Stream URL Extraction**: Can extract direct audio/video URLs from API responses
- **Authentication Optional**: Most content accessible without login, cookies help with rate limits

### API Endpoints (Community-Documented)

- **Search**: `https://api.bilibili.com/x/web-interface/search/type?search_type=video&keyword={query}`
- **Video Info**: `https://api.bilibili.com/x/web-interface/view?bvid={bvid}`
- **Playback URL**: `https://api.bilibili.com/x/player/playurl?bvid={bvid}&cid={cid}&qn={quality}`

### Implementation Notes

- Parse JSON responses with jsoncpp
- Extract audio-only streams (quality=30216 for 64kbps audio)
- Handle rate limiting (typical limit: 10 req/s per IP)
- Cache results to minimize API calls
- Respect robots.txt and terms of service

### Alternatives Considered

- **Browser Automation (Selenium, Puppeteer)**: Too heavy, fragile, slow
- **Official API (if available)**: Not available for C++

---

## 5. YouTube Platform Integration

### Decision

Use yt-dlp (or youtube-dl) as subprocess to extract stream URLs, avoiding direct API integration.

### Rationale

- **Official API Complexity**: YouTube Data API v3 requires OAuth, quotas, and doesn't provide direct stream URLs
- **yt-dlp Reliability**: Community-maintained tool that handles YouTube's frequent changes
- **Subprocess Approach**: Call yt-dlp binary, parse JSON output, extract audio stream URL
- **Audio-Only Support**: Can request audio-only formats (webm, m4a) to save bandwidth

### Implementation Approach

```
1. Invoke yt-dlp as subprocess:
   yt-dlp --dump-json --format bestaudio <youtube-url>
2. Parse JSON output to extract:
   - title, duration, uploader
   - direct audio stream URL
   - thumbnail URL
3. Pass stream URL to FFmpeg for decoding
4. Cache URLs with expiration (typically valid for 6 hours)
```

### Best Practices

- **Error Handling**: yt-dlp may fail if video is removed, region-locked, or age-restricted
- **Update Management**: yt-dlp requires periodic updates to track YouTube changes
- **Rate Limiting**: YouTube may throttle requests; implement retry logic
- **Format Selection**: Prefer audio-only formats (140, 251) over full video

### Alternatives Considered

- **YouTube Data API v3**: Requires OAuth, quota limits, doesn't provide stream URLs
- **Direct Stream Extraction**: Fragile, YouTube frequently changes algorithms
- **libyt (hypothetical library)**: No stable C++ library available

---

## 6. Playlist Persistence with jsoncpp

### Decision

Use jsoncpp for serializing/deserializing playlists and configuration to JSON files.

### Rationale

- **Human-Readable Format**: JSON files can be manually edited if needed
- **Structured Data**: Supports nested objects (playlists contain categories contain songs)
- **Conan Available**: Managed via Conan, lightweight, fast parsing
- **C++ Integration**: Clean C++ API, integrates well with modern C++

### Schema Design

```json
{
  "version": "1.0",
  "categories": [
    {
      "id": "cat-001",
      "name": "Favorites",
      "playlists": [
        {
          "id": "pl-001",
          "name": "Chill Mix",
          "created": "2025-11-13T10:00:00Z",
          "modified": "2025-11-13T12:30:00Z",
          "items": [
            {
              "id": "item-001",
              "source_type": "bilibili",
              "url": "https://bilibili.com/video/BV1...",
              "title": "Song Title",
              "duration": 180,
              "uploader": "Artist Name",
              "thumbnail": "https://..."
            },
            {
              "id": "item-002",
              "source_type": "local",
              "path": "C:/Music/song.mp3",
              "title": "Local Song",
              "duration": 240
            }
          ]
        }
      ]
    }
  ]
}
```

### Implementation Notes

- Auto-save on changes with debouncing (5-second delay after last edit)
- Atomic writes (write to temp file, rename on success)
- Backup previous version on save
- Validate schema version on load, migrate if needed

### Alternatives Considered

- **Binary Format (Protocol Buffers, MessagePack)**: Faster but not human-readable
- **SQLite**: Overkill for simple playlist data, adds dependency
- **XML**: More verbose than JSON, less modern tooling

---

## 7. Threading Architecture with Qt and std::thread

### Decision

Use hybrid threading model: Qt signals/slots for UI communication, `std::thread` + `std::mutex` + `std::condition_variable` for audio/network threads.

### Rationale

- **UI Thread Isolation**: Qt UI must run on main thread, signals/slots for cross-thread communication
- **Audio Thread Dedicated**: Separate thread for WASAPI playback to avoid blocking
- **Decoder Thread Dedicated**: Separate thread for FFmpeg decoding
- **Network Threads**: Thread pool or async tasks for HTTP requests
- **Standard Primitives**: Prefer `std::mutex` over QMutex for portability and debuggability

### Threading Model

```
Main Thread (Qt UI):
  - Handles user input, updates UI widgets
  - Receives signals from background threads
  - Emits signals to trigger background work

Decoder Thread:
  - Reads from StreamingBuffer (network data)
  - Decodes with FFmpeg
  - Writes PCM to AudioBuffer (circular buffer)
  - Uses condition variable to wake on new data

Playback Thread (WASAPI):
  - Reads from AudioBuffer (circular buffer)
  - Writes to WASAPI output buffer
  - Uses condition variable to wake on buffer availability

Network Worker Threads:
  - HTTP requests via cpp-httplib
  - Stream downloads to StreamingBuffer
  - Emit signals on completion/error
```

### Synchronization Primitives

- **Circular Buffers**: Mutex-protected, condition variables for producer/consumer
- **Signals/Slots**: Qt's thread-safe signal/slot mechanism for UI updates
- **Unsafe Pattern**: Public methods lock mutex, delegate to private *Unsafe() methods

### Best Practices

- Minimize critical sections (copy data out, release lock, process)
- Use `std::lock_guard` or `std::unique_lock` for RAII locking
- Document lock ordering to prevent deadlocks
- Test with ThreadSanitizer to detect data races

### Alternatives Considered

- **Qt Threading (QThread)**: Can be used but `std::thread` preferred for non-UI threads
- **Single-Threaded Async**: Not feasible for audio (requires dedicated thread for WASAPI)

---

## 8. Configuration Management

### Decision

Use jsoncpp for config file I/O, in-memory Settings structure, ConfigManager singleton for access.

### Rationale

- **Centralized Access**: Single ConfigManager instance simplifies access patterns
- **Type Safety**: Settings struct provides compile-time type checking
- **Auto-Save**: Debounced writes on changes (5-second delay)
- **Defaults**: Hardcoded defaults if config file missing or corrupt

### Configuration Schema

```json
{
  "version": "1.0",
  "audio": {
    "buffer_size_ms": 500,
    "output_device": "default",
    "sample_rate": 48000,
    "bit_depth": 16
  },
  "network": {
    "cache_size_mb": 1024,
    "connection_timeout_ms": 3000,
    "read_timeout_ms": 10000,
    "retry_count": 3
  },
  "ui": {
    "language": "en",
    "theme": "light",
    "window_width": 1280,
    "window_height": 720
  },
  "platforms": {
    "bilibili": {
      "enabled": true,
      "cookies": "..."
    },
    "youtube": {
      "enabled": true,
      "yt_dlp_path": "yt-dlp.exe"
    }
  }
}
```

### Implementation Pattern

```cpp
class ConfigManager {
public:
    static ConfigManager& instance();
    
    Settings getSettings() const;
    void setSettings(const Settings& settings);
    
    void save();  // Async, debounced
    void load();  // Sync, on startup
    
private:
    std::mutex mutex_;
    Settings settings_;
    // ... save timer, debouncing logic
};
```

---

## 9. Event Bus for Inter-Component Communication

### Decision

Implement lightweight EventBus for decoupled component communication, using observer pattern with type-safe events.

### Rationale

- **Decoupling**: UI, playlist manager, audio player don't directly reference each other
- **Extensibility**: Easy to add new event types without modifying subscribers
- **Type Safety**: Template-based event types prevent runtime errors
- **Thread Safety**: Mutex-protected subscriber list, signals emitted from event bus thread

### Event Types

- `PlaybackStarted(AudioSource)`
- `PlaybackPaused()`
- `PlaybackStopped()`
- `PlaybackError(string error)`
- `PlaylistModified(Playlist)`
- `BufferingProgress(int percent)`
- `NetworkError(string url, string error)`

### Implementation Pattern

```cpp
class EventBus {
public:
    template<typename EventT>
    void subscribe(std::function<void(const EventT&)> handler);
    
    template<typename EventT>
    void publish(const EventT& event);
    
private:
    std::mutex mutex_;
    std::map<std::type_index, std::vector<std::function<void(const void*)>>> subscribers_;
};
```

---

## 10. Streaming Buffer Design

### Decision

Use circular buffer with mutex and condition variables for producer/consumer coordination between network and decoder threads.

### Rationale

- **Lock-Free Alternative Too Complex**: Lock-free ring buffers hard to debug, overkill for this use case
- **Bounded Buffer**: Fixed size prevents unbounded memory growth
- **Blocking Operations**: Condition variables allow efficient waiting (no busy polling)
- **Backpressure**: Producer blocks when buffer full, preventing memory overflow

### Implementation Approach

```cpp
class StreamingBuffer {
public:
    void write(const uint8_t* data, size_t size);  // Blocks if buffer full
    size_t read(uint8_t* buffer, size_t max_size); // Blocks if buffer empty
    void setEOF();  // Signal end of stream
    void cancel();  // Wake up blocked threads on shutdown
    
private:
    std::mutex mutex_;
    std::condition_variable cv_read_;
    std::condition_variable cv_write_;
    std::vector<uint8_t> buffer_;
    size_t read_pos_ = 0;
    size_t write_pos_ = 0;
    bool eof_ = false;
    bool cancelled_ = false;
};
```

### Buffer Sizing

- Default: 10-30 seconds of audio (configurable, default 10s)
- Ring buffer implementation: Atomic-protected for lock-free read/write
- Bounded memory: ~160KB for 10s @ 128kbps (prevents memory bloat)
- Underrun handling: Pause playback, show "buffering..." indicator
- Overrun handling: Network thread blocks (backpressure) until consumer reads

---

## Phase 0 Technical Investigations (2025-11-14)

### R01: Bilibili Audio Extraction

**Finding**: Bilibili separates audio and video streams. Audio-only URLs available via API with AAC codec.
- **Auth**: Requires session cookie (SESSDATA) + user agent
- **Rate Limits**: ~100 req/min metadata API; stream endpoints no hard limit
- **Format**: AAC standard for all audio extraction
- **Error Codes**: 404 (invalid video), 403 (geo-blocked), 429 (rate limited)
- **Implementation**: Use cpp-httplib + cookie management; retry with exponential backoff

### R02: FFmpeg 7.1.1 Codec Support

**Finding**: All MVP audio formats natively supported in FFmpeg 7.1.1
- MP3, AAC, WAV, OGG, FLAC all decode without additional codecs
- Performance: MP3/AAC fastest (~10-50ms frame), FLAC slower (~20-100ms) but acceptable
- No special compilation flags required; native builds from Conan package

### R03: Qt 6.8.0 Threading Safety

**Finding**: Qt signal/slot mechanism is thread-safe for main↔worker communication
- Use `Qt::QueuedConnection` for cross-thread signals
- Separate main thread (UI) from worker threads (I/O, decode)
- Atomic operations for shared state (buffer fill, playback position)
- Existing code validates patterns; no changes needed

### R04: Ring Buffer Architecture

**Finding**: Lock-free ring buffer with atomics is optimal for streaming
- Producer (network) writes via atomic operation
- Consumer (decoder) reads via atomic operation
- Resize rare, protected by QMutex
- Size recommendation: 10 seconds @ 128kbps = ~160KB default

### R05: Network Error Recovery

**Finding**: Exponential backoff retry with user notification balances reliability + responsiveness
- Transient errors: Auto-retry up to 3-5 times with 1s, 2s, 4s backoff
- Non-recoverable: Show error dialog (not found, geo-blocked, auth failed)
- Connection loss: Detect and attempt recovery within ~30 seconds
- Test: Existing fake_network_producer_test validates recovery patterns

### R06: Playlist Persistence

**Finding**: MVP uses JSON; Phase 2 upgrades to SQLite
- **MVP**: JSON files at ~/.config/BilibiliPlayer/playlists.json
- Format: Playlists array with items, categories, metadata
- Validation: UUID uniqueness, URL validation, duration bounds
- **Phase 2**: SQLite schema for O(1) lookups on 1000+ item playlists

### R07: HTTP Connection Pooling

**Finding**: Per-worker-thread HTTP client with keep-alive is optimal
- Pool size: 10 connections per host
- Timeouts: 10s establish, 30s request, 30s keep-alive
- Thread-safe: Each worker thread gets own cpp-httplib HttpClient instance
- Streaming: Dedicated connections for audio download (long-lived)

### R08: Bilibili Search API (Phase 2)

**Finding**: Bilibili public search API available; deferred to Phase 2
- Endpoint: `https://api.bilibili.com/x/web-interface/search/type?search_type=audio`
- Pagination: `pn` (page 1-indexed), `ps` (20 items per page)
- Latency: 500-1500ms typical (meets 2s target from FR-009)
- MVP: URL paste sufficient; search deferred as optional P3

---

## Summary of Key Decisions

| Area | Technology | Rationale |
|------|-----------|-----------|
| Audio Decoding | FFmpeg 7.1.1 (libavformat, libavcodec, libswresample) | Universal format support, native codec support for MP3/AAC/WAV/OGG/FLAC |
| Audio Output | WASAPI (Windows), Qt Multimedia (fallback) | Low latency (<100ms), direct hardware access |
| Network Client | cpp-httplib + OpenSSL + per-thread instances | Simple API, HTTPS support, thread-safe connection pooling |
| Bilibili Integration | Direct HTTP API + cookie management | No official SDK, platform provides audio-only streams (AAC) |
| YouTube Integration | yt-dlp wrapper (Phase 2) | Handles YouTube changes, audio-only extraction, video-decode abstraction |
| Playlist Storage | JSON (MVP), SQLite (Phase 2) | Human-readable and debuggable for MVP; indexed queries for large playlists Phase 2 |
| Threading | Qt signals/slots + atomic operations | Hybrid: Qt for UI events, atomics for shared streaming state |
| Configuration | jsoncpp + ConfigManager singleton | Centralized, type-safe, auto-save |
| Event Bus | Custom observer pattern | Decoupling, extensibility, thread-safe pub/sub |
| Streaming Buffer | Atomic-protected ring buffer | Lock-free read/write, bounded memory, backpressure |
| Error Recovery | Exponential backoff with user notification | Balances resilience with responsiveness; transparent to user for transient errors |

---

**Research Status**: ✅ COMPLETE - All Phase 0 investigations resolved

**Next Phase**: Phase 1 Design & Contracts (data-model.md, contracts/, quickstart.md)
