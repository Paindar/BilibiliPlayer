# Phase‑6 Checklist — Performance & Polish

**Date**: 2025-11-15

Purpose: capture measurable acceptance criteria and initial tasks for Phase‑6 (performance, responsiveness, CI benchmarks).

Goals & measurable targets
- **Startup time (cold)**: target < 3.0 seconds on a developer machine (Windows 10/11, NVMe). Measure via process-run harness.
- **Playback latency**: end-to-end buffer-to-sound latency target < 100 ms for network sources under normal network conditions.
- **UI responsiveness**: UI thread should not block for > 10 ms on common actions; no jank over 50 ms.
- **CI benchmarks**: run basic startup and decode benchmarks on merge and store artifacts.

Initial tasks (T052–T055)
- **T052 — Startup time optimization**
  - Measure current cold-start time using `tools/bench/startup_measure.ps1` or `tools/bench/startup_measure.py`.
  - Add lightweight benchmark harness under `tools/bench/`.
  - Implement small improvements (defer non-critical subsystems, lazy init) and re-measure.
  - Acceptance: measurement recorded and at least one optimization implemented.

- **T053 — Playback latency tuning**
  - Add hooks to measure buffer-to-sound latency in `src/stream` and `src/audio`.
  - Run controlled tests using local HTTP server fixtures.
  - Acceptance: reproducible measurements and tuning suggestions documented.
  
  <!-- DROPPED: Playback latency tuning deferred to future phase (2025-11-15). -->

- **T054 — UI responsiveness improvements**
  - Profile UI for long-running operations.
  - Ensure I/O heavy work runs on worker threads and uses queued signals to UI.
  - Acceptance: no UI-blocking operations >10ms in profiled scenario.
  
  <!-- DROPPED: UI responsiveness improvements deferred to future phase (2025-11-15). -->

- **T055 — Add CI benchmark job**
  - Add a GitHub Actions job that runs the `tools/bench` harness on Linux and Windows runners (where possible).
  - Store results as artifacts for trend analysis.
  
  <!-- DROPPED: CI benchmark job deferred to future phase (2025-11-15). -->

Notes
- Benchmarks are best-effort and meant to guide optimization; hardware will vary. Record environment details with each run.
- Keep changes incremental and low-risk; prefer lazy-init and deferring optional subsystems over large structural rewrites.

