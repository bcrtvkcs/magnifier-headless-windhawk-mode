// ==WindhawkMod==
// @id              magnifier-headless
// @name            Magnifier Headless Mode
// @description     Blocks the Magnifier window creation, keeping zoom functionality with win+"-" and win+"+" keyboard shortcuts.
// @version         1.0.0
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

## Features
- Completely hides the Magnifier UI while preserving zoom functionality
- Thread-safe implementation with race condition protection
- Performance optimized with HWND caching
- Comprehensive API coverage for all window visibility methods

## Hooked APIs
The mod hooks multiple Windows API functions to ensure complete coverage:

**Core Window APIs:**
- `CreateWindowExW` - Intercepts window creation
- `ShowWindow` - Blocks window showing
- `SetWindowPos` - Prevents position-based showing
- `SetWindowLongPtrW` - Blocks style changes

**Layered Window APIs:**
- `UpdateLayeredWindow` - Blocks layered window updates
- `SetLayeredWindowAttributes` - Blocks transparency changes

**Animation & Foreground APIs:**
- `AnimateWindow` - Blocks animated showing
- `BringWindowToTop` - Prevents bringing to front
- `SetForegroundWindow` - Blocks foreground activation

**Advanced APIs:**
- `SetWindowRgn` - Blocks region-based visibility
- `DwmSetWindowAttribute` - Blocks DWM attribute changes (Windows 11+)

**Window Message Interception:**
- Window Procedure Subclassing - Direct interception of window messages
  * Blocks `WM_SHOWWINDOW` (show requests)
  * Modifies `WM_WINDOWPOSCHANGING` (prevents showing via position changes)
  * Enforces hiding on `WM_WINDOWPOSCHANGED`
  * Blocks `WM_ACTIVATE` and `WM_NCACTIVATE` (activation prevention)
  * Suppresses `WM_PAINT` and `WM_ERASEBKGND` (no visual artifacts)
  * Blocks `WM_SETFOCUS` (focus prevention)
  * Blocks `WM_MOUSEACTIVATE` (mouse activation prevention)
  * Blocks `WM_SYSCOMMAND` (SC_RESTORE, SC_MAXIMIZE prevention)
- `WH_CALLWNDPROC` hook - Detects Magnifier windows and applies subclassing automatically

## Technical Implementation
- Uses CRITICAL_SECTION for thread-safe global state management
- Atomic operations (InterlockedExchange) for initialization flags
- Circular buffer cache for fast window detection
- RAII pattern (AutoCriticalSection) for safe lock management
- Proper hook ordering to prevent race conditions
*/
// ==/WindhawkModReadme==

#include <windows.h>
#include <windhawk_api.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

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

// Window procedure hook handle
HHOOK g_hCallWndProcHook = NULL;

// Original window procedure and HWND for subclassed Magnifier window (protected by g_csGlobalState)
WNDPROC g_OriginalMagnifierWndProc = NULL;
HWND g_hSubclassedMagnifierWnd = NULL;

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

// UpdateLayeredWindow hook to prevent layered window updates from showing the window
using UpdateLayeredWindow_t = decltype(&UpdateLayeredWindow);
UpdateLayeredWindow_t UpdateLayeredWindow_Original = nullptr;
BOOL WINAPI UpdateLayeredWindow_Hook(
    HWND hWnd, HDC hdcDst, POINT* pptDst, SIZE* psize,
    HDC hdcSrc, POINT* pptSrc, COLORREF crKey,
    BLENDFUNCTION* pblend, DWORD dwFlags) {

    // Only proceed if initialization is complete
    if (InterlockedCompareExchange(&g_lInitialized, 0, 0) == 0) {
        return UpdateLayeredWindow_Original ? UpdateLayeredWindow_Original(hWnd, hdcDst, pptDst, psize,
                                              hdcSrc, pptSrc, crKey, pblend, dwFlags) : FALSE;
    }

    if (IsMagnifierWindow(hWnd)) {
        Wh_Log(L"Magnifier Headless: Blocked UpdateLayeredWindow for HWND 0x%p", hWnd);
        return TRUE; // Pretend it succeeded
    }
    return UpdateLayeredWindow_Original(hWnd, hdcDst, pptDst, psize, hdcSrc, pptSrc, crKey, pblend, dwFlags);
}

