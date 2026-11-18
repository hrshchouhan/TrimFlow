# Contributing to TrimFlow

Thank you for your interest in contributing to **TrimFlow**!

You do **not** need to modify the Win32 windowing or process-enumeration code to contribute heuristic improvements. The classification rules, hysteresis thresholds, and protection policies are isolated in a pure header ([`src/heuristics.h`](src/heuristics.h)) backed by deterministic unit and scenario tests ([`tests/heuristics_test.cpp`](tests/heuristics_test.cpp)).

---

## 1. Windows Setup

TrimFlow uses standard `C++17` and the Windows SDK with no third-party dependencies.

### Option A: MinGW-w64 / MSYS2 or LLVM-MinGW
```powershell
winget install MSYS2.MSYS2
# Ensure g++ (e.g. C:\msys64\ucrt64\bin) is on your PATH
g++ --version
```

### Option B: Visual Studio 2022 (`cl.exe`)
Open the **x64 Native Tools Command Prompt for VS 2022** and compile the test suite with:
```powershell
cl /std:c++17 /O2 /EHsc /Isrc tests\heuristics_test.cpp
```

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
3. **Add or Update a Testable Scenario:**
   Add a row to `TestBenchmarkScenarios()` in [`tests/heuristics_test.cpp`](tests/heuristics_test.cpp) verifying the expected `SubprocRole` and foreground/background retention policy (`ShouldKeepSubprocessHot`).
4. **Build & Verify CLI Output:**
   ```powershell
   .\build.bat
   .\TrimFlow.exe --status | Out-Host
   ```

---

## 3. Expected Behavior Matrix

Changes to [`src/heuristics.h`](src/heuristics.h) should preserve the following behavior:

| Process Category | Command-Line / Signature | App in Foreground (`isForegroundApp = true`) | App Covered / Minimized (`isForegroundApp = false`) |
| :--- | :--- | :--- | :--- |
| **OS Kernel / Anti-Cheat** | `csrss.exe`, `dwm.exe`, `audiodg.exe`, `vgc.exe`, `EasyAntiCheat.exe` | **Excluded** (`IsSystemProtectedProcess`) | **Excluded** (`IsSystemProtectedProcess`) |
| **Audio & Network Services** | `audio.mojom.AudioService`, `network.mojom.NetworkService` | **Protected** (Normal QoS, not trimmed) | **Protected** (Normal QoS, not trimmed) |
| **Main UI Host & GPU Compositor** | No `--type=` flag, or `--type=gpu-process` | **Protected** | **Trimmed + EcoQoS** (Restored on `EVENT_SYSTEM_FOREGROUND`) |
| **Primary Active Renderer Tab** | `--type=renderer` with highest CPU delta in foreground app | **Protected** | **Trimmed + EcoQoS** |
| **Idle Background Renderers / Workers** | `--type=renderer`, `--type=utility`, `-contentproc` | **Trimmed + EcoQoS** (with `+8 MB` hysteresis) | **Trimmed + EcoQoS** (with `+8 MB` hysteresis) |

---

## 4. `v1.1` Scope & Open Issues

`v1.1` is scoped to **diagnostics, configuration validation, UI polish, and scenario coverage**:

1. **Startup & Process Diagnostics (`--diagnose`)**: Print each discovered session process, its classified `SubprocRole`, foreground/background status, and why it was protected or trimmed.
2. **JSON Output Mode (`--json`)**: Emit structured JSON for `--status` and `--once`.
3. **Configuration File & Validation (`trimflow.ini` / `--validate-config`)**: Load optional user exclusions and hysteresis thresholds from `trimflow.ini`.
4. **Tray & Toast UI Polish**: Respect Windows system light/dark theme (`AppsUseLightTheme`) in the owner-drawn tray menu and status banner.
5. **Optional Event Logging (`--log`)**: Log per-sweep Working Set deltas and focus restoration events when enabled via CLI.
6. **Localization Table (`src/strings.h`)**: Extract tray menu and status toast strings into a language table.
