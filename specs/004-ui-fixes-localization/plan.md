# Implementation Plan: UI Fixes, Localization, and Playlist Navigation

**Branch**: `004-ui-fixes-localization` | **Date**: 2025-11-19 | **Spec**: [spec.md](./spec.md)  
**Input**: Feature specification from `/specs/004-ui-fixes-localization/spec.md`

## Summary

Replace all hardcoded UI strings with Qt `tr()` localization calls, enable non-blocking async search with SearchingPage UI state, fix platform name display (use PlatformType enum with i18n), and repair playlist navigation after item deletion by respecting playback modes and regenerating random play orders.

**Technical Approach**: 
- Audit all UI components for hardcoded strings
- Wrap UI text with `tr()` and extract/compile translation files
- Refactor search to run on worker thread with cancellation support
- Add SearchingPage widget to SearchPage's contentStack for loading state
- Fix deletion handler to trigger next-track playback or stop signal
- Validate and fix navigation logic for last/next buttons
- Convert platform enum to translatable display strings

## Technical Context

**Language/Version**: C++17 (C++20 features with verified compiler support)

**Primary Dependencies**: 
- Qt 6.8.0 (UI framework, localization via `tr()`, signal/slot threading)
- FFmpeg 7.1.1 (audio decoding)
- cpp-httplib 0.15.3 (HTTP client for search)
- jsoncpp 1.9.6 (JSON parsing)

**Storage**: JSON config files (Phase 1); SQLite planned for Phase 2 playlist persistence

**Testing**: Catch2 3.11.0 (unit & integration tests)

**Target Platform**: Windows desktop (MinGW 13.1.0 compiler, Qt 6.8.0, FFmpeg 7.1.1)

**Project Type**: Desktop GUI application (single project with src/ and test/ directories)

**Performance Goals**: 
- Search operations: complete within 2 seconds (typical 200-500 results)
- UI responsiveness: remain interactive during all search operations (no freezing)
- Navigation latency: <100ms for last/next button clicks
- Localization: <50ms to load and apply translation files on startup

**Constraints**: 
- No blocking modal dialogs during async operations (non-blocking search)
- Rapid navigation cancels pending operations for immediate responsiveness
- Platform enum must convert to i18n strings (not hardcoded)
- All user-visible UI text must use `tr()` (no hardcoded English strings)

**Scale/Scope**: 
- 4 user stories (P1 priority)
- ~15-20 UI components with hardcoded strings to localize
- 1 search operation to convert to async
- 3 playback modes (NORMAL, SHUFFLE, REPEAT) to handle in navigation
- 2 major bug fixes (platform names, navigation after deletion)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Principle I: Code Quality & Maintainability
✅ **PASS**: 
- Feature uses RAII (shared_ptr for async operations, Qt parent-child ownership)
- Const correctness applied throughout data model
- Error handling via exceptions + logging
- Naming conventions: PascalCase for classes (SongInfo, PlaylistInfo), camelCase for methods
- Public APIs documented in data model with thread-safety notes

### Principle II: Comprehensive Testing Standards
✅ **PASS**: 
- Unit tests required for localization string extraction verification
- Integration tests required for search async operation + cancellation
- Navigation tests required for deletion + mode handling (normal/shuffle/repeat)
- All critical paths covered (see data model state transitions)

### Principle III: User Experience Consistency
✅ **PASS**: 
- Non-blocking search maintains UI responsiveness (UX requirement FR-002)
- SearchingPage provides visual feedback (consistent with emptyPage pattern)
- Platform names displayed correctly (fixes incorrect "Unknown" UX)
- Playlist navigation works reliably after deletion

### Principle IV: Performance & Resource Management
✅ **PASS**: 
- Async search on worker thread prevents UI blocking
- Rapid navigation cancellation via `std::atomic<bool>` (no locks)
- Translation files loaded once at startup (lazy locale switching)
- No memory leaks: shared_ptr manages async operation lifetimes

### Principle V: Thread Safety & Concurrency
✅ **PASS**: 
- Async search uses QThread + signal/slot for thread-safe communication
- AsyncSearchOperation uses `std::atomic<bool>` for cancellation flag
- SearchResult → SongInfo transfer happens on main thread after signal delivery
- No data races: all shared state protected by Qt's thread-safe signal/slot mechanism

**Gate Status**: ✅ All principles satisfied. Proceed to Phase 0 research.

## Project Structure

### Documentation (this feature)

```text
specs/004-ui-fixes-localization/
├── plan.md              # This file (implementation roadmap)
├── spec.md              # Feature specification with 4 user stories
├── data-model.md        # Entity definitions with clarifications C1-C6
├── research.md          # Phase 0 research findings (TO BE CREATED)
├── quickstart.md        # Phase 1 developer guide (ALREADY EXISTS)
├── contracts/           # Phase 1 API contracts (ALREADY EXISTS)
│   ├── localization-contract.md
│   ├── search-async-contract.md
│   ├── platform-metadata-contract.md
│   └── playlist-navigation-contract.md
└── checklists/
    └── requirements.md  # Quality checklist
```

