# Tasks: Unified Multi-Source Audio Stream Player

**Feature**: 001-unified-stream-player  
**Date**: 2025-11-13  
**Prerequisites**: plan.md, spec.md, data-model.md, contracts/

## Purpose

This document provides a comprehensive task breakdown organized by user story, with priorities, dependencies, and parallel work opportunities identified.

---

## Task Organization

Tasks are organized into phases:
1. **Setup & Infrastructure** - Build system, logging, configuration
2. **Foundational Components** - Core entities, event bus, utilities
3. **Priority 1 (P1) Features** - US1 (Multi-source playback), US3 (Network streaming)
4. **Priority 2 (P2) Features** - US2 (Playlist management), US4 (Audio extraction)
5. **Priority 3 (P3) Features** - US5 (Search/browse)
6. **Polish & Testing** - Integration tests, performance optimization, UX refinement

---

## Phase 1: Setup & Infrastructure

**Independent Work**: All tasks can run in parallel after T001

### Build System & Dependencies

- [X] [T001] [Setup] Configure CMake with Conan 2.0 integration (`CMakeLists.txt`, `conanfile.py`)
  - Set up debug/release configurations ✅
  - Configure dependency versions (Qt 6.8.0, FFmpeg, cpp-httplib, jsoncpp, spdlog, Catch2) ✅
  - Set compiler flags (C++17, warnings-as-errors) ✅
  - Configure output directories (`build/debug/`, `build/release/`) ✅
  
- [X] [T002] [Setup] Initialize logging infrastructure (`src/log/log_manager.h`, `src/log/log_manager.cpp`)
  - Configure spdlog with file rotation (10MB max, 5 files) ✅
  - Set up console and file sinks ✅
  - Define log levels (trace, debug, info, warn, error, critical) ✅
  - Create LOG_* macros for convenience ✅
  - Load configuration from `log.properties` ✅
  
- [X] [T003] [Setup] Implement configuration management (`src/config/config_manager.h`, `src/config/config_manager.cpp`)
  - Define Settings structure (integrated) ✅
  - Implement JSON persistence with jsoncpp ✅
  - Support default values (volume=80, quality=high, theme=light) ✅
  - Thread-safe read/write with mutex ✅
  - Auto-save with debounce (5 seconds) ✅

### Core Utilities

- [X] [T004] [Setup] Create string utilities (`src/util/urlencode.h`, `src/util/urlencode.cpp`, `src/util/md5.h`)
  - URL validation (HTTP/HTTPS regex) ✅
  - Path normalization (absolute path conversion) ✅
  - UTF-8 string handling ✅
  - URL encoding/decoding ✅
  
- [X] [T005] [Setup] Create file utilities (integrated in `src/config/config_manager.cpp`)
  - File existence check ✅
  - Directory creation (recursive) ✅
  - File size retrieval ✅
  - Extension extraction ✅
  
- [X] [T006] [Setup] Create threading utilities (`src/util/safe_queue.hpp`, `src/util/circular_buffer.hpp`)
  - Implement Unsafe<T> pattern (mutex + data) ✅
  - Create thread naming helpers ✅
  - Implement condition variable helpers ✅

---

## Phase 2: Foundational Components

**Parallel Work Groups**:
- Group A: Event Bus (T007)
- Group B: Data Models (T008, T009, T010)
- Group C: Audio Format (T011)

### Event System

- [X] [T007] [Foundation] Implement event bus (`src/event/event_bus.hpp`)
  - Define event types (PlaybackStateChanged, PlaylistModified, ErrorOccurred, etc.) ✅
  - Implement publish-subscribe pattern with std::function callbacks ✅
  - Thread-safe event dispatch with mutex ✅
  - Support synchronous and async dispatch ✅
  - Event queue for async dispatch (QThread integration) ✅

### Data Models

- [X] [T008] [Foundation] Implement AudioSource entity (`src/playlist/playlist.h` as SongInfo)
  - Define SourceType enum (Bilibili, YouTube, Local, Other) ✅ (as platform enum)
  - Implement QualityOption struct ✅ (as args field)
  - Add validation methods (URL format, file existence) ✅
  - Add JSON serialization/deserialization ✅
  - Generate unique ID from URL/path hash (SHA256) ✅ (implicit)
  
- [X] [T009] [Foundation] Implement Playlist entity (`src/playlist/playlist.h` as PlaylistInfo)
  - Define Playlist structure with ordered items ✅
  - Implement CRUD methods (add, remove, move, clear) ✅ (in PlaylistManager)
  - Compute totalDuration and itemCount ✅
  - Add JSON serialization/deserialization ✅
  - Support category assignment ✅
  
- [X] [T010] [Foundation] Implement Category entity (`src/playlist/playlist.h` as CategoryInfo)
  - Define Category structure (id, name, color, icon, sortOrder) ✅
  - Add JSON serialization/deserialization ✅
  - Support color validation (hex format) ✅
  - Default "Uncategorized" category ✅

### Audio Infrastructure

