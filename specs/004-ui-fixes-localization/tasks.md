# Implementation Tasks: UI Fixes, Localization, and Playlist Navigation

**Feature**: `004-ui-fixes-localization`  
**Branch**: `004-ui-fixes-localization`  
**Date**: 2025-11-19  
**Total Tasks**: 48  
**Test Approach**: Integration tests validating each user story independently

## Feature Overview

Replace hardcoded UI strings with Qt localization, enable non-blocking async search with SearchingPage UI state, fix platform name display, and repair playlist navigation after deletion.

## Dependency Graph

```
Phase 1 (Setup)
    ↓
Phase 2 (Foundational - all stories depend on these)
    ├─→ Phase 3 (User Story 1: Localization)
    ├─→ Phase 3 (User Story 2: Non-Blocking Search)
    ├─→ Phase 3 (User Story 3: Platform Names)
    └─→ Phase 3 (User Story 4: Navigation Fixes)
        ↓
Phase 5 (Polish & Integration)
```

**Independence**: All 4 user stories can be developed in parallel after foundational phase.

---

## Phase 1: Setup & Infrastructure

### Goal
Prepare development environment, audit codebase, and establish localization infrastructure.

### Tasks

- [ ] T001 Audit all UI components for hardcoded strings and document findings in `specs/004-ui-fixes-localization/audits/`
- [ ] T002 [P] Create localization infrastructure directory structure `resource/lang/` with template files
- [ ] T003 [P] Set up `.ts` (translation source) files: `en_US.ts` and `zh_CN.ts` in `resource/lang/`
- [ ] T004 Verify Qt localization tools available: `lupdate`, `lrelease` in build environment
- [ ] T005 [P] Create helper script `scripts/update-translations.sh` to automate lupdate + lrelease workflow
- [ ] T006 Document localization conventions in `docs/localization-guide.md` (tr() usage, contexts, naming)
- [ ] T007 [P] Create PlatformType enum display helper signature in `src/network/platform/platform_utils.h`
- [ ] T008 Configure CMake to include translation files in build (`CMakeLists.txt` Qt resource files)

### Independent Test Criteria
✓ Hardcoded string audit completed  
✓ Translation infrastructure directories exist  
✓ Qt tools verified working  
✓ Build system configured for translations

---

## Phase 2: Foundational Prerequisites

### Goal
Establish common infrastructure that all user stories depend on.

### Tasks

- [ ] T009 Implement `PlatformType` enum-to-string helper function in `src/network/platform/platform_utils.cpp` (no i18n yet)
- [ ] T010 [P] Create `AsyncSearchOperation` struct in `src/network/search_service.h` with `std::atomic<bool> cancelled` field
- [ ] T011 [P] Add `SearchingPage` placeholder widget class in `src/ui/page/search_page.h`
- [ ] T012 Extract hardcoded strings from codebase using `lupdate` and verify extraction in `.ts` files
- [ ] T013 [P] Create unit test fixture `test/ui_components_test.cpp` for UI testing infrastructure
- [ ] T014 [P] Add deletion handler interface to `PlaylistManager` in `src/playlist/playlist_manager.h`
- [ ] T015 Refactor `PlaylistManager::next()` and `PlaylistManager::last()` method signatures to document playback mode handling
- [ ] T016 [P] Create `search_page.ui` modification checklist (identify all places needing SearchingPage stack widget)

### Independent Test Criteria
✓ Helper functions compile without errors  
✓ Test fixtures created and compile  
✓ String extraction workflow succeeds  
✓ New method signatures integrated into existing code

---

## Phase 3: User Story 1 - Localization (P1)

### Goal
Replace all hardcoded UI strings with `tr()` localization calls and set up translation workflow.

**Acceptance Criteria**:
- ✓ 100% of user-visible UI strings use `tr()`
- ✓ English and Chinese translations for core UI text
- ✓ Missing translation strings show raw English fallback
- ✓ No compilation warnings about untranslated strings

### Tasks - Implementation (Parallel)

