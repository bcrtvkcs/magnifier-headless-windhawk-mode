// ==WindhawkMod==
// @id              magnifier-headless-mode
// @name            Magnifier Headless Mode
// @description     Blocks all Magnifier window creation, keeping only zoom functionality
// @version         1.0
// @author          BCRTVKCS
// @github          bcrtvkcs
// @twitter         bcrtvkcs
// @homepage        https://grdigital.pro
// @include         magnify.exe
// @exclude         ^(?!.*magnify.exe)
// @compilerOptions -luser32 -lkernel32
// ==/WindhawkMod==

#include <windows.h>
#include <tlhelp32.h>

// Hook CreateWindowExW to block all window creation
using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t CreateWindowExW_Original;

HWND WINAPI CreateWindowExW_Hook(DWORD dwExStyle, LPCWSTR lpClassName, 
                                LPCWSTR lpWindowName, DWORD dwStyle,
                                int X, int Y, int nWidth, int nHeight,
                                HWND hWndParent, HMENU hMenu, 
                                HINSTANCE hInstance, LPVOID lpParam) {
    
    // Block ALL window creation for magnifier
    // Return a dummy invisible window handle to prevent crashes
    static HWND dummyWindow = NULL;
    
    if (!dummyWindow) {
        // Create one hidden dummy window that magnifier can reference
        dummyWindow = CreateWindowExW_Original(
            WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            L"Static", 
            L"", 
            WS_POPUP,
            -32000, -32000, 1, 1,  // Off-screen position
            NULL, NULL, hInstance, NULL
        );
        
        if (dummyWindow) {
            ShowWindow(dummyWindow, SW_HIDE);
        }
    }
    
    // Return the same dummy window for all creation requests
    return dummyWindow;
}

// Hook CreateWindowExA
using CreateWindowExA_t = decltype(&CreateWindowExA);
CreateWindowExA_t CreateWindowExA_Original;

HWND WINAPI CreateWindowExA_Hook(DWORD dwExStyle, LPCSTR lpClassName,
                                LPCSTR lpWindowName, DWORD dwStyle,
                                int X, int Y, int nWidth, int nHeight,
                                HWND hWndParent, HMENU hMenu,
                                HINSTANCE hInstance, LPVOID lpParam) {
    
    // Same logic for ANSI version
    static HWND dummyWindow = NULL;
    
    if (!dummyWindow) {
        dummyWindow = CreateWindowExA_Original(
            WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            "Static",
            "",
            WS_POPUP,
            -32000, -32000, 1, 1,
            NULL, NULL, hInstance, NULL
        );
        
        if (dummyWindow) {
            ShowWindow(dummyWindow, SW_HIDE);
        }
    }
    
    return dummyWindow;
}

// Hook ShowWindow to prevent any window from being shown
using ShowWindow_t = decltype(&ShowWindow);
ShowWindow_t ShowWindow_Original;

BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    // Never show any windows - always return success but don't actually show
    if (nCmdShow == SW_HIDE || nCmdShow == SW_MINIMIZE) {
        // Allow hiding/minimizing
        return ShowWindow_Original(hWnd, nCmdShow);
    }
    
    // Block all show commands
    return TRUE; // Pretend it worked
}

// Hook SetWindowPos to prevent positioning/showing
using SetWindowPos_t = decltype(&SetWindowPos);
SetWindowPos_t SetWindowPos_Original;

BOOL WINAPI SetWindowPos_Hook(HWND hWnd, HWND hWndInsertAfter, int X, int Y, 
                             int cx, int cy, UINT uFlags) {
    // Force all windows to be hidden and off-screen
    return SetWindowPos_Original(hWnd, HWND_BOTTOM, -32000, -32000, 
                                1, 1, SWP_HIDEWINDOW | SWP_NOACTIVATE);
}

// Hook SetWindowLongW to prevent window property changes that might show windows
using SetWindowLongW_t = decltype(&SetWindowLongW);
SetWindowLongW_t SetWindowLongW_Original;

LONG WINAPI SetWindowLongW_Hook(HWND hWnd, int nIndex, LONG dwNewLong) {
    // Block visibility-related style changes
    if (nIndex == GWL_STYLE) {
        // Remove visible styles, force popup hidden style
        dwNewLong &= ~WS_VISIBLE;
        dwNewLong |= WS_POPUP;
        dwNewLong &= ~WS_OVERLAPPED;
        dwNewLong &= ~WS_OVERLAPPEDWINDOW;
    }
    
    if (nIndex == GWL_EXSTYLE) {
        // Add toolwindow and noactivate styles
        dwNewLong |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    }
    
    return SetWindowLongW_Original(hWnd, nIndex, dwNewLong);
}

// Hook SetWindowLongA
using SetWindowLongA_t = decltype(&SetWindowLongA);
SetWindowLongA_t SetWindowLongA_Original;