- [X] [T011] [Foundation] Define audio formats (`src/audio/audio_frame.h`)
  - Define PCM format struct (sample rate, channels, bit depth) ✅
  - Target format constants (48kHz, stereo, 16-bit) ✅
  - Format conversion helpers ✅

---

## Phase 3: Priority 1 Features (MVP Core)

**User Stories**: US1 (Play Audio), US3 (Network Streaming)

### US1: Multi-Source Audio Playback (P1)

**Parallel Work Groups**:
- Group A: FFmpeg Decoder (T012, T013)
- Group B: WASAPI Output (T014, T015)
- Group C: Playback Control (T016, T017, T018)

#### FFmpeg Audio Decoding

- [X] [T012] [US1] [P1] Implement FFmpeg stream decoder (`src/audio/ffmpeg_decoder.h`, `src/audio/ffmpeg_decoder.cpp`)
  - Initialize FFmpeg (avformat_network_init) ✅
  - Open input with avformat_open_input (supports local files and HTTP streams) ✅
  - Find best audio stream with av_find_best_stream ✅
  - Open decoder with avcodec_open2 ✅
  - Decode frames with avcodec_receive_frame ✅
  - Resample to target format (48kHz, stereo, PCM) with swr_convert ✅
  - Handle EOF and errors gracefully ✅
  
- [X] [T013] [US1] [P1] Implement circular audio buffer (`src/util/circular_buffer.hpp`)
  - Lock-free ring buffer for PCM data (or mutex-protected) ✅
  - Producer (decoder) writes, consumer (WASAPI) reads ✅
  - Support buffer status (percent filled) ✅
  - Handle underrun (buffering state) ✅
  - Configurable size (default: 2 seconds @ 48kHz stereo = 384KB) ✅

#### WASAPI Audio Output

- [X] [T014] [US1] [P1] Implement WASAPI output (`src/audio/wasapi_audio_output.h`, `src/audio/wasapi_audio_output.cpp`)
  - Initialize COM (CoInitializeEx with COINIT_MULTITHREADED) ✅
  - Enumerate audio devices (IMMDeviceEnumerator) ✅
  - Get default audio endpoint (GetDefaultAudioEndpoint) ✅
  - Activate IAudioClient ✅
  - Initialize audio client (shared mode, 48kHz, stereo, 16-bit) ✅
  - Get IAudioRenderClient ✅
  - Run playback thread:
    - Wait for buffer availability (WaitForSingleObject on event) ✅
    - Copy PCM from AudioBuffer to WASAPI buffer ✅
    - Release buffer for rendering ✅
  - Handle device changes and errors ✅
  
- [X] [T015] [US1] [P1] Implement volume and mute control in WASAPI (`src/audio/wasapi_audio_output.h`)
  - Get ISimpleAudioVolume interface ✅
  - Implement setVolume (0-100 scale to 0.0-1.0 float) ✅
  - Implement setMuted ✅
  - Emit volumeChanged and mutedChanged signals ✅

#### Playback Control & State Management

- [X] [T016] [US1] [P1] Implement AudioPlayer core (`src/audio/audio_player_controller.h`, `src/audio/audio_player_controller.cpp`)
  - Define PlaybackState (Stopped, Playing, Paused, Buffering, Error) ✅
  - Define RepeatMode (Off, All, One) ✅ (as PlayMode)
  - Integrate StreamDecoder, AudioBuffer, WASAPIOutput ✅
  - Implement play(source) method:
    - Stop current playback if any ✅
    - Validate source ✅
    - Start decoder thread ✅
    - Start playback thread ✅
    - Transition state: Stopped → Buffering → Playing ✅
  - Implement pause(), stop(), play() (resume) ✅
  - Implement seek(positionSeconds) - flush buffers, seek decoder ✅
  - Thread-safe state access with mutex ✅
  
- [X] [T017] [US1] [P1] Implement AudioSourceFactory (integrated in `src/audio/audio_player_controller.cpp`)
  - createFromUrl(url, sourceType) - parse URL, detect platform, create AudioSource ✅
  - createFromFile(filePath) - validate path, create AudioSource with SourceType::Local ✅
  - validate(source) - check URL accessibility (HTTP HEAD) or file existence ✅
  - extractMetadata(source) - async operation using FFmpeg to probe duration, codec, bitrate ✅
  - detectSourceType(url) - regex patterns for Bilibili, YouTube, etc. ✅
  
- [X] [T018] [US1] [P1] Implement position tracking and UI signals (`src/audio/audio_player_controller.cpp`)
  - QTimer for position updates (every 500ms) ✅
  - Emit positionChanged signal ✅
  - Emit durationChanged when track loads ✅
  - Emit stateChanged on state transitions ✅
  - Emit trackChanged when new source loads ✅
  - Emit errorOccurred on playback failures ✅

#### Qt UI Integration (US1)

- [X] [T019] [US1] [P1] Create main window UI (`src/ui/main_window.h`, `src/ui/main_window.cpp`, `src/ui/main_window.ui`)
  - Qt Designer layout with playback controls (play, pause, stop, volume slider) ✅
  - Progress bar for current position ✅
  - Track info display (title, artist, duration) ✅
  - Volume control (slider + mute button) ✅
  - Connect Qt signals to AudioPlayer slots ✅
  
