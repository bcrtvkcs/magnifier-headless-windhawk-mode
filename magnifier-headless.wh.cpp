// ==WindhawkMod==
// @id              magnifier-headless
// @name            Magnifier Headless Mode
// @description     Blocks the Magnifier window creation, keeping zoom functionality with win+"-" and win+"+" keyboard shortcuts.
// @version         0.8.0
// @author          BCRTVKCS
// @github          https://github.com/bcrtvkcs
// @twitter         https://x.com/bcrtvkcs
// @homepage        https://grdigital.pro
// @include         magnify.exe
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Magnifier Headless Mode
This mod blocks the Magnifier window from ever appearing, while keeping the zoom functionality (Win + `-` and Win + `+`) available. It also prevents the Magnifier from showing up in the taskbar.

This is achieved by hooking several Windows API functions (`CreateWindowExW`, `ShowWindow`, `SetWindowPos`, and `SetWindowLongPtrW`) to intercept any attempts to create, show, or change the style of the Magnifier window.
*/
// ==/WindhawkModReadme==

#include <windows.h>
#include <windhawk_api.h>

// ===========================
// THREAD-SAFE GLOBAL STATE
// ===========================

// Critical section to protect global state
CRITICAL_SECTION g_csGlobalState;
BOOL g_bCriticalSectionInitialized = FALSE;

// Global handle to our hidden host window (protected by g_csGlobalState)
volatile HWND g_hHostWnd = NULL;

// Atomic flag to track initialization status
volatile LONG g_lInitialized = 0;

// HWND cache for fast magnifier window detection (protected by g_csGlobalState)
#define MAX_CACHED_MAGNIFIER_WINDOWS 8
struct {
    HWND hwnd;
    BOOL isMagnifier;
} g_windowCache[MAX_CACHED_MAGNIFIER_WINDOWS] = {0};
int g_cacheIndex = 0;

// Helper: Safe enter/leave critical section
class AutoCriticalSection {
private:
    CRITICAL_SECTION* m_pCs;
    BOOL m_bEntered;
public:
    AutoCriticalSection(CRITICAL_SECTION* pCs) : m_pCs(pCs), m_bEntered(FALSE) {
        if (g_bCriticalSectionInitialized && pCs) {
            EnterCriticalSection(pCs);
            m_bEntered = TRUE;
        }
    }
    ~AutoCriticalSection() {
        if (m_bEntered && m_pCs) {
            LeaveCriticalSection(m_pCs);
        }
    }
};

// Thread-safe function to check if a window is the Magnifier window
BOOL IsMagnifierWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    // Check cache first (thread-safe)
    {
        AutoCriticalSection lock(&g_csGlobalState);
        for (int i = 0; i < MAX_CACHED_MAGNIFIER_WINDOWS; i++) {
            if (g_windowCache[i].hwnd == hwnd) {
                return g_windowCache[i].isMagnifier;
            }
        }
    }

    // Not in cache, check class name
    wchar_t className[256] = {0};
    if (!GetClassNameW(hwnd, className, sizeof(className)/sizeof(wchar_t))) {
        return FALSE;
    }

    // Check for known Magnifier class names
    BOOL isMagnifier = (wcscmp(className, L"MagUIClass") == 0 ||
                        wcscmp(className, L"ScreenMagnifierUIWnd") == 0);

    // Add to cache (thread-safe with circular buffer)
    {
        AutoCriticalSection lock(&g_csGlobalState);
        g_windowCache[g_cacheIndex].hwnd = hwnd;
        g_windowCache[g_cacheIndex].isMagnifier = isMagnifier;
        g_cacheIndex = (g_cacheIndex + 1) % MAX_CACHED_MAGNIFIER_WINDOWS;
    }

    return isMagnifier;
}

// --- HOOKS ---

// ShowWindow hook to catch attempts to show the Magnifier window
using ShowWindow_t = decltype(&ShowWindow);
ShowWindow_t ShowWindow_Original = nullptr;
BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    // Only proceed if initialization is complete
    if (InterlockedCompareExchange(&g_lInitialized, 0, 0) == 0) {
        return ShowWindow_Original ? ShowWindow_Original(hWnd, nCmdShow) : FALSE;
    }

    if (IsMagnifierWindow(hWnd) && nCmdShow != SW_HIDE) {
        // Pretend we showed it, but do nothing
        Wh_Log(L"Magnifier Headless: Blocked ShowWindow for HWND 0x%p (cmd: %d)", hWnd, nCmdShow);
        return TRUE;
    }
    return ShowWindow_Original(hWnd, nCmdShow);
}

