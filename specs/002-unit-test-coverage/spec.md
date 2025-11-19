# Specification: Comprehensive Unit Test Coverage

**Feature ID**: 002-unit-test-coverage  
**Created**: 2025-11-13  
**Status**: Draft  
**Dependencies**: 001-unified-stream-player  
**Author**: AI-assisted development

---

## 1. Overview

### 1.1 Purpose

Implement comprehensive unit tests for the BilibiliPlayer project to ensure code quality, prevent regressions, and validate edge case handling across all critical components.

### 1.2 Scope

This specification covers unit tests for:
- **FFmpeg Decoder**: Audio decoding, stream handling, format conversion
- **Playlist Management**: CRUD operations, JSON persistence, thread safety
- **Stream Components**: Circular buffers, realtime pipes, audio buffers
- **Theme System**: Theme loading, applying, JSON serialization
- **Utility Classes**: CircularBuffer, SafeQueue, MD5 hashing
- **Network Integration**: BilibiliNetworkInterface edge cases

### 1.3 Out of Scope

- Integration tests (already covered in spec 001: realtime_pipe_test.cpp, event_bus_test.cpp)
- UI interaction tests (requires Qt Test framework)
- Performance benchmarking
- Load testing

---

## 2. Test Organization

### 2.1 Test File Structure

```
test/
├── ffmpeg_decoder_test.cpp         # FFmpeg decoding tests
├── playlist_test.cpp                # Playlist entity tests  
├── playlist_manager_test.cpp        # PlaylistManager tests (EXISTING - keep)
├── theme_manager_test.cpp           # Theme system tests
├── circular_buffer_test.cpp         # CircularBuffer utility tests
├── safe_queue_test.cpp              # SafeQueue utility tests
├── bilibili_network_test.cpp        # BilibiliNetworkInterface tests
├── event_bus_test.cpp               # EventBus tests (EXISTING - keep)
├── realtime_pipe_test.cpp           # RealtimePipe tests (EXISTING - keep)
└── CMakeLists.txt                   # Test build configuration
```

### 2.2 Test Framework

- **Framework**: Catch2 (v3.x)
- **Build System**: CMake
- **Dependencies**: Conan-managed (Qt6, FFmpeg, jsoncpp)

### 2.3 Files to Remove

These existing test files are outdated and should be removed:
- `test/streaming_buffer_test.cpp` (replaced by realtime_pipe_test.cpp)
- `test/streaming_integration_test.cpp` (replaced by realtime_pipe_test.cpp)
- `test/streaming_cancellation_test.cpp` (covered in new tests)
- `test/fake_network_producer_test.cpp` (not aligned with current architecture)
- `test/config_manager_test.cpp` (outdated, needs rewrite)
- `test/test_cookie_jar.cpp` (integrated into bilibili_network_test.cpp)
- `test/test_http_pool.cpp` (integrated into bilibili_network_test.cpp)

---

## 3. Test Requirements by Component

### 3.1 FFmpeg Decoder Tests (`ffmpeg_decoder_test.cpp`)

**Reference**: `src/audio/ffmpeg_decoder.h`, `src/audio/ffmpeg_decoder.cpp`

#### Edge Cases from Spec 001

1. **Invalid Input Handling**
   - Empty stream (0 bytes)
   - Corrupted audio data
   - Unsupported codec
   - Incomplete stream (truncated file)

2. **Format Edge Cases**
   - Non-standard sample rates (8kHz, 96kHz)
   - Mono vs stereo vs 5.1 surround
   - Various bit depths (8-bit, 24-bit, 32-bit float)
   - Codec format changes mid-stream

3. **State Management**
   - Pause during initialization
   - Resume after completion
   - Destroy during active decoding
   - Multiple sequential decodes without cleanup

4. **Thread Safety**
   - Concurrent getAudioFormatAsync calls
   - Pause/resume race conditions
   - Decoder destruction while threads active