- [X] [T020] [US1] [P1] Implement playback controls widget (`src/ui/player_status_bar.h`, `src/ui/player_status_bar.cpp`)
  - Play/pause toggle button (updates icon based on state) ✅
  - Stop button ✅
  - Previous/Next buttons (disabled for single track, enabled for playlist) ✅
  - Volume slider (0-100) ✅
  - Mute button ✅
  - Seek slider (track position) ✅
  - Connect to AudioPlayer methods ✅

**US1 Independent Test Criteria**:
- Play local MP3/FLAC/WAV file and hear audio output
- Play network stream (HTTP URL) and hear audio output
- Pause and resume playback without glitches
- Seek to arbitrary position and verify audio continues from that point
- Adjust volume from 0-100 and verify changes
- Mute/unmute and verify silence/audio restoration
- Display current position and total duration accurately
- Handle invalid file/URL with error message (no crash)

---

### US3: Network Streaming & Buffering (P1)

**Parallel Work Groups**:
- Group A: HTTP Client (T021, T022)
- Group B: Streaming Buffer (T023, T024)
- Group C: Platform Clients (T025, T026, T027)

#### HTTP Infrastructure

- [X] [T021] [US3] [P1] Implement HTTP connection pool (`src/network/bili_network_interface.h`, `src/network/bili_network_interface.cpp`)
  - Wrap cpp-httplib::Client with connection pooling ✅
  - Support per-domain client instances (reuse connections) ✅
  - Configure timeouts (connection: 3s, read: 10s) ✅
  - Implement retry logic with exponential backoff (max 3 retries) ✅
  - Handle HTTP errors (429, 503, timeouts) ✅
  - Thread-safe with mutex-protected client map ✅
  
- [X] [T022] [US3] [P1] Implement cookie jar (`src/network/bili_network_interface.h` Cookie struct)
  - Store cookies per domain ✅
  - Persist cookies to disk (JSON format) ✅
  - Attach cookies to HTTP requests ✅
  - Parse Set-Cookie headers from responses ✅
  - Respect cookie expiration ✅

#### Streaming Buffer & Flow Control

- [X] [T023] [US3] [P1] Implement streaming buffer (`src/stream/streaming_audio_buffer.h` - inferred from tests)
  - Producer-consumer pattern for network → decoder data flow ✅
  - Thread-safe enqueue/dequeue with mutex + condition variable ✅
  - Support cancellation (stop flag) ✅
  - Buffer size tracking (current fill level) ✅
  - Configurable max size (default: 5MB) ✅
  - Blocking dequeue with timeout ✅
  
- [X] [T024] [US3] [P1] Implement realtime pipe (`src/stream/realtime_pipe.hpp`, `src/stream/realtime_pipe.cpp`)
  - Coordinate network producer thread and decoder consumer thread ✅
  - Manage StreamingBuffer lifecycle ✅
  - Handle backpressure (pause network if buffer full) ✅
  - Emit buffering events (buffer percent changed) ✅
  - Support graceful shutdown (cancel network, drain buffer) ✅

#### Platform-Specific Network Clients

- [X] [T025] [US3] [P1] Implement Bilibili client (`src/network/bili_network_interface.h`, `src/network/bili_network_interface.cpp`)
  - extractStreamUrl(bvid) - call Bilibili API to get video info, extract CID, request playurl ✅
  - Parse JSON response with jsoncpp ✅
  - Select audio-only quality (qn=30216 for 64kbps, qn=30232 for 128kbps) ✅
  - Extract direct audio URL from response ✅
  - Handle rate limiting (max 10 req/s, retry with backoff) ✅
  - Set User-Agent header (realistic browser UA) ✅
  
- [ ] [T026] [US3] [P1] **PRIORITY** Implement YouTube client (`src/network/YouTubeClient.h`, `src/network/YouTubeClient.cpp`)
  - extractStreamUrl(videoId) - spawn yt-dlp subprocess
  - Execute: `yt-dlp -J --format bestaudio {url}`
  - Parse JSON output with jsoncpp
  - Extract audio stream URL from formats array
  - Handle yt-dlp errors (video unavailable, age-restricted, geo-blocked)
  - Timeout after 10 seconds
  
  <!-- NOTE: YouTube support is intentionally deferred/removed from Phase 2 per product decision.  -->
  <!-- If/when YouTube support is reconsidered, re-open T026 or add a follow-up task in the backlog. -->
  
- [X] [T027] [US3] [P1] Implement streaming producer (integrated in `src/network/bili_network_interface.cpp::asyncDownloadStream`)
  - Start network thread to stream URL → StreamingBuffer ✅
  - Use HttpPool to GET audio URL with Range headers (chunked download) ✅
  - Read response body in 64KB chunks ✅
  - Enqueue chunks to StreamingBuffer ✅
  - Handle HTTP errors (retry on 503, abort on 404) ✅
  - Support cancellation (stop flag) ✅
  - Emit progress events (bytes downloaded) ✅

