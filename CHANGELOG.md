# Changelog

All notable changes to **TrimFlow** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased] — v1.1.0 Milestone (Narrowly Scoped)

`v1.1.0` is intentionally scoped to **diagnostics, observability, and zero-friction installation** without bloating the core memory manager:

- **Startup & Process Diagnostics (`--diagnose`)**: Detailed tree dump showing each discovered process group, detected command-line role (`BrowserMain`, `GpuProcess`, `AudioService`, `Renderer`, `Utility`), window visibility state, and exact reason a PID was trimmed or protected.
- **Machine-Readable Output (`--json`)**: JSON output for `--status` and `--once` so scripts, Rainmeter skins, and benchmark harnesses can consume TrimFlow metrics.
- **Configuration Validation (`--validate-config` / `trimflow.ini`)**: Optional lightweight INI config for custom exclusion lists, hysteresis thresholds, and polling intervals with strict validation warnings.
- **Package Manager Automation**: Automated Scoop & WinGet manifest hash bump workflow on tagged releases.

---

## [1.0.0] - 2026-11-18

### Added
- **Dynamic Session Multi-Process Tree Discovery**: Automatically discovers any multi-process desktop app or >150 MB application group in the current interactive user session without hardcoded app lists.
- **Command-Line Subprocess Role Classification (`NtQueryInformationProcess`)**: Parses remote PEB command lines in-memory to distinguish `--type=gpu-process`, `audio.mojom.audioservice`, `network.mojom.networkservice`, `--type=renderer`, `--type=utility`, and main UI hosts.
- **Foreground Window Protection & Instant Promotion (`SetWinEventHook`)**: Hooks `EVENT_SYSTEM_FOREGROUND` and `EVENT_SYSTEM_MINIMIZESTART` out-of-context to promote newly focused apps to normal QoS in `< 1 ms` and schedule a gentle 1.5s delayed sweep on minimize.
- **Smart `+8 MB` Growth Hysteresis**: Records post-trim working set baselines per PID and skips re-trimming dormant processes until they allocate at least `+8 MB` of new dirty pages.
- **Zero-Flicker Custom Vector Tray UI & Status Toast**: Launches with a non-intrusive dark status card above the system tray showing live memory saved and provides a live tray dashboard (`Pause`, `Run Smart Sweep Now`, `Run at Windows Login`, `Exit`).
- **Headless CLI Modes**: `--once`, `--status`, `--quiet`, and `--quit`.
- **Testable Heuristic Engine**: Extracted pure classification and hysteresis decision logic into `src/heuristics.h` with 50+ deterministic unit & scenario tests in `tests/heuristics_test.cpp`.
