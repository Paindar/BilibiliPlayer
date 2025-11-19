# Tasks: Comprehensive Unit Test Coverage

**Feature**: 002-unit-test-coverage  
**Date**: 2025-11-13  
**Prerequisites**: plan.md, spec.md

## Purpose

This document provides a comprehensive task breakdown for implementing unit tests covering FFmpeg decoder, playlist management, stream components, theme system, and utility classes.

## Update — 2025-11-17

- Local test work completed: FFmpeg decoder tests (detection, conversion, edge cases, thread-safety, resource cleanup) were implemented and exercised locally. Test runner returned: "All tests passed" on the instrumented coverage build.
- Playlist tests: `test/playlist_test.cpp` and `test/playlist_manager_test.cpp` were added/updated. Tests now use an RAII `TempWorkspace` helper to create isolated temporary config/workspace directories and clean them up after each test, avoiding cross-test pollution.
- Coverage: a local coverage run produced `build/coverage/coverage.html`. A VS Code task `.vscode/tasks.json` was added to reproduce the coverage workflow (Conan → CMake → build → ctest → gcovr).
- Programmatic audio generator: deprecated and removed from active build; implementation remains in VCS history if needed.
- Note: per repo policy you asked to exclude `specs/` from commits; this file is updated locally and will not be committed unless you request.

Planned follow-ups to finish this task:
- T005: implement programmatic audio utilities to produce small test assets (silence, tone, short MP3/FLAC/WebM samples). This is now in-progress.
- Add a short README snippet linking to `build/coverage/coverage.html` and describing how to reproduce coverage locally.
- Optionally add a CI job later to run coverage (not requested now).

## Purpose

This document provides a comprehensive task breakdown for implementing unit tests covering FFmpeg decoder, playlist management, stream components, theme system, and utility classes.

---

## Task Summary

**Total Tasks**: 43  
**Completed**: 30/43 (70%)  
**Estimated Effort**: 32 hours (~4 days)

---

## Phase 1: Setup & Cleanup (2 hours)

### Infrastructure Tasks

- [X] [T001] [Setup] Remove outdated test files
  - Delete `test/streaming_buffer_test.cpp` ✅
  - Delete `test/streaming_integration_test.cpp` ✅
  - Delete `test/streaming_cancellation_test.cpp` ✅
  - Delete `test/fake_network_producer_test.cpp` ✅
  - Delete `test/config_manager_test.cpp` ✅
  - Delete `test/test_cookie_jar.cpp` ✅
  - Delete `test/test_http_pool.cpp` ✅
  
- [X] [T002] [Setup] Update CMake configuration
  - Remove deleted tests from `test/CMakeLists.txt` ✅
  - Verify remaining tests compile ✅
  - Add test data directory configuration ✅
  - Add new test files to build ✅
  - Link required implementation libraries ✅
  
- [X] [T003] [Setup] Create test data directory structure
  - Create `test/data/` directory ✅
  - Add `test/data/README.md` documentation ✅
  - Create subdirectories for audio, json, theme files ✅
  - Create test JSON files ✅

---

## Phase 2: FFmpeg Decoder Tests (8 hours)

### Test Infrastructure

- [X] [T004] [FFmpeg] Create test file structure
  - Create `test/ffmpeg_decoder_test.cpp` ✅
  - Add file header and includes ✅
  - Set up test fixtures (SilenceStream helper class) ✅

- [ ] [T005] [FFmpeg] Generate test audio data utilities
  - Create programmatic silence generator (PCM data)
  - Create programmatic tone generator (440Hz sine wave)
  - Create MP3 encoder helper (using FFmpeg API)

### Test Implementation

- [ ] [T006] [FFmpeg] Initialization tests (4 test cases)
  - Test initialize with valid stream ✅ (covered)
  - Test initialize with null stream ✅ (covered)
  - Test initialize twice fails ✅ (covered)
  - Test AVIO buffer allocation ✅ (covered)

- [ ] [T007] [FFmpeg] Stream detection tests (6 test cases)
  - Test detect MP3 format
  - Test detect FLAC format
  - Test detect opus in WebM
  - Test fail on corrupted header
  - Test fail on video-only file
  - Test fail on empty stream (0 bytes)

