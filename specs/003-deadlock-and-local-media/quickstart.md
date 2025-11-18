# Spec 003: Quick Start Guide

**Local Media Playlist & Audio Diagnostics - Getting Started**

---

## Overview

This guide covers:
1. Building and testing the implementation
2. Running specific test suites
3. Using the audio diagnostics tools
4. Adding local media files to playlists

---

## Prerequisites

- **Build Tools**: CMake 3.16+, MinGW 13+, Conan 2.0+
- **Dependencies**: Qt 6.8.0, FFmpeg 7.1.1, TagLib 2.0, Catch2 3.11.0
- **OS**: Windows 10+ (or cross-platform with appropriate tools)

---

## Building the Project

### Full Debug Build

```powershell
cd d:\Project\BilibiliPlayer

# Run CMake configuration
cmake --preset conan-debug

# Build with 6 parallel jobs
cmake --build --preset conan-debug -j 6
```

**Output**: `build/debug/src/BilibiliPlayer.exe` and `build/debug/test/BilibiliPlayerTest.exe`

### Release Build

```powershell
# For production builds
cmake --preset conan-release
cmake --build --preset conan-release
```

---

## Running Tests

### Full Test Suite

```powershell
cd d:\Project\BilibiliPlayer\build\debug\test
./BilibiliPlayerTest.exe
```

**Expected Output**:
```
test cases:      77 |      74 passed | 3 skipped
assertions: 1000419 | 1000419 passed
```

### Filter by Test Category

```powershell
# Playlist management tests
./BilibiliPlayerTest.exe "[playlist_manager]"

# Local media tests
./BilibiliPlayerTest.exe "[playlist][local]"

# Audio diagnostics
./BilibiliPlayerTest.exe "[diagnostics][audio]"

# FFmpeg probe metadata tests
./BilibiliPlayerTest.exe "[ffmpeg_probe][metadata]"

# Audio quality tests
./BilibiliPlayerTest.exe "[diagnostics][audio][sine]"
```

### Filter by Specific Test Case

```powershell
# Test local file addition
./BilibiliPlayerTest.exe "[playlist_manager][songs]"

# Test duration probing
./BilibiliPlayerTest.exe "[playlist][local][duration]"

# Test cover extraction
./BilibiliPlayerTest.exe "[playlist][cover]"
```

### Run with Verbose Output

```powershell
./BilibiliPlayerTest.exe -v
```

### Test Coverage Report

```powershell
# Run coverage build
cmake --preset conan-coverage-debug
cmake --build --preset conan-coverage-debug

# Run tests
ctest --test-dir build/coverage

# Generate HTML report
cd build/coverage
gcovr -r ../.. --html --html-details -o coverage.html
```

---

## Local Media Feature - API Usage

### Adding Local Audio to Playlist

```cpp
#include <playlist/playlist_manager.h>
#include <playlist/playlist.h>

// Initialize
PlaylistManager manager(&configManager);
manager.initialize();

// Create song entry
playlist::SongInfo song;
song.title = "My Song";
song.uploader = "Artist Name";  // Optional - extracted from metadata if available
song.filepath = "/path/to/audio.mp3";  // Local absolute path or file:// URL
song.platform = 1;  // Will be overridden to Local for local files

// Add to playlist
QUuid playlistId = manager.getCurrentPlaylist();
bool success = manager.addSongToPlaylist(song, playlistId);

// Listen for metadata update signal
connect(&manager, &PlaylistManager::songUpdated, 
    [](const playlist::SongInfo& updated, const QUuid& playlistId) {
        qDebug() << "Song updated:"
                 << "Duration:" << updated.duration << "s"
                 << "Artist:" << updated.uploader
                 << "Cover:" << updated.coverName;
    });
```

### What Happens Automatically

1. **Immediately**:
   - File is verified as local
   - Platform set to `Local` (0x1)
   - Initial `SongInfo` persisted to playlist

2. **Async (Background Thread 1)**:
   - Duration probed via FFmpeg
   - Artist extracted from ID3/Vorbis tags
   - Sample rate and channels detected
   - Song updated signal emitted

3. **Async (Background Thread 2)**:
   - Cover art extracted (TagLib → FFmpeg fallback)
   - Cover cached with MD5 keyed filename
   - Cache path stored in `SongInfo.coverName`
   - Second update signal emitted

