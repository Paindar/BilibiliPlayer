PR Summary: Network module refactor — 001-unified-stream-player

This file is a lightweight, non-ignored summary for the refactor branch so it can be committed and included in a PR without adding ignored spec files.

Summary of changes (high level):
- Moved Bilibili platform implementation into `src/network/platform/` and introduced a canonical platform interface (`i_platform.h`).
- NetworkManager and BilibiliPlatform use shared ownership (`std::shared_ptr`) and `enable_shared_from_this` for safe async lifetime handling.
- Added `exitFlag_` atomics to signal shutdown to background workers.
- Converted async lambda captures to `self = shared_from_this()` and replaced raw `this` usage with `self->...` to avoid use-after-free.
- Updated unit tests and quickstart docs locally (these files are under `specs/` and are intentionally ignored by the repo `.gitignore`).
- Deferred T026 (YouTube client) for a future iteration.

Notes for reviewers:
- The full doc/checklist edits live under `specs/001-unified-stream-player/` and a PR draft exists at `.github/PR_DRAFTS/001-unified-stream-player-refactor.md` (these are currently ignored by .gitignore).
- This summary provides a non-ignored artifact to include in a PR if desired.

How to review locally:
1. Checkout the branch created for this PR (will be pushed to origin by automation).
2. Inspect `PR_SUMMARY_001-unified-stream-player.md` and the code changes in the feature branch (or review the spec/checklist files in the repo if you temporarily disable .gitignore for review).

Signed-off-by: automated refactor agent