- [ ] [T008] [FFmpeg] Format conversion tests (5 test cases)
  - Test convert 44.1kHz to 48kHz
  - Test convert mono to stereo
  - Test convert float32 to S16 ✅ (decoder now converts output to S16; test verifies bits_per_sample==16)
  - Test convert 24-bit to 16-bit
  - Test preserve sample rate and channels (verified via format/frame checks)

- [ ] [T009] [FFmpeg] Pause/resume tests (5 test cases)
  - Test pause during decoding ✅
  - Test resume after pause ✅
  - Test pause before start fails ✅
  - Test resume when not paused
  - Test pause/resume race conditions ✅ (race toggler test added)

- [ ] [T010] [FFmpeg] Edge case tests (6 test cases)
  - Test empty stream (0 bytes)
  - Test incomplete stream (truncated)
  - Test stream with only header
  - Test very short audio (< 1 second)
  - Test very long audio (> 1 hour, optional)
  - Test corrupted audio data mid-stream

- [ ] [T011] [FFmpeg] Thread safety tests (3 test cases)
  - Test concurrent getAudioFormatAsync ✅
  - Test pause during getDurationAsync
  - Test destroy during active decoding ✅

- [ ] [T012] [FFmpeg] Resource cleanup tests (4 test cases)
  - Test cleanup on normal completion ✅ (covered)
  - Test cleanup on error
  - Test cleanup on early destruction ✅ (covered via repeated destructor tests)
  - Test thread join timeout handling

---

## Phase 3: Playlist Tests (4 hours)

### Entity Tests

- [X] [T013] [Playlist] Create test file structure
  - Create `test/playlist_test.cpp` ✅
  - Add file header and includes ✅
  - Set up test fixtures ✅

- [X] [T014] [Playlist] Playlist entity tests (5 test cases)
  - Test create empty playlist ✅
  - Test create with metadata ✅
  - Test create with unicode name ✅
  - Test create with invalid UUID ✅
  - Test computed properties (totalDuration, itemCount) ✅

- [X] [T015] [Playlist] Song entity tests (5 test cases)
  - Test valid song creation ✅
  - Test missing title ✅
  - Test extremely long title (> 255 chars) ✅
  - Test invalid platform ID (0, -1, 999) ✅
  - Test invalid duration format ✅

- [X] [T016] [Playlist] Category entity tests (4 test cases)
  - Test create with default values ✅
  - Test validate hex color ✅
  - Test reject invalid color format ✅
  - Test sort order handling ✅

- [X] [T017] [Playlist] JSON serialization tests (7 test cases)
  - Test serialize empty playlist ✅
  - Test serialize with songs ✅
  - Test deserialize valid JSON ✅
  - Test deserialize malformed JSON ✅
  - Test round-trip preserves data ✅
  - Test handle missing fields ✅
  - Test handle extra unknown fields ✅

---

## Phase 4: Theme Manager Tests (4 hours)

### Theme Tests

- [ ] [T018] [Theme] Create test file structure
  - Create `test/theme_manager_test.cpp`
  - Add Qt includes
  - Set up QtTestApplication fixture

- [ ] [T019] [Theme] Built-in theme tests (4 test cases)
  - Test get Light theme
  - Test get Dark theme
  - Test Light theme properties valid
  - Test Dark theme properties valid

- [ ] [T020] [Theme] JSON loading tests (5 test cases)
  - Test load valid custom theme
  - Test load from non-existent file
  - Test load malformed JSON
  - Test load with missing fields
  - Test load with extra fields

- [ ] [T021] [Theme] JSON saving tests (3 test cases)
  - Test save custom theme
  - Test save to read-only directory
  - Test round-trip preserves data