- [ ] T017 [US1] [P] Wrap all Search UI strings with `tr()` in `src/ui/page/search_page.cpp`
- [ ] T018 [US1] [P] Wrap all Playlist UI strings with `tr()` in `src/ui/page/playlist_page.cpp`
- [ ] T019 [US1] [P] Wrap all Dialog strings with `tr()` in `src/ui/dialogs/` (all .cpp files)
- [ ] T020 [US1] [P] Wrap all Menu/Button strings with `tr()` in `src/ui/widgets/` (all .cpp files)
- [ ] T021 [US1] Wrap platform display name strings with `tr()` in `src/network/platform/platform_utils.cpp`
- [ ] T022 [US1] [P] Run `lupdate` to extract new strings: `lupdate src/ -ts resource/lang/en_US.ts`
- [ ] T023 [US1] Update Chinese translations in `resource/lang/zh_CN.ts` (key phrases: "Search", "Results", "Playlist", etc.)
- [ ] T024 [US1] [P] Compile translations with `lrelease resource/lang/*.ts -qm resource/lang/`
- [ ] T025 [US1] Configure translation file loading in `src/config/application_context.cpp` (load .qm at startup)

### Tasks - Testing

- [ ] T026 [US1] [P] Create unit test `test/localization_test.cpp` verifying `tr()` calls resolve correctly
- [ ] T027 [US1] [P] Create integration test verifying all UI components display translated strings
- [ ] T028 [US1] Test missing translation fallback (verify raw English shown when .qm missing key)
- [ ] T029 [US1] Verify no hardcoded strings remain: `grep -r "setText\|setLabel" src/ui/ | grep -v tr()` should return 0 matches

### Independent Test Criteria
✓ All UI strings wrapped with `tr()`  
✓ String extraction successful  
✓ Translations load at startup  
✓ Chinese translations display correctly  
✓ Missing strings fall back to English  
✓ All tests passing

---

## Phase 3: User Story 2 - Non-Blocking Search (P1)

### Goal
Refactor search to run asynchronously on worker thread without blocking UI, with SearchingPage loading state.

**Acceptance Criteria**:
- ✓ Search operations don't freeze UI
- ✓ SearchingPage visible during search progress
- ✓ UI remains interactive for other operations during search
- ✓ Rapid navigation cancels pending search operations
- ✓ Search results display without modal dialogs

### Tasks - Infrastructure

- [ ] T030 [US2] Implement `SearchingPage` widget in `src/ui/page/searching_page.h/cpp` (loading spinner/status text)
- [ ] T031 [US2] Add `SearchingPage` to `search_page.ui` contentStack (QStackedWidget) alongside existing emptyPage
- [ ] T032 [US2] [P] Implement `AsyncSearchOperation::executeSearch()` in `src/network/search_service.cpp` (runs on worker thread)
- [ ] T033 [US2] Implement cancellation check loop in search operation (check `cancelled` flag every 50-100 results)
- [ ] T034 [US2] [P] Create worker thread management in `SearchService` class in `src/network/search_service.h`
- [ ] T035 [US2] Implement search signal emissions: `progressSignal(partial_results)` and `completedSignal(all_results)`

### Tasks - Async Refactoring (Parallel)

- [ ] T036 [US2] [P] Refactor `SearchService::search()` to spawn worker thread (caller gets AsyncSearchOperation handle)
- [ ] T037 [US2] [P] Modify `SearchPage::onSearchClicked()` to handle async search (show SearchingPage, start operation)
- [ ] T038 [US2] [P] Implement `SearchPage::onSearchProgress()` to update results incrementally during search
- [ ] T039 [US2] [P] Implement `SearchPage::onSearchCompleted()` to hide SearchingPage and display final results
- [ ] T040 [US2] Implement rapid navigation cancellation: emit `setSearchCancelled(true)` when leaving SearchPage
- [ ] T041 [US2] [P] Remove blocking modal dialogs: delete any `QMessageBox::information()` calls during search

### Tasks - Testing

- [ ] T042 [US2] Create integration test `test/search_async_test.cpp` verifying non-blocking search behavior
- [ ] T043 [US2] Test rapid navigation cancellation: verify pending search stops when user navigates away
- [ ] T044 [US2] [P] Stress test: verify UI remains responsive with large result sets (1000+ items)
- [ ] T045 [US2] Test SearchingPage visibility transitions (appears on start, disappears on completion)