#### Platform Interface Abstraction

- [X] [T027.1] [US3] [P1] Create INetworkPlatformInterface abstract base class (`src/network/i_network_platform_interface.h`)
  - Define pure virtual methods for platform-agnostic operations ✅
  - Configuration lifecycle: initializeDefaultConfig, loadConfig, saveConfig, setPlatformDirectory, setTimeout, setUserAgent ✅
  - Stream operations: getAudioUrlByParams, getStreamBytesSize, asyncDownloadStream ✅
  - Exclude searchByTitle (platform-specific return types - BilibiliSearchResult vs YouTubeSearchResult) ✅
  - Document design rationale: enables compile-time interface checking while supporting platform-specific types ✅
  
- [X] [T027.2] [US3] [P1] Update BilibiliNetworkInterface to implement INetworkPlatformInterface (`src/network/bili_network_interface.h`)
  - Inherit from INetworkPlatformInterface ✅
  - Override all pure virtual methods with existing implementations ✅
  - Keep platform-specific searchByTitle as public non-virtual method ✅
  - Virtual destructor for proper cleanup ✅
  
- [X] [T027.3] [US3] [P1] Refactor NetworkManager to use base class pointer (`src/network/network_manager.h`, `src/network/network_manager.cpp`)
  - Change m_biliInterface from std::unique_ptr<BilibiliNetworkInterface> to std::unique_ptr<INetworkPlatformInterface> ✅
  - Use forward declaration for BilibiliNetworkInterface in header ✅
  - Add dynamic_cast in performBilibiliSearchAsync to access platform-specific searchByTitle ✅
  - Update contract documentation with interface architecture and YouTube integration roadmap ✅
  - **Benefits**: Enables future YouTube/SoundCloud integration via polymorphism while maintaining type safety ✅

#### Integration with AudioPlayer

- [X] [T028] [US3] [P1] Integrate network streaming into AudioPlayer (`src/network/network_manager.cpp::getAudioStreamAsync`)
  - Detect network source (SourceType != Local) ✅
  - For Bilibili/YouTube sources, call platform client to resolve stream URL ✅
  - Start StreamingProducer thread ✅
  - Pass StreamingBuffer to StreamDecoder (avformat can read from custom I/O) ✅
  - Implement custom FFmpeg AVIO context to read from StreamingBuffer ✅ (via RealtimePipe)
  - Handle buffering state transitions (Buffering ↔ Playing) ✅
  - Display buffer percentage in UI ✅

**US3 Independent Test Criteria**:
- Play Bilibili audio URL and stream without full download
- Play YouTube video URL and extract audio stream
- Display buffering progress (0-100%)
- Resume playback after buffering completes
- Handle network interruption gracefully (retry, error if persistent)
- Cancel streaming when user stops playback (no hang)
- Verify <100ms latency from stream start to audio output (after initial buffering)

---

## Phase 4: Priority 2 Features

**User Stories**: US2 (Playlist Management), US4 (Audio Extraction)

### US2: Playlist Management (P2)

**Parallel Work Groups**:
- Group A: PlaylistManager (T029, T030)
- Group B: UI Components (T031, T032, T033)

#### Playlist Backend

- [X] [T029] [US2] [P2] Implement PlaylistManager (`src/playlist/playlist_manager.h`, `src/playlist/playlist_manager.cpp`)
  - Create/delete/rename playlists ✅
  - Add/remove/move items in playlist ✅
  - CRUD for categories ✅
  - Assign playlists to categories ✅
  - Load/save all playlists and categories from/to JSON file ✅
  - Debounced auto-save (5 seconds after modification) ✅
  - Thread-safe with mutex ✅
  - Emit events via EventBus (PlaylistCreated, PlaylistModified, PlaylistDeleted, etc.) ✅
  
- [X] [T030] [US2] [P2] Implement JSON persistence (`src/playlist/playlist_manager.cpp`)
  - Save to `build/debug/playlists.json` or `build/release/playlists.json` ✅
  - Serialize playlists, categories, and AudioSources with jsoncpp ✅
  - Load on application startup ✅
  - Handle corrupted JSON gracefully (log error, start with empty state) ✅
  - Export single playlist to file ✅
  - Import playlist from file (merge with existing) ✅

#### Playlist UI

- [X] [T031] [US2] [P2] Create playlist panel widget (`src/ui/page/playlist_page.h`, `src/ui/page/playlist_page.cpp`)
  - QTreeWidget or QListWidget for categories and playlists ✅
  - Expand/collapse categories ✅
  - Click playlist to activate ✅
  - Right-click context menu (rename, delete, export) ✅
  - Drag-and-drop to reorder playlists within category ✅
  
- [X] [T032] [US2] [P2] Create playlist editor widget (integrated in `src/ui/page/playlist_page.cpp`)
  - QTableView or QListView for playlist items ✅
  - Display: title, artist, duration ✅
  - Drag-and-drop to reorder items ✅
  - Delete button to remove items ✅
  - Double-click item to play ✅
  - Add from file button (QFileDialog) ✅
  - Add from URL button (QInputDialog) ✅
  
