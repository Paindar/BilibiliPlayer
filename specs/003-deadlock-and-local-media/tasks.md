# Tasks: Spec 003 - Deadlock Analysis & Local Media Playlist (Generated)

Owner: TBD

Phase 1 — Setup

- [X] T001 Create research template `specs/003-deadlock-and-local-media/research.md`
- [X] T002 Add scan helper script `specs/003-deadlock-and-local-media/scripts/run-scans.ps1` (grep-based emit/lock/i18n scans)
- [X] T003 Add CI/test target entry (if missing) in `test/CMakeLists.txt` to ensure `test/playlist_local_add_test.cpp` is built (file: `test/CMakeLists.txt`)

Phase 2 — Foundational (blocking prerequisites)

- [X] T004 [P] Create outputs directory `specs/003-deadlock-and-local-media/outputs/` and placeholder `i18n-scan.csv` (path: `specs/003-deadlock-and-local-media/outputs/i18n-scan.csv`)
- [X] T005 [P] Create `emit-locks.csv` placeholder (path: `specs/003-deadlock-and-local-media/outputs/emit-locks.csv`)
- [X] T006 [P] Create `lock-block.csv` placeholder (path: `specs/003-deadlock-and-local-media/outputs/lock-block.csv`)

Phase 3 — User Story US1: Deadlock analysis & minimal fixes (Priority: P1)

- [X] T007 [US1] Run static heuristics scan and write initial findings to `specs/003-deadlock-and-local-media/research.md` (script: `specs/003-deadlock-and-local-media/scripts/run-scans.ps1`)
- [X] T008 [US1] Produce `specs/003-deadlock-and-local-media/outputs/emit-locks.csv` with columns (file,line,context,severity) (file: `specs/003-deadlock-and-local-media/outputs/emit-locks.csv`)
- [X] T009 [US1] Triage findings and add classification (Safe / Needs fix / Intentional) to `specs/003-deadlock-and-local-media/research.md` (file: `specs/003-deadlock-and-local-media/research.md`)
- [X] T010 [US1] Create explicit PR tasks for each high-severity finding and list them in `specs/003-deadlock-and-local-media/research.md` (file examples: `src/audio/ffmpeg_decoder.cpp`, `src/audio/audio_player_controller_callback.cpp`)
- [ ] T011 [US1] Implement unlock-before-emit / queued-emission fix for `src/audio/ffmpeg_decoder.cpp` and add unit test under `test/` (files: `src/audio/ffmpeg_decoder.cpp`, `test/diagnostics/ffmpeg_decoder_unlock_emit_test.cpp`)
- [ ] T012 [US1] Implement unlock-before-emit fix for `src/audio/audio_player_controller_callback.cpp` (file: `src/audio/audio_player_controller_callback.cpp`)
- [ ] T013 [US1] Add regression tests reproducing previously observed issues under `test/diagnostics/` (directory: `test/diagnostics/`)

Phase 4 — User Story US2: Add Local Media to Playlist (Priority: P1)

