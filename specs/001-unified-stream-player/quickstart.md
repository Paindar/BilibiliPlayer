# Developer Quickstart: BilibiliPlayer

**Feature**: 001-unified-stream-player  
**Date**: 2025-11-13  
**Audience**: Developers implementing or extending BilibiliPlayer

## Purpose

This document provides step-by-step instructions for setting up the development environment, understanding the architecture, and implementing the core features of BilibiliPlayer.

---

## Table of Contents

1. [Development Environment Setup](#development-environment-setup)
2. [Architecture Overview](#architecture-overview)
3. [Building the Project](#building-the-project)
4. [Running Tests](#running-tests)
5. [Implementation Workflow](#implementation-workflow)
6. [Key Components](#key-components)
7. [Common Tasks](#common-tasks)
8. [Debugging Tips](#debugging-tips)
9. [Contributing Guidelines](#contributing-guidelines)

---

## Development Environment Setup

### Prerequisites

- **Operating System**: Windows 10/11 (primary), Linux/macOS (secondary)
- **Compiler**: MinGW GCC 13.1.0+ (Windows), GCC 8+/Clang 7+ (Linux/macOS)
- **CMake**: 3.16 or higher
- **Conan**: 2.0 or higher
- **Qt**: 6.8.0 (prebuilt installation)
- **Python**: 3.8+ (for Conan)
- **Git**: 2.30+

### Installation Steps

#### 1. Install Qt 6.8.0

Download and install Qt 6.8.0 from [qt.io](https://www.qt.io/download-qt-installer):

```bash
# Windows: Run Qt Online Installer
# Select: Qt 6.8.0 → Desktop MinGW 64-bit
# Install to: D:/Qt/6.8.0/mingw_64 (or custom path)

# Linux (example for Ubuntu):
sudo apt install qt6-base-dev qt6-multimedia-dev

# macOS:
brew install qt@6
```

**Important**: Note your Qt installation path. You'll need it for CMake configuration.

#### 2. Install Conan

```bash
pip install conan
conan --version  # Verify installation (should be 2.x)
```

#### 3. Clone Repository

```bash
git clone https://github.com/Paindar/BilibiliPlayer.git
cd BilibiliPlayer
```

#### 4. Configure Conan Profile

```bash
# Create default profile (auto-detects compiler)
conan profile detect

# View profile
conan profile show default

# (Optional) Customize profile for MinGW on Windows
# Edit: ~/.conan2/profiles/default
# Set compiler=gcc, compiler.version=13, compiler.libcxx=libstdc++11
```

#### 5. Install Dependencies with Conan

```bash
# Debug build
conan install . --output-folder=build/debug --build=missing --settings=build_type=Debug

# Release build
conan install . --output-folder=build/release --build=missing --settings=build_type=Release
```

This downloads and builds:
- FFmpeg
- cpp-httplib
- OpenSSL
- jsoncpp
- fmt
- spdlog
- magic_enum
- Catch2

**Expected duration**: 5-30 minutes depending on network and CPU.

#### 6. Set Qt Path (if not in PATH)

**Windows** (PowerShell):
```powershell
$env:CMAKE_PREFIX_PATH = "D:/Qt/6.8.0/mingw_64"
$env:PATH += ";D:/Qt/6.8.0/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin"
```

**Linux/macOS**:
```bash
export CMAKE_PREFIX_PATH=/usr/lib/qt6  # Adjust for your Qt installation
```

#### 7. Verify Setup

```bash
cmake --version   # Should be >= 3.16
conan --version   # Should be >= 2.0
qmake --version   # Should show Qt 6.8.0
```

---

## Architecture Overview

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        UI Layer (Qt6)                       │
│  MainWindow, PlaylistView, PlayerControls, SourceBrowser   │
└───────────────────┬─────────────────────────────────────────┘
                    │ Qt Signals/Slots
┌───────────────────▼─────────────────────────────────────────┐
│                     Event Bus Layer                         │
│   Decouples components via publish/subscribe events        │
└───────────────────┬─────────────────────────────────────────┘
                    │
        ┌───────────┼───────────┬───────────────┐
        ▼           ▼           ▼               ▼
┌────────────┐ ┌──────────┐ ┌────────────┐ ┌──────────────┐
│  Playlist  │ │  Audio   │ │  Network   │ │Configuration │
│  Manager   │ │  Player  │ │  Clients   │ │   Manager    │
└────────────┘ └──────────┘ └────────────┘ └──────────────┘
        │           │             │                │
        ▼           ▼             ▼                ▼
┌─────────────────────────────────────────────────────────────┐
│                    Storage Layer                            │
│   JSON Files (playlists, config), File Cache, Logs         │
└─────────────────────────────────────────────────────────────┘
```

### Threading Model

```
Main Thread (Qt UI)
    ├─ Handles user interactions
    ├─ Updates UI widgets
    └─ Receives signals from background threads

Decoder Thread (per playback)
    ├─ Reads from network/file
    ├─ Decodes with FFmpeg
    └─ Writes PCM to AudioBuffer

Playback Thread (WASAPI)
    ├─ Reads PCM from AudioBuffer
    ├─ Writes to WASAPI output
    └─ Low-latency audio output

Network Worker Threads (thread pool)
    ├─ HTTP requests via cpp-httplib
    └─ Platform API calls (Bilibili, YouTube)
```

---

## Building the Project

### Debug Build

```bash
# 1. Configure CMake
cmake --preset conan-debug

# 2. Build
cmake --build --preset conan-debug -j 6

# 3. Verify build
ls build/debug/src/BilibiliPlayer.exe  # Windows
ls build/debug/src/BilibiliPlayer      # Linux/macOS
```

### Release Build

```bash
# 1. Configure CMake
cmake --preset conan-release

# 2. Build
cmake --build --preset conan-release -j 6

# 3. Verify build
ls build/release/src/BilibiliPlayer.exe
```

### Build Options

**Parallel builds**: Use `-j N` where N = CPU cores (e.g., `-j 6` for 6 cores)

**Clean build**:
```bash
# Debug
rm -rf build/debug/CMakeCache.txt build/debug/CMakeFiles
cmake --preset conan-debug
cmake --build --preset conan-debug

# Release
rm -rf build/release/CMakeCache.txt build/release/CMakeFiles
cmake --preset conan-release
cmake --build --preset conan-release
```

---

## Running Tests

### All Tests

```bash
cd build/debug/test
./BilibiliPlayerTest.exe  # Windows
./all_tests      # Linux/macOS
```

### Specific Test

```bash
# List available test cases and tags
./BilibiliPlayerTest.exe --list-tests

# Run tests matching a name pattern (wildcard)
./BilibiliPlayerTest.exe "PlaylistManager*"

# Run tests by Catch2 tag (use tag name in square brackets)
./BilibiliPlayerTest.exe "[ConfigManager]"
./BilibiliPlayerTest.exe "[FFmpegDecoder][Integration]"  # combine tags

# Run a single test by exact test case description
./BilibiliPlayerTest.exe "RealtimePipe basic write/read"
```

### Test with Output

```bash
./BilibiliPlayerTest.exe --success  # Show all test results, not just failures
```

### Coverage (Linux/macOS)

```bash
# Build with coverage flags
cmake --preset conan-debug -DCMAKE_CXX_FLAGS="--coverage"
cmake --build --preset conan-debug

# Run tests
cd build/debug/test
./all_tests

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage-report
```

---

## Implementation Workflow

### Phase-Based Development (Recommended)

Follow the spec-driven development process:

1. **Phase 0**: Read `research.md` for technical decisions
2. **Phase 1**: Review `data-model.md` and `contracts/` for component interfaces
3. **Phase 2**: Implement user stories in priority order (P1 → P2 → P3)

### TDD Workflow (Per User Story)

For each user story (e.g., "Play Audio from Multiple Sources"):

1. **Write Tests First** (Red):
   ```bash
   # Create test file: test/audio_player_test.cpp
   # Write failing test
   ```

2. **Implement Feature** (Green):
   ```bash
   # Implement in src/audio/AudioPlayer.cpp
   # Run tests until they pass
   ```

3. **Refactor** (Refactor):
   ```bash
   # Improve code quality, extract helpers
   # Re-run tests to ensure still passing
   ```

4. **Validate Constitution Compliance**:
   - Check RAII usage
   - Verify thread safety
   - Measure performance (latency, memory)
   - Review code quality

---

## Key Components

### 1. AudioPlayer (`src/audio/AudioPlayer.h/cpp`)

**Purpose**: Manages audio playback, coordinates decoder and WASAPI output.

**Key Methods**:
- `bool play(unique_ptr<AudioSource> source)` - Start playback
- `void pause()` - Pause playback
- `void stop()` - Stop playback
- `bool seek(int positionSeconds)` - Seek to position
- `void setVolume(int volume)` - Adjust volume (0-100)

**Signals** (Qt):
- `stateChanged(PlayerState)` - Playback state change
- `positionChanged(int)` - Position update (every 100ms)
- `errorOccurred(QString)` - Playback error

**Threading**:
- Public methods called from main thread
- Spawns decoder thread and playback thread
- Emits signals on main thread (via Qt event loop)

---

### 2. PlaylistManager (`src/playlist/PlaylistManager.h/cpp`)

**Purpose**: Manages playlists and categories, handles persistence.

**Key Methods**:
- `Playlist* createPlaylist(string name, string categoryId)` - Create new playlist
- `bool addItem(string playlistId, unique_ptr<AudioSource> source)` - Add item
- `bool save()` - Persist to disk
- `bool load()` - Load from disk

**Thread Safety**:
- All public methods mutex-protected
- Uses Unsafe pattern for internal operations

---

### 3. StreamDecoder (`src/audio/StreamDecoder.h/cpp`)

**Purpose**: Decodes audio using FFmpeg, handles local files and network streams.

**Key Methods**:
- `bool open(const AudioSource& source)` - Open source for decoding
- `int decode(uint8_t* buffer, int maxSize)` - Decode next chunk
- `bool seek(int64_t timestampMs)` - Seek to timestamp

**FFmpeg Integration**:
```cpp
AVFormatContext* formatCtx_;
AVCodecContext* codecCtx_;
SwrContext* swrCtx_;  // Resampler
```

---

### 4. WASAPIOutput (`src/audio/AudioPlayer.cpp` or separate file)

**Purpose**: Low-latency audio output using Windows Audio Session API.

**Key APIs**:
- `IAudioClient::Initialize()` - Setup audio session
- `IAudioRenderClient::GetBuffer()` - Get writable buffer
- `IAudioRenderClient::ReleaseBuffer()` - Submit audio

**Configuration**:
- Sample rate: 48000 Hz
- Bit depth: 16-bit
- Channels: 2 (stereo)
- Buffer size: 1024 frames (~21ms latency at 48kHz)

---

### 5. BilibiliClient (`src/network/BilibiliClient.h/cpp`)

**Purpose**: Bilibili platform integration (search, metadata, stream URLs).

**Key Methods**:
- `SearchResult search(string query)` - Search Bilibili
- `VideoInfo getVideoInfo(string bvid)` - Get video metadata
- `string getStreamUrl(string bvid, int quality)` - Extract audio stream URL

**API Endpoints**:
```cpp
const string SEARCH_API = "https://api.bilibili.com/x/web-interface/search/type";
const string VIDEO_INFO_API = "https://api.bilibili.com/x/web-interface/view";
const string PLAYURL_API = "https://api.bilibili.com/x/player/playurl";
```

---

## Common Tasks

### Add a New Audio Source Type

1. **Update SourceType enum** (`src/playlist/AudioSource.h`):
   ```cpp
   enum class SourceType {
       Bilibili,
       YouTube,
       Local,
       SoundCloud,  // New type
       Other
   };
   ```

2. **Create platform client** (`src/network/SoundCloudClient.h/cpp`):
   ```cpp
   class SoundCloudClient {
   public:
       MetadataResult getMetadata(const string& url);
       string getStreamUrl(const string& url);
   };
   ```

3. **Update AudioSourceFactory** (`src/playlist/AudioSourceFactory.cpp`):
   ```cpp
   SourceType AudioSourceFactory::detectSourceType(const string& url) {
       if (url.find("soundcloud.com") != string::npos)
           return SourceType::SoundCloud;
       // ... existing logic
   }
   ```

4. **Add tests** (`test/soundcloud_client_test.cpp`)

---

### Add a UI Feature

1. **Design UI in Qt Creator** (or code manually):
   ```cpp
   // src/ui/NewFeatureWidget.h
   class NewFeatureWidget : public QWidget {
       Q_OBJECT
   public:
       explicit NewFeatureWidget(QWidget* parent = nullptr);
   signals:
       void featureTriggered(const QString& data);
   private slots:
       void onButtonClicked();
   private:
       Ui::NewFeatureWidget* ui_;
   };
   ```

2. **Integrate with MainWindow**:
   ```cpp
   // src/ui/MainWindow.cpp
   newFeatureWidget_ = new NewFeatureWidget(this);
   connect(newFeatureWidget_, &NewFeatureWidget::featureTriggered,
           this, &MainWindow::handleFeature);
   ```

3. **Add i18n strings** (`resource/lang/bilibiliPlayer_en.ts`):
   ```xml
   <message>
       <source>New Feature</source>
       <translation>New Feature</translation>
   </message>
   ```

4. **Run Qt's `lupdate` to extract strings**:
   ```bash
   lupdate src/ -ts resource/lang/bilibiliPlayer_en.ts
   lrelease resource/lang/bilibiliPlayer_en.ts
   ```

---

### Debug Memory Leaks

**Windows (Visual Studio)**:
```cpp
// main.cpp
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

int main(int argc, char* argv[]) {
    #ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif
    
    QApplication app(argc, argv);
    // ... rest of main
}
```

**Linux/macOS (Valgrind)**:
```bash
valgrind --leak-check=full --show-leak-kinds=all ./build/debug/src/BilibiliPlayer
```

---

### Profile Performance

**Qt Creator Profiler**:
1. Open project in Qt Creator
2. Build in Profile mode
3. Run → Analyze → CPU Profiler

**Manual Profiling**:
```cpp
// src/util/Profiler.h
class ScopedTimer {
public:
    ScopedTimer(const string& name) : name_(name), start_(steady_clock::now()) {}
    ~ScopedTimer() {
        auto duration = steady_clock::now() - start_;
        spdlog::info("{} took {} ms", name_, 
                     duration_cast<milliseconds>(duration).count());
    }
private:
    string name_;
    steady_clock::time_point start_;
};

// Usage:
void expensiveOperation() {
    ScopedTimer timer("expensiveOperation");
    // ... code to profile
}
```

---

## Debugging Tips

### Enable Verbose Logging

Edit `log.properties`:
```properties
# Change log level
log4cpp.rootCategory=DEBUG, console, fileAppender

# Enable specific component
log4cpp.category.AudioPlayer=DEBUG
log4cpp.category.NetworkClient=DEBUG
```

### Qt Debug Output

```cpp
#include <QDebug>

qDebug() << "Playback state:" << state_;
qWarning() << "Buffer underrun!";
qCritical() << "WASAPI init failed:" << errorCode;
```

### Breakpoint in Threading Code

**VS Code (launch.json)**:
```json
{
    "type": "cppdbg",
    "request": "launch",
    "program": "${workspaceFolder}/build/debug/src/BilibiliPlayer.exe",
    "stopAtEntry": false,
    "MIMode": "gdb",
    "miDebuggerPath": "C:/Qt/Tools/mingw1310_64/bin/gdb.exe"
}
```

**GDB Commands**:
```bash
# Show all threads
(gdb) info threads

# Switch to thread
(gdb) thread 2

# Show backtrace
(gdb) bt

# Break on thread creation
(gdb) catch thread
```

---

## Contributing Guidelines

### Code Style

- Follow constitution naming conventions (PascalCase classes, camelCase methods, m_camelCase_ members)
- Use `clang-format` (config in `.clang-format`)
- Max line length: 100 characters
- Use `auto` sparingly (prefer explicit types for readability)

### Commit Messages

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types**: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`

**Example**:
```
feat(audio): add WASAPI exclusive mode support

- Implement exclusive mode initialization
- Add fallback to shared mode if exclusive fails
- Measure latency improvement (~40ms → ~10ms)

Closes #42
```

### Pull Request Process

1. Create feature branch: `git checkout -b 002-feature-name`
2. Implement feature following TDD workflow
3. Run all tests: `cmake --build build/debug && cd build/debug/test && ./all_tests`
4. Update documentation if needed
5. Submit PR with:
   - Description of changes
   - Link to spec/task
   - Test results
   - Screenshots (if UI changes)

---

## Next Steps

1. **Read Spec Documents**:
   - `specs/001-unified-stream-player/spec.md` - User stories and requirements
   - `specs/001-unified-stream-player/research.md` - Technical decisions
   - `specs/001-unified-stream-player/data-model.md` - Data structures
   - `specs/001-unified-stream-player/contracts/` - Component interfaces

2. **Explore Codebase**:
   - `src/main.cpp` - Application entry point
   - `src/ui/MainWindow.cpp` - Main UI
   - `src/audio/AudioPlayer.cpp` - Playback logic
   - `src/playlist/PlaylistManager.cpp` - Playlist management

3. **Run Demo Applications**:
   ```bash
   cd build/debug/demo
   ./network_demo.exe <bilibili-url>
   ./audio_demo.exe <local-file.mp3>
   ```

4. **Implement First User Story**:
   - Start with "Play Audio from Multiple Sources" (P1)
   - Follow TDD workflow
   - Reference contracts for interface definitions

---

**Quickstart Complete**: You're ready to build and extend BilibiliPlayer!

For questions or issues, consult:
- Constitution: `.specify/memory/constitution.md`
- Spec documents: `specs/001-unified-stream-player/`
- GitHub Issues: https://github.com/Paindar/BilibiliPlayer/issues