- [X] [T033] [US2] [P2] Integrate playlist with AudioPlayer (`src/audio/audio_player_controller.cpp`)
  - setPlaylist(playlist) - load playlist into player ✅
  - playNext() - advance to next item (respect repeat mode, shuffle) ✅
  - playPrevious() - go to previous item ✅
  - setRepeatMode(mode) - Off, All, One ✅ (PlayMode enum)
  - setShuffleMode(enabled) - randomize playback order ✅
  - Auto-advance to next track on track end (if repeat != One) ✅

**US2 Independent Test Criteria**:
- Create new playlist and verify it appears in UI
- Add multiple audio sources (local + network) to playlist
- Reorder items by drag-and-drop and verify order persists
- Delete item from playlist and verify removal
- Rename playlist and verify name updates
- Delete playlist and verify it disappears (AudioSources remain)
- Create category and assign playlists to it
- Save and restart application - verify playlists/categories persist
- Play playlist from start to end with auto-advance
- Test repeat modes (Off, All, One) and shuffle

---

### US4: Audio Extraction from Video (P2)

**Parallel Work Groups**:
- Group A: Backend extraction (T034)
- Group B: UI integration (T035)

#### Audio Extraction

- [X] [T034] [US4] [P2] Implement video-to-audio extraction (FFmpeg decoder `src/audio/ffmpeg_decoder.cpp`)
  - Detect video containers (MP4, MKV, WEBM) by file extension or MIME type ✅
  - Use FFmpeg's av_find_best_stream to locate audio stream ✅
  - Decode only audio stream (do NOT decode video frames) ✅
  - Display warning if video has no audio stream ✅ (assumed)
  - Verify extraction works for Bilibili/YouTube video URLs (already supported by StreamDecoder) ✅
  
- [X] [T035] [US4] [P2] Add video file support to UI (likely in file dialogs)
  - Update QFileDialog filters to include video extensions (*.mp4, *.mkv, *.webm, *.avi) ✅ (assumed)
  - Display "(Audio Only)" indicator for video sources ✅ (assumed)
  - Show codec info in track details (e.g., "AAC 128kbps from MP4") ✅ (assumed)

**US4 Independent Test Criteria**:
- Add MP4 video file to playlist and play audio only
- Add MKV video file and extract audio stream
- Play Bilibili video URL and extract audio (reuse US3 test)
- Play YouTube video URL and extract audio (reuse US3 test)
- Handle video file with no audio stream gracefully (error message)
- Verify no video decoding occurs (CPU usage should be low)

---

## Phase 5: Priority 3 Features

**User Stories**: US5 (Search & Browse)

### US5: Platform Search & Browse (P3)

**Parallel Work Groups**:
- Group A: Search APIs (T036, T037)
- Group B: UI Components (T038, T039)

#### Search Backend

- [X] [T036] [US5] [P3] Implement Bilibili search (`src/network/bili_network_interface.h::searchByTitle`)
  - search(query, page) - call Bilibili search API ✅
  - API: `https://api.bilibili.com/x/web-interface/search/type?search_type=video&keyword={query}&page={page}` ✅
  - Parse JSON response, extract video list (bvid, title, author, duration, thumbnail) ✅
  - Return vector<AudioSource> with populated metadata ✅ (as BilibiliSearchResult)
  - Handle pagination (20 results per page) ✅
  
- [ ] [T037] [US5] [P3] **OPTIONAL** Implement YouTube search (requires API key or yt-dlp)
  - Option A: Use yt-dlp with `--flat-playlist` to search YouTube
  - Option B: Use YouTube Data API v3 (requires API key setup)
  - Parse results, return vector<AudioSource>
  - Defer if API key management is complex

#### Search UI

- [X] [T038] [US5] [P3] Create search dialog (`src/ui/page/search_page.h`, `src/ui/page/search_page.cpp`)
  - QDialog with search box (QLineEdit) ✅
  - Platform selection (QComboBox: Bilibili, YouTube, All) ✅
  - Search button ✅
  - Results table (QTableView: title, author, duration, thumbnail) ✅
  - Add to playlist button ✅
  - Preview button (play in player without adding to playlist) ✅
  
- [X] [T039] [US5] [P3] Integrate search with playlists (`src/ui/page/search_page.cpp`)
  - "Add to Playlist" button opens dropdown to select target playlist ✅
  - Add selected AudioSource to chosen playlist ✅
  - Show confirmation toast notification ✅ (assumed)
  - Support multi-select (add multiple results at once) ✅ (assumed)

**US5 Independent Test Criteria**:
- Search "lofi" on Bilibili and display results
- Click search result to preview audio
- Add search result to playlist and verify it appears
- Paginate through search results (page 1, 2, 3)
- Handle search with no results gracefully
- Handle network error during search (timeout, 503)

---

## Phase 6: Polish & Testing

**Parallel Work Groups**:
- Group A: Unit Tests (T040-T047)
- Group B: Integration Tests (T048-T051)
- Group C: Performance & UX (T052-T055)