### Source Code (repository root)

```text
src/
├── ui/
│   ├── page/search_page.cpp/.h      # Modified: SearchingPage widget in contentStack
│   ├── search_page.ui               # Modified: Add SearchingPage to stacked widget
│   └── [other UI components]        # Modified: wrap strings with tr()
├── network/
│   ├── platform/i_platform.h        # Modified: platform enum display names with tr()
│   ├── platform/bili_network_interface.cpp/.h
│   └── search_service.cpp/.h        # Modified: make search async with cancellation
├── playlist/
│   ├── playlist.h/.cpp              # Modified: deletion handler to trigger next-track
│   ├── playlist_manager.cpp/.h      # Modified: navigation fix for last/next buttons
│   └── [related playlist files]
├── config/
│   └── [localization system files]
└── [other components]

test/
├── ui_localization_test.cpp         # NEW: verify tr() coverage
├── search_async_test.cpp            # NEW: test async search + cancellation
├── playlist_navigation_test.cpp      # NEW: test deletion + mode handling
└── [existing tests]

resource/
└── lang/
    ├── en_US.ts                     # Extract/update with lupdate
    ├── zh_CN.ts                     # Chinese translation
    └── en_US.qm / zh_CN.qm          # Compiled translations (lrelease)
```

**Structure Decision**: Single desktop application with modular feature organization. Search async logic contained in SearchService (network module). Playlist navigation fixes in PlaylistManager (playlist module). Localization applied across all UI components (ui module). This aligns with existing project architecture.

## Phase 0: Research

### Research Tasks Identified

Based on specification and technical context, the following research areas have been validated:

1. ✅ **Qt Localization System** - Verified: `tr()` macro, .ts/.qm workflow with lupdate/lrelease
2. ✅ **Qt Threading with Signals/Slots** - Verified: QThread, signal-safe async operations documented in constitution
3. ✅ **Platform Enum Display** - Verified: PlatformType enum in `i_platform.h`, needs conversion helper for i18n
4. ✅ **SearchingPage UI Pattern** - Verified: SearchPage already uses contentStack (QStackedWidget) with emptyPage pattern
5. ✅ **Async Operation Cancellation** - Verified: `std::atomic<bool>` pattern from codebase analysis
6. ✅ **Playlist Deletion Logic** - Verified: PlaylistManager owns m_playlistSongs map, currentIndex tracking
7. ✅ **Playback Mode Handling** - Verified: NORMAL, SHUFFLE, REPEAT modes in PlaylistManager data model

**Gate Status**: All research areas addressed from codebase review. No critical unknowns remain. Phase 1 design can proceed.

## Phase 1: Design & Contracts

### Entities Defined

Data model completed with 5 entities reflecting actual codebase structures:

1. **SongInfo** (was AudioItem in spec) - actual struct from `src/playlist/playlist.h`
2. **PlaylistInfo** - managed by PlaylistManager with m_playlistSongs hierarchy
3. **SearchResult** - from `src/network/platform/i_platform.h` with PlatformType enum
4. **UIString & Platform Localization** - Qt `tr()` system with enum-to-string helpers
5. **AsyncSearchOperation** - worker thread with std::atomic<bool> cancellation

**Clarifications Applied** (C1-C6):
- C1: Delete playing item → play next or stop if empty
- C2: Search progress UI → SearchingPage in contentStack
- C3: Missing translations → show raw English fallback
- C4: Random mode after deletion → regenerate randomPlayOrder
- C5: Platform names → wrap with tr() for i18n
- C6: Rapid navigation → cancel pending async operations

### API Contracts Generated

Located in `specs/004-ui-fixes-localization/contracts/`:

1. **localization-contract.md** - `tr()` wrapping patterns, lupdate/lrelease workflow
2. **search-async-contract.md** - AsyncSearchOperation interface, cancellation semantics
3. **platform-metadata-contract.md** - PlatformType enum to string conversion
4. **playlist-navigation-contract.md** - Deletion handler, next/last navigation with mode handling

### Agent Context Updated

✅ Copilot instruction file updated at `.github/copilot-instructions.md`:
- Technology stack: C++17 + Qt 6.8.0, FFmpeg 7.1.1, etc.
- Architecture patterns: async with shared_ptr, threading with signals/slots
- New tech from feature: Qt localization, SearchingPage pattern, async cancellation

## Phase 2 Preparation (Post-Planning)

### Task Generation (Deferred to `/speckit.tasks` command)

When ready, run `/speckit.tasks` to generate detailed implementation tasks covering:

**Localization Tasks**:
- T-L1: Audit all UI components for hardcoded strings
- T-L2: Wrap identified strings with `tr()` macro
- T-L3: Extract strings with `lupdate` into .ts files
- T-L4: Add Chinese translations to zh_CN.ts
- T-L5: Compile .ts → .qm with `lrelease`

