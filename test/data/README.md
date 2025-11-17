# Test data

This directory contains compact, pre-generated test assets encoded as hex strings to avoid binary blobs in the repository.

Usage:
- Use the helper in `test/test_utils.h` to decode a `.hex` file and write the real binary asset before running tests.
- Example in C++ tests:

```cpp
#include "test/test_utils.h"
testutils::writeHexAsset("test/data/silence.wav.hex", "build/test_assets/silence.wav");
```

Current assets:
- `silence.wav.hex` — a minimal 1-sample WAV (mono, 8kHz, 16-bit) useful for format-detection tests.
# Test Data Directory

This directory contains test data files used by the BilibiliPlayer test suite.

## Directory Structure

```
test/data/
├── audio/          # Test audio files (generated programmatically)
├── json/           # Test JSON files for playlists and themes
└── README.md       # This file
```

## Audio Files

Audio test files are generated programmatically during test execution to avoid committing large binary files. Test utilities create:

- **Silence**: PCM silence data for basic decoder testing
- **Tone**: 440Hz sine wave for format conversion testing
- **MP3/FLAC**: Encoded audio using FFmpeg API

## JSON Test Files

### Theme JSONs

- `valid_theme.json` - Complete valid theme with all required fields
- `malformed_theme.json` - Invalid JSON syntax for error handling tests
- `missing_fields_theme.json` - Valid JSON but missing required theme fields
- `extra_fields_theme.json` - Theme with unknown extra fields (forward compatibility)

### Playlist JSONs

- `valid_playlist.json` - Complete valid playlist with songs
- `empty_playlist.json` - Valid empty playlist
- `corrupted_categories.json` - Corrupted categories file for error handling

## Usage in Tests

Test files use Catch2 fixtures to:
1. Create temporary directories for test execution
2. Generate audio files on-demand using helper functions
3. Load JSON files from this directory for validation tests
4. Clean up temporary data after tests complete

## Notes

- Audio files are NOT committed to git (see `.gitignore`)
- JSON files ARE committed for reproducible tests
- All test data uses minimal size to keep tests fast
- Bad Apple!! test uses real Bilibili API (integration test)
