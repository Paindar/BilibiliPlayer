# Quickstart: Implementation Setup

**Date**: 2025-11-19  
**Feature**: `004-ui-fixes-localization`  
**Status**: Ready for Development

## Developer Checklist

Before starting implementation, ensure you have:

- [ ] Read `specs/004-ui-fixes-localization/spec.md` (feature requirements)
- [ ] Read `specs/004-ui-fixes-localization/plan.md` (overall plan)
- [ ] Read all 4 contracts in `specs/004-ui-fixes-localization/contracts/` (API details)
- [ ] Read `specs/004-ui-fixes-localization/data-model.md` (data structures)
- [ ] Clone/checkout branch `004-ui-fixes-localization`
- [ ] Build project locally: `cmake --preset conan-debug && cmake --build --preset conan-debug`
- [ ] Understand the four independent work streams

## Four Independent Work Streams

This feature can be decomposed into 4 parallel streams, each independently testable:

### Stream 1: UI Text Localization

**Goal**: Replace all hardcoded UI strings with `tr()` localization calls

**Scope**: `src/ui/**/*.{h,cpp}`

**Tasks**:
1. Audit all UI files for hardcoded English strings
2. Wrap strings with `tr()` macro
3. Run `lupdate` to extract and update `.ts` files
4. Verify `.qm` files compile without errors
5. Test UI displays correctly with localized strings

**Test**: `ui_localization_test.cpp`

**Effort**: ~3-5 days (medium - many files but mechanical changes)

**See**: `contracts/localization-contract.md`

---

### Stream 2: Non-Blocking Search

**Goal**: Move search to worker thread and replace blocking message boxes

**Scope**: `src/ui/pages/search_page.{h,cpp}`, `src/network/platform/bili_network_interface.{h,cpp}`

**Tasks**:
1. Create worker thread for search operation
2. Refactor BilibiliPlatform::search() to emit signals
3. Connect signals to UI slots for progress/completion
4. Replace QMessageBox calls with status bar feedback
5. Add cancellation support
6. Test UI remains responsive during search

**Test**: `search_async_test.cpp`

**Effort**: ~3-4 days (medium - requires async patterns, threading)

**Dependencies**: Understanding of Qt signals/slots, threading

**See**: `contracts/search-async-contract.md`

---

### Stream 3: Platform Name Metadata

**Goal**: Ensure correct platform name flows from search → playlist

**Scope**: `src/playlist/audio_item.{h,cpp}`, `src/network/platform/bili_network_interface.cpp`, `src/ui/pages/search_page.cpp`

**Tasks**:
1. Verify SearchResult includes platformName field
2. Ensure BilibiliPlatform::search() sets platformName = "Bilibili"
3. AudioItem creation validates platformName is non-empty
4. SearchResult → AudioItem transfer preserves platform name
5. PlaylistPage displays platform name (not "Unknown")
6. Test data flow end-to-end

**Test**: Integration tests in `playlist_manager_test.cpp`

**Effort**: ~2-3 days (low-medium - data flow changes, no complex logic)

**See**: `contracts/platform-metadata-contract.md`

---

### Stream 4: Playlist Navigation

**Goal**: Fix last/next buttons after deletion and mode changes

**Scope**: `src/playlist/playlist_manager.{h,cpp}`, `src/ui/pages/playlist_page.cpp`

**Tasks**:
1. Debug current navigation implementation
2. Add validation to `last()` and `next()` methods
3. Fix currentIndex adjustment after deletion
4. Handle playback mode changes correctly
5. Test all edge cases (empty playlist, single item, rapid nav, mode changes)
6. Add comprehensive test coverage

**Test**: `playlist_navigation_test.cpp` (expanded)

**Effort**: ~4-5 days (medium-high - complex state logic, many edge cases)

**Dependencies**: Understanding of playlist data model

**See**: `contracts/playlist-navigation-contract.md`

---

## Build Instructions

### Initial Setup

```bash
# Clone branch
git checkout 004-ui-fixes-localization

# Install dependencies (Conan)
conan install . --output-folder=build/debug --build=missing --settings=build_type=Debug

# Configure CMake
cmake --preset conan-debug

# Build
cmake --build --preset conan-debug -j 6

# Run existing tests
./build/debug/test/BilibiliPlayerTest.exe
```

### After Making Changes

```bash
# Rebuild after code changes
cmake --build --preset conan-debug -j 6

# Run tests
./build/debug/test/BilibiliPlayerTest.exe

# Run specific test file
./build/debug/test/BilibiliPlayerTest.exe [TestName]

# Extract localization strings (after adding tr() calls)
lupdate . -ts resource/lang/en_US.ts resource/lang/zh_CN.ts

# Compile localization files
lrelease resource/lang/en_US.ts -qm resource/lang/en_US.qm
lrelease resource/lang/zh_CN.ts -qm resource/lang/zh_CN.qm
```