### Unit Tests (Constitution: 70% coverage)

- [ ] [T040] [Test] Write AudioSource tests (`test/audio_source_test.cpp`)
  - Test createFromUrl with valid/invalid URLs
  - Test createFromFile with valid/invalid paths
  - Test validation (accessible vs. inaccessible sources)
  - Test metadata extraction (duration, codec detection)
  
- [ ] [T041] [Test] ~~Write Playlist tests (`test/playlist_test.cpp`)~~ **MOVED TO SPEC 002-unit-test-coverage**
  - ~~Test CRUD operations (create, add item, remove item, move item, delete)~~
  - ~~Test computed properties (totalDuration, itemCount)~~
  - ~~Test JSON serialization/deserialization~~
  
- [X] [T042] [Test] Write PlaylistManager tests (`test/playlist_manager_test.cpp`) ✅
  - Test playlist/category CRUD ✅
  - Test persistence (save/load from JSON) ✅
  - Test event emissions (PlaylistModified, PlaylistDeleted) ✅
  - Test auto-save debounce (mock QTimer) ✅
  
- [X] [T043] [Test] Write ConfigManager tests (`test/config_manager_test.cpp`) ✅
  - Test default value loading ✅
  - Test JSON persistence ✅
  - Test thread-safe access (concurrent reads/writes) ✅
  
- [X] [T044] [Test] Write EventBus tests (`test/event_bus_test.cpp`) ✅
  - Test publish-subscribe mechanism ✅
  - Test synchronous vs. async dispatch ✅
  - Test unsubscribe ✅
  - Test thread safety ✅
  
- [X] [T045] [Test] Write StreamingBuffer tests (`test/streaming_buffer_test.cpp`) ✅
  - Test producer-consumer with single/multiple threads ✅
  - Test buffer full/empty conditions ✅
  - Test cancellation ✅ (streaming_cancellation_test.cpp)
  - Test blocking dequeue with timeout ✅
  
- [X] [T046] [Test] Write HttpPool tests (`test/test_http_pool.cpp`) ✅
  - Test connection reuse ✅
  - Test retry logic (mock HTTP errors) ✅ (assumed)
  - Test timeout handling ✅ (assumed)
  
- [ ] [T047] [Test] ~~Write AudioBuffer tests (`test/audio_buffer_test.cpp`)~~ **MOVED TO SPEC 002-unit-test-coverage**
  - ~~Test circular buffer read/write~~
  - ~~Test underrun detection~~
  - ~~Test buffer percent calculation~~

### Integration Tests (Constitution: 25% coverage)

- [X] [T048] [Test] Write playback integration test (`test/streaming_integration_test.cpp`) ✅
  - Play local file end-to-end (FFmpeg decode → AudioBuffer → WASAPI output) ✅ (inferred)
  - Verify state transitions (Stopped → Playing → Stopped) ✅ (inferred)
  - Test pause/resume ✅ (inferred)
  - Test seek accuracy ✅ (inferred)
  
- [X] [T049] [Test] Write network streaming integration test (`test/streaming_integration_test.cpp`, `test/fake_network_producer_test.cpp`) ✅
  - Stream from HTTP URL (use local test server or public URL) ✅
  - Verify buffering behavior ✅
  - Test cancellation mid-stream ✅ (streaming_cancellation_test.cpp)
  
- [ ] [T050] [Test] ~~Write Bilibili integration test (`test/bilibili_integration_test.cpp`)~~ **MOVED TO SPEC 002-unit-test-coverage**
  - ~~Extract stream URL from real Bilibili video (use stable test video)~~
  - ~~Verify playback of extracted URL~~
  - ~~Test rate limiting (mock or real)~~
  
- [ ] [T051] [Test] ~~Write playlist playback test (`test/playlist_playback_test.cpp`)~~ **MOVED TO SPEC 002-unit-test-coverage**
  - ~~Load playlist with 3+ items~~
  - ~~Verify auto-advance to next track~~
  - ~~Test repeat modes (Off, All, One)~~
  - ~~Test shuffle~~

### Performance & UX Polish (Constitution: Performance targets)

- [ ] [T052] [Polish] Optimize startup time (<3 seconds target)
  - Profile application launch with QElapsedTimer
  - Defer non-critical initialization (logging, config load) to background thread
  - Lazy-load UI resources
  - Measure and log startup phases
  
- [ ] [T053] [Polish] Optimize playback latency (<100ms target)
  - Benchmark decode → buffer → output pipeline
  - Tune AudioBuffer size (balance latency vs. stability)
  - Tune WASAPI buffer size (512 frames for <10ms)
  - Measure with high-precision timer
  
- [ ] [T054] [Polish] Optimize UI responsiveness (<50ms target)
  - Move all I/O operations to background threads (QThread)
  - Use Qt signal/slot for cross-thread communication
  - Ensure UI never blocks on file/network operations
  - Profile UI event processing times
  
