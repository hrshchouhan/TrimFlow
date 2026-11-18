#pragma once
// Pure, testable heuristics for TrimFlow's subprocess role classification, system/anti-cheat protection,
// dynamic group qualification, and anti-thrashing hysteresis.

#include <windows.h>
#include <cwctype>
#include <string>

namespace trimflow {

enum class SubprocRole {
    MainOrStandalone,  // Root UI / browser process (no --type= flag)
    GpuCompositor,     // --type=gpu-process (protect while app window is visible)
    AudioOrNetwork,    // audio.mojom.AudioService / network.mojom.NetworkService (always protect)
    RendererOrWorker,  // --type=renderer / -contentproc / background tab / extension worker
};

constexpr SIZE_T    kMinGroupBytes       = 15ULL * 1024 * 1024;  // 15 MB minimum multi-process group footprint
constexpr SIZE_T    kMinSingleBytes      = 25ULL * 1024 * 1024;  // 25 MB minimum single-process footprint
constexpr SIZE_T    kReTrimGrowthBytes   = 8ULL  * 1024 * 1024;  // +8 MB growth hysteresis before re-trimming
constexpr ULONGLONG kActiveCpuDelta100ns = 150000ULL;            // 1.5% of 1 core over a 1000 ms window

inline std::wstring LowerStr(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}

// Critical OS kernel, session, display, audio server, and anti-cheat processes that are never touched.
inline bool IsSystemProtectedProcess(const std::wstring& exeName) {
    static const wchar_t* const kSystemProtected[] = {
        L"system",
        L"registry",
        L"memcompression",
        L"memory compression",
        L"smss.exe",
        L"csrss.exe",
        L"wininit.exe",
        L"services.exe",
        L"lsass.exe",
        L"svchost.exe",
        L"winlogon.exe",
        L"dwm.exe",
        L"fontdrvhost.exe",
        L"sihost.exe",
        L"taskhostw.exe",
        L"explorer.exe",
        L"audiodg.exe",
        L"wmiprvse.exe",
        L"spoolsv.exe",
        L"msmpeng.exe",
        L"nissrv.exe",
        L"securityhealthservice.exe",
        L"ctfmon.exe",
        L"conhost.exe",
        L"dllhost.exe",
        L"runtimebroker.exe",
        L"trimflow.exe",
        L"wirelessbattery.exe",
        L"vgc.exe",
        L"vgtray.exe",
        L"easyanticheat.exe",
        L"easyanticheat_eos.exe",
        L"beservice.exe",
        L"bedaisy.sys",
    };
    for (const wchar_t* s : kSystemProtected)
        if (_wcsicmp(exeName.c_str(), s) == 0) return true;
    return false;
}

// Classifies a subprocess from its command line within a multi-process group.
inline SubprocRole ClassifyCommandLine(const std::wstring& rawCmdLine, int groupCount) {
    if (groupCount <= 1 || rawCmdLine.empty()) return SubprocRole::MainOrStandalone;
    std::wstring cmd = LowerStr(rawCmdLine);

    if (cmd.find(L"audio.mojom.audioservice") != std::wstring::npos ||
        cmd.find(L"network.mojom.networkservice") != std::wstring::npos ||
        cmd.find(L"--utility-sub-type=audio") != std::wstring::npos) {
        return SubprocRole::AudioOrNetwork;
    }
    if (cmd.find(L"--type=gpu-process") != std::wstring::npos) {
        return SubprocRole::GpuCompositor;
    }
    if (cmd.find(L"--type=") != std::wstring::npos ||
        cmd.find(L"-contentproc") != std::wstring::npos) {
        return SubprocRole::RendererOrWorker;
    }
    return SubprocRole::MainOrStandalone;
}

// Determines whether a dynamically discovered process group qualifies for management.
inline bool ShouldQualifyGroup(int groupProcCount,
                               SIZE_T groupTotalBytes,
                               bool multiProcOnly,
                               bool isExplicitlyExcluded,
                               bool fullscreenGameDetected,
                               bool isForegroundExe) {
    if (fullscreenGameDetected && isForegroundExe && groupProcCount == 1) return false;
    if (isExplicitlyExcluded) return true;
    if (groupProcCount >= 2 && groupTotalBytes >= kMinGroupBytes) return true;
    if (!multiProcOnly && groupTotalBytes >= kMinSingleBytes) return true;
    return false;
}

// Determines whether an idle subprocess should have K32EmptyWorkingSet invoked on this pass.
inline bool ShouldReTrimWithHysteresis(ULONGLONG lastTrimTick,
                                       SIZE_T currentWorkingSetBytes,
                                       SIZE_T lastPostTrimBytes,
                                       bool forceTrim) {
    if (forceTrim || lastTrimTick == 0) return true;
    return currentWorkingSetBytes > lastPostTrimBytes + kReTrimGrowthBytes;
}

// Determines whether a subprocess is CPU-active over a 1000 ms sampling window.
inline bool IsCpuActiveDelta(ULONGLONG cpuTime1, ULONGLONG cpuTime2) {
    ULONGLONG delta = cpuTime2 >= cpuTime1 ? (cpuTime2 - cpuTime1) : 0;
    return delta > kActiveCpuDelta100ns;
}

// Determines whether a subprocess must be kept hot in physical RAM (exempt from K32EmptyWorkingSet).
// - Audio/Network services and CPU-active processes are always protected.
// - For the active foreground application, its main UI host, GPU compositor, and primary active renderer are protected.
// - For covered or minimized background applications, idle UI/GPU/renderer workers are eligible for working-set trim.
inline bool ShouldKeepSubprocessHot(SubprocRole role,
                                    bool isForegroundApp,
                                    bool isCpuActive,
                                    bool isPrimaryRenderer) {
    if (isCpuActive) return true;
    if (role == SubprocRole::AudioOrNetwork) return true;
    if (isForegroundApp && (role == SubprocRole::MainOrStandalone ||
                            role == SubprocRole::GpuCompositor ||
                            isPrimaryRenderer)) {
        return true;
    }
    return false;
}

}  // namespace trimflow
