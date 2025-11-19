# Phase 4: Code Quality Focus - Strategic Plan

**Focus Areas**: Memory Leaks, Const-Correctness, Error Handling  
**Priority**: HIGH  
**Estimated Effort**: 2-3 hours

---

## 1. Memory Leak Verification with AddressSanitizer

### Strategy
Run test suite under AddressSanitizer (Debug mode) to detect leaks in:
- Async operations (`std::async`, lambda captures with `shared_ptr`)
- Signal connections (Qt queued signals, lifetime management)
- Playlist deletion (vector erase, potential dangling pointers)

### Implementation Steps

#### 1.1 Enable AddressSanitizer in CMake
- Add `-fsanitizer=address` to Debug build flags
- Configure Conan to include sanitizer libraries
- Update CMakePresets.json or CMakeLists.txt

#### 1.2 Run Tests Under Sanitizer
```bash
cmake --preset conan-debug  # May need ASAN flags
cmake --build --preset conan-debug -j6
./build/debug/test/BilibiliPlayerTest.exe 2>&1 | tee asan-report.txt
```

#### 1.3 Analyze Output
- Grep for "SUMMARY" in output
- Identify leak sources (if any)
- Focus on:
  - `src/network/network_manager.cpp` (async operations)
  - `src/ui/page/search_page.cpp` (signal connections)
  - `src/playlist/playlist_manager.cpp` (deletion handlers)

#### 1.4 Fix Leaks
- Most likely culprits:
  - Improper `shared_ptr` lifecycle (use `enable_shared_from_this` correctly) ✅ Already done
  - Qt signal receivers not disconnected (ensure destructor cleans up)
  - Lambda captures holding unnecessary references

---

## 2. Const-Correctness Audit

### Strategy
Systematically review code for proper const-ness:
- Function parameters that should be `const&`
- Member functions that don't mutate (mark `const`)
- Return values that shouldn't be modified
- Loop variables that shouldn't be modified

### Modules to Review (Priority Order)

#### 2.1 Network Module
**File**: `src/network/platform/i_platform.h`
- [ ] `getPlatformName()` - Already inline, check param usage
- [ ] Signal parameters in SearchResult structs

**File**: `src/network/network_manager.h/cpp`
- [ ] `executeMultiSourceSearch(const QString& query, ...)` - Check params
- [ ] Search result callbacks - params should be `const&`
- [ ] `cancelAllSearches()` - Doesn't mutate externally, mark `const`? (no, it's a mutator)

#### 2.2 Playlist Module
**File**: `src/playlist/playlist_manager.h/cpp`
- [ ] `getNextSong(const QUuid& current, ...)` - Check all params
- [ ] `getPreviousSong(const QUuid& current, ...)` - Check all params
- [ ] `removeSongFromPlaylist()` - Check parameters
- [ ] Loop variables in UUID lookup should be `const auto&`
- [ ] Songs vector iteration - use `const` reference if not modifying

#### 2.3 UI Module
**File**: `src/ui/page/search_page.h/cpp`
- [ ] `performSearch()` parameters and captures
- [ ] Lambda captures - use `const auto&` for callbacks
- [ ] `onSearchProgress()` parameters should be `const` where possible

**File**: `src/ui/page/playlist_page.h/cpp`
- [ ] Display methods that only read data - mark `const`
- [ ] Signal handlers that only read - check parameter constness

### Implementation Pattern

Before:
```cpp
void getNextSong(QUuid currentSongId, PlaylistMode mode) {
    // code
}

for (auto song : songs) {  // Unnecessary copy!
    if (song.uuid == currentSongId) { /* ... */ }
}
```

After:
```cpp
std::optional<QUuid> getNextSong(const QUuid& currentSongId, PlayMode mode) const {
    // code (if method doesn't mutate)
}

for (const auto& song : songs) {  // Reference, not copy
    if (song.uuid == currentSongId) { /* ... */ }
}
```

---

## 3. Error Handling Audit

### Strategy
Comprehensive review of all error paths to ensure graceful degradation:

#### 3.1 Navigation Logic (`src/playlist/playlist_manager.cpp`)