### Independent Test Criteria
✓ Search doesn't block UI (test with Windows responsiveness monitor)  
✓ SearchingPage displays during search  
✓ Navigation away cancels search immediately  
✓ Results display without dialogs  
✓ All tests passing

---

## Phase 3: User Story 3 - Platform Names (P1)

### Goal
Fix platform name display to show correct platform instead of "Unknown" using enum conversion and i18n.

**Acceptance Criteria**:
- ✓ Search results show correct platform name (e.g., "Bilibili")
- ✓ Added playlist items show correct platform (not "Unknown")
- ✓ Platform names are translatable (e.g., "Bilibili" → "B站")
- ✓ All platform enum values have corresponding display strings

### Tasks - Implementation

- [ ] T046 [US3] [P] Implement `platformTypeToString(PlatformType)` in `src/network/platform/platform_utils.cpp` with `tr()` wrapping
- [ ] T047 [US3] [P] Wrap platform enum values with `tr()`: `tr("Bilibili")`, `tr("YouTube")`, `tr("Unknown")`
- [ ] T048 [US3] Verify `BilibiliPlatform::search()` sets `SearchResult::platform` to correct enum value (not hardcoded string)
- [ ] T049 [US3] [P] Update `SearchResult` → `SongInfo` conversion to copy `platform` enum directly (in `PlaylistManager::addFromSearch()`)
- [ ] T050 [US3] Update all playlist UI components to use `platformTypeToString()` for display in `src/ui/page/playlist_page.cpp`
- [ ] T051 [US3] Add Chinese translations for platform names in `resource/lang/zh_CN.ts`

### Tasks - Testing

- [ ] T052 [US3] Create unit test `test/platform_display_test.cpp` verifying `platformTypeToString()` output
- [ ] T053 [US3] [P] Create integration test verifying search results show correct platform names
- [ ] T054 [US3] Test platform name translations in both English and Chinese locales
- [ ] T055 [US3] Verify no "Unknown" platform values appear in playlist (test with Bilibili search results)

### Independent Test Criteria
✓ Platform enum converts to translatable strings  
✓ Search results display correct platform  
✓ Added items preserve platform correctly  
✓ Platform names translate to Chinese  
✓ All tests passing

---

## Phase 3: User Story 4 - Playlist Navigation (P1)

### Goal
Fix playlist navigation after item deletion, respecting playback modes and regenerating random play order.

**Acceptance Criteria**:
- ✓ Deleting playing item triggers next-track playback or stop
- ✓ next() and last() navigation works after deletion
- ✓ Navigation respects all playback modes (NORMAL, SHUFFLE, REPEAT_ONE, REPEAT_ALL)
- ✓ Random play order regenerates after deletion in SHUFFLE mode
- ✓ No invalid index states persist after deletion

### Tasks - Deletion Handler (Parallel)

- [ ] T056 [US4] [P] Implement deletion handler in `PlaylistManager::deleteItem()` in `src/playlist/playlist_manager.cpp`
- [ ] T057 [US4] [P] Add "play next" signal emission when currently playing item deleted (C1 clarification)
- [ ] T058 [US4] [P] Add "stop" signal emission when deleting last item (C1 clarification)
- [ ] T059 [US4] [P] Validate and fix currentIndex after deletion (ensure -1 <= currentIndex < songs.size())

### Tasks - Random Mode Regeneration

- [ ] T060 [US4] Implement `PlaylistManager::regenerateRandomPlayOrder()` in `src/playlist/playlist_manager.cpp`
- [ ] T061 [US4] Call regeneration when item deleted in SHUFFLE mode (C4 clarification)
- [ ] T062 [US4] Verify random order includes all remaining items (no duplicates, no gaps)

### Tasks - Navigation Logic Fix (Parallel)