- [X] T014 [US2] Update `src/playlist/playlist_manager.cpp` to mark local items: set `SongInfo.uploader = "local"`, `SongInfo.platform = Platform::Local`, and persist initial entry (file: `src/playlist/playlist_manager.cpp`)
- [X] T015 [US2] Implement FFmpeg probe API `src/audio/ffmpeg_probe.h` / `src/audio/ffmpeg_probe.cpp` with fallback strategy (metadata → FFmpeg probe → INT_MAX) (files: `src/audio/ffmpeg_probe.h`, `src/audio/ffmpeg_probe.cpp`)
- [X] T016 [US2] Integrate async duration probe into `PlaylistManager::addSongToPlaylist` so duration populates after add (file: `src/playlist/playlist_manager.cpp`)
- [X] T017 [US2] Implement TagLib-based cover extractor `src/audio/taglib_cover.h` / `src/audio/taglib_cover.cpp` returning raw image bytes and integrate FFmpeg fallback (files: `src/audio/taglib_cover.h`, `src/audio/taglib_cover.cpp`)
- [X] T018 [US2] Implement cover cache utilities `src/util/cover_cache.h` / `src/util/cover_cache.cpp` using `ConfigManager`'s cache path and `util::md5Hash` keying (files: `src/util/cover_cache.h`, `src/util/cover_cache.cpp`)
- [X] T019 [US2] Integrate cover extraction flow: on add, persist initial SongInfo, run cover extractor async, save cache file, update `SongInfo.coverPath`, persist update, and emit queued update (files: `src/playlist/playlist_manager.cpp`, `src/util/cover_cache.cpp`)
- [X] T020 [US2] Add unit tests: `test/playlist_local_add_test.cpp` (existing) and new `test/playlist_local_duration_test.cpp` and `test/playlist_local_cover_test.cpp` (files: `test/playlist_local_duration_test.cpp`, `test/playlist_local_cover_test.cpp`)
- [X] T021 [US2] Update `test/CMakeLists.txt` to include the new tests (file: `test/CMakeLists.txt`)

Phase 5 — Investigate periodic noise in local playback (US2 secondary)

- [X] T022 [US2] Create diagnostics reproducer script `tools/diagnostics/local-audio-reproducer.ps1` (script: `tools/diagnostics/local-audio-reproducer.ps1`)
- [ ] T023 [US2] Instrument audio playback logs and route verbose FFmpeg logs into `log/` (files: `src/audio/ffmpeg_decoder.cpp`, `src/audio/ffmpeg_log.cpp`)
- [X] T024 [US2] Add playback comparison test `test/diagnostics/sine_wave_playback_test.cpp` that compares PCM output against expected waveform (file: `test/diagnostics/sine_wave_playback_test.cpp`)
- [X] T025 [US2] Analyze results and document mitigation in `specs/003-deadlock-and-local-media/research.md` (file: `specs/003-deadlock-and-local-media/research.md`)

Final Phase — Polish & Cross-cutting

- [X] T026 Ensure all modified files have unit tests and add links to PRs in `specs/003-deadlock-and-local-media/research.md` (file: `specs/003-deadlock-and-local-media/research.md`)
- [X] T027 Update `specs/003-deadlock-and-local-media/spec.md` and add `quickstart.md` with test instructions (files: `specs/003-deadlock-and-local-media/spec.md`, `specs/003-deadlock-and-local-media/quickstart.md`)
- [ ] T028 Run full test suite and confirm no regressions: `./build/debug/test/BilibiliPlayerTest.exe` (command to run: `build/debug/test/BilibiliPlayerTest.exe`) (file: `build/debug/test/BilibiliPlayerTest.exe`)

Dependencies & Execution Order

- T001 → T002,T003 → T004..T006 → T007 → T009 → T010 → T011/T012/T013 → T014 → T015 → T016 → T017 → T018 → T019 → T020 → T021 → T022..T025 → T026 → T027 → T028

Parallel Opportunities

- [P] T004, T005, T006 (output file prep) can run in parallel
- [P] T011 and T012 (per-file fixes) can be worked on in parallel if disjoint
- [P] T015 (FFmpeg probe) and T017 (TagLib cover extractor) can be developed in parallel with T014 (PlaylistManager plumbing)

Counts & Summary

- Total tasks: 28
- Tasks for Deadlock analysis (US1): T007–T013 (7 tasks)
- Tasks for Local media (US2): T014–T025 (12 tasks)
- Parallelizable tasks: T004,T005,T006,T011,T012,T015,T017

Suggested MVP

- Implement T001–T009 (analysis + triage) and T014–T016 (basic add-local flow + async duration probe). This covers discovery plus the core feature.

Validation

- All tasks use the required checklist format with TaskIDs and file paths.