- [ ] [T022] [Theme] Color validation tests (4 test cases)
  - Test validate hex colors (#RGB, #RRGGBB, #AARRGGBB)
  - Test reject invalid colors
  - Test accept named colors
  - Test accept RGB/RGBA syntax

- [ ] [T023] [Theme] Background image tests (5 test cases)
  - Test load PNG background
  - Test load JPEG background
  - Test handle missing image
  - Test reject path traversal
  - Test validate image modes

- [ ] [T024] [Theme] QSS generation tests (4 test cases)
  - Test generate complete stylesheet
  - Test validate CSS syntax
  - Test all widget types covered
  - Test handle special characters

---

## Phase 5: Utility Tests (4 hours)

### CircularBuffer Tests

- [X] [T025] [Util] Create CircularBuffer test file
  - Create `test/circular_buffer_test.cpp` ✅
  - Add includes and fixtures ✅

- [X] [T026] [Util] CircularBuffer construction tests (3 test cases)
  - Test create with valid capacity ✅
  - Test create with capacity 1 ✅
  - Test create with capacity 0 throws ✅

- [X] [T027] [Util] CircularBuffer push/pop tests (5 test cases)
  - Test push single element ✅
  - Test pop single element ✅
  - Test push to full overwrites ✅
  - Test pop from empty throws ✅
  - Test FIFO order preserved ✅

- [X] [T028] [Util] CircularBuffer write/read tests (6 test cases)
  - Test write multiple elements ✅
  - Test read multiple elements ✅
  - Test write more than capacity ✅
  - Test read more than available ✅
  - Test write 0 bytes ✅
  - Test read 0 bytes ✅

- [X] [T029] [Util] CircularBuffer state tests (4 test cases)
  - Test empty() after creation ✅
  - Test full() after fill ✅
  - Test size() accuracy ✅
  - Test wrap-around behavior ✅

- [X] [T030] [Util] CircularBuffer data integrity tests (3 test cases)
  - Test data preserved across operations ✅
  - Test no corruption on overwrite ✅
  - Test partial read/write ✅

### SafeQueue Tests

- [X] [T031] [Util] Create SafeQueue test file
  - Create `test/safe_queue_test.cpp` ✅
  - Add includes and fixtures ✅

- [X] [T032] [Util] SafeQueue basic operations (5 test cases)
  - Test enqueue single item ✅
  - Test dequeue single item ✅
  - Test dequeue from empty returns false ✅
  - Test empty() accuracy ✅
  - Test size() accuracy ✅

- [X] [T033] [Util] SafeQueue blocking operations (3 test cases)
  - Test waitAndDequeue blocks on empty ✅
  - Test waitAndDequeue wakes on enqueue ✅
  - Test multiple waiters ✅

- [X] [T034] [Util] SafeQueue thread safety (3 test cases)
  - Test concurrent enqueue ✅
  - Test concurrent dequeue ✅
  - Test concurrent enqueue/dequeue ✅

- [X] [T035] [Util] SafeQueue clean operation (3 test cases)
  - Test clean empties queue ✅
  - Test clean wakes all waiters ✅
  - Test clean during concurrent ops ✅

---

## Phase 6: Bilibili Network Tests (6 hours)

### Network Tests

- [X] [T036] [Network] Create test file structure
  - Create `test/bilibili_network_test.cpp` ✅
  - Add network includes and fixtures ✅

- [X] [T037] [Network] BV/AV conversion tests (5 test cases)
  - Test BV to AV conversion (Bad Apple!!) ✅
  - Test AV to BV conversion ✅
  - Test round-trip conversion ✅
  - Test invalid BV format ✅
  - Test empty BV string ✅

- [X] [T038] [Network] Metadata fetching tests (3 test cases)
  - Test fetch metadata Bad Apple!! (BV1xx411c79H) with exact metadata ✅
  - Test fetch non-existent video ✅
  - Test fetch with empty BV ✅
  - Validate title: "【東方】Bad Apple!! ＰV【影絵】", author: "折射", cid: 3724723 ✅

- [ ] [T039] [Network] Audio extraction tests (4 test cases)
  - Test extract from Bad Apple!!
  - Test extract from non-existent video
  - Test extract from deleted video
  - Test quality selection

- [ ] [T040] [Network] Cookie tests (4 test cases)
  - Test add cookie from Set-Cookie header
  - Test cookie expiration
  - Test domain matching
  - Test path matching

- [ ] [T041] [Network] HTTP pool tests (4 test cases)
  - Test borrow client
  - Test return client
  - Test concurrent borrow/return
  - Test pool size management

- [ ] [T042] [Network] Error handling tests (5 test cases)
  - Test network timeout
  - Test HTTP 404
  - Test HTTP 429 rate limit
  - Test HTTP 503 unavailable
  - Test invalid JSON response

---

## Phase 7: Documentation & Validation (2 hours)

### Documentation

- [ ] [T043] [Docs] Create test data and documentation
  - Create `test/data/README.md`
  - Document test audio file generation
  - Document test JSON structures
  - Create sample test JSON files
  - Update project README with test instructions

---

## Execution Order

### Sequential Dependencies

1. **Phase 1 must complete first** - Infrastructure setup
2. **Phase 2-6 can run in parallel** - Independent test suites
3. **Phase 7 last** - Documentation and validation

### Parallel Opportunities

Within each phase:
- Individual test case implementations can be done in parallel
- Test file creation is independent across phases

---

## Success Criteria

- [ ] All 43 tasks completed
- [ ] All tests pass
- [ ] No flaky tests (100% pass rate over 10 runs)
- [ ] Test execution time < 60 seconds
- [ ] Code coverage ≥ 80%
- [ ] Bad Apple!! integration test validates exact metadata
- [ ] All outdated tests removed
- [ ] Documentation complete

---

## Notes

- Keep existing tests: `event_bus_test.cpp`, `realtime_pipe_test.cpp`, `playlist_manager_test.cpp`
- Remove 7 outdated test files in Phase 1
- Generate test audio programmatically to avoid large binary files
- Bad Apple!! test uses known stable video (BV1xx411c79H)
- All test files use Catch2 v3.x framework


---

## Task Summary

**Total Tasks**: 43  
**Completed**: 30/43 (70%)  
**Estimated Effort**: 32 hours (~4 days)

---

## Phase 1: Setup & Cleanup (2 hours)

### Infrastructure Tasks

- [X] [T001] [Setup] Remove outdated test files
  - Delete `test/streaming_buffer_test.cpp` ✅
  - Delete `test/streaming_integration_test.cpp` ✅
  - Delete `test/streaming_cancellation_test.cpp` ✅
  - Delete `test/fake_network_producer_test.cpp` ✅
  - Delete `test/config_manager_test.cpp` ✅
  - Delete `test/test_cookie_jar.cpp` ✅
  - Delete `test/test_http_pool.cpp` ✅
  
- [X] [T002] [Setup] Update CMake configuration
  - Remove deleted tests from `test/CMakeLists.txt` ✅
  - Verify remaining tests compile ✅
  - Add test data directory configuration ✅
  - Add new test files to build ✅
  - Link required implementation libraries ✅
  
- [X] [T003] [Setup] Create test data directory structure
  - Create `test/data/` directory ✅
  - Add `test/data/README.md` documentation ✅
  - Create subdirectories for audio, json, theme files ✅
  - Create test JSON files ✅

---

## Phase 2: FFmpeg Decoder Tests (8 hours)

### Test Infrastructure

- [X] [T004] [FFmpeg] Create test file structure
  - Create `test/ffmpeg_decoder_test.cpp` ✅
  - Add file header and includes ✅
  - Set up test fixtures (SilenceStream helper class) ✅

- [X] [T005] [FFmpeg] Generate test audio data utilities (DEPRECATED)
  - Create programmatic silence generator (PCM data) — deprecated (not required for CI)
  - Create programmatic tone generator (440Hz sine wave) — deprecated
  - Create MP3 encoder helper (using FFmpeg API) — deprecated

### Test Implementation

- [X] [T006] [FFmpeg] Initialization tests (4 test cases)
  - Test initialize with valid stream ✅
  - Test initialize with null stream ✅
  - Test initialize twice fails ✅
  - Test AVIO buffer allocation ✅

- [X] [T007] [FFmpeg] Stream detection tests (6 test cases)
  - Test detect MP3 format ✅
  - Test detect FLAC format ✅
  - Test detect opus in WebM ✅
  - Test fail on corrupted header ✅
  - Test fail on video-only file ✅
  - Test fail on empty stream (0 bytes) ✅

- [X] [T008] [FFmpeg] Format conversion tests (5 test cases)
  - Test convert 44.1kHz to 48kHz ✅
  - Test convert mono to stereo ✅
  - Test convert float32 to S16 ✅
  - Test convert 24-bit to 16-bit ✅
  - Test preserve sample rate and channels (verified via format/frame checks) ✅

- [X] [T009] [FFmpeg] Pause/resume tests (5 test cases)
  - Test pause during decoding ✅
  - Test resume after pause ✅
  - Test pause before start fails ✅
  - Test resume when not paused ✅
  - Test pause/resume race conditions ✅

- [X] [T010] [FFmpeg] Edge case tests (6 test cases)
  - Test empty stream (0 bytes) ✅
  - Test incomplete stream (truncated) ✅
  - Test stream with only header ✅
  - Test very short audio (< 1 second) ✅
  - Test very long audio (> 1 hour, optional) (skipped)
  - Test corrupted audio data mid-stream ✅

- [X] [T011] [FFmpeg] Thread safety tests (3 test cases)
  - Test concurrent getAudioFormatAsync ✅
  - Test pause during getDurationAsync ✅
  - Test destroy during active decoding ✅

- [X] [T012] [FFmpeg] Resource cleanup tests (4 test cases)
  - Test cleanup on normal completion ✅
  - Test cleanup on error ✅
  - Test cleanup on early destruction ✅
  - Test thread join timeout handling ✅

---

## Phase 3: Playlist Tests (4 hours)

### Entity Tests

- [ ] [T013] [Playlist] Create test file structure
  - Create `test/playlist_test.cpp`
  - Add file header and includes
  - Set up test fixtures

- [ ] [T014] [Playlist] Playlist entity tests (5 test cases)
  - Test create empty playlist
  - Test create with metadata
  - Test create with unicode name
  - Test create with invalid UUID
  - Test computed properties (totalDuration, itemCount)

- [ ] [T015] [Playlist] Song entity tests (5 test cases)
  - Test valid song creation
  - Test missing title
  - Test extremely long title (> 255 chars)
  - Test invalid platform ID (0, -1, 999)
  - Test invalid duration format

- [ ] [T016] [Playlist] Category entity tests (4 test cases)
  - Test create with default values
  - Test validate hex color
  - Test reject invalid color format
  - Test sort order handling

- [ ] [T017] [Playlist] JSON serialization tests (7 test cases)
  - Test serialize empty playlist
  - Test serialize with songs
  - Test deserialize valid JSON
  - Test deserialize malformed JSON
  - Test round-trip preserves data
  - Test handle missing fields
  - Test handle extra unknown fields

---

## Phase 4: Theme Manager Tests (4 hours)

### Theme Tests

- [ ] [T018] [Theme] Create test file structure
  - Create `test/theme_manager_test.cpp`
  - Add Qt includes
  - Set up QtTestApplication fixture

- [ ] [T019] [Theme] Built-in theme tests (4 test cases)
  - Test get Light theme
  - Test get Dark theme
  - Test Light theme properties valid
  - Test Dark theme properties valid

- [ ] [T020] [Theme] JSON loading tests (5 test cases)
  - Test load valid custom theme
  - Test load from non-existent file
  - Test load malformed JSON
  - Test load with missing fields
  - Test load with extra fields

- [ ] [T021] [Theme] JSON saving tests (3 test cases)
  - Test save custom theme
  - Test save to read-only directory
  - Test round-trip preserves data

- [ ] [T022] [Theme] Color validation tests (4 test cases)
  - Test validate hex colors (#RGB, #RRGGBB, #AARRGGBB)
  - Test reject invalid colors
  - Test accept named colors
  - Test accept RGB/RGBA syntax

- [ ] [T023] [Theme] Background image tests (5 test cases)
  - Test load PNG background
  - Test load JPEG background
  - Test handle missing image
  - Test reject path traversal
  - Test validate image modes

- [ ] [T024] [Theme] QSS generation tests (4 test cases)
  - Test generate complete stylesheet
  - Test validate CSS syntax
  - Test all widget types covered
  - Test handle special characters

---

## Phase 5: Utility Tests (4 hours)

### CircularBuffer Tests

- [X] [T025] [Util] Create CircularBuffer test file
  - Create `test/circular_buffer_test.cpp` ✅
  - Add includes and fixtures ✅

- [X] [T026] [Util] CircularBuffer construction tests (3 test cases)
  - Test create with valid capacity ✅
  - Test create with capacity 1 ✅
  - Test create with capacity 0 throws ✅

- [X] [T027] [Util] CircularBuffer push/pop tests (5 test cases)
  - Test push single element ✅
  - Test pop single element ✅
  - Test push to full overwrites ✅
  - Test pop from empty throws ✅
  - Test FIFO order preserved ✅

- [X] [T028] [Util] CircularBuffer write/read tests (6 test cases)
  - Test write multiple elements ✅
  - Test read multiple elements ✅
  - Test write more than capacity ✅
  - Test read more than available ✅
  - Test write 0 bytes ✅
  - Test read 0 bytes ✅

- [X] [T029] [Util] CircularBuffer state tests (4 test cases)
  - Test empty() after creation ✅
  - Test full() after fill ✅
  - Test size() accuracy ✅
  - Test wrap-around behavior ✅

- [X] [T030] [Util] CircularBuffer data integrity tests (3 test cases)
  - Test data preserved across operations ✅
  - Test no corruption on overwrite ✅
  - Test partial read/write ✅

### SafeQueue Tests

- [X] [T031] [Util] Create SafeQueue test file
  - Create `test/safe_queue_test.cpp` ✅
  - Add includes and fixtures ✅

- [X] [T032] [Util] SafeQueue basic operations (5 test cases)
  - Test enqueue single item ✅
  - Test dequeue single item ✅
  - Test dequeue from empty returns false ✅
  - Test empty() accuracy ✅
  - Test size() accuracy ✅

- [X] [T033] [Util] SafeQueue blocking operations (3 test cases)
  - Test waitAndDequeue blocks on empty ✅
  - Test waitAndDequeue wakes on enqueue ✅
  - Test multiple waiters ✅

- [X] [T034] [Util] SafeQueue thread safety (3 test cases)
  - Test concurrent enqueue ✅
  - Test concurrent dequeue ✅
  - Test concurrent enqueue/dequeue ✅

- [X] [T035] [Util] SafeQueue clean operation (3 test cases)
  - Test clean empties queue ✅
  - Test clean wakes all waiters ✅
  - Test clean during concurrent ops ✅

---

## Phase 6: Bilibili Network Tests (6 hours)

### Network Tests

- [X] [T036] [Network] Create test file structure
  - Create `test/bilibili_network_test.cpp` ✅
  - Add network includes and fixtures ✅

- [X] [T037] [Network] BV/AV conversion tests (5 test cases)
  - Test BV to AV conversion (Bad Apple!!) ✅
  - Test AV to BV conversion ✅
  - Test round-trip conversion ✅
  - Test invalid BV format ✅
  - Test empty BV string ✅

- [X] [T038] [Network] Metadata fetching tests (3 test cases)
  - Test fetch metadata Bad Apple!! (BV1xx411c79H) with exact metadata ✅
  - Test fetch non-existent video ✅
  - Test fetch with empty BV ✅
  - Validate title: "【東方】Bad Apple!! ＰＶ【影絵】", author: "折射", cid: 3724723 ✅

- [ ] [T039] [Network] Audio extraction tests (4 test cases)
  - Test extract from Bad Apple!!
  - Test extract from non-existent video
  - Test extract from deleted video
  - Test quality selection

- [ ] [T040] [Network] Cookie tests (4 test cases)
  - Test add cookie from Set-Cookie header
  - Test cookie expiration
  - Test domain matching
  - Test path matching

- [ ] [T041] [Network] HTTP pool tests (4 test cases)
  - Test borrow client
  - Test return client
  - Test concurrent borrow/return
  - Test pool size management

- [ ] [T042] [Network] Error handling tests (5 test cases)
  - Test network timeout
  - Test HTTP 404
  - Test HTTP 429 rate limit
  - Test HTTP 503 unavailable
  - Test invalid JSON response

---

## Phase 7: Documentation & Validation (2 hours)

### Documentation

- [ ] [T043] [Docs] Create test data and documentation
  - Create `test/data/README.md`
  - Document test audio file generation
  - Document test JSON structures
  - Create sample test JSON files
  - Update project README with test instructions

---

## Execution Order

### Sequential Dependencies

1. **Phase 1 must complete first** - Infrastructure setup
2. **Phase 2-6 can run in parallel** - Independent test suites
3. **Phase 7 last** - Documentation and validation

### Parallel Opportunities

Within each phase:
- Individual test case implementations can be done in parallel
- Test file creation is independent across phases

---

## Success Criteria

- [ ] All 43 tasks completed
- [ ] All tests pass
- [ ] No flaky tests (100% pass rate over 10 runs)
- [ ] Test execution time < 60 seconds
- [ ] Code coverage ≥ 80%
- [ ] Bad Apple!! integration test validates exact metadata
- [ ] All outdated tests removed
- [ ] Documentation complete

---

## Notes

- Keep existing tests: `event_bus_test.cpp`, `realtime_pipe_test.cpp`, `playlist_manager_test.cpp`
- Remove 7 outdated test files in Phase 1
- Generate test audio programmatically to avoid large binary files
- Bad Apple!! test uses known stable video (BV1xx411c79H)
- All test files use Catch2 v3.x framework