**Scenarios to Check**:
- [ ] What if `currentSongId` is null/invalid?
  - Expected: Return `nullopt` (don't crash)
  - Verify: All branches handle this
- [ ] What if playlist is empty?
  - Expected: Return `nullopt` gracefully
  - Verify: Modulo never divides by zero: `(index ± 1) % size` when size > 0
- [ ] What if PlayMode enum has unknown value?
  - Expected: Default to `PlaylistLoop` or return error
  - Verify: Switch statement has default case

**Test Cases**:
- Empty playlist + getNextSong → should return `nullopt`
- Single song + PlaylistLoop + getNextSong → should wrap to self
- Invalid UUID + getNextSong → should return `nullopt`

#### 3.2 Deletion Signals (`src/playlist/playlist_manager.cpp`)

**Scenarios to Check**:
- [ ] What if deleted song is only song?
  - Expected: `nextSongId = QUuid()` (null), AudioPlayer stops
  - Verify: Code produces null UUID
- [ ] What if deleted song is at middle of list?
  - Expected: `nextSongId = songs[deletedIndex + 1]` (successor)
  - Verify: Boundary check prevents out-of-bounds
- [ ] What if deletion happens during playback?
  - Expected: Signal emitted, AudioPlayer responds, UI updates
  - Verify: Signal connection exists and handles it

#### 3.3 Search Operations (`src/network/network_manager.cpp`)

**Scenarios to Check**:
- [ ] What if search query is empty?
  - Expected: Return empty results or error signal
  - Verify: Early return or validation
- [ ] What if network request fails?
  - Expected: Emit error signal, not crash
  - Verify: Error handling in async lambda
- [ ] What if search is cancelled during execution?
  - Expected: Gracefully exit, don't emit stale results
  - Verify: `isCancelled()` check in loop, proper cleanup

#### 3.4 Platform Names (`src/network/platform/i_platform.h`)

**Scenarios to Check**:
- [ ] What if PlatformType is unknown/invalid?
  - Expected: Return "Unknown" string
  - Verify: Switch has default case, always returns QString
- [ ] What if SongInfo.platform is corrupted (out-of-enum range)?
  - Expected: Cast to enum, default case catches it
  - Verify: Safe cast with enum validation

### Implementation Approach

Add defensive checks:
```cpp
// Before: Assumes valid state
int currentIndex = -1;
for (int i = 0; i < songs.size(); ++i) {
    if (songs[i].uuid == currentSongId) {
        currentIndex = i;
        break;
    }
}
// Implicit failure if currentIndex remains -1

// After: Explicit error handling
auto it = std::find_if(songs.begin(), songs.end(),
    [&](const auto& song) { return song.uuid == currentSongId; });
if (it == songs.end()) {
    LOG_WARN("Song UUID not found in playlist");
    return std::nullopt;
}
int currentIndex = std::distance(songs.begin(), it);
```

---

## 4. Minimal Tasks (Defer or Skip)

### End-to-End Tests [DEPRECATED]
**Reason**: Existing unit tests (`search_async_test.cpp`, `playlist_navigation_test.cpp`) already cover 4 stories in isolation. Integration testing would be useful but not critical for Phase 4.
**Action**: Skip - defer to Phase 5

### Release Build Validation [MINIMAL]
**Reason**: Focus is on code quality, not performance optimization yet.
**Action**: Quick validation only:
1. Build Release: `cmake --preset conan-release && cmake --build --preset conan-release`
2. Run app once to verify no crashes
3. Skip detailed profiling (defer to Phase 5)

### Documentation Update [MINIMAL]
**Reason**: PHASE_3_IMPLEMENTATION.md is comprehensive; README doesn't need overhaul.
**Action**: 
1. Add 2-3 line Phase 3 summary to README.md under "Features"
2. Link to PHASE_3_IMPLEMENTATION.md for details
3. Skip detailed developer guide (defer to Phase 5)

---

## Execution Checklist

### Phase 4a: Memory Leak Verification
- [ ] Enable AddressSanitizer in Debug build
- [ ] Run test suite under ASAN
- [ ] Parse ASAN output for leak summaries
- [ ] Fix any critical leaks (heap-use-after-free, buffer overflow)
- [ ] Verify no leaks with repeat run
- [ ] Commit: "Phase 4a: AddressSanitizer leak fixes"

### Phase 4b: Const-Correctness
- [ ] Audit `src/network/platform/i_platform.h` - mark params `const&`
- [ ] Audit `src/network/network_manager.cpp` - review loops, callbacks
- [ ] Audit `src/playlist/playlist_manager.cpp` - const correctness
- [ ] Audit `src/ui/page/search_page.cpp` - signal handlers
- [ ] Audit `src/ui/page/playlist_page.cpp` - display methods
- [ ] Update loop variables: `for (const auto&` instead of `for (auto`
- [ ] Mark read-only methods as `const`
- [ ] Commit: "Phase 4b: Improve const-correctness across Phase 3 code"

### Phase 4c: Error Handling
- [ ] Add null UUID validation in navigation
- [ ] Add empty playlist guards
- [ ] Add enum bounds checking
- [ ] Add logging for all error paths
- [ ] Test edge cases manually
- [ ] Commit: "Phase 4c: Harden error handling in navigation and deletion"

### Phase 4d: Minimal Tasks
- [ ] Quick Release build: `cmake --preset conan-release && cmake --build --preset conan-release`
- [ ] Run app once - verify no crashes
- [ ] Add 2-3 lines to README.md Phase 3 feature summary
- [ ] Commit: "Phase 4d: Quick validation and documentation"

---

## Expected Outcomes

✅ **Memory Safety**: No AddressSanitizer leaks reported  
✅ **Code Quality**: Proper const-correctness across all Phase 3 modules  
✅ **Robustness**: Graceful handling of edge cases with clear logging  
✅ **Build Status**: Both Debug and Release pass without warnings  
✅ **Documentation**: PHASE_3_IMPLEMENTATION.md verified accurate  

---

**Next Step**: Begin Phase 4a - AddressSanitizer setup
