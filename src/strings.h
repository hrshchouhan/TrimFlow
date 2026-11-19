#pragma once
// User-facing text for TrimFlow's tray tooltip, context menu and status toasts.
//
// All wording lives here so that translations can be added without touching the Win32 window
// procedures in main.cpp. Strings are grouped in one table (UiStrings) per language, and
// GetStrings() picks the table that matches the Windows UI language.
//
// Adding a language:
//   1. Copy kEnglish below, rename it (for example kFrench) and translate every field.
//   2. Add a `case LANG_xxx: return kXxx;` line to GetStrings().
// Fields ending in "Fmt" are printf-style format strings: keep the same conversion specifiers
// (%.1f, %lu, %ls, ...) in the same order, because main.cpp passes the arguments in that order.

#include <windows.h>

struct UiStrings {
    // Tray tooltip. Args: used GB (%.1f), total GB (%.1f), memory load % (%lu), session MB reclaimed (%.0f).
    const wchar_t* tooltipFmt;

    // Context menu.
    const wchar_t* menuHeaderFmt;         // Same four arguments as tooltipFmt.
    const wchar_t* menuOptimizeNow;
    const wchar_t* menuAutoOptimize;
    const wchar_t* menuInterval;          // Submenu title.
    const wchar_t* menuInterval15;
    const wchar_t* menuInterval30;
    const wchar_t* menuInterval60;
    const wchar_t* menuProtectVisible;
    const wchar_t* menuGameBoost;
    const wchar_t* menuMultiProcOnly;
    const wchar_t* menuDiscoveredHeader;
    // Per-app line. Args: app name (%ls), process count (%d), procSingular/procPlural (%ls),
    // working set MB (%.1f), excludedSuffix or "" (%ls).
    const wchar_t* menuAppFmt;
    const wchar_t* menuAppExcludedFmt;    // Args: app name (%ls).
    const wchar_t* procSingular;
    const wchar_t* procPlural;
    const wchar_t* excludedSuffix;
    const wchar_t* menuAutostart;
    const wchar_t* menuExit;

    // Status toasts.
    const wchar_t* toastStartedTitle;
    const wchar_t* toastStartedBody;
    const wchar_t* toastRunningTitle;
    const wchar_t* toastReclaimedFmt;     // Args: MB reclaimed (%.0f), app count (%d).
    const wchar_t* toastMonitoringFmt;    // Args: app count (%d).
    const wchar_t* toastAlreadyRunningTitle;
    const wchar_t* toastAlreadyRunningFmt;  // Args: used GB (%.1f), total GB (%.1f), load % (%lu), MB reclaimed (%.0f).
};

inline const UiStrings kEnglish = {
    /* tooltipFmt */               L"TrimFlow — RAM: %.1f / %.1f GB (%lu%%)\nSession reclaimed: %.0f MB",
    /* menuHeaderFmt */            L"System RAM: %.1f / %.1f GB (%lu%%)  |  Reclaimed: %.0f MB",
    /* menuOptimizeNow */          L"Optimize background apps now",
    /* menuAutoOptimize */         L"Auto-optimize idle subprocesses",
    /* menuInterval */             L"Auto-optimize interval",
    /* menuInterval15 */           L"Every 15 seconds",
    /* menuInterval30 */           L"Every 30 seconds (Recommended)",
    /* menuInterval60 */           L"Every 60 seconds",
    /* menuProtectVisible */       L"Protect visible (non-minimized) UI & GPU processes",
    /* menuGameBoost */            L"Game-Boost Mode (aggressive trim during fullscreen games)",
    /* menuMultiProcOnly */        L"Only target multi-process apps (browsers, Electron, WebView)",
    /* menuDiscoveredHeader */     L"Discovered applications (click to enable/exclude):",
    /* menuAppFmt */               L"%ls  —  %d %ls (%.1f MB)%ls",
    /* menuAppExcludedFmt */       L"%ls  [excluded]",
    /* procSingular */             L"proc",
    /* procPlural */               L"procs",
    /* excludedSuffix */           L"  [excluded]",
    /* menuAutostart */            L"Start with Windows",
    /* menuExit */                 L"Exit",
    /* toastStartedTitle */        L"TrimFlow started",
    /* toastStartedBody */         L"Scanning background subprocesses...",
    /* toastRunningTitle */        L"TrimFlow is running in background",
    /* toastReclaimedFmt */        L"Reclaimed %.0f MB across %d active apps",
    /* toastMonitoringFmt */       L"Monitoring %d apps in system tray",
    /* toastAlreadyRunningTitle */ L"TrimFlow is already running",
    /* toastAlreadyRunningFmt */   L"RAM: %.1f / %.1f GB (%lu%%)  •  Reclaimed: %.0f MB",
};

// Returns the string table for the current Windows UI language. Regional variants share a table
// because only the primary language is compared (for example fr-FR and fr-CA are both LANG_FRENCH).
// Any language without a table falls back to English.
inline const UiStrings& GetStrings() {
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
        // case LANG_FRENCH: return kFrench;
        default: return kEnglish;
    }
}