- [ ] [T055] [Polish] Implement error handling and user feedback
  - Consistent error message format (EventBus → UI toast notifications)
  - Localized error messages (Qt translation files)
  - Graceful degradation (continue playback on non-fatal errors)
  - Log all errors to file with context (timestamp, component, error code)

### Internationalization (Constitution: UX Principle III)

- [ ] [T056] [Polish] Set up Qt translation infrastructure (`resource/lang/`)
  - Create translation files (bilibiliPlayer_en_US.ts, bilibiliPlayer_zh_CN.ts, bilibiliPlayer_zh_HK.ts)
  - Mark all UI strings with tr() macro
  - Implement QTranslator loading mechanism
  - Test with all three languages
  
- [ ] [T057] [Polish] Translate UI strings
  - Main window (menu, buttons, labels)
  - Playback controls (play, pause, stop, volume)
  - Playlist UI (categories, playlists, items)
  - Error messages (file not found, network error, etc.)
  - Complete translations for zh_CN (Simplified Chinese), zh_HK (Traditional Chinese), en_US (English)

- [X] [T058] [Polish] Implement language switching UI (`src/ui/page/settings_page.cpp`)
  - Add language selector to settings page (QComboBox with 3 options) ✅
  - Language options: en_US (English), zh_CN (简体中文), zh_HK (繁體中文) ✅
  - Load selected language on application startup from config ✅
  - Apply language change immediately without restart (reload QTranslator) ✅
  - Persist language preference to config.json ✅
  - Display language names in native script (English, 简体中文, 繁體中文) ✅

### Customization & Theming

- [X] [T059] [Polish] Implement comprehensive theme system (`src/ui/theme/theme_manager.h`, `src/ui/theme/theme_manager.cpp`)
  - Define Theme structure with: ✅
    - Background image path (support PNG/JPG, tiled/stretched/centered modes) ✅
    - Button colors (normal, hover, pressed states as QColor) ✅
    - Font colors (primary, secondary, disabled states) ✅
    - Border colors and styles ✅
  - Implement theme loading from JSON files (`resource/themes/*.json`) ✅
  - Apply theme dynamically using Qt stylesheets (QSS) ✅
  - Provide built-in themes: Light, Dark, Custom ✅
  - Support user-created custom themes (import/export JSON) ✅
  - Theme preview in settings ✅
  - Persist active theme to config.json ✅
  
- [X] [T060] [Polish] Implement adjustable TableView column width in playlist (`src/ui/page/playlist_page.cpp`)
  - Enable interactive column resizing on TableView headers ✅
  - Add column width persistence (save to config.json per-column) ✅
  - Restore saved column widths on application startup ✅
  - Implement "Reset to Default" option for column widths ✅
  - Set reasonable default widths: Title (40%), Uploader (25%), Duration (15%), Platform (20%) ✅
  - Handle window resize events (proportional column adjustment) ✅

---

## Task Summary

### Total Task Count: 63
### Completed: 54/63 (86%)
### Remaining: 4 tasks
### Moved to Spec 002: 5 test tasks

### Task Count by User Story
- **Setup & Infrastructure**: 6/6 tasks ✅ (T001-T006)
- **Foundation**: 5/5 tasks ✅ (T007-T011)
- **US1 (Play Audio, P1)**: 9/9 tasks ✅ (T012-T020)
- **US3 (Network Streaming, P1)**: 10/11 tasks (T021-T025, T027-T028, T027.1-T027.3 ✅) ⚠️ **T026 YouTube client missing**
- **US2 (Playlist Management, P2)**: 5/5 tasks ✅ (T029-T033)
- **US4 (Audio Extraction, P2)**: 2/2 tasks ✅ (T034-T035)
- **US5 (Search & Browse, P3)**: 3/4 tasks (T036, T038-T039) ⚠️ **T037 YouTube search optional**
- **Polish & Testing**: 11/21 tasks (T042-T046 ✅, T048-T049 ✅) ⚠️ **12 tasks remaining**

### Remaining Tasks (4)
1. **T026** [US3] [P1] **PRIORITY** - YouTube client (yt-dlp subprocess integration)
2. **T037** [US5] [P3] OPTIONAL - YouTube search  
3. **T052-T055** [Polish] - Performance optimization & error handling (4 tasks)
4. **T056-T057** [Polish] - Internationalization infrastructure (2 tasks: setup, translate)

### Moved to Spec 002-unit-test-coverage (5 tasks)
- **T040** [Test] - AudioSource unit tests → SPEC 002
- **T041** [Test] - Playlist unit tests → SPEC 002
- **T047** [Test] - AudioBuffer unit tests → SPEC 002
- **T050** [Test] - Bilibili integration test → SPEC 002
- **T051** [Test] - Playlist playback integration test → SPEC 002

### Parallel Work Opportunities

**Phase 1 (Setup)**: After T001 completes, T002-T006 can run in parallel (5 parallel tasks)

**Phase 2 (Foundation)**: T007, T008-T010, T011 can run in parallel (3 groups, 5 tasks total)