// SetLayeredWindowAttributes hook to prevent layered window attribute changes
using SetLayeredWindowAttributes_t = decltype(&SetLayeredWindowAttributes);
SetLayeredWindowAttributes_t SetLayeredWindowAttributes_Original = nullptr;
BOOL WINAPI SetLayeredWindowAttributes_Hook(HWND hWnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags) {
    // Only proceed if initialization is complete
    if (InterlockedCompareExchange(&g_lInitialized, 0, 0) == 0) {
        return SetLayeredWindowAttributes_Original ? SetLayeredWindowAttributes_Original(hWnd, crKey, bAlpha, dwFlags) : FALSE;
    }

    if (IsMagnifierWindow(hWnd)) {
        Wh_Log(L"Magnifier Headless: Blocked SetLayeredWindowAttributes for HWND 0x%p", hWnd);
        return TRUE; // Pretend it succeeded
    }
    return SetLayeredWindowAttributes_Original(hWnd, crKey, bAlpha, dwFlags);
}

// AnimateWindow hook to prevent animated window showing
using AnimateWindow_t = decltype(&AnimateWindow);
AnimateWindow_t AnimateWindow_Original = nullptr;
BOOL WINAPI AnimateWindow_Hook(HWND hWnd, DWORD dwTime, DWORD dwFlags) {
    // Only proceed if initialization is complete
    if (InterlockedCompareExchange(&g_lInitialized, 0, 0) == 0) {
        return AnimateWindow_Original ? AnimateWindow_Original(hWnd, dwTime, dwFlags) : FALSE;
    }

    if (IsMagnifierWindow(hWnd)) {
        // Only block if it's trying to show the window
        if (!(dwFlags & AW_HIDE)) {
            Wh_Log(L"Magnifier Headless: Blocked AnimateWindow (show) for HWND 0x%p", hWnd);
            return TRUE; // Pretend it succeeded
        }
    }
    return AnimateWindow_Original(hWnd, dwTime, dwFlags);
}

// BringWindowToTop hook to prevent bringing window to foreground
using BringWindowToTop_t = decltype(&BringWindowToTop);
BringWindowToTop_t BringWindowToTop_Original = nullptr;
BOOL WINAPI BringWindowToTop_Hook(HWND hWnd) {
    // Only proceed if initialization is complete
    if (InterlockedCompareExchange(&g_lInitialized, 0, 0) == 0) {
        return BringWindowToTop_Original ? BringWindowToTop_Original(hWnd) : FALSE;
    }

    if (IsMagnifierWindow(hWnd)) {
        Wh_Log(L"Magnifier Headless: Blocked BringWindowToTop for HWND 0x%p", hWnd);
        return TRUE; // Pretend it succeeded
    }
    return BringWindowToTop_Original(hWnd);
}

// SetForegroundWindow hook to prevent setting as foreground window
using SetForegroundWindow_t = decltype(&SetForegroundWindow);
SetForegroundWindow_t SetForegroundWindow_Original = nullptr;
BOOL WINAPI SetForegroundWindow_Hook(HWND hWnd) {
    // Only proceed if initialization is complete
    if (InterlockedCompareExchange(&g_lInitialized, 0, 0) == 0) {
        return SetForegroundWindow_Original ? SetForegroundWindow_Original(hWnd) : FALSE;
    }

    if (IsMagnifierWindow(hWnd)) {
        Wh_Log(L"Magnifier Headless: Blocked SetForegroundWindow for HWND 0x%p", hWnd);
        return TRUE; // Pretend it succeeded
    }
    return SetForegroundWindow_Original(hWnd);
}

