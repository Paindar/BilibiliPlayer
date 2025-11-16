# Benchmark Tools

This directory contains simple, low-risk benchmarking helpers used during Phase‑6 work.

Scripts:
- `startup_measure.ps1` — PowerShell script to measure cold-start time by launching the built executable and recording elapsed time. Use as a starting point to collect measurements locally.

Usage (PowerShell):

```powershell
# from workspace root
.\tools\bench\startup_measure.ps1 -BuildType debug -ExePath .\build\debug\src\BilibiliPlayer.exe -RunSeconds 3
```

Notes:
- The script is intentionally lightweight and non-invasive; it currently measures process spawn time and basic startup activity.
- For more accurate "app ready" timing you can extend the harness later to wait for a readiness log message or an IPC signal, but that is not required by the current Phase‑6 bench plan.
- For CI integration, prefer headless tests or a special `--bench` command-line switch in the app. If we add such a switch, update the harness to wait for a readiness message.