## Code Review Checklist

Before requesting review, verify:

### All Streams
- [ ] No compiler warnings
- [ ] All tests pass locally
- [ ] Follows project naming conventions (PascalCase classes, camelCase methods, m_snake_case_ members)
- [ ] Code properly documented (Doxygen comments for public APIs)
- [ ] Thread-safe if concurrent access possible
- [ ] No memory leaks (RAII patterns, smart pointers)
- [ ] No hardcoded strings (use `tr()` or constants)
- [ ] Const correctness maintained
- [ ] Error handling explicit (no silent failures)

### Stream 1: Localization
- [ ] All user-visible strings use `tr()`
- [ ] `.ts` files extracted with `lupdate`
- [ ] `.qm` files compile without errors
- [ ] UI displays correctly with string replacement

### Stream 2: Async Search
- [ ] No `QMessageBox` calls during search
- [ ] Search runs on separate thread
- [ ] Signals used for UI updates (not direct calls)
- [ ] UI remains responsive during search
- [ ] Thread cleanup properly handled
- [ ] Cancellation supported

### Stream 3: Platform Metadata
- [ ] SearchResult has platformName field
- [ ] BilibiliPlatform sets platformName to "Bilibili"
- [ ] AudioItem validates platformName is non-empty
- [ ] Platform name preserved through SearchResult → AudioItem
- [ ] Displayed as platform name, never "Unknown"

### Stream 4: Playlist Navigation
- [ ] Deletion updates currentIndex correctly
- [ ] Navigation works after all deletion scenarios
- [ ] Playback modes respected (NORMAL, SHUFFLE, REPEAT_ALL, REPEAT_ONE)
- [ ] Edge cases handled (empty playlist, single item, boundaries)
- [ ] Tests cover all identified scenarios
- [ ] No off-by-one errors

## Key Files to Understand

```
src/ui/
├── pages/search_page.h/cpp           # Search UI, blocking dialogs (fix: async + localize)
├── pages/playlist_page.h/cpp         # Playlist UI, display (fix: localize + platform name)
└── dialogs/                          # All dialogs (fix: localize strings)

src/network/
└── platform/
    ├── i_platform.h                  # Platform interface
    └── bili_network_interface.h/cpp  # Search implementation (fix: set platform name, async)

src/playlist/
├── playlist_manager.h/cpp            # Navigation logic (fix: deletion handling)
├── audio_item.h/cpp                  # Item storage (fix: validate platform name)
└── playlist.h/cpp                    # Playlist state (fix: index management)

resource/
└── lang/
    ├── en_US.ts                      # English strings to extract
    └── zh_CN.ts                      # Chinese translations

test/
├── ui_localization_test.cpp          # NEW: String replacement tests
├── search_async_test.cpp             # NEW: Non-blocking search tests
└── playlist_navigation_test.cpp       # UPDATED: Deletion and nav tests
```

## Testing Strategy

### Unit Tests
- Test each component in isolation (AudioItem, Playlist, PlaylistManager)
- Mock external dependencies (network, UI)
- Cover all edge cases documented in contracts

### Integration Tests
- Test SearchResult → AudioItem flow
- Test async search operation end-to-end
- Test playlist deletion and navigation scenarios

### Manual Testing
- Run application and manually test each feature
- Search for content → verify no UI freeze
- Add to playlist → verify platform name displays
- Delete items → verify navigation still works
- Switch playback modes → verify navigation respects mode

## Performance Considerations

- Search progress signals: Emit every 5-10 results or 100ms
- UI responsiveness target: Button clicks respond within 50ms during search
- Navigation: O(1) complexity for all last/next operations
- Memory: No unbounded growth in result vectors

## Documentation to Update

After implementation, update:

- [ ] `README.md` - Add feature description
- [ ] Code comments - Explain non-obvious implementations
- [ ] `.ts` files - Ensure all translations are complete

## Success Metrics

Feature is complete when:

1. ✅ All user-visible UI text uses `tr()` localization (SC-001)
2. ✅ Search operations never freeze UI (SC-002)
3. ✅ Audio from search shows correct platform name (SC-003)
4. ✅ Navigation works after all deletion scenarios (SC-004)
5. ✅ Navigation respects playback modes (SC-005)
6. ✅ All translation strings present in language files (SC-006)
7. ✅ All tests pass with >80% code coverage
8. ✅ No compiler warnings or memory leaks
9. ✅ Code review approved
10. ✅ Feature merged to master

---

**Ready to start development!**

Next: `/speckit.tasks` to generate detailed task breakdown.
