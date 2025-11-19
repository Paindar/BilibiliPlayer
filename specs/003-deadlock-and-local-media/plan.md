# Implementation Plan: Spec 003 - Deadlock Analysis & Local Media Playlist

**Branch**: `spec/003-deadlock-local-media` | **Date**: 2025-11-17 | **Spec**: `specs/003-deadlock-and-local-media/spec.md`
**Input**: Feature specification from `/specs/003-deadlock-and-local-media/spec.md`

## Summary

Two parallel goals:
- Detect and remediate use-before-unlock and deadlock-prone patterns across the codebase (signals emitted while holding locks, double-locks, holding locks across blocking calls, etc.). Deliver targeted fixes and unit tests where possible.
- Add backend support for adding local media to playlists. Decision: store `file://` absolute references to original files (no copy). Add `playlist_local_add_test.cpp` to assert behavior and persistence.

Additionally, add a UI i18n verification task: scan UI code and .ui files for hard-coded display strings that are not translated or exposed to Qt translation system (e.g., missing `tr()` usage or untranslated .ui text).

## Technical Context

**Language/Version**: C++17 (project default)
**Primary Dependencies**: Qt6 (Core, Widgets, Test), FFmpeg, Catch2 (tests), JsonCpp, spdlog, fmt
**Storage**: filesystem for workspace and JSON files for playlists
**Testing**: Catch2 unit tests; CI executes `BilibiliPlayerTest` via CTest
**Target Platform**: Windows (primary dev), cross-platform supported via CMake
**Performance Goals**: No regression to test runtime; fixes must avoid introducing blocking on UI thread
**Constraints**: Keep fixes small-scope and local; prefer unlocking-before-emit and copy-then-emit patterns; do not rearchitect threading model in this phase

## Constitution Check

- Must avoid changes that break public APIs or tests without accompanying test updates.
- All changes require unit tests or a clear rationale documented in the spec.

## Project Structure Decisions

Keep code changes targeted to existing modules (`src/playlist`, `src/stream`, `src/network`, `src/event`). New test lives in `test/playlist_local_add_test.cpp` (already added).

## Phase 0: Research (T100)

Goals:
- Static heuristics to find candidate problematic patterns
- Source locations for UI text and translation usage

Tasks:
- T100.1: Grep for `emit` and identify occurrences surrounded by `QReadLocker` / `QWriteLocker` or explicit mutex locking. Record file/line and short snippet.
- T100.2: Grep for `QMutexLocker`, `std::lock_guard`, `std::unique_lock`, `mutex.lock\(` patterns; flag occurrences followed by blocking calls (`sleep_for`, file I/O, network calls, heavy processing).
- T100.3: Grep for double-lock idioms and recursion on same mutex (search for `.lock()` within functions that may hold locks). Flag suspicious patterns.
- T100.4: UI i18n scan: search for hard-coded UI strings in `.cpp`, `.h`, and `.ui` files; detect places not wrapped in `tr()` or not present in Qt `.ts` files (`resource/lang/*.ts`). Produce a list of candidates and severity (high/medium/low).
- T100.5: Produce `research.md` summarizing findings, with candidate fixes and estimated effort per fix.

Deliverable: `specs/003-deadlock-and-local-media/research.md`

## Phase 1: Design & Contracts

Prerequisite: `research.md` complete and reviewed.

Tasks:
- T101.1: For each flagged `emit`-while-locked occurrence, choose a remediation strategy: unlock before emit, copy necessary data then emit, or document and leave as-is if safe.
- T101.2: For blocking calls under locks, shrink lock scope or move blocking call out of lock. Add unit tests that assert no deadlock in a stress test with timeouts.
- T101.3: For double-lock and recursive lock patterns, refactor to a single owner or use `QRecursiveMutex` only if justified; document rationale.
- T101.4: Implement UI i18n fixes: for each hard-coded string flagged, either wrap with `tr()` in code, or mark in .ui for translation, and update `.ts` files if present. Add a test that scans compiled UI strings (or static check) to ensure no remaining high-severity hard-coded strings.
- T101.5: Implement `PlaylistManager` change to accept `file://` references: if incoming `SongInfo.filepath` is a `file://` or local path, preserve as-is and save to JSON; do not call `generateStreamingFilepath` for local files.

Outputs:
- Patch PRs per fix (small, focused)
- Updated unit tests
- `data-model.md` only if data model changes (not expected)

## Phase 2: Implementation & Tests

Tasks:
- T102.1: Apply the `unlock-before-emit` or `copy-and-emit` changes, add tests.
- T102.2: Shrink lock scopes around blocking calls; add stress tests with timeouts to detect hangs.
- T102.3: Modify `PlaylistManager::addSongToPlaylist` handling of local files; ensure `playlist_local_add_test.cpp` passes.
- T102.4: Implement UI i18n fixes: add `tr()` where needed, mark .ui fields for translation, update `specs/003-deadlock-and-local-media/quickstart.md` with how to run `lupdate`/`lrelease` and check missing translations.

## Phase 3: Validation

- T104: Run full test suite, ensure all tests pass.
- T105: Finalize `specs/003-deadlock-and-local-media/research.md` and `specs/003-deadlock-and-local-media/tasks.md` with links to PRs.


## UI i18n scan specifics (requested addition)

Add a targeted subtask in Phase 0 and Phase 1:
- Phase 0: Static scan that finds strings in code and .ui that are not wrapped in `tr()` or missing in `.ts` files. Produce `i18n-scan.csv` with columns: file, line, text, severity.
- Phase 1: For each high severity item: update code to use `tr()` (e.g., change `label->setText("Hello")` to `label->setText(tr("Hello"))`) OR update `.ui` translation properties so Qt's tools pick them up. Add a unit-check that runs `lupdate` and compares extracted strings vs. existing `.ts` files; fail if new high-severity strings exist.


## Next steps (immediate)

1. Run Phase 0 greps and produce `research.md` and `i18n-scan.csv` (T100). I can run these now if you confirm.
2. After review of `research.md`, iterate on Phase 1 tasks.


---