// SetWindowRgn hook to prevent region changes that might make window visible
using SetWindowRgn_t = decltype(&SetWindowRgn);
SetWindowRgn_t SetWindowRgn_Original = nullptr;
int WINAPI SetWindowRgn_Hook(HWND hWnd, HRGN hRgn, BOOL bRedraw) {
    // Only proceed if initialization is complete
    if (InterlockedCompareExchange(&g_lInitialized, 0, 0) == 0) {
        return SetWindowRgn_Original ? SetWindowRgn_Original(hWnd, hRgn, bRedraw) : 0;
    }

    if (IsMagnifierWindow(hWnd)) {
        // Block redraw for magnifier window
        bRedraw = FALSE;
        Wh_Log(L"Magnifier Headless: Modified SetWindowRgn (disabled redraw) for HWND 0x%p", hWnd);
    }
    return SetWindowRgn_Original(hWnd, hRgn, bRedraw);
}

// DwmSetWindowAttribute hook for Windows 11+ DWM features
using DwmSetWindowAttribute_t = HRESULT (WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
DwmSetWindowAttribute_t DwmSetWindowAttribute_Original = nullptr;
HRESULT WINAPI DwmSetWindowAttribute_Hook(HWND hWnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute) {
    // Only proceed if initialization is complete
    if (InterlockedCompareExchange(&g_lInitialized, 0, 0) == 0) {
        return DwmSetWindowAttribute_Original ? DwmSetWindowAttribute_Original(hWnd, dwAttribute, pvAttribute, cbAttribute) : E_FAIL;
    }

    if (IsMagnifierWindow(hWnd)) {
        // Block certain DWM attributes that could make the window visible
        if (dwAttribute == DWMWA_CLOAK || dwAttribute == DWMWA_NCRENDERING_ENABLED) {
            Wh_Log(L"Magnifier Headless: Blocked DwmSetWindowAttribute (attr: %lu) for HWND 0x%p", dwAttribute, hWnd);
            return S_OK; // Pretend it succeeded
        }
    }
    return DwmSetWindowAttribute_Original(hWnd, dwAttribute, pvAttribute, cbAttribute);
}

// --- WINDOW PROCEDURE HOOK ---

// Subclassed window procedure for Magnifier window
LRESULT CALLBACK MagnifierWndProc_Hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SHOWWINDOW:
            // Block WM_SHOWWINDOW if trying to show
            if (wParam) {
                Wh_Log(L"Magnifier Headless: Blocked WM_SHOWWINDOW in WndProc for HWND 0x%p", hWnd);
                return 0; // Message handled
            }
            break;

        case WM_WINDOWPOSCHANGING:
            // Modify WINDOWPOS structure to prevent showing
            if (lParam) {
                WINDOWPOS* pWp = (WINDOWPOS*)lParam;
                BOOL modified = FALSE;

                if (!(pWp->flags & SWP_NOACTIVATE)) {
                    pWp->flags |= SWP_NOACTIVATE; // Prevent activation
                    modified = TRUE;
                }
                if (pWp->flags & SWP_SHOWWINDOW) {
                    pWp->flags &= ~SWP_SHOWWINDOW; // Remove show flag
                    pWp->flags |= SWP_HIDEWINDOW;  // Add hide flag
                    modified = TRUE;
                }

                if (modified) {
                    Wh_Log(L"Magnifier Headless: Modified WM_WINDOWPOSCHANGING in WndProc for HWND 0x%p", hWnd);
                }
            }
            break;

        case WM_WINDOWPOSCHANGED:
            // Ensure window remains hidden after position change
            if (IsWindowVisible(hWnd)) {
                ShowWindow_Original(hWnd, SW_HIDE);
                Wh_Log(L"Magnifier Headless: Re-hid window in WndProc after WM_WINDOWPOSCHANGED for HWND 0x%p", hWnd);
            }
            break;

        case WM_ACTIVATE:
        case WM_NCACTIVATE:
            // Block activation
            if (wParam != WA_INACTIVE) {
                Wh_Log(L"Magnifier Headless: Blocked activation message 0x%X in WndProc for HWND 0x%p", uMsg, hWnd);
                return 0; // Message handled
            }
            break;

        case WM_PAINT:
        case WM_ERASEBKGND:
            // Suppress painting
            Wh_Log(L"Magnifier Headless: Suppressed paint message 0x%X in WndProc for HWND 0x%p", uMsg, hWnd);
            ValidateRect(hWnd, NULL); // Mark as painted
            return 0; // Message handled
            break;

        case WM_SETFOCUS:
            // Block focus - set focus back to NULL
            Wh_Log(L"Magnifier Headless: Blocked WM_SETFOCUS in WndProc for HWND 0x%p", hWnd);
            SetFocus(NULL);
            return 0; // Message handled
            break;

        case WM_MOUSEACTIVATE:
            // Prevent mouse activation
            Wh_Log(L"Magnifier Headless: Blocked WM_MOUSEACTIVATE in WndProc for HWND 0x%p", hWnd);
            return MA_NOACTIVATE;
            break;

        case WM_SYSCOMMAND:
            // Block system commands that might show the window
            if (wParam == SC_RESTORE || wParam == SC_MAXIMIZE) {
                Wh_Log(L"Magnifier Headless: Blocked WM_SYSCOMMAND (0x%X) in WndProc for HWND 0x%p", wParam, hWnd);
                return 0;
            }
            break;
    }

    // Call original window procedure for unhandled messages
    return g_OriginalMagnifierWndProc ? CallWindowProcW(g_OriginalMagnifierWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// CallWndProc hook to detect and subclass Magnifier windows
LRESULT CALLBACK CallWndProc_Hook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && InterlockedCompareExchange(&g_lInitialized, 0, 0) != 0) {
        CWPSTRUCT* pCwp = (CWPSTRUCT*)lParam;

        if (pCwp && IsMagnifierWindow(pCwp->hwnd)) {
            // Thread-safe check if already subclassed
            BOOL alreadySubclassed = FALSE;
            {
                AutoCriticalSection lock(&g_csGlobalState);
                alreadySubclassed = (g_hSubclassedMagnifierWnd == pCwp->hwnd);
            }

            if (!alreadySubclassed) {
                // Check if this window's WndProc is not already our hook
                WNDPROC currentProc = (WNDPROC)GetWindowLongPtrW(pCwp->hwnd, GWLP_WNDPROC);
                if (currentProc != MagnifierWndProc_Hook && currentProc != NULL) {
                    // Thread-safe subclassing
                    {
                        AutoCriticalSection lock(&g_csGlobalState);
                        g_OriginalMagnifierWndProc = currentProc;
                        g_hSubclassedMagnifierWnd = pCwp->hwnd;
                    }

                    SetWindowLongPtrW(pCwp->hwnd, GWLP_WNDPROC, (LONG_PTR)MagnifierWndProc_Hook);
                    Wh_Log(L"Magnifier Headless: Subclassed Magnifier window (HWND: 0x%p, Original WndProc: 0x%p)", pCwp->hwnd, currentProc);
                }
            }
        }
    }

    return CallNextHookEx(g_hCallWndProcHook, nCode, wParam, lParam);
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

    // Core window hooks
    if (!Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_Hook, (void**)&CreateWindowExW_Original) ||
        !Wh_SetFunctionHook((void*)ShowWindow, (void*)ShowWindow_Hook, (void**)&ShowWindow_Original) ||
        !Wh_SetFunctionHook((void*)SetWindowPos, (void*)SetWindowPos_Hook, (void**)&SetWindowPos_Original) ||
        !Wh_SetFunctionHook((void*)SetWindowLongPtrW, (void*)SetWindowLongPtrW_Hook, (void**)&SetWindowLongPtrW_Original)) {
        Wh_Log(L"Magnifier Headless: Failed to set up core window hooks.");
        DeleteCriticalSection(&g_csGlobalState);
        g_bCriticalSectionInitialized = FALSE;
        return FALSE;
    }

    // Layered window hooks
    if (!Wh_SetFunctionHook((void*)UpdateLayeredWindow, (void*)UpdateLayeredWindow_Hook, (void**)&UpdateLayeredWindow_Original) ||
        !Wh_SetFunctionHook((void*)SetLayeredWindowAttributes, (void*)SetLayeredWindowAttributes_Hook, (void**)&SetLayeredWindowAttributes_Original)) {
        Wh_Log(L"Magnifier Headless: Failed to set up layered window hooks.");
        DeleteCriticalSection(&g_csGlobalState);
        g_bCriticalSectionInitialized = FALSE;
        return FALSE;
    }

    // Animation and foreground hooks
    if (!Wh_SetFunctionHook((void*)AnimateWindow, (void*)AnimateWindow_Hook, (void**)&AnimateWindow_Original) ||
        !Wh_SetFunctionHook((void*)BringWindowToTop, (void*)BringWindowToTop_Hook, (void**)&BringWindowToTop_Original) ||
        !Wh_SetFunctionHook((void*)SetForegroundWindow, (void*)SetForegroundWindow_Hook, (void**)&SetForegroundWindow_Original)) {
        Wh_Log(L"Magnifier Headless: Failed to set up animation/foreground hooks.");
        DeleteCriticalSection(&g_csGlobalState);
        g_bCriticalSectionInitialized = FALSE;
        return FALSE;
    }

    // Region hook
    if (!Wh_SetFunctionHook((void*)SetWindowRgn, (void*)SetWindowRgn_Hook, (void**)&SetWindowRgn_Original)) {
        Wh_Log(L"Magnifier Headless: Failed to set up region hook.");
        DeleteCriticalSection(&g_csGlobalState);
        g_bCriticalSectionInitialized = FALSE;
        return FALSE;
    }

    // DWM hook (optional - may not exist on older Windows versions)
    HMODULE hDwmapi = LoadLibraryW(L"dwmapi.dll");
    if (hDwmapi) {
        DwmSetWindowAttribute_Original = (DwmSetWindowAttribute_t)GetProcAddress(hDwmapi, "DwmSetWindowAttribute");
        if (DwmSetWindowAttribute_Original) {
            if (!Wh_SetFunctionHook((void*)DwmSetWindowAttribute_Original, (void*)DwmSetWindowAttribute_Hook, (void**)&DwmSetWindowAttribute_Original)) {
                Wh_Log(L"Magnifier Headless: Warning - Failed to set up DWM hook (non-critical).");
            } else {
                Wh_Log(L"Magnifier Headless: DWM hook set up successfully.");
            }
        }
    }

    Wh_Log(L"Magnifier Headless: All hooks set up successfully.");

    // Install window procedure hook to intercept messages
    Wh_Log(L"Magnifier Headless: Installing window procedure hook...");
    g_hCallWndProcHook = SetWindowsHookExW(WH_CALLWNDPROC, CallWndProc_Hook, NULL, GetCurrentThreadId());
    if (!g_hCallWndProcHook) {
        Wh_Log(L"Magnifier Headless: Warning - Failed to install window procedure hook (error: %lu).", GetLastError());
        // Non-critical, continue anyway
    } else {
        Wh_Log(L"Magnifier Headless: Window procedure hook installed successfully.");
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

    // Unhook window procedure hook
    if (g_hCallWndProcHook) {
        UnhookWindowsHookEx(g_hCallWndProcHook);
        g_hCallWndProcHook = NULL;
        Wh_Log(L"Magnifier Headless: Window procedure hook removed.");
    }

    // Restore original window procedure if subclassed
    HWND hSubclassedWnd = NULL;
    WNDPROC originalProc = NULL;
    {
        AutoCriticalSection lock(&g_csGlobalState);
        hSubclassedWnd = g_hSubclassedMagnifierWnd;
        originalProc = g_OriginalMagnifierWndProc;
        g_hSubclassedMagnifierWnd = NULL;
        g_OriginalMagnifierWndProc = NULL;
    }

    if (hSubclassedWnd && IsWindow(hSubclassedWnd) && originalProc) {
        SetWindowLongPtrW(hSubclassedWnd, GWLP_WNDPROC, (LONG_PTR)originalProc);
        Wh_Log(L"Magnifier Headless: Restored original WndProc for HWND 0x%p", hSubclassedWnd);
    }

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