### Metadata Extraction Behavior

| Field | Source | Fallback |
|-------|--------|----------|
| Artist | ID3 TPE1 / Vorbis artist tag | Album artist tag |
| Album Artist | (if artist not found) | - |
| Default | - | "local" |
| Duration | FFmpeg format probe | INT_MAX (unknown) |
| Sample Rate | Audio stream probe | 0 (unknown) |
| Channels | Audio stream probe | 0 (unknown) |

---

## Audio Diagnostics Tools

### 1. PowerShell Reproducer Script

**Location**: `tools/diagnostics/local-audio-reproducer.ps1`

**Purpose**: Systematically test audio playback for artifacts

**Usage**:

```powershell
cd d:\Project\BilibiliPlayer

# Basic test: 60 seconds, single iteration
.\tools\diagnostics\local-audio-reproducer.ps1 -AudioFile "C:\music\song.mp3"

# Extended test: 120 seconds, 3 iterations with verbose logging
.\tools\diagnostics\local-audio-reproducer.ps1 `
    -AudioFile "song.mp3" `
    -Duration 120 `
    -Iterations 3 `
    -VerboseLogging $true `
    -OutputLog "./diagnostic_results.log"

# With PCM quality checks
.\tools\diagnostics\local-audio-reproducer.ps1 `
    -AudioFile "song.wav" `
    -CheckPCMQuality $true
```

**Output**: Timestamped diagnostic log with:
- Audio file metadata (codec, sample rate, channels)
- Artifact detection (clicks, pops, glitches, dropouts)
- Timing variance analysis
- Recommendations for issues found

### 2. Sine Wave Quality Test

**Location**: `test/diagnostics/sine_wave_playback_test.cpp`

**What it tests**:
- Pure sine wave generation (440Hz, 1kHz, etc.)
- RMS energy measurement
- THD (Total Harmonic Distortion) calculation
- Click/pop detection
- Signal correlation analysis

**Run tests**:

```powershell
cd d:\Project\BilibiliPlayer\build\debug\test
./BilibiliPlayerTest.exe "[diagnostics][audio][sine]"
```

**Quality Metrics**:

```
Pure Signal Baseline:
- THD: < 5%
- Clicks/minute: < 100
- Correlation vs reference: > 0.98
- RMS = Amplitude / √2

Degraded Signal Detection:
- THD: > 10%
- Clicks/minute: > 100
- Correlation: < 0.95
```

### 3. Diagnostic Workflow

**Step 1: Reproduce**
```powershell
# Run reproducer on problematic audio file
.\tools\diagnostics\local-audio-reproducer.ps1 `
    -AudioFile "problem_file.mp3" `
    -Duration 120 `
    -Iterations 3 `
    -OutputLog "results.log"
```

**Step 2: Analyze Results**
```
Review results.log for:
- Artifact patterns (random vs. periodic)
- Artifact frequency (clicks per minute)
- Timing consistency (variance analysis)
```

**Step 3: Run Unit Tests**
```powershell
# Verify quality metrics
./BilibiliPlayerTest.exe "[diagnostics][audio]"
```

**Step 4: Apply Mitigations**
- Increase buffer size if timing variance high
- Use high-priority thread for audio playback
- Check sample rate compatibility
- Validate codec support

**Step 5: Re-test**
```powershell
# Verify fix
.\tools\diagnostics\local-audio-reproducer.ps1 `
    -AudioFile "problem_file.mp3" `
    -OutputLog "results_after_fix.log"
```

---

## File Structure

```
BilibiliPlayer/
├── src/
│   ├── audio/
│   │   ├── ffmpeg_probe.h/cpp          # Metadata extraction
│   │   └── taglib_cover.h/cpp          # Cover extraction
│   ├── util/
│   │   └── cover_cache.h/cpp           # Cache management
│   └── playlist/
│       └── playlist_manager.cpp        # Local media handling
├── test/
│   ├── playlist_local_*.cpp            # Local media tests
│   ├── ffmpeg_probe_metadata_test.cpp  # Metadata extraction tests
│   └── diagnostics/
│       └── sine_wave_playback_test.cpp # Audio quality tests
├── tools/
│   └── diagnostics/
│       └── local-audio-reproducer.ps1  # PowerShell reproducer
└── specs/
    └── 003-deadlock-and-local-media/
        ├── spec.md                     # This specification
        ├── quickstart.md               # This guide
        ├── research.md                 # Findings & mitigations
        └── tasks.md                    # Task breakdown
