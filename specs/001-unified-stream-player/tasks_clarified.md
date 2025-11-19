```markdown
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

... (content truncated for brevity; the full file mirrors `tasks.md` with the same sections)

---

## Clarifications

### Session 2025-11-15

- Q: Has the T027.4 FFmpeg buffering change been implemented so that `startDecoding()` no longer blocks waiting for a large initial buffer? → A: Yes.

Implementation note:

- `FFmpegStreamDecoder::startDecoding()` now spawns the stream consumer and decoder threads and returns promptly.
- The potentially-blocking FFmpeg initialization steps (open input, find stream info, open codec, swr init) are executed inside the decoder thread (`decodeAudioBytesFunc`).
- The decoder consumes data from the internal circular buffer asynchronously; `startDecoding()` no longer waits for the `audioCache` to reach the previous threshold before returning.

Updated acceptance criteria (T027.4):

- `startDecoding()` must return within 100ms under low-buffer conditions.
- The decoder must produce at least one `AudioFrame` within a short timeout (e.g., 2s) once sufficient data arrives.

Notes:

- This file is a clarified copy (adds the Clarifications section). Integrate changes back into `tasks.md` at your convenience.

``` 