// SetWindowPos hook to catch attempts to show the Magnifier window via position changes
using SetWindowPos_t = decltype(&SetWindowPos);
SetWindowPos_t SetWindowPos_Original = nullptr;
BOOL WINAPI SetWindowPos_Hook(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) {
    // Only proceed if initialization is complete
    if (InterlockedCompareExchange(&g_lInitialized, 0, 0) == 0) {
        return SetWindowPos_Original ? SetWindowPos_Original(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags) : FALSE;
    }

    if (IsMagnifierWindow(hWnd)) {
        UINT originalFlags = uFlags;
        uFlags &= ~SWP_SHOWWINDOW; // Remove the show flag
        uFlags |= SWP_HIDEWINDOW;  // Force hide flag
        if (originalFlags != uFlags) {
            Wh_Log(L"Magnifier Headless: Modified SetWindowPos flags for HWND 0x%p", hWnd);
        }
    }
    return SetWindowPos_Original(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

// SetWindowLongPtrW hook to catch attempts to make the window visible or add it to the taskbar
using SetWindowLongPtrW_t = decltype(&SetWindowLongPtrW);
SetWindowLongPtrW_t SetWindowLongPtrW_Original = nullptr;
LONG_PTR WINAPI SetWindowLongPtrW_Hook(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
    // Only proceed if initialization is complete
    if (InterlockedCompareExchange(&g_lInitialized, 0, 0) == 0) {
        return SetWindowLongPtrW_Original ? SetWindowLongPtrW_Original(hWnd, nIndex, dwNewLong) : 0;
    }

    if (IsMagnifierWindow(hWnd)) {
        LONG_PTR originalValue = dwNewLong;
        if (nIndex == GWL_STYLE) {
            dwNewLong &= ~WS_VISIBLE; // Remove the visible style
            if (originalValue != dwNewLong) {
                Wh_Log(L"Magnifier Headless: Removed WS_VISIBLE from style for HWND 0x%p", hWnd);
            }
        }
        if (nIndex == GWL_EXSTYLE) {
            dwNewLong &= ~WS_EX_APPWINDOW; // Remove the taskbar button style
            dwNewLong |= WS_EX_TOOLWINDOW;  // Add tool window style (no taskbar)
            if (originalValue != dwNewLong) {
                Wh_Log(L"Magnifier Headless: Modified extended style for HWND 0x%p", hWnd);
            }
        }
    }
    return SetWindowLongPtrW_Original(hWnd, nIndex, dwNewLong);
}

// CreateWindowExW hook to catch Magnifier window creation
using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t CreateWindowExW_Original = nullptr;
HWND WINAPI CreateWindowExW_Hook(
    DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
    int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu,
    HINSTANCE hInstance, LPVOID lpParam) {

    // Only proceed if initialization is complete
    if (InterlockedCompareExchange(&g_lInitialized, 0, 0) == 0) {
        return CreateWindowExW_Original ? CreateWindowExW_Original(dwExStyle, lpClassName, lpWindowName,
                                          dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam) : NULL;
    }

    BOOL isMagnifierClass = FALSE;
    if (((ULONG_PTR)lpClassName & ~(ULONG_PTR)0xffff) != 0) {
        if (wcscmp(lpClassName, L"MagUIClass") == 0 ||
            wcscmp(lpClassName, L"ScreenMagnifierUIWnd") == 0) {
            isMagnifierClass = TRUE;
            // Proactively remove styles that would make the window visible or show it in the taskbar
            dwStyle &= ~WS_VISIBLE;
            dwExStyle &= ~WS_EX_APPWINDOW;
            dwExStyle |= WS_EX_TOOLWINDOW; // Force tool window (no taskbar)
            Wh_Log(L"Magnifier Headless: Intercepting Magnifier window creation (class: %s)", lpClassName);
        }
    }

    HWND hwnd = CreateWindowExW_Original(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y,
                                  nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

    // If the created window is the Magnifier UI, immediately re-parent it to our hidden host window
    if (hwnd && isMagnifierClass) {
        Wh_Log(L"Magnifier Headless: Magnifier window created (HWND: 0x%p). Applying restrictions...", hwnd);

        // Thread-safe access to g_hHostWnd
        HWND hostWnd = NULL;
        {
            AutoCriticalSection lock(&g_csGlobalState);
            hostWnd = g_hHostWnd;
        }

        if (hostWnd) {
            SetParent(hwnd, hostWnd);
        }

        // Ensure it's explicitly hidden
        ShowWindow_Original(hwnd, SW_HIDE);

        // Force update styles
        SetWindowLongPtrW_Original(hwnd, GWL_STYLE, GetWindowLongPtrW(hwnd, GWL_STYLE) & ~WS_VISIBLE);
        SetWindowLongPtrW_Original(hwnd, GWL_EXSTYLE,
            (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & ~WS_EX_APPWINDOW) | WS_EX_TOOLWINDOW);
    }

    return hwnd;
}


// --- MOD INITIALIZATION ---

BOOL Wh_ModInit() {
    Wh_Log(L"Magnifier Headless: Initializing (thread-safe version)...");

    // Initialize critical section first (before any hook operations)
    if (!InitializeCriticalSectionAndSpinCount(&g_csGlobalState, 0x400)) {
        Wh_Log(L"Magnifier Headless: Failed to initialize critical section.");
        return FALSE;
    }
    g_bCriticalSectionInitialized = TRUE;

    // Set up all hooks BEFORE creating windows to prevent race conditions
    Wh_Log(L"Magnifier Headless: Setting up function hooks...");
    if (!Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_Hook, (void**)&CreateWindowExW_Original) ||
        !Wh_SetFunctionHook((void*)ShowWindow, (void*)ShowWindow_Hook, (void**)&ShowWindow_Original) ||
        !Wh_SetFunctionHook((void*)SetWindowPos, (void*)SetWindowPos_Hook, (void**)&SetWindowPos_Original) ||
        !Wh_SetFunctionHook((void*)SetWindowLongPtrW, (void*)SetWindowLongPtrW_Hook, (void**)&SetWindowLongPtrW_Original)) {
        Wh_Log(L"Magnifier Headless: Failed to set up one or more hooks.");
        DeleteCriticalSection(&g_csGlobalState);
        g_bCriticalSectionInitialized = FALSE;
        return FALSE;
    }

    // Now create the hidden window (after hooks are in place)
    Wh_Log(L"Magnifier Headless: Creating hidden host window...");
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.lpszClassName = L"MagnifierHeadlessHost";
    wc.hInstance = GetModuleHandle(NULL);

    if (!RegisterClassW(&wc)) {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_CLASS_ALREADY_EXISTS) {
            Wh_Log(L"Magnifier Headless: Failed to register window class (error: %lu).", dwError);
            DeleteCriticalSection(&g_csGlobalState);
            g_bCriticalSectionInitialized = FALSE;
            return FALSE;
        }
    }

    HWND hHostWnd = CreateWindowExW(
        0, wc.lpszClassName, L"Magnifier Headless Host", 0,
        0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL
    );

    if (!hHostWnd) {
        Wh_Log(L"Magnifier Headless: Failed to create host window (error: %lu).", GetLastError());
        DeleteCriticalSection(&g_csGlobalState);
        g_bCriticalSectionInitialized = FALSE;
        return FALSE;
    }

    // Thread-safe assignment using atomic operation
    {
        AutoCriticalSection lock(&g_csGlobalState);
        g_hHostWnd = hHostWnd;
    }

    Wh_Log(L"Magnifier Headless: Host window created (HWND: 0x%p).", hHostWnd);

    // Mark initialization as complete (atomic operation)
    InterlockedExchange(&g_lInitialized, 1);

    Wh_Log(L"Magnifier Headless: Initialization complete. All systems ready.");
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Magnifier Headless: Uninitializing...");

    // Mark as uninitialized (atomic operation)
    InterlockedExchange(&g_lInitialized, 0);

    // Thread-safe cleanup
    HWND hHostWnd = NULL;
    {
        AutoCriticalSection lock(&g_csGlobalState);
        hHostWnd = g_hHostWnd;
        g_hHostWnd = NULL;

        // Clear cache
        for (int i = 0; i < MAX_CACHED_MAGNIFIER_WINDOWS; i++) {
            g_windowCache[i].hwnd = NULL;
            g_windowCache[i].isMagnifier = FALSE;
        }
        g_cacheIndex = 0;
    }

    if (hHostWnd) {
        DestroyWindow(hHostWnd);
        Wh_Log(L"Magnifier Headless: Host window destroyed.");
    }

    // Cleanup critical section
    if (g_bCriticalSectionInitialized) {
        DeleteCriticalSection(&g_csGlobalState);
        g_bCriticalSectionInitialized = FALSE;
    }

    Wh_Log(L"Magnifier Headless: Uninitialization complete.");
}