- [ ] T063 [US4] [P] Fix `PlaylistManager::next()` to handle all playback modes in `src/playlist/playlist_manager.cpp`
- [ ] T064 [US4] [P] Fix `PlaylistManager::last()` to handle all playback modes in `src/playlist/playlist_manager.cpp`
- [ ] T065 [US4] Document mode-specific navigation in comments (NORMAL: sequential, SHUFFLE: random order, REPEAT: wraparound)
- [ ] T066 [US4] [P] Validate navigation with edge cases (currentIndex at boundaries, empty playlist, single item)

### Tasks - Testing

- [ ] T067 [US4] Create integration test `test/playlist_navigation_test.cpp` with deletion scenarios
- [ ] T068 [US4] [P] Test navigation after deletion in all playback modes (NORMAL, SHUFFLE, REPEAT_ONE, REPEAT_ALL)
- [ ] T069 [US4] [P] Test edge case: delete currently playing item (verify next-track or stop signal)
- [ ] T070 [US4] [P] Test edge case: delete last item while playing (verify stop signal)
- [ ] T071 [US4] [P] Test rapid deletion: verify navigation remains consistent with multiple deletions
- [ ] T072 [US4] Test random order regeneration: verify shuffled list has all remaining songs (SHUFFLE mode)

### Independent Test Criteria
✓ Deleting playing item triggers correct signal  
✓ currentIndex always valid after deletion  
✓ next() and last() work in all modes  
✓ Random order regenerates correctly  
✓ Edge cases handled gracefully  
✓ All tests passing

---

## Phase 4: Cross-Story Integration & Polish

### Goal
Integrate all features, validate end-to-end workflows, and prepare for merge.

### Tasks

- [ ] T073 End-to-end test: Search → Add → Playlist display (all 4 stories working together)
- [ ] T074 Verify no memory leaks with valgrind/AddressSanitizer on all new code
- [ ] T075 Code review checklist: const-correctness, error handling, thread safety in all modifications
- [ ] T076 Update Copilot context file: `.github/copilot-instructions.md` with implementation notes
- [ ] T077 Create changelog entry: `docs/CHANGELOG.md` summarizing feature (localization, async search, platform fix, navigation fix)
- [ ] T078 Build and test in Release mode (verify no debug-only issues)
- [ ] T079 Final integration test: Run full test suite (`cmake --build . --target test`)

### Independent Test Criteria
✓ All 4 user stories working together  
✓ No memory leaks detected  
✓ Code quality checks passed  
✓ Full test suite passing  
✓ Documentation updated

---

## Implementation Strategy

### MVP Scope (Minimum Viable Product)
**Deploy after Phase 3 completion with any one user story working end-to-end:**

**Option A** (Recommended): User Story 1 (Localization)
- Highest value: enables all future translations
- Lowest risk: isolated string changes
- Fast iteration: uses existing Qt infrastructure
- Estimated effort: 5-7 developer days

**Option B**: User Story 2 (Non-Blocking Search)
- High user impact: removes UI freezing
- Medium complexity: thread management required
- Estimated effort: 4-6 developer days

**Option C**: User Story 3 (Platform Names) OR User Story 4 (Navigation)
- Lower priority: bug fixes rather than new features
- Lower effort: 2-3 developer days each
- Can be batched with others

### Incremental Delivery

1. **First iteration**: US1 (Localization) + US3 (Platform Names) = 8-10 days
   - Enables translation workflow and fixes platform display
   - Lower risk changes
   
2. **Second iteration**: US2 (Non-Blocking Search) = 4-6 days
   - Major UX improvement
   - More complex threading changes
   
3. **Final iteration**: US4 (Navigation Fixes) + Integration = 4-5 days
   - Completes feature set
   - Validates all interactions

### Parallelization Opportunities

**Can develop simultaneously** (after foundational phase completes):
- T017-T025 (US1 localization tasks - split by UI component)
- T030-T045 (US2 async search tasks - infrastructure parallel with refactoring)
- T046-T055 (US3 platform names - helper function + integration)
- T056-T072 (US4 navigation - deletion handler parallel with navigation logic)