5. **Buffer Behavior**
   - Audio cache overflow (when stream faster than decoding)
   - Audio cache underflow (when decoding faster than stream)
   - Read packet timeout scenarios
   - EOF handling with partial data

6. **Resource Cleanup**
   - Memory leak detection (AVFormatContext, AVCodecContext, SwrContext)
   - Thread join timeout handling
   - AVIO buffer cleanup

#### Test Cases

```cpp
TEST_CASE("FFmpegDecoder initialization") {
    SECTION("Initialize with valid stream") { }
    SECTION("Initialize with null stream") { }
    SECTION("Initialize twice fails") { }
    SECTION("AVIO buffer allocation") { }
}

TEST_CASE("FFmpegDecoder stream detection") {
    SECTION("Detect MP3 format") { }
    SECTION("Detect FLAC format") { }
    SECTION("Detect opus in WebM") { }
    SECTION("Fail on corrupted header") { }
    SECTION("Fail on video-only file") { }
}

TEST_CASE("FFmpegDecoder format conversion") {
    SECTION("Convert 44.1kHz to 48kHz") { }
    SECTION("Convert mono to stereo") { }
    SECTION("Convert float32 to S16") { }
    SECTION("Convert 24-bit to 16-bit") { }
}

TEST_CASE("FFmpegDecoder pause/resume") {
    SECTION("Pause during decoding") { }
    SECTION("Resume after pause") { }
    SECTION("Pause before start fails") { }
    SECTION("Resume when not paused") { }
}

TEST_CASE("FFmpegDecoder edge cases") {
    SECTION("Handle empty stream (0 bytes)") { }
    SECTION("Handle incomplete stream") { }
    SECTION("Handle stream with only header") { }
    SECTION("Handle very short audio (< 1 second)") { }
    SECTION("Handle very long audio (> 1 hour)") { }
}

TEST_CASE("FFmpegDecoder thread safety") {
    SECTION("Concurrent getAudioFormatAsync") { }
    SECTION("Pause during getDurationAsync") { }
    SECTION("Destroy during active decoding") { }
}

TEST_CASE("FFmpegDecoder resource cleanup") {
    SECTION("Cleanup on normal completion") { }
    SECTION("Cleanup on error") { }
    SECTION("Cleanup on early destruction") { }
    SECTION("Thread join timeout handling") { }
}
```

---

### 3.2 Playlist Tests (`playlist_test.cpp`)

**Reference**: `src/playlist/playlist.h`

#### Edge Cases from Spec 001

1. **Playlist Entity**
   - Empty playlist creation
   - Playlist with 1000+ songs
   - Unicode characters in name/description
   - Special characters in cover URI
   - Invalid UUID handling

2. **Song Entity**
   - Missing metadata fields
   - Extremely long titles (> 255 chars)
   - Invalid duration formats
   - Platform ID edge values (0, -1, 999)

3. **Category Entity**
   - Empty category name
   - Duplicate category names
   - Color validation (invalid hex, rgb(), named colors)
   - Sort order edge cases (negative, duplicate)

4. **JSON Serialization**
   - Round-trip serialization
   - Malformed JSON handling
   - Missing required fields
   - Extra unknown fields (forward compatibility)

#### Test Cases

```cpp
TEST_CASE("Playlist entity creation") {
    SECTION("Create empty playlist") { }
    SECTION("Create with metadata") { }
    SECTION("Create with unicode name") { }
    SECTION("Create with invalid UUID") { }
}

TEST_CASE("Playlist JSON serialization") {
    SECTION("Serialize empty playlist") { }
    SECTION("Serialize with songs") { }
    SECTION("Deserialize valid JSON") { }
    SECTION("Deserialize malformed JSON") { }
    SECTION("Round-trip preserves data") { }
}

TEST_CASE("Song entity validation") {
    SECTION("Valid song creation") { }
    SECTION("Missing title") { }
    SECTION("Extremely long title") { }
    SECTION("Invalid platform ID") { }
    SECTION("Invalid duration") { }
}

TEST_CASE("Category entity") {
    SECTION("Create with default values") { }
    SECTION("Validate hex color") { }
    SECTION("Reject invalid color format") { }
    SECTION("Sort order handling") { }
}
```

