# TrimFlow

**Context-Aware Subprocess Working-Set & EcoQoS Manager for Windows 10/11**

[![CI Build & Tests](https://github.com/vncwr/TrimFlow/actions/workflows/ci.yml/badge.svg)](https://github.com/vncwr/TrimFlow/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-10b981.svg)](LICENSE)
[![Platform: Windows 10/11 x64](https://img.shields.io/badge/Platform-Windows_10%2F11_x64-blue.svg)](https://github.com/vncwr/TrimFlow/releases)

`TrimFlow` is a native C++17/Win32 system tray and CLI utility that trims idle working-set pages and enables Windows Efficiency Mode (`EcoQoS`) on background multi-process applications (browsers, Electron/CEF/WebView2 apps, and IDEs) while leaving foreground windows, GPU compositors, audio/network services, and games untouched.

![TrimFlow Demo — Task Manager Working Set Reduction and Tray UI](docs/demo.gif)

---

## 1. Quick Start

### Install via Scoop
```powershell
scoop install https://raw.githubusercontent.com/vncwr/TrimFlow/main/packaging/scoop/trimflow.json
```

### Install via WinGet (Local Manifest)
```powershell
winget install --manifest packaging\winget\vncwr.TrimFlow.yaml
```

### Portable Binary
Download `TrimFlow.exe` (`~302 KB` statically linked x64 binary, zero external runtime dependencies) from [**GitHub Releases**](https://github.com/vncwr/TrimFlow/releases/latest).

```powershell
TrimFlow.exe                   # Launch tray utility (shows a 4s status banner on startup)
TrimFlow.exe --quiet           # Launch silently in the system tray
TrimFlow.exe --status | Out-Host  # Print current per-group Working Set without modifying processes
TrimFlow.exe --once   | Out-Host  # Run one optimization pass, print Before/After table, and exit
TrimFlow.exe --status --json | Out-Host  # Same data as JSON for scripts (also works with --once)
TrimFlow.exe --quit            # Restore throttled processes and exit the running tray instance

*(Note: Because `TrimFlow.exe` is compiled with `-mwindows` for tray usage, pipe CLI invocations to `| Out-Host` in PowerShell so the shell waits for the 1-second CPU sampling window to finish.)*

---

## 2. How It Works (and What It Actually Does to Memory)

Multi-process desktop applications (Chromium, Firefox, Electron, WebView2, VS Code) spawn 10–30+ child processes per application. When those apps sit behind your active window or minimized in the taskbar, their background renderer and utility processes often retain hundreds of megabytes of inactive pages in their resident **Working Set**.

Blanket "RAM cleaner" tools call `EmptyWorkingSet` across every process on a fixed timer, which evicts pages from the active foreground window, GPU compositor, and audio threads—causing immediate soft page faults and UI/audio hiccups.

TrimFlow applies four narrower rules:

1. **Subprocess Role Classification (`NtQueryInformationProcess`)**
   * Queries `ProcessCommandLineInformation` (`60`) via the internal NT API `NtQueryInformationProcess` (`ntdll.dll`) using `PROCESS_QUERY_LIMITED_INFORMATION` (no DLL injection or `ReadProcessMemory`).
   * Classifies child processes into `MainOrStandalone`, `GpuCompositor` (`--type=gpu-process`), `AudioOrNetwork` (`audio.mojom.AudioService`, `network.mojom.NetworkService`), and `RendererOrWorker` (`--type=renderer`, `--type=utility`, `-contentproc`).
   * `AudioOrNetwork` subprocesses are **never trimmed or throttled**, even when the parent application is minimized.

2. **Foreground Window Protection & Focus Restoration (`SetWinEventHook`)**
   * For the currently focused foreground application (`GetForegroundWindow`), its main UI process, `--type=gpu-process` compositor, and highest-CPU renderer tab are excluded from working-set eviction and power throttling.
   * For background or minimized applications, idle child processes are set to `MEMORY_PRIORITY_VERY_LOW`, `BELOW_NORMAL_PRIORITY_CLASS`, and `PROCESS_POWER_THROTTLING_EXECUTION_SPEED` (**EcoQoS**), and trimmed via `K32EmptyWorkingSet`.
   * An out-of-context `SetWinEventHook` (`EVENT_SYSTEM_FOREGROUND`) restores a background application's processes back to `NORMAL_PRIORITY_CLASS`, `MEMORY_PRIORITY_NORMAL`, and normal QoS as soon as you focus its window.

3. **`+8 MB` Growth Hysteresis**
   * After trimming a process, TrimFlow records its post-trim resident Working Set (`lastPostTrimBytes`). Subsequent background sweeps skip `K32EmptyWorkingSet` on that PID unless its Working Set grows by more than **`+8 MB`**, avoiding repeated page-table walks on dormant processes.

4. **What `K32EmptyWorkingSet` Does (Working Set vs. Commit)**
   * Trimming a process's Working Set does **not** deallocate its virtual `Private Bytes` (commit charge). Instead, it removes untouched pages from the process's resident Working Set and moves them to the Windows kernel's **Standby / Modified page lists** and in-RAM **Memory Compression store (`MemCompression`)**.
   * Those physical pages immediately become available to foreground applications or games without disk I/O, while pages the background app needs again are faulted back in from RAM (soft page faults).

---

## 3. Reproducible Measurement (`TrimFlow.exe --once`)

### Methodology
* `TrimFlow.exe --once` enumerates non-system processes in the current user session (`ProcessIdToSessionId`), samples `GetProcessTimes` over a `1000 ms` window to detect CPU-active workers, applies EcoQoS + `K32EmptyWorkingSet` to qualifying idle subprocesses, waits `200 ms`, and queries `PROCESS_MEMORY_COUNTERS.WorkingSetSize` via `K32GetProcessMemoryInfo`.
* You can reproduce and inspect the exact per-application resident Working Set delta on your own machine at any time by running:
  ```powershell
  .\TrimFlow.exe --status | Out-Host
  .\TrimFlow.exe --once   | Out-Host
  ```

### Sample Raw CLI Output (Windows 11 24H2, 16 GB RAM)
```text
Application (Dynamic)       Procs  Trimmed   Active  Before (MB)   After (MB)   Delta (MB)
--------------------------------------------------------------------------------------------
antigravity.exe                18       16        2       513.8M       112.6M      -401.2M
brave.exe                      14       12        2       871.0M        49.8M      -821.2M
code.exe                       12       11        1       490.8M       124.3M      -366.6M
discord.exe                     6        4        2       221.5M       165.0M       -56.5M
language_server.exe             1        1        0       195.2M        17.6M      -177.6M
msedge.exe                      5        4        1        36.6M        19.3M       -17.3M
msedgewebview2.exe              6        5        1        81.8M        22.9M       -59.0M
node.exe                        2        2        0        38.2M         0.2M       -37.9M
powershell.exe                  2        2        0        78.7M         2.5M       -76.1M
searchhost.exe                  1        1        0        26.0M         0.0M       -26.0M
snippingtool.exe                1        0        0       138.1M       138.1M        +0.0M
spotify.exe                     7        6        1       142.5M        23.8M      -118.6M
--------------------------------------------------------------------------------------------
TOTAL                                                    2834.2M       676.2M     -2158.1M
```

### Resource Footprint of `TrimFlow.exe`
* **Binary size:** `~302 KB` on disk (statically linked with `-static-libstdc++ -static-libgcc`, no external DLL dependencies).
* **Memory usage:** `~80–150 KB` Private Working Set (`~500 KB` total Working Set including shared Win32 system DLL pages) after self-trimming.

---

## 4. Safety & Exclusions

* **No Code Injection or Foreign Memory Writes:** Uses `SetProcessInformation`, `SetPriorityClass`, `K32EmptyWorkingSet`, and read-only `NtQueryInformationProcess` (`PROCESS_QUERY_LIMITED_INFORMATION`).
* **System & Anti-Cheat Exclusion List:** Explicitly skips Windows kernel/session services (`csrss.exe`, `dwm.exe`, `audiodg.exe`, `explorer.exe`, `svchost.exe`), anti-cheat processes (`vgc.exe`, `vgtray.exe`, `EasyAntiCheat.exe`, `BEService.exe`), and fullscreen foreground games (`src/heuristics.h`).
* **Clean Exit Restoration:** Exiting TrimFlow (`Exit` in tray menu or `TrimFlow.exe --quit`) restores all managed PIDs back to `NORMAL_PRIORITY_CLASS`, `MEMORY_PRIORITY_NORMAL`, and un-throttled QoS.

---

## 5. Building & Testing from Source

Requires [MinGW-w64](https://www.mingw-w64.org/) (`g++` with C++17 support on `PATH`):

```powershell
.\test.bat    # Compile and run the 99 unit & scenario assertions in tests/heuristics_test.cpp
.\build.bat   # Build TrimFlow.exe
```

See [**CONTRIBUTING.md**](CONTRIBUTING.md) for the architecture overview, expected behavior matrix, and open [**`good first issue`**](https://github.com/vncwr/TrimFlow/labels/good%20first%20issue) tasks for `v1.1`.

---

## 6. License

Released under the [MIT License](LICENSE).
