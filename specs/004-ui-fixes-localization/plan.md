# Implementation Plan: UI Fixes, Localization, and Playlist Navigation

**Branch**: `004-ui-fixes-localization` | **Date**: 2025-11-19 | **Spec**: `specs/004-ui-fixes-localization/spec.md`
**Input**: Feature specification from `/specs/004-ui-fixes-localization/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

This feature addresses four interconnected UI/functionality issues in the BilibiliPlayer desktop application:

1. **Replace hardcoded UI strings with `tr()` localization calls** to enable multi-language support and eliminate technical debt
2. **Remove blocking message boxes during search** to prevent UI freeze and maintain responsiveness
3. **Fix "Unknown" platform name display** for audio items added from search results
4. **Fix playlist navigation (last/next buttons)** after deleting items and during mode changes

The implementation leverages the existing Qt framework's localization system, async search capabilities, and requires debugging the playlist navigation logic to handle all deletion and mode-change scenarios correctly.

## Technical Context

**Language/Version**: C++17 (C++20 features with verified compiler support)
**Primary Dependencies**: Qt 6.8.0, FFmpeg 7.1.1, cpp-httplib 0.15.3, jsoncpp 1.9.6, Catch2 3.11.0
**Storage**: Configuration via JSON files + SQLite (planned for Phase 2)
**Testing**: Catch2 3.11.0 for unit/integration tests
**Target Platform**: Windows desktop (Qt 6.8.0 with MinGW 13.1.0)
**Project Type**: Single desktop application with modular architecture
**Performance Goals**: UI responsiveness (no freezing during search), search completion within reasonable time
**Constraints**: Must maintain backward compatibility with existing playlist data, thread-safe operations for async search
**Scale/Scope**: ~50 source files, multi-module architecture (network, ui, audio, playlist, config)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Principle I: Code Quality & Maintainability

✅ **PASS**: Feature work maintains RAII patterns (smart pointers, Qt ownership). No new raw pointers or manual memory management required. Const correctness and naming conventions align with project standards (PascalCase classes, camelCase methods, m_snake_case_ members).

✅ **PASS**: All UI text must use localization system via `tr()` calls - enforces const correctness and proper error handling. No magic numbers or inconsistent naming.

### Principle II: Comprehensive Testing Standards

✅ **PASS**: Feature includes acceptance scenarios for all 4 user stories covering unit (string replacement), integration (search async operation), and edge cases (deletion states, mode changes). Tests will follow Catch2 framework already in place.

### Principle III: User Experience Consistency

✅ **PASS**: Feature directly addresses UX issues (blocking dialogs, incorrect platform names, broken navigation). All changes improve user experience without degrading existing functionality.

### Principle IV: Performance & Resource Management

✅ **PASS**: Async search prevents UI thread blocking, improving responsiveness. No memory leaks expected - Qt object model handles cleanup via parent-child relationships.

### Principle V: Thread Safety & Concurrency

✅ **PASS**: Async search requires thread-safe operations. Qt's signal/slot mechanism ensures thread-safe UI updates. Playlist navigation must handle concurrent deletion scenarios safely.

**Constitution Check Result**: ✅ **PASS** - All principles aligned. Feature is compliant.

## Project Structure

### Documentation (this feature)

```text
specs/004-ui-fixes-localization/
├── spec.md              # Feature specification
├── plan.md              # This file (implementation plan)
├── research.md          # Phase 0 output (research findings)
├── data-model.md        # Phase 1 output (entity definitions)
├── quickstart.md        # Phase 1 output (dev setup)
├── contracts/           # Phase 1 output (API contracts)
│   ├── localization-contract.md
│   ├── search-async-contract.md
│   ├── platform-metadata-contract.md
│   └── playlist-navigation-contract.md
├── checklists/
│   └── requirements.md   # Quality checklist
└── tasks.md             # Phase 2 output (task breakdown - NOT created by /speckit.plan)
```

### Source Code (repository root)

**Selected Structure**: Single C++ desktop application with modular architecture (Option 1)

```text
src/
├── ui/
│   ├── pages/
│   │   ├── search_page.h/cpp        # Contains hardcoded strings, blocking dialogs
│   │   └── playlist_page.h/cpp       # Contains hardcoded strings
│   ├── dialogs/                     # All dialogs with localization
│   ├── widgets/                     # Widget strings needing localization
│   ├── i18n/                        # Localization helpers (may be new)
│   └── CMakeLists.txt
├── network/
│   ├── platform/
│   │   ├── i_platform.h             # Platform interface
│   │   ├── bili_network_interface.h/cpp  # Search implementation with platform info
│   │   └── CMakeLists.txt
│   └── CMakeLists.txt
├── playlist/
│   ├── playlist_manager.h/cpp       # Navigation logic (last/next buttons)
│   ├── audio_item.h/cpp             # Platform name storage
│   └── CMakeLists.txt
├── config/
│   ├── config_manager.h/cpp
│   └── CMakeLists.txt
└── CMakeLists.txt

test/
├── ui_localization_test.cpp         # NEW: String replacement tests
├── search_async_test.cpp            # NEW: Non-blocking search tests
├── playlist_navigation_test.cpp      # NEW/UPDATED: Navigation after deletion
├── CMakeLists.txt
└── util/

resource/
├── lang/                            # Existing translation files
│   ├── en_US.ts
│   └── zh_CN.ts
├── themes/
└── wav/
```

**Structure Decision**: Single desktop application structure is appropriate. No web/mobile components. Feature changes are localized to: UI text strings, search async handling, playlist navigation, and platform metadata in audio items. No new major modules required.

## Complexity Tracking

**No violations**: Feature design aligns with all Constitution principles. No complexity exceptions needed.