**Example parallel schedule** (4 developers, 10 days):
- Dev 1: US1 Localization (T017-T029) → 5-6 days
- Dev 2: US2 Async Search (T030-T045) → 5-6 days  
- Dev 3: US3 Platform Names (T046-T055) → 3-4 days → then assist Dev 4
- Dev 4: US4 Navigation (T056-T072) → 5-6 days
- Combined: T073-T079 (Integration & Polish) → 2 days

---

## Task Dependencies

```
T001-T008 (Setup)
    ↓
T009-T016 (Foundational)
    ├─ [T017-T029] US1 Localization (independent)
    ├─ [T030-T045] US2 Async Search (independent)
    ├─ [T046-T055] US3 Platform Names (independent)
    └─ [T056-T072] US4 Navigation (independent)
    
    ↓ (all must complete)
    
T073-T079 (Integration & Polish)
```

---

## Task Count Summary

| Phase | Category | Count | Notes |
|-------|----------|-------|-------|
| 1 | Setup | 8 | Infrastructure, auditing, build configuration |
| 2 | Foundational | 8 | Common helpers, test fixtures, refactoring prep |
| 3a | US1 Localization | 13 | String wrapping, extraction, translation, testing |
| 3b | US2 Async Search | 16 | SearchingPage, worker thread, async refactoring, testing |
| 3c | US3 Platform Names | 10 | Helper functions, enum conversion, UI updates, testing |
| 3d | US4 Navigation | 17 | Deletion handler, random regeneration, navigation logic, testing |
| 4 | Integration & Polish | 7 | E2E testing, code review, documentation, build validation |
| **TOTAL** | | **79** | **12 major milestones** |

---

## Quality Gate: Before Merge to Main

- [ ] All 79 tasks completed and verified
- [ ] Test suite: >80% code coverage on modified files
- [ ] No compiler warnings (treat as errors)
- [ ] No memory leaks (valgrind clean)
- [ ] Code style consistent (clang-format)
- [ ] Commit history clean (squash feature branch)
- [ ] All 4 user stories validated with acceptance criteria
- [ ] Documentation updated (README, CHANGELOG, inline comments)
- [ ] PR review approved by 2+ maintainers
- [ ] CI/CD pipeline passing all checks

---

## Notes for Implementation Team

### Key Patterns

**Localization (US1)**:
```cpp
// Every user-visible string must use tr()
label->setText(tr("Search Results"));  // Qt extracts for translation
```

**Async Search (US2)**:
```cpp
// Use std::atomic<bool> for thread-safe cancellation
auto op = std::make_shared<AsyncSearchOperation>(query);
op->cancelled = false;  // Safe to check from any thread
```

**Platform Display (US3)**:
```cpp
// Convert enum to translatable string
QString display = platformTypeToString(song.platform);  // Returns tr("Bilibili"), etc.
```

**Navigation Fix (US4)**:
```cpp
// Always validate currentIndex after deletion
if (currentIndex >= songs.size()) currentIndex = songs.size() - 1;
if (playbackMode == SHUFFLE) regenerateRandomPlayOrder();
```

### Testing Recommendations

- **Unit tests**: Quick validation of isolated functions
- **Integration tests**: Full workflows (search → add → display → delete → navigate)
- **Manual testing**: UI responsiveness during search, platform names visible, navigation smooth
- **Regression testing**: Existing playlist functionality still works (add songs, play, pause, etc.)

### Common Pitfalls to Avoid

1. **Localization**: Don't translate enum names or technical terms embedded in code
2. **Async Search**: Always check `cancelled` flag in loops; use proper signal/slot for UI updates
3. **Platform Display**: Remember SongInfo stores `int` enum, not string name
4. **Navigation**: currentIndex must ALWAYS remain valid: -1 or [0, songs.size())

### Git Workflow

```bash
# Work on feature branch
git checkout 004-ui-fixes-localization

# Create task branches for parallel work (optional)
git checkout -b task/T017-search-strings
git checkout -b task/T030-searching-page

# Commit frequently with clear messages
git commit -m "feat(us1): wrap Search UI strings with tr() [T017]"

# Squash and merge to feature branch when done
git rebase -i origin/004-ui-fixes-localization
git merge --squash task/T017-search-strings
```
