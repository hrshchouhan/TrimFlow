// Unit and behavioral benchmark scenario test suite for TrimFlow heuristics & subprocess classification.
#include "../src/heuristics.h"
#include "../src/strings.h"

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <type_traits>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;

void Expect(bool cond, const char* expr, int line) {
    if (cond) {
        g_passed++;
    } else {
        g_failed++;
        printf("[FAIL] Line %d: %s\n", line, expr);
    }
}

#define EXPECT_TRUE(x)  Expect((x), #x, __LINE__)
#define EXPECT_FALSE(x) Expect(!(x), "!(" #x ")", __LINE__)
#define EXPECT_EQ(a, b) Expect((a) == (b), #a " == " #b, __LINE__)

void TestSystemAndAntiCheatProtection() {
    using trimflow::IsSystemProtectedProcess;
    // Critical Windows processes must be protected
    EXPECT_TRUE(IsSystemProtectedProcess(L"System"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"Memory Compression"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"csrss.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"dwm.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"explorer.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"audiodg.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"svchost.exe"));

    // Anti-cheat processes must never be touched
    EXPECT_TRUE(IsSystemProtectedProcess(L"vgc.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"vgtray.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"EasyAntiCheat.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"EasyAntiCheat_EOS.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"BEService.exe"));

    // Self-protection
    EXPECT_TRUE(IsSystemProtectedProcess(L"TrimFlow.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"WirelessBattery.exe"));

    // User-space desktop apps should be eligible for discovery
    EXPECT_FALSE(IsSystemProtectedProcess(L"brave.exe"));
    EXPECT_FALSE(IsSystemProtectedProcess(L"chrome.exe"));
    EXPECT_FALSE(IsSystemProtectedProcess(L"spotify.exe"));
    EXPECT_FALSE(IsSystemProtectedProcess(L"discord.exe"));
    EXPECT_FALSE(IsSystemProtectedProcess(L"steamwebhelper.exe"));
    EXPECT_FALSE(IsSystemProtectedProcess(L"code.exe"));
}

void TestSubprocessRoleClassification() {
    using trimflow::ClassifyCommandLine;
    using trimflow::SubprocRole;

    // Single-process apps are always MainOrStandalone
    EXPECT_EQ(ClassifyCommandLine(L"\"C:\\App\\foo.exe\" --type=renderer", 1), SubprocRole::MainOrStandalone);

    // Root browser process (no --type= flag)
    EXPECT_EQ(ClassifyCommandLine(L"\"C:\\Program Files\\BraveSoftware\\Brave-Browser\\Application\\brave.exe\"", 15),
              SubprocRole::MainOrStandalone);

    // GPU compositor subprocess
    EXPECT_EQ(ClassifyCommandLine(L"\"brave.exe\" --type=gpu-process --gpu-preferences=KAAA", 15),
              SubprocRole::GpuCompositor);

    // Audio & Network services (must be protected unconditionally)
    EXPECT_EQ(ClassifyCommandLine(L"\"Spotify.exe\" --type=utility --utility-sub-type=audio.mojom.AudioService", 7),
              SubprocRole::AudioOrNetwork);
    EXPECT_EQ(ClassifyCommandLine(L"\"msedge.exe\" --type=utility --utility-sub-type=network.mojom.NetworkService", 12),
              SubprocRole::AudioOrNetwork);

    // Chromium renderer & utility workers
    EXPECT_EQ(ClassifyCommandLine(L"\"brave.exe\" --type=renderer --lang=en-US --num-raster-threads=4", 20),
              SubprocRole::RendererOrWorker);
    EXPECT_EQ(ClassifyCommandLine(L"\"Discord.exe\" --type=utility --utility-sub-type=storage.mojom.StorageService", 6),
              SubprocRole::RendererOrWorker);

    // Firefox content tab subprocess
    EXPECT_EQ(ClassifyCommandLine(L"\"C:\\Program Files\\Mozilla Firefox\\firefox.exe\" -contentproc -childID 8 -isForBrowser", 10),
              SubprocRole::RendererOrWorker);
}

void TestElectronNetworkServiceSandboxRegression() {
    using trimflow::ClassifyCommandLine;
    using trimflow::SubprocRole;

    // Regression test: Electron / VS Code passes `--disable-features=...,NetworkServiceSandbox` to every renderer
    // and extension host. It must NOT be misclassified as AudioOrNetwork unless `network.mojom.networkservice` is present.
    const wchar_t* vscodeRendererCmd =
        L"\"C:\\Users\\user\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe\" "
        L"--type=renderer --disable-features=CalculateNativeWinOcclusion,NetworkServiceSandbox "
        L"--vscode-window-config=vscode:1234";
    EXPECT_EQ(ClassifyCommandLine(vscodeRendererCmd, 13), SubprocRole::RendererOrWorker);

    const wchar_t* vscodeExtHostCmd =
        L"\"C:\\Users\\user\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe\" "
        L"--type=utility --utility-sub-type=node.mojom.NodeService "
        L"--disable-features=CalculateNativeWinOcclusion,NetworkServiceSandbox";
    EXPECT_EQ(ClassifyCommandLine(vscodeExtHostCmd, 13), SubprocRole::RendererOrWorker);

    const wchar_t* vscodeRealNetServiceCmd =
        L"\"C:\\Users\\user\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe\" "
        L"--type=utility --utility-sub-type=network.mojom.NetworkService "
        L"--disable-features=CalculateNativeWinOcclusion,NetworkServiceSandbox";
    EXPECT_EQ(ClassifyCommandLine(vscodeRealNetServiceCmd, 13), SubprocRole::AudioOrNetwork);
}

void TestDynamicGroupQualification() {
    using trimflow::ShouldQualifyGroup;
    constexpr SIZE_T MB = 1024ULL * 1024ULL;

    // Multi-process group >= 15 MB qualifies in both modes
    EXPECT_TRUE(ShouldQualifyGroup(12, 400 * MB, false, false, false, false));
    EXPECT_TRUE(ShouldQualifyGroup(12, 400 * MB, true,  false, false, false));

    // Tiny multi-process group (< 15 MB) is skipped
    EXPECT_FALSE(ShouldQualifyGroup(2, 8 * MB, false, false, false, false));

    // Single-process background app >= 25 MB qualifies in default mode, skipped in multi-only mode
    EXPECT_TRUE(ShouldQualifyGroup(1, 50 * MB, false, false, false, false));
    EXPECT_FALSE(ShouldQualifyGroup(1, 50 * MB, true,  false, false, false));

    // Active fullscreen single-process game is NEVER targeted
    EXPECT_FALSE(ShouldQualifyGroup(1, 4096 * MB, false, false, true, true));

    // Background multi-process browser while fullscreen game runs IS targeted
    EXPECT_TRUE(ShouldQualifyGroup(15, 1200 * MB, false, false, true, false));
}

void TestAntiThrashingHysteresis() {
    using trimflow::ShouldReTrimWithHysteresis;
    constexpr SIZE_T MB = 1024ULL * 1024ULL;

    // First-ever trim (lastTrimTick == 0) always runs
    EXPECT_TRUE(ShouldReTrimWithHysteresis(0, 120 * MB, 0, false));

    // Already trimmed to 10 MB, grew only to 14 MB (< +8 MB threshold) -> skip to prevent thrashing
    EXPECT_FALSE(ShouldReTrimWithHysteresis(1000, 14 * MB, 10 * MB, false));

    // Grew from 10 MB to 20 MB (> +8 MB threshold) -> re-trim
    EXPECT_TRUE(ShouldReTrimWithHysteresis(1000, 20 * MB, 10 * MB, false));

    // Forced CLI pass always trims
    EXPECT_TRUE(ShouldReTrimWithHysteresis(1000, 11 * MB, 10 * MB, true));
}

void TestCpuDeltaThreshold() {
    using trimflow::IsCpuActiveDelta;
    // 0.1% single-core over 1000 ms (10,000 100ns units) -> idle noise
    EXPECT_FALSE(IsCpuActiveDelta(500000, 510000));

    // 1.5% boundary (150,000 100ns units) -> still idle
    EXPECT_FALSE(IsCpuActiveDelta(500000, 650000));

    // 3.0% single-core over 1000 ms (300,000 100ns units) -> active
    EXPECT_TRUE(IsCpuActiveDelta(500000, 800000));
}

// Converts benchmark claims into deterministic table-driven scenario tests so contributors
// can verify application-level policies without modifying Win32 process enumeration code.
void TestBenchmarkScenarios() {
    using trimflow::ClassifyCommandLine;
    using trimflow::ShouldKeepSubprocessHot;
    using trimflow::SubprocRole;

    struct SimulatedProc {
        const wchar_t* cmdLine;
        SubprocRole expectedRole;
        bool isCpuActive;
        bool isPrimaryRenderer;
        bool expectHotWhenForeground;
        bool expectHotWhenBackground;
    };

    using Role = SubprocRole;
    const SimulatedProc scenarios[] = {
        // VS Code with multiple windows and an idle extension host.
        {L"\"Code.exe\"", Role::MainOrStandalone, false, false, true, false},
        {L"\"Code.exe\" --type=gpu-process --disable-features=NetworkServiceSandbox",
         Role::GpuCompositor, false, false, true, false},
        {L"\"Code.exe\" --type=utility --utility-sub-type=network.mojom.NetworkService",
         Role::AudioOrNetwork, false, false, true, true},
        {L"\"Code.exe\" --type=renderer --disable-features=NetworkServiceSandbox",
         Role::RendererOrWorker, false, true, true, false},
        {L"\"Code.exe\" --type=renderer --disable-features=NetworkServiceSandbox",
         Role::RendererOrWorker, false, false, false, false},
        {L"\"Code.exe\" --type=utility --utility-sub-type=node.mojom.NodeService --disable-features=NetworkServiceSandbox",
         Role::RendererOrWorker, false, false, false, false},

        // Spotify playing audio while minimized.
        {L"\"Spotify.exe\" --type=utility --utility-sub-type=audio.mojom.AudioService",
         Role::AudioOrNetwork, false, false, true, true},
        {L"\"Spotify.exe\" --type=renderer --lang=en", Role::RendererOrWorker, false, false, false, false},

        // Firefox tabs and a busy remote data decoder (RDD) child.
        {L"\"firefox.exe\"", Role::MainOrStandalone, false, false, true, false},
        {L"\"firefox.exe\" -contentproc -childID 2 -isForBrowser", Role::RendererOrWorker, false, true, true, false},
        {L"\"firefox.exe\" -contentproc -childID 3 -isForBrowser", Role::RendererOrWorker, false, false, false, false},
        {L"\"firefox.exe\" -contentproc -parentPid 1000 rdd", Role::RendererOrWorker, true, false, true, true},

        // Minimized Discord voice call with an idle UI.
        {L"\"Discord.exe\" --type=utility --utility-sub-type=audio.mojom.AudioService",
         Role::AudioOrNetwork, false, false, true, true},
        {L"\"Discord.exe\" --type=renderer", Role::RendererOrWorker, false, false, false, false},

        // WebView2 browser and renderer for the Windows Widgets host.
        {L"\"msedgewebview2.exe\" --embedded-browser-webview=1 --webview-exe-name=Widgets.exe",
         Role::MainOrStandalone, false, false, true, false},
        {L"\"msedgewebview2.exe\" --type=renderer --embedded-browser-webview=1",
         Role::RendererOrWorker, false, false, false, false},

        // Slack passes NetworkServiceSandbox to both its helpers and the actual network service.
        {L"\"slack.exe\" --type=renderer --disable-features=CalculateNativeWinOcclusion,NetworkServiceSandbox",
         Role::RendererOrWorker, false, false, false, false},
        {L"\"slack.exe\" --type=utility --utility-sub-type=network.mojom.NetworkService "
         L"--disable-features=CalculateNativeWinOcclusion,NetworkServiceSandbox",
         Role::AudioOrNetwork, false, false, true, true},
    };

    for (const auto& p : scenarios) {
        const int failuresBefore = g_failed;
        const auto role = ClassifyCommandLine(p.cmdLine, /*groupCount=*/2);
        EXPECT_EQ(role, p.expectedRole);
        EXPECT_EQ(ShouldKeepSubprocessHot(role, /*isForegroundApp=*/true, p.isCpuActive, p.isPrimaryRenderer),
                  p.expectHotWhenForeground);
        EXPECT_EQ(ShouldKeepSubprocessHot(role, /*isForegroundApp=*/false, p.isCpuActive, p.isPrimaryRenderer),
                  p.expectHotWhenBackground);
        if (g_failed != failuresBefore) {
            printf("  Command line: %ls (primary renderer: %d)\n", p.cmdLine, p.isPrimaryRenderer);
        }
    }
}

// A string table must have every field filled in; a missing translation would otherwise be a null pointer.
bool AllStringFieldsSet(const UiStrings& table) {
    static_assert(std::is_standard_layout<UiStrings>::value, "UiStrings must stay a plain table of pointers");
    const wchar_t* const* fields = reinterpret_cast<const wchar_t* const*>(&table);
    for (size_t i = 0; i < sizeof(UiStrings) / sizeof(const wchar_t*); ++i) {
        if (!fields[i] || !fields[i][0]) return false;
    }
    return true;
}

// Guards the move of the UI text into strings.h: the English output must stay exactly what it was.
void TestEnglishStringsKeepOriginalText() {
    const UiStrings& s = kEnglish;
    EXPECT_TRUE(AllStringFieldsSet(s));
    EXPECT_TRUE(AllStringFieldsSet(GetStrings()));

    wchar_t buf[256];
    swprintf(buf, 256, s.tooltipFmt, 8.0, 16.0, 50UL, 120.0);
    EXPECT_TRUE(wcscmp(buf, L"TrimFlow — RAM: 8.0 / 16.0 GB (50%)\nSession reclaimed: 120 MB") == 0);

    swprintf(buf, 256, s.menuHeaderFmt, 8.0, 16.0, 50UL, 120.0);
    EXPECT_TRUE(wcscmp(buf, L"System RAM: 8.0 / 16.0 GB (50%)  |  Reclaimed: 120 MB") == 0);

    swprintf(buf, 256, s.menuAppFmt, L"chrome.exe", 3, s.procPlural, 512.5, s.excludedSuffix);
    EXPECT_TRUE(wcscmp(buf, L"chrome.exe  —  3 procs (512.5 MB)  [excluded]") == 0);
    swprintf(buf, 256, s.menuAppFmt, L"code.exe", 1, s.procSingular, 64.0, L"");
    EXPECT_TRUE(wcscmp(buf, L"code.exe  —  1 proc (64.0 MB)") == 0);
    swprintf(buf, 256, s.menuAppExcludedFmt, L"slack.exe");
    EXPECT_TRUE(wcscmp(buf, L"slack.exe  [excluded]") == 0);

    swprintf(buf, 256, s.toastReclaimedFmt, 42.0, 5);
    EXPECT_TRUE(wcscmp(buf, L"Reclaimed 42 MB across 5 active apps") == 0);
    swprintf(buf, 256, s.toastMonitoringFmt, 5);
    EXPECT_TRUE(wcscmp(buf, L"Monitoring 5 apps in system tray") == 0);
    swprintf(buf, 256, s.toastAlreadyRunningFmt, 8.0, 16.0, 50UL, 120.0);
    EXPECT_TRUE(wcscmp(buf, L"RAM: 8.0 / 16.0 GB (50%)  •  Reclaimed: 120 MB") == 0);

    EXPECT_TRUE(wcscmp(s.menuExit, L"Exit") == 0);
    EXPECT_TRUE(wcscmp(s.menuAutostart, L"Start with Windows") == 0);
    EXPECT_TRUE(wcscmp(s.toastStartedTitle, L"TrimFlow started") == 0);
    EXPECT_TRUE(wcscmp(s.toastRunningTitle, L"TrimFlow is running in background") == 0);
    EXPECT_TRUE(wcscmp(s.toastAlreadyRunningTitle, L"TrimFlow is already running") == 0);
}

}  // namespace

int main() {
    printf("Running TrimFlow unit & benchmark scenario tests...\n");
    TestSystemAndAntiCheatProtection();
    TestSubprocessRoleClassification();
    TestElectronNetworkServiceSandboxRegression();
    TestDynamicGroupQualification();
    TestAntiThrashingHysteresis();
    TestCpuDeltaThreshold();
    TestBenchmarkScenarios();
    TestEnglishStringsKeepOriginalText();

    printf("Results: %d passed, %d failed.\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