---

### 3.3 Playlist Manager Tests (`playlist_manager_test.cpp`)

**Reference**: `src/playlist/playlist_manager.h`, `src/playlist/playlist_manager.cpp`

**Status**: EXISTING TEST - Keep and enhance

#### Additional Edge Cases

1. **Initialization**
   - Initialize without ConfigManager (should throw)
   - Initialize twice (should warn)
   - Initialize with missing playlist directory
   - Initialize with corrupted categories.json

2. **Concurrent Operations**
   - Simultaneous addSong from multiple threads
   - Concurrent save/load operations
   - Auto-save during manual save
   - Delete playlist while iterating songs

3. **Large Data Sets**
   - 100+ categories
   - 1000+ playlists
   - 10,000+ songs in single playlist
   - JSON file > 10MB

4. **File System Edge Cases**
   - Read-only filesystem
   - Disk full during save
   - File locked by another process
   - Path traversal attacks (../../../)

#### Test Cases (Additions)

```cpp
TEST_CASE("PlaylistManager initialization edge cases") {
    SECTION("Initialize with corrupted JSON") { }
    SECTION("Initialize with missing directory") { }
    SECTION("Initialize with read-only filesystem") { }
}

TEST_CASE("PlaylistManager concurrent operations") {
    SECTION("Concurrent addSong") { }
    SECTION("Concurrent save/load") { }
    SECTION("Delete during iteration") { }
}

TEST_CASE("PlaylistManager large datasets") {
    SECTION("100 categories") { }
    SECTION("1000 playlists") { }
    SECTION("10000 songs in playlist") { }
    SECTION("JSON file > 10MB") { }
}

TEST_CASE("PlaylistManager file system errors") {
    SECTION("Handle disk full") { }
    SECTION("Handle locked file") { }
    SECTION("Reject path traversal") { }
}
```

---

### 3.4 Theme Manager Tests (`theme_manager_test.cpp`)

**Reference**: `src/ui/theme/theme_manager.h`, `src/ui/theme/theme_manager.cpp`

#### Edge Cases from Spec 001

1. **Theme Loading**
   - Load built-in themes (Light, Dark)
   - Load custom theme from JSON
   - Handle missing JSON file
   - Handle malformed JSON
   - Handle missing required fields

2. **Theme Application**
   - Apply theme without QApplication
   - Apply theme twice
   - Apply during active rendering
   - Revert to previous theme