```

---

## Common Issues & Troubleshooting

### Issue: "File not found" when adding local media

**Solution**:
- Ensure absolute path or valid file:// URL
- Check file permissions (read access required)
- Verify file format is supported (MP3, WAV, FLAC, etc.)

### Issue: Artist metadata not extracted

**Solution**:
- Check if ID3/Vorbis tags exist in file (use ffprobe)
- Falls back to "local" - this is expected behavior
- To add metadata: use mp3tag or tagtool

### Issue: Cover extraction fails

**Solution**:
- Verify cover art is embedded in file
- Try with different format (JPG vs PNG)
- Check cache directory permissions
- FFmpeg fallback should handle most cases

### Issue: Duration shows as "Unknown" (INT_MAX)

**Solution**:
- File may be corrupted or in unsupported format
- Use ffprobe to verify file integrity
- Supports common formats: MP3, WAV, FLAC, OGG, M4A

### Issue: Tests fail with build errors

**Solution**:
```powershell
# Clean rebuild
cmake --preset conan-debug
cmake --build --preset conan-debug --clean-first -j 6

# Or manually clean
Remove-Item -Recurse -Force build/debug/CMakeFiles
Remove-Item build/debug/CMakeCache.txt
```

### Issue: "Test data not available" warnings

**Expected**: 3 skipped tests are normal (video-only test data)
- These tests intentionally skip when test data is unavailable
- No action needed

---

## Performance Benchmarks

| Operation | Time | Notes |
|-----------|------|-------|
| Add local file to playlist | <50ms + async | UI not blocked |
| Metadata extraction | 50-200ms | Depends on file size |
| Cover extraction | 100-500ms | Depends on embedded cover |
| Full test suite | ~5-10s | 77 tests on modern CPU |

---

## Configuration

### Metadata Extraction Settings

All settings in `src/audio/ffmpeg_probe.cpp`:

```cpp
// Artist tag search order
const char* artistTags[] = {"artist", "Artist", "ARTIST", "TPE1", "TP1", nullptr};

// Album artist fallback
const char* albumArtistTags[] = {"album_artist", "Album Artist", "ALBUM_ARTIST", "TPE2", nullptr};
```

### Cover Cache Settings

All settings in `src/util/cover_cache.cpp`:

```cpp
// Cache directory: <workspace>/cache/covers/
// Key format: MD5(path|size|mtime) + extension
// Eviction: LRU with configurable size limit (default 100MB)
```

### Diagnostic Settings

In `tools/diagnostics/local-audio-reproducer.ps1`:

```powershell
$Duration = 60              # Playback duration in seconds
$Iterations = 1             # Number of repetitions
$VerboseLogging = $false    # Detailed per-interval logging
$CheckPCMQuality = $false   # PCM quality analysis
```

---

## Advanced: Customization

### Add Custom Metadata Tags

Edit `src/audio/ffmpeg_probe.cpp`:

```cpp
QString FFmpegProbe::probeArtist(const QString& path)
{
    // Add custom tags here
    const char* customTags[] = {"my_tag", "CUSTOM_ARTIST", nullptr};
    // ... existing code ...
}
```

### Adjust Click Detection Threshold

Edit `test/diagnostics/sine_wave_playback_test.cpp`:

```cpp
int clicks = detectClicks(cleanSine, 0.3f);  // Change 0.3f threshold
```

### Modify Cache Location

Edit `src/util/cover_cache.cpp`:

```cpp
// Change cache directory
QString cacheDir = m_configManager->getCoverCacheDirectory();
// Or override with custom path
```

---

## Support & Documentation

- **Spec Document**: `specs/003-deadlock-and-local-media/spec.md`
- **Research & Findings**: `specs/003-deadlock-and-local-media/research.md`
- **Task Breakdown**: `specs/003-deadlock-and-local-media/tasks.md`
- **Diagnostic Summary**: `specs/003-deadlock-and-local-media/T022_T024_T025_DIAGNOSTICS_SUMMARY.md`

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-19 | Initial release with full Phase 4-6 implementation |

---

**Last Updated**: 2025-11-19  
**Build Status**: ✓ PASSED (74/77 tests)  
**Ready for**: Merge to main branch