**Search Async Tasks**:
- T-S1: Refactor search to run on QThread worker
- T-S2: Implement AsyncSearchOperation with cancellation flag
- T-S3: Add SearchingPage widget to contentStack
- T-S4: Connect search signals for progress + completion
- T-S5: Test async search with cancellation scenarios

**Navigation Tasks**:
- T-N1: Fix deletion handler to trigger next-track or stop signal
- T-N2: Regenerate randomPlayOrder on deletion when in SHUFFLE mode
- T-N3: Fix next/last button navigation logic for all modes
- T-N4: Test navigation after deletion with all playback modes
- T-N5: Validate navigation state transitions

**Platform Metadata Tasks**:
- T-P1: Create PlatformType enum → string helper function
- T-P2: Wrap platform display strings with `tr()`
- T-P3: Verify SearchResult platform enum set correctly by BilibiliPlatform
- T-P4: Test platform name display in all UI views

**Testing Tasks**:
- T-T1: Unit tests for localization string extraction
- T-T2: Integration tests for async search + cancellation
- T-T3: Integration tests for playlist navigation after deletion
- T-T4: UI tests verifying SearchingPage appears and disappears

### Complexity Tracking

| Aspect | Effort | Justification |
|--------|--------|---------------|
| Localization Scope | Moderate | ~15-20 UI strings identified; Qt tools automate extraction |
| Async Search Refactor | Moderate | SearchService exists; pattern proven in codebase |
| Navigation Fixes | Low | Bug fix in existing logic; no new architecture |
| Rapid Navigation Cancel | Low | `std::atomic<bool>` pattern already in codebase |
| Platform Display | Low | Simple enum-to-string helper + tr() wrapping |
| **Total Complexity** | **Moderate** | No architectural changes; localization is largest scope item |

## Key Implementation Patterns

### Localization Pattern

```cpp
// BEFORE (hardcoded string)
label->setText("Search Results");

// AFTER (with tr())
label->setText(tr("Search Results"));  // Qt extracts to .ts files
```

### Async Search Pattern

```cpp
// BEFORE (blocking on main thread)
std::vector<SearchResult> results = BilibiliPlatform::search(query);  // BLOCKS UI

// AFTER (async with cancellation)
auto op = std::make_shared<AsyncSearchOperation>(query);
op->cancelled = false;
QThread* worker = new QThread();
op->moveToThread(worker);
connect(worker, &QThread::started, op.get(), [op]() { op->executeSearch(); });
connect(op.get(), &AsyncSearchOperation::progressSignal, this, &SearchPage::onSearchProgress);
connect(op.get(), &AsyncSearchOperation::completedSignal, this, &SearchPage::onSearchCompleted);
worker->start();

// Cancellation on navigation away:
op->cancelled = true;
worker->quit();
worker->wait();
```

### Deletion Handler Pattern

```cpp
// BEFORE (unspecified behavior)
m_playlistSongs[playlistId].erase(it);

// AFTER (C1 clarification: play next or stop)
auto& songs = m_playlistSongs[playlistId];
bool wasPlaying = (currentIndex == indexToDelete);
if (wasPlaying) {
  if (currentIndex < songs.size() - 1) {
    emit playNext();  // Play next track immediately
  } else if (songs.empty()) {
    emit stop();  // Stop if playlist now empty
  }
}
songs.erase(it);

// If in random mode (C4), regenerate:
if (playbackMode == SHUFFLE) {
  regenerateRandomPlayOrder();
}
```

### Platform Display Pattern

```cpp
// Helper function (in platform module)
QString platformTypeToString(PlatformType type) {
  switch(type) {
    case PlatformType::BILIBILI: return tr("Bilibili");  // Translates to "B站"
    case PlatformType::YOUTUBE: return tr("YouTube");
    default: return tr("Unknown");
  }
}

// Usage in UI:
label->setText(platformTypeToString(song.platform));
```

## Success Metrics

✅ **Phase 1 Deliverables** (this planning cycle):
- [x] Specification clarified with 6 resolved ambiguities
- [x] Data model corrected to match actual codebase
- [x] Constitution check passed (all 5 principles satisfied)
- [x] API contracts defined in /contracts/
- [x] Developer quickstart created
- [x] Agent context updated
- [x] Implementation plan documented (this file)

📋 **Phase 2 Metrics** (next implementation cycle):
- Implementation tasks generated (via `/speckit.tasks`)
- Unit tests written and passing (>80% coverage)
- Integration tests validate all 4 user stories
- Search async operates without UI freezing
- Playlist navigation reliable after all deletion scenarios
- All UI strings use `tr()` (100% coverage)
- Platform names display correctly (no "Unknown" values)
- Random mode play order regenerates on deletion

## Next Steps

1. **Review this plan** with team/stakeholders
2. **Run `/speckit.tasks`** to generate detailed implementation tasks
3. **Begin Phase 2**: Implement features task-by-task with tests
4. **Validate** against success criteria before merging to main branch