3. **Color Validation**
   - Valid hex colors (#RGB, #RRGGBB, #AARRGGBB)
   - Invalid color formats
   - Named colors (red, blue, etc.)
   - RGB/RGBA function syntax

4. **Background Image**
   - Load PNG image
   - Load JPEG image
   - Load non-existent image
   - Invalid image path (path traversal)
   - Image mode validation (tiled, stretched, centered)

5. **QSS Generation**
   - Generate complete stylesheet
   - Verify all widgets styled
   - Validate CSS syntax
   - Handle special characters in colors

#### Test Cases

```cpp
TEST_CASE("ThemeManager built-in themes") {
    SECTION("Get Light theme") { }
    SECTION("Get Dark theme") { }
    SECTION("Light theme properties valid") { }
    SECTION("Dark theme properties valid") { }
}

TEST_CASE("ThemeManager JSON loading") {
    SECTION("Load valid custom theme") { }
    SECTION("Load from non-existent file") { }
    SECTION("Load malformed JSON") { }
    SECTION("Load with missing fields") { }
    SECTION("Load with extra fields") { }
}

TEST_CASE("ThemeManager JSON saving") {
    SECTION("Save custom theme") { }
    SECTION("Save to read-only directory") { }
    SECTION("Round-trip preserves data") { }
}

TEST_CASE("ThemeManager color validation") {
    SECTION("Validate hex colors") { }
    SECTION("Reject invalid colors") { }
    SECTION("Accept named colors") { }
    SECTION("Accept RGB syntax") { }
}

TEST_CASE("ThemeManager background images") {
    SECTION("Load PNG background") { }
    SECTION("Load JPEG background") { }
    SECTION("Handle missing image") { }
    SECTION("Reject path traversal") { }
    SECTION("Validate image modes") { }
}

TEST_CASE("ThemeManager QSS generation") {
    SECTION("Generate complete stylesheet") { }
    SECTION("Validate CSS syntax") { }
    SECTION("All widget types covered") { }
    SECTION("Handle special characters") { }
}

TEST_CASE("ThemeManager application") {
    SECTION("Apply theme without QApplication") { }
    SECTION("Apply theme twice") { }
    SECTION("Revert to previous theme") { }
}
```

---

### 3.5 CircularBuffer Tests (`circular_buffer_test.cpp`)

**Reference**: `src/util/circular_buffer.hpp`

#### Edge Cases

1. **Capacity Edge Cases**
   - Capacity = 1
   - Capacity = 0 (should throw)
   - Capacity = SIZE_MAX (allocation fail)

2. **Read/Write Operations**
   - Write to full buffer (overwrite)
   - Read from empty buffer
   - Write 0 bytes
   - Read 0 bytes
   - Write more than capacity
   - Read more than available

3. **State Transitions**
   - Empty → Full → Empty cycle
   - Wrap-around behavior
   - Simultaneous read/write at boundary

4. **Data Integrity**
   - FIFO ordering preserved
   - No data corruption on overwrite
   - Partial read/write handling

#### Test Cases

```cpp
TEST_CASE("CircularBuffer construction") {
    SECTION("Create with valid capacity") { }
    SECTION("Create with capacity 1") { }
    SECTION("Create with capacity 0 throws") { }
}

TEST_CASE("CircularBuffer push/pop") {
    SECTION("Push single element") { }
    SECTION("Pop single element") { }
    SECTION("Push to full overwrites") { }
    SECTION("Pop from empty throws") { }
    SECTION("FIFO order preserved") { }
}

TEST_CASE("CircularBuffer write/read") {
    SECTION("Write multiple elements") { }
    SECTION("Read multiple elements") { }
    SECTION("Write more than capacity") { }
    SECTION("Read more than available") { }
    SECTION("Write 0 bytes") { }
    SECTION("Read 0 bytes") { }
}

TEST_CASE("CircularBuffer state management") {
    SECTION("empty() after creation") { }
    SECTION("full() after fill") { }
    SECTION("size() accuracy") { }
    SECTION("Wrap-around behavior") { }
}

TEST_CASE("CircularBuffer data integrity") {
    SECTION("Data preserved across operations") { }
    SECTION("No corruption on overwrite") { }
    SECTION("Partial read/write") { }
}
```

---

### 3.6 SafeQueue Tests (`safe_queue_test.cpp`)

**Reference**: `src/util/safe_queue.hpp`

#### Edge Cases

1. **Thread Safety**
   - Concurrent enqueue from multiple threads
   - Concurrent dequeue from multiple threads
   - Enqueue while dequeue blocked
   - Clean while threads waiting

2. **Blocking Behavior**
   - waitAndDequeue blocks on empty
   - waitAndDequeue wakes on enqueue
   - Multiple waiters wake correctly
   - Timeout on blocked operations

3. **State Management**
   - empty() accuracy
   - size() accuracy under concurrent ops
   - clean() wakes all waiters

#### Test Cases

```cpp
TEST_CASE("SafeQueue basic operations") {
    SECTION("Enqueue single item") { }
    SECTION("Dequeue single item") { }
    SECTION("Dequeue from empty returns false") { }
    SECTION("empty() accuracy") { }
    SECTION("size() accuracy") { }
}

TEST_CASE("SafeQueue blocking operations") {
    SECTION("waitAndDequeue blocks on empty") { }
    SECTION("waitAndDequeue wakes on enqueue") { }
    SECTION("Multiple waiters") { }
}

TEST_CASE("SafeQueue thread safety") {
    SECTION("Concurrent enqueue") { }
    SECTION("Concurrent dequeue") { }
    SECTION("Concurrent enqueue/dequeue") { }
}

TEST_CASE("SafeQueue clean operation") {
    SECTION("Clean empties queue") { }
    SECTION("Clean wakes all waiters") { }
    SECTION("Clean during concurrent ops") { }
}
```

---

### 3.7 BilibiliNetworkInterface Tests (`bilibili_network_test.cpp`)

**Reference**: `src/network/bili_network_interface.h`, `src/network/bili_network_interface.cpp`

#### Edge Cases from Spec 001

1. **WBI Signature**
   - Valid signature generation
   - Timestamp generation
   - Mixin key generation
   - Parameter encoding

2. **Search Functionality**
   - Search with known video (Bad Apple!! - BV1xx411c79H)
   - Search with no results
   - Search with special characters
   - Search with unicode
   - Pagination handling

3. **Audio URL Extraction**
   - Extract from known video (BV1xx411c79H)
   - Extract from non-existent video
   - Extract from deleted video
   - Extract from geo-blocked video
   - Quality selection (64kbps, 128kbps, 192kbps)

4. **Cookie Management**
   - Add cookie from Set-Cookie header
   - Cookie expiration handling
   - Domain matching
   - Path matching
   - Secure flag handling

5. **HTTP Client Pool**
   - Borrow/return client
   - Pool size management
   - Concurrent access
   - Client reuse

6. **Error Handling**
   - Network timeout
   - HTTP 404
   - HTTP 429 (rate limit)
   - HTTP 503 (service unavailable)
   - Invalid JSON response

#### Test Cases

```cpp
TEST_CASE("BilibiliNetworkInterface WBI signature") {
    SECTION("Generate valid signature") { }
    SECTION("Timestamp format") { }
    SECTION("Mixin key generation") { }
    SECTION("Parameter encoding") { }
}

TEST_CASE("BilibiliNetworkInterface search") {
    SECTION("Search Bad Apple!! (BV1xx411c79H)") {
        // Expected result:
        // title: "【東方】Bad Apple!! ＰＶ【影絵】"
        // author: "折射"
        // bvid: "BV1xx411c79H"
        // cid: 3724723
        // part_name: "1"
    }
    SECTION("Search with no results") { }
    SECTION("Search with special characters") { }
    SECTION("Search with unicode") { }
    SECTION("Pagination") { }
}

TEST_CASE("BilibiliNetworkInterface audio extraction") {
    SECTION("Extract from Bad Apple!!") { }
    SECTION("Extract from non-existent video") { }
    SECTION("Extract from deleted video") { }
    SECTION("Quality selection") { }
}

TEST_CASE("BilibiliNetworkInterface cookies") {
    SECTION("Add cookie from header") { }
    SECTION("Cookie expiration") { }
    SECTION("Domain matching") { }
    SECTION("Path matching") { }
}

TEST_CASE("BilibiliNetworkInterface HTTP pool") {
    SECTION("Borrow client") { }
    SECTION("Return client") { }
    SECTION("Concurrent borrow/return") { }
    SECTION("Pool size management") { }
}

TEST_CASE("BilibiliNetworkInterface error handling") {
    SECTION("Network timeout") { }
    SECTION("HTTP 404") { }
    SECTION("HTTP 429 rate limit") { }
    SECTION("HTTP 503 unavailable") { }
    SECTION("Invalid JSON response") { }
}
```

---

## 4. Test Data Management

### 4.1 Test Fixtures

```cpp
// Fixture for temporary workspace
struct TemporaryWorkspace {
    std::filesystem::path path;
    
    TemporaryWorkspace() {
        auto tmp = std::filesystem::temp_directory_path();
        path = tmp / ("BilibiliPlayerTest_" + 
                     std::to_string(std::chrono::system_clock::now()
                                   .time_since_epoch().count()));
        std::filesystem::create_directories(path);
    }
    
    ~TemporaryWorkspace() {
        std::filesystem::remove_all(path);
    }
};

// Fixture for Qt application (required for theme tests)
struct QtTestApplication {
    QCoreApplication* app;
    
    QtTestApplication(int& argc, char** argv) {
        app = new QCoreApplication(argc, argv);
    }
    
    ~QtTestApplication() {
        delete app;
    }
};
```

### 4.2 Test Audio Files

Create minimal test audio files:

```
test/data/
├── silence_1s_48khz_stereo.mp3    # Valid MP3, 1 second silence
├── tone_440hz_44khz_mono.flac     # Valid FLAC, 440Hz tone
├── corrupted_header.mp3           # Corrupted file
├── video_only.mp4                 # MP4 with no audio stream
├── truncated.opus                 # Incomplete opus file
└── README.md                      # Test data documentation
```

### 4.3 Test JSON Files

```
test/data/
├── valid_theme.json               # Valid theme JSON
├── malformed_theme.json           # Invalid JSON syntax
├── missing_fields_theme.json      # Missing required fields
├── valid_playlist.json            # Valid playlist JSON
└── corrupted_categories.json      # Corrupted categories file
```

---

## 5. Testing Strategy

### 5.1 Coverage Goals

- **Line Coverage**: ≥ 80% for all production code
- **Branch Coverage**: ≥ 70% for conditional logic
- **Function Coverage**: 100% for public APIs

### 5.2 Test Execution

```bash
# Run all tests
./build/debug/test/all_tests

# Run specific test file
./build/debug/test/all_tests "[FFmpegDecoder]"

# Run with verbose output
./build/debug/test/all_tests -v

# Run with coverage collection (if configured)
cmake --build build/debug --target coverage
```

### 5.3 Continuous Integration

- Run all tests on every commit
- Block merge if tests fail
- Generate coverage report
- Fail CI if coverage drops below threshold

---

## 6. Edge Case Validation

### 6.1 Bad Apple!! Test Case

**Purpose**: Validate BilibiliNetworkInterface against known stable video

**Video Details**:
- **Title**: "【東方】Bad Apple!! ＰＶ【影絵】"
- **Author**: "折射"
- **BV ID**: BV1xx411c79H
- **CID**: 3724723
- **Part Name**: "1"

**Test Implementation**:

```cpp
TEST_CASE("BilibiliNetworkInterface Bad Apple!! integration") {
    BilibiliNetworkInterface iface;
    
    // Search for Bad Apple!!
    auto results = iface.searchByTitle("Bad Apple!!", 1);
    
    REQUIRE(results.size() > 0);
    
    // Find exact match
    auto it = std::find_if(results.begin(), results.end(), 
        [](const BilibiliSearchResult& r) {
            return r.bvid == "BV1xx411c79H";
        });
    
    REQUIRE(it != results.end());
    REQUIRE(it->title == "【東方】Bad Apple!! ＰＶ【影絵】");
    REQUIRE(it->author == "折射");
    REQUIRE(it->cid == 3724723);
    REQUIRE(it->part_name == "1");
    
    // Extract audio URL
    auto url = iface.getAudioUrlByParams(it->bvid, it->cid, it->part_name);
    REQUIRE(!url.empty());
    REQUIRE(url.find("http") == 0); // Valid URL
}
```

### 6.2 FFmpeg Decoder Edge Cases

**Minimum Audio File**: 100ms silence
**Maximum Audio File**: 1 hour test (optional, disabled by default)
**Corrupted Data**: Random bytes with valid header

---

## 7. Non-Functional Requirements

### 7.1 Performance

- **Test Execution Time**: All tests complete in < 60 seconds
- **Individual Test Timeout**: No single test > 10 seconds
- **Memory Usage**: No test allocates > 100MB

### 7.2 Reliability

- **Flakiness**: Zero flaky tests (< 0.1% failure rate)
- **Determinism**: Tests produce same results on repeated runs
- **Isolation**: Tests can run in any order

### 7.3 Maintainability

- **Readability**: Test names describe what they test
- **Documentation**: Complex tests have explanatory comments
- **DRY Principle**: Shared test utilities extracted to helpers

---

## 8. Success Criteria

### 8.1 Acceptance Criteria

- [ ] All test cases implemented
- [ ] All tests pass on Windows
- [ ] Code coverage ≥ 80%
- [ ] No flaky tests
- [ ] Test execution < 60 seconds
- [ ] Bad Apple!! integration test passes
- [ ] Outdated tests removed

### 8.2 Quality Metrics

- **Test Count**: ≥ 200 test cases
- **Assertions**: ≥ 500 assertions
- **Edge Cases**: All identified edge cases covered
- **Documentation**: All test files have header comments

---

## 9. Implementation Notes

### 9.1 CMake Configuration

Update `test/CMakeLists.txt`:

```cmake
# Add new test files
add_executable(all_tests
    # Existing tests (keep these)
    event_bus_test.cpp
    realtime_pipe_test.cpp
    playlist_manager_test.cpp
    
    # New tests
    ffmpeg_decoder_test.cpp
    playlist_test.cpp
    theme_manager_test.cpp
    circular_buffer_test.cpp
    safe_queue_test.cpp
    bilibili_network_test.cpp
)

# Link test data
file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
```

### 9.2 Test Dependencies

```python
# conanfile.py
def requirements(self):
    # ... existing dependencies ...
    self.requires("catch2/3.5.0", test=True)
```

### 9.3 Coverage Tools (Optional)

```cmake
# Enable coverage with -DENABLE_COVERAGE=ON
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(ENABLE_COVERAGE)
    target_compile_options(all_tests PRIVATE --coverage)
    target_link_options(all_tests PRIVATE --coverage)
endif()
```

---

## 10. Risks and Mitigations

### 10.1 Identified Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Network tests fail due to Bilibili API changes | High | Medium | Mock Bilibili responses, document known stable videos |
| FFmpeg tests require large audio files | Medium | Low | Generate minimal test audio programmatically |
| Qt tests require GUI environment | Low | Medium | Use QCoreApplication for non-GUI tests |
| Coverage tool conflicts with Conan | Low | Low | Use CMake-native coverage tools |

### 10.2 Mitigation Strategies

1. **Network Isolation**: Mock network calls for most tests, use real API only for integration tests
2. **Test Data Generation**: Create audio files programmatically instead of checking in binaries
3. **CI Environment**: Configure CI to run headless Qt tests
4. **Documentation**: Document all external dependencies and expected behaviors

---

## 11. Timeline Estimate

| Phase | Duration | Description |
|-------|----------|-------------|
| Setup | 2 hours | Configure CMake, add Catch2, remove old tests |
| FFmpeg Tests | 8 hours | Implement 50+ test cases for decoder |
| Playlist Tests | 4 hours | Implement 30+ test cases for entities |
| Theme Tests | 4 hours | Implement 25+ test cases for theme manager |
| Utility Tests | 4 hours | Implement 40+ test cases for CircularBuffer, SafeQueue |
| Network Tests | 6 hours | Implement 30+ test cases including Bad Apple!! |
| Test Data | 2 hours | Create minimal audio/JSON test files |
| Documentation | 2 hours | Add test documentation, README |
| **Total** | **32 hours** | ~4 days full-time development |

---

## 12. References

- [Catch2 Documentation](https://github.com/catchorg/Catch2/blob/devel/docs/Readme.md)
- [FFmpeg Documentation](https://ffmpeg.org/documentation.html)
- [Qt Test Framework](https://doc.qt.io/qt-6/qtest-overview.html)
- Spec 001: Unified Stream Player
- [Bilibili API Documentation](https://github.com/SocialSisterYi/bilibili-API-collect)
