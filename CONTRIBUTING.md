# Contributing to TrimFlow

Thank you for your interest in contributing to **TrimFlow**!

You do **not** need to understand the entire Win32 process-management codebase to contribute. The classification rules, hysteresis thresholds, and protection policies are isolated in a pure header ([`src/heuristics.h`](src/heuristics.h)) backed by fast, deterministic scenario tests ([`tests/heuristics_test.cpp`](tests/heuristics_test.cpp)) that compile and run in under 2 seconds.

---

## 1. Windows Setup (2 Minutes)

TrimFlow uses **zero external dependencies**—only standard `C++17` and the Windows SDK.

### Option A: MinGW-w64 / MSYS2 (Recommended)
```powershell
winget install MSYS2.MSYS2
# Ensure C:\msys64\ucrt64\bin is on your PATH, or install mingw-w64-ucrt-x86_64-gcc
g++ --version
```

### Option B: Visual Studio 2022 (`cl.exe`)
Open the **x64 Native Tools Command Prompt for VS 2022**—`build.bat` and `test.bat` work with MinGW out of the box, or compile directly with `cl /std:c++17 /O2 /EHsc /Isrc tests\heuristics_test.cpp`.

---

## 2. How to Contribute (4-Step Workflow)

```
┌──────────────────────────┐     ┌──────────────────────────────┐     ┌──────────────┐     ┌───────────────┐
│ 1. Edit src/heuristics.h │ ──► │ 2. Add scenario test in      │ ──► │ 3. Run       │ ──► │ 4. Run        │
│    or src/main.cpp       │     │    tests/heuristics_test.cpp │     │    test.bat  │     │    build.bat  │
└──────────────────────────┘     └──────────────────────────────┘     └──────────────┘     └───────────────┘
```

1. **Fork & Clone:**
   ```powershell
   git clone https://github.com/vncwr/TrimFlow.git
   cd TrimFlow
   ```
2. **Run the Unit & Scenario Test Suite:**
   ```powershell
   .\test.bat
   ```
3. **Add or Update a Testable Benchmark Scenario:**
   Instead of manually launching 15 desktop apps to test a heuristic tweak, add a table row to `TestBenchmarkScenarios()` in [`tests/heuristics_test.cpp`](tests/heuristics_test.cpp):
   ```cpp
   // Example: Verify that a new subprocess command-line stays hot or gets trimmed
   SubprocRole role = ClassifyCommandLine(
       L"\"Slack.exe\" --type=renderer --disable-features=NetworkServiceSandbox", 9);
   EXPECT_EQ(role, SubprocRole::RendererOrWorker);
   EXPECT_FALSE(ShouldKeepSubprocessHot(role, /*isForegroundApp=*/false, /*isCpuActive=*/false, /*isPrimaryRenderer=*/false));
   ```
4. **Build & Verify Live Footprint:**
   ```powershell
   .\build.bat
   .\TrimFlow.exe --status
   ```

---

## 3. Expected Behavior Matrix

Every change must preserve the following invariants:

| Process Category | Command-Line / Signature | App in Foreground (`isForegroundApp = true`) | App Covered / Minimized (`isForegroundApp = false`) |
| :--- | :--- | :--- | :--- |
| **OS Kernel / Anti-Cheat** | `csrss.exe`, `dwm.exe`, `audiodg.exe`, `vgc.exe`, `EasyAntiCheat.exe` | **Never touched** (`IsSystemProtectedProcess`) | **Never touched** (`IsSystemProtectedProcess`) |
| **Audio & Network Services** | `audio.mojom.AudioService`, `network.mojom.NetworkService` | **Protected** (Normal QoS, never trimmed) | **Protected** (Normal QoS, never trimmed so background audio/calls never drop) |
| **Main UI Host & GPU Compositor** | No `--type=` flag, or `--type=gpu-process` | **Protected** (Zero UI or scrolling stutter) | **Trimmed + EcoQoS** (Promoted back in `< 1 ms` via `SetWinEventHook` on focus) |
| **Primary Active Renderer Tab** | `--type=renderer` with highest CPU delta in foreground app | **Protected** | **Trimmed + EcoQoS** |
| **Idle Background Renderers / Workers** | `--type=renderer`, `--type=utility`, `-contentproc` | **Trimmed + EcoQoS** (with `+8 MB` hysteresis) | **Trimmed + EcoQoS** (with `+8 MB` hysteresis) |

---

## 4. Narrow `v1.1` Scope & Beginner-Friendly Issues

We keep `v1.1` intentionally narrow—focused on **diagnostics, configuration validation, UI polish, and testable app scenarios** while keeping `TrimFlow.exe` under `200 KB` Private Working Set:

1. **Startup & Process Diagnostics (`--diagnose`)**: Print a formatted table of every discovered session process, its classified `SubprocRole`, foreground/background status, and why it was protected or trimmed.
2. **JSON Output Mode (`--json`)**: Emit structured JSON for `--status` and `--once` for scripting and benchmark automation.
3. **Configuration File & Validation (`trimflow.ini` / `--validate-config`)**: Load optional user exclusions and hysteresis thresholds from `trimflow.ini` next to the executable.
4. **Scenario Tests for Additional App Families**: Add table-driven tests in `tests/heuristics_test.cpp` for Firefox (`-contentproc`), WebView2 (`msedgewebview2.exe`), Slack, Discord, and JetBrains IDEs.
5. **Tray & Toast UI Polish**: Respect Windows system light/dark theme in the owner-drawn tray menu and bottom-right toast banner.
6. **Lightweight Event Logging (`--log`)**: Optional ring-buffer or file log recording per-sweep reclaimed MB and focus promotion latency.
7. **Localization Table (`src/strings.h`)**: Extract tray menu and status toast strings into a clean lookup table for multi-language support.