LONG WINAPI SetWindowLongA_Hook(HWND hWnd, int nIndex, LONG dwNewLong) {
    if (nIndex == GWL_STYLE) {
        dwNewLong &= ~WS_VISIBLE;
        dwNewLong |= WS_POPUP;
        dwNewLong &= ~WS_OVERLAPPED;
        dwNewLong &= ~WS_OVERLAPPEDWINDOW;
    }
    
    if (nIndex == GWL_EXSTYLE) {
        dwNewLong |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    }
    
    return SetWindowLongA_Original(hWnd, nIndex, dwNewLong);
}

// Hook UpdateWindow to prevent window updates
using UpdateWindow_t = decltype(&UpdateWindow);
UpdateWindow_t UpdateWindow_Original;

BOOL WINAPI UpdateWindow_Hook(HWND hWnd) {
    // Don't update any windows - just return success
    return TRUE;
}

// Hook RedrawWindow to prevent redraws
using RedrawWindow_t = decltype(&RedrawWindow);
RedrawWindow_t RedrawWindow_Original;

BOOL WINAPI RedrawWindow_Hook(HWND hWnd, CONST RECT *lprcUpdate, HRGN hrgnUpdate, UINT flags) {
    // Don't redraw - just return success
    return TRUE;
}

// Hook InvalidateRect to prevent invalidation
using InvalidateRect_t = decltype(&InvalidateRect);
InvalidateRect_t InvalidateRect_Original;

BOOL WINAPI InvalidateRect_Hook(HWND hWnd, CONST RECT *lpRect, BOOL bErase) {
    // Don't invalidate - just return success
    return TRUE;
}

// Hook SetForegroundWindow to prevent window activation
using SetForegroundWindow_t = decltype(&SetForegroundWindow);
SetForegroundWindow_t SetForegroundWindow_Original;

BOOL WINAPI SetForegroundWindow_Hook(HWND hWnd) {
    // Never bring magnifier windows to foreground
    return TRUE;
}

// Hook SetActiveWindow 
using SetActiveWindow_t = decltype(&SetActiveWindow);
SetActiveWindow_t SetActiveWindow_Original;

HWND WINAPI SetActiveWindow_Hook(HWND hWnd) {
    // Never activate magnifier windows
    return NULL;
}

// Hook SetFocus
using SetFocus_t = decltype(&SetFocus);
SetFocus_t SetFocus_Original;

HWND WINAPI SetFocus_Hook(HWND hWnd) {
    // Never focus magnifier windows
    return NULL;
}

// Hide or destroy any windows that might have been created before hooks were set
BOOL CALLBACK HideWindowEnumProc(HWND hWnd, LPARAM lParam) {
    ShowWindow(hWnd, SW_HIDE);
    DestroyWindow(hWnd);
    return TRUE;
}

void HideExistingProcessWindows() {
    DWORD pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID == pid) {
                    EnumThreadWindows(te.th32ThreadID, HideWindowEnumProc, 0);
                }
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
    }
}

BOOL Wh_ModInit() {
    // Hook window creation
    Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_Hook, (void**)&CreateWindowExW_Original);
    Wh_SetFunctionHook((void*)CreateWindowExA, (void*)CreateWindowExA_Hook, (void**)&CreateWindowExA_Original);
    
    // Hook window visibility
    Wh_SetFunctionHook((void*)ShowWindow, (void*)ShowWindow_Hook, (void**)&ShowWindow_Original);
    Wh_SetFunctionHook((void*)SetWindowPos, (void*)SetWindowPos_Hook, (void**)&SetWindowPos_Original);
    
    // Hook window properties
    Wh_SetFunctionHook((void*)SetWindowLongW, (void*)SetWindowLongW_Hook, (void**)&SetWindowLongW_Original);
    Wh_SetFunctionHook((void*)SetWindowLongA, (void*)SetWindowLongA_Hook, (void**)&SetWindowLongA_Original);
    
    // Hook window drawing
    Wh_SetFunctionHook((void*)UpdateWindow, (void*)UpdateWindow_Hook, (void**)&UpdateWindow_Original);
    Wh_SetFunctionHook((void*)RedrawWindow, (void*)RedrawWindow_Hook, (void**)&RedrawWindow_Original);
    Wh_SetFunctionHook((void*)InvalidateRect, (void*)InvalidateRect_Hook, (void**)&InvalidateRect_Original);
    
    // Hook window activation
    Wh_SetFunctionHook((void*)SetForegroundWindow, (void*)SetForegroundWindow_Hook, (void**)&SetForegroundWindow_Original);
    Wh_SetFunctionHook((void*)SetActiveWindow, (void*)SetActiveWindow_Hook, (void**)&SetActiveWindow_Original);
    Wh_SetFunctionHook((void*)SetFocus, (void*)SetFocus_Hook, (void**)&SetFocus_Original);

    // Hide or destroy any windows that were created before hooks were installed
    HideExistingProcessWindows();

    return TRUE;
}

void Wh_ModUninit() {
    // Cleanup handled by Windhawk
}