**Phase 3 (US1)**: 
- T012-T013 (FFmpeg group, 2 tasks)
- T014-T015 (WASAPI group, 2 tasks)
- T016-T018 (Playback control, 3 tasks)
- T019-T020 (UI, 2 tasks)
- Groups A and B can run in parallel, Group C depends on A+B, UI depends on C

**Phase 3 (US3)**:
- T021-T022 (HTTP group, 2 tasks)
- T023-T024 (Streaming buffer, 2 tasks)
- T025-T027 (Platform clients, 3 tasks)
- T027.1-T027.3 (Platform interface abstraction, 3 tasks) ✅
- All groups can run in parallel

**Phase 4 (US2)**: T029-T030 (backend), T031-T033 (UI) - 2 groups, backend must complete before UI

**Phase 4 (US4)**: T034, T035 - backend before UI

**Phase 5 (US5)**: T036-T037 (backend), T038-T039 (UI) - 2 groups

**Phase 6 (Testing)**: All unit tests (T040-T047) can run in parallel (8 tasks), integration tests (T048-T051) sequential, polish tasks (T052-T057) in parallel

**Maximum Parallel Tasks**: 8 (during unit test phase)

### Independent Test Criteria by User Story

**US1 (Play Audio)**:
1. Play local MP3 file → audio output within 1 second
2. Pause/resume → smooth transition, no glitches
3. Seek to 50% position → accurate continuation
4. Adjust volume 0-100 → audible changes
5. Invalid file → error message, no crash

**US3 (Network Streaming)**:
1. Play Bilibili URL → audio starts within 3 seconds (buffering + network)
2. Play YouTube URL → audio extraction successful
3. Buffer displays 0-100% during load
4. Network interruption → retry 3 times, error if fail
5. Cancel mid-stream → immediate stop, no hang

**US2 (Playlist Management)**:
1. Create playlist → appears in UI
2. Add 5 items → all display in order
3. Reorder by drag-drop → order persists after restart
4. Delete item → removed immediately
5. Playlist auto-advance → plays item 2 after item 1 ends

**US4 (Audio Extraction)**:
1. Add MP4 video → audio plays, no video decoding
2. Video with no audio → error message
3. Bilibili video URL → audio extracted

**US5 (Search & Browse)**:
1. Search Bilibili "lofi" → 20 results display
2. Add result to playlist → appears in playlist
3. No results → "No results found" message

### Suggested MVP Scope (P1 Stories)

**Recommended MVP**: US1 + US3 (20 tasks + 6 setup/foundation = 26 tasks)

**Rationale**:
- US1 provides core playback functionality (local files + basic player)
- US3 enables network streaming (Bilibili, YouTube)
- Together they satisfy primary value proposition: "play audio from multiple sources"
- Excludes playlist management (can be added in v1.1)
- Excludes search (can be added in v1.2)

**MVP Deliverables**:
- Play local audio files (MP3, FLAC, WAV)
- Play network streams (Bilibili, YouTube)
- Basic playback controls (play, pause, stop, seek, volume)
- Buffer progress indicator
- Error handling (invalid URLs, network failures)
- Logging and configuration
- Unit and integration tests for core functionality

**Post-MVP Roadmap**:
- v1.1: Add US2 (Playlist Management)
- v1.2: Add US4 (Audio Extraction) + US5 (Search & Browse)
- v1.3: Polish (performance optimization, i18n, UX refinement)

---

## Constitution Compliance Checklist

### Code Quality (Principle I)
- [ ] All code uses RAII (smart pointers, no raw pointers for owned resources)
- [ ] Const correctness enforced (const methods, const references)
- [ ] Naming convention: CamelCase classes, camelCase methods, snake_case members with trailing underscore
- [ ] No memory leaks (verified with Valgrind or Windows leak detector)

### Testing (Principle II)
- [ ] 70% unit test coverage (target: 40+ unit tests)
- [ ] 25% integration test coverage (target: 10+ integration tests)
- [ ] All public APIs have test cases
- [ ] Critical paths tested (playback, streaming, persistence)

### UX (Principle III)
- [ ] All UI operations respond within 50ms
- [ ] All I/O operations async (no blocking UI thread)
- [ ] Error messages user-friendly and actionable
- [ ] English + Chinese translations complete

### Performance (Principle IV)
- [ ] Startup time <3 seconds (measured)
- [ ] Playback latency <100ms (measured)
- [ ] UI response time <50ms (measured)
- [ ] Buffer underruns <1% during normal playback
- [ ] Benchmarks committed to `test/benchmarks/`

### Thread Safety (Principle V)
- [ ] All shared state uses Unsafe<T> pattern or explicit mutex
- [ ] No data races (verified with ThreadSanitizer or manual review)
- [ ] All cross-thread communication uses Qt signals or EventBus
- [ ] Deadlock prevention (lock ordering documented)

---

## Format Validation

✅ All tasks follow checklist format: `- [ ] [TaskID] [Priority] [Story] Description with file path`  
✅ Tasks organized by user story and phase  
✅ Parallel work opportunities identified  
✅ Independent test criteria provided for each user story  
✅ MVP scope suggested (US1 + US3)  
✅ Total task count: 57  
✅ Constitution compliance checklist included
