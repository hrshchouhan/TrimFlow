# Architecture & Technical Reference

This document describes the internal design of **TrimFlow**, how it interacts with the Windows 10/11 Virtual Memory Manager (VMM) and process scheduler, and the tradeoffs involved in working-set trimming.

---

## 1. Working Set vs. Commit Charge on Windows

Each user-mode process on Windows maintains a **Working Set** (`PROCESS_MEMORY_COUNTERS.WorkingSetSize`)—the subset of its virtual address space pages currently resident in physical RAM.

When multi-process applications such as Chromium (`brave.exe`, `chrome.exe`, `msedge.exe`), Firefox (`firefox.exe`), or Electron/CEF (`Spotify.exe`, `Discord.exe`, `Code.exe`) run:
* They spawn a **Main / Browser UI Process**, a **GPU Compositor Process**, **Audio & Network Service Processes**, and multiple **Renderer / Utility Subprocesses**.
* Idle background renderer processes frequently retain large V8/SpiderMonkey heaps and DOM caches in their resident Working Set while covered or minimized.
* Calling `K32EmptyWorkingSet` does **not** reduce a process's committed virtual memory (`Private Bytes`). Instead, it removes pages from the process's active Working Set and places them onto the kernel's **Standby / Modified page lists** (or compresses them inside the Windows `MemCompression` process when paired with `MEMORY_PRIORITY_VERY_LOW`). This makes physical RAM frames available to active processes while allowing the background process to soft-fault pages back from RAM when needed.

---

## 2. Five-Stage Optimization Pipeline

`TrimFlow` implements a five-stage pipeline in [`src/main.cpp`](src/main.cpp) and [`src/heuristics.h`](src/heuristics.h):

### Stage 1: Dynamic Session Discovery & Window Topology (`InspectWindows`)
* Enumerates processes in the current user session (`ProcessIdToSessionId`), skipping critical OS kernel/session daemons and anti-cheat processes (`IsSystemProtectedProcess` in [`src/heuristics.h`](src/heuristics.h)).
* Identifies the foreground window (`GetForegroundWindow`), checks for fullscreen applications (`MonitorFromWindow` bounds check), and records visible non-minimized top-level windows (`EnumWindows`).

### Stage 2: Subprocess Role Classification (`NtQueryInformationProcess`)
* Queries `ProcessCommandLineInformation` (`60`) via the internal NT native API `NtQueryInformationProcess` (`ntdll.dll`) using `PROCESS_QUERY_LIMITED_INFORMATION` (avoiding DLL injection or `ReadProcessMemory`).
* Classifies each subprocess in a multi-process group into one of four roles (`SubprocRole`):
  1. **`AudioOrNetwork` (`audio.mojom.AudioService`, `network.mojom.NetworkService`):** Unconditionally excluded from throttling and working-set trimming so background audio and network streams are not interrupted.
  2. **`GpuCompositor` (`--type=gpu-process`):** Protected while its parent application is in the foreground.
  3. **`MainOrStandalone` (root process without `--type=`):** Protected while its application is in the foreground.
  4. **`RendererOrWorker` (`--type=renderer`, `-contentproc`, `--type=utility`):** Eligible for demotion and trimming unless it is the foreground app's primary active renderer or exceeds the CPU activity threshold.

### Stage 3: Event-Driven Focus Restoration & Minimize Sweep (`SetWinEventHook`)
* Registers out-of-context hooks (`WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS`) for:
  * **`EVENT_SYSTEM_FOREGROUND` & `EVENT_SYSTEM_MINIMIZEEND`:** Synchronously restores a newly focused application's subprocesses back to `NORMAL_PRIORITY_CLASS`, `MEMORY_PRIORITY_NORMAL` (`5`), and disables `EcoQoS`.
  * **`EVENT_SYSTEM_MINIMIZESTART`:** Schedules a debounced optimization pass `1.5 seconds` after an application is minimized.

### Stage 4: Hysteresis & Anti-Thrashing (`ProcHistory`)
* Tracks `lastPostTrimBytes` and `lastTrimTick` per `PID`.
* Once an idle subprocess has been trimmed, `TrimFlow` skips `K32EmptyWorkingSet` on subsequent sweeps unless the subprocess's Working Set has grown by more than **`+8 MB`** (`kReTrimGrowthBytes`).

### Stage 5: Process Demotion & Clean Exit Restoration
* For qualifying idle subprocesses:
  1. `SetProcessInformation(..., ProcessMemoryPriority)` → `MEMORY_PRIORITY_VERY_LOW` (`1`).
  2. `SetProcessInformation(..., ProcessPowerThrottling)` → `PROCESS_POWER_THROTTLING_EXECUTION_SPEED` (**EcoQoS**).
  3. `SetPriorityClass(..., BELOW_NORMAL_PRIORITY_CLASS)`.
  4. `K32EmptyWorkingSet()` when the `+8 MB` growth hysteresis threshold is exceeded.
* **Clean Exit Restoration (`RestoreAllThrottledProcesses`):** On exit (`WM_DESTROY` / `--quit`), `TrimFlow` iterates through all tracked PIDs and restores normal memory priority, normal scheduling priority, and un-throttled execution state.

---

## 3. Binary & Memory Footprint

* Compiled with `-Os -ffunction-sections -fdata-sections -Wl,--gc-sections` and static C++ runtime linkage (`~302 KB` standalone `.exe`).
* Uses a `64 KB` thread stack reservation (`-Wl,--stack,65536` and `STACK_SIZE_PARAM_IS_A_RESERVATION`) and trims its own working set after each sweep (`~80–150 KB` Private Working Set, `~500 KB` total Working Set including shared system DLLs).
