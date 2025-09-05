// ==WindhawkMod==
// @id              magnifier-headless-mode
// @name            Magnifier Headless Mode
// @description     Hides the Magnifier interface window named "Büyüteç" or "Magnifier"
// @version         0.0.2test3
// @author          BCRTVKCS
// @github          bcrtvkcs
// @twitter         bcrtvkcs
// @homepage        https://grdigital.pro
// @include         magnify.exe
// @compilerOptions -luser32 -lkernel32 -ldwmapi
// ==/WindhawkMod==

#include <windows.h>
#include <dwmapi.h>
#include <string>

// Function to check if window title contains magnifier-related text
bool IsMagnifierWindow(HWND hwnd) {
    wchar_t title[256] = {0};
    wchar_t className[256] = {0};
    
    GetWindowTextW(hwnd, title, 256);
    GetClassNameW(hwnd, className, 256);
    
    // Convert to lowercase for case-insensitive comparison
    std::wstring windowTitle(title);
    std::wstring windowClass(className);
    
    // Check for various magnifier-related strings
    if (wcsstr(title, L"Büyüteç") != nullptr ||
        wcsstr(title, L"Magnifier") != nullptr ||
        wcsstr(title, L"büyüteç") != nullptr ||
        wcsstr(title, L"magnifier") != nullptr ||
        wcsstr(title, L"Ekran Büyüteci") != nullptr ||
        wcsstr(title, L"Screen Magnifier") != nullptr) {
        return true;
    }
    
    // Check for known Magnifier class names
    if (wcscmp(className, L"MagUIClass") == 0 ||
        wcscmp(className, L"ScreenMagnifierUIWnd") == 0 ||
        wcscmp(className, L"ApplicationFrameWindow") == 0 ||
        wcscmp(className, L"Windows.UI.Core.CoreWindow") == 0) {
        
        // For ApplicationFrameWindow and CoreWindow, do additional title check
        if (wcscmp(className, L"ApplicationFrameWindow") == 0 || 
            wcscmp(className, L"Windows.UI.Core.CoreWindow") == 0) {
            if (wcsstr(title, L"Büyüteç") != nullptr || 
                wcsstr(title, L"Magnifier") != nullptr) {
                return true;
            }
        } else {
            return true;
        }
    }
    
    return false;
}

// Enum windows callback to find and hide all magnifier windows
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (IsMagnifierWindow(hwnd)) {
        // Multiple methods to hide the window
        ShowWindow(hwnd, SW_HIDE);
        
        // Make window fully transparent
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, 
            GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
        SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
        
        // Move window off-screen as backup
        SetWindowPos(hwnd, HWND_BOTTOM, -10000, -10000, 0, 0, 
            SWP_NOSIZE | SWP_NOACTIVATE | SWP_HIDEWINDOW);
        
        // Try to minimize
        ShowWindow(hwnd, SW_MINIMIZE);
        ShowWindow(hwnd, SW_HIDE);
        
        // Use DWM to exclude from peek
        BOOL cloak = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
    }
    return TRUE;
}

// Function to hide all magnifier windows
void HideMagnifierWindows() {
    // Enumerate all top-level windows
    EnumWindows(EnumWindowsProc, 0);
    
    // Also specifically search for known window titles
    const wchar_t* titles[] = {
        L"Büyüteç",
        L"Magnifier",
        L"Ekran Büyüteci",
        L"Screen Magnifier"
    };
    
    for (int i = 0; i < sizeof(titles) / sizeof(titles[0]); i++) {
        HWND hwnd = NULL;
        do {
            hwnd = FindWindowW(NULL, titles[i]);
            if (hwnd) {
                ShowWindow(hwnd, SW_HIDE);
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, 
                    GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
                SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
                
                BOOL cloak = TRUE;
                DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
            }
        } while (hwnd);
    }
}

// Hook for WH_CBT to catch window creation
LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HCBT_ACTIVATE || nCode == HCBT_CREATEWND) {
        HWND hwnd = (HWND)wParam;
        
        // Delay check to allow window title to be set
        Sleep(10);
        
        if (IsMagnifierWindow(hwnd)) {
            ShowWindow(hwnd, SW_HIDE);
            
            // Post a message to hide it again after a delay
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Global hook handle
HHOOK g_hook = NULL;

// Thread function for continuous monitoring
DWORD WINAPI MonitorThread(LPVOID lpParam) {
    while (true) {
        HideMagnifierWindows();
        Sleep(100); // Check every 100ms
    }
    return 0;
}

// Handle for monitor thread
HANDLE g_thread = NULL;

// Mod initialization
BOOL Wh_ModInit() {
    // Initial hide
    HideMagnifierWindows();
    
    // Install CBT hook for all threads
    g_hook = SetWindowsHookExW(WH_CBT, CBTProc, GetModuleHandleW(NULL), 0);
    
    // Create monitoring thread
    g_thread = CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);
    
    return TRUE;
}

// Mod uninitialization  
void Wh_ModUninit() {
    // Terminate monitoring thread
    if (g_thread) {
        TerminateThread(g_thread, 0);
        CloseHandle(g_thread);
        g_thread = NULL;
    }
    
    // Unhook
    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = NULL;
    }
    
    // Restore hidden windows
    HWND hwnd = NULL;
    const wchar_t* titles[] = {
        L"Büyüteç",
        L"Magnifier"
    };
    
    for (int i = 0; i < sizeof(titles) / sizeof(titles[0]); i++) {
        hwnd = FindWindowW(NULL, titles[i]);
        if (hwnd) {
            ShowWindow(hwnd, SW_SHOW);
            
            // Remove transparency and cloaking
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, 
                GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & ~(WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW));
            
            BOOL cloak = FALSE;
            DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
            
            // Restore position
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, 
                SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOMOVE);
        }
    }
}

// Settings
BOOL Wh_ModSettingsInit() {
    return TRUE;
}

// Symbol hook for CreateWindowExW to catch window creation early
using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t CreateWindowExW_Original;

HWND WINAPI CreateWindowExW_Hook(
    DWORD dwExStyle,
    LPCWSTR lpClassName,
    LPCWSTR lpWindowName,
    DWORD dwStyle,
    int X,
    int Y,
    int nWidth,
    int nHeight,
    HWND hWndParent,
    HMENU hMenu,
    HINSTANCE hInstance,
    LPVOID lpParam
) {
    HWND hwnd = CreateWindowExW_Original(
        dwExStyle, lpClassName, lpWindowName, dwStyle,
        X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam
    );
    
    // Check if this is a magnifier window
    if (hwnd && lpWindowName) {
        if (wcsstr(lpWindowName, L"Büyüteç") || 
            wcsstr(lpWindowName, L"Magnifier")) {
            ShowWindow(hwnd, SW_HIDE);
        }
    }
    
    return hwnd;
}

// Symbol hook for SetWindowTextW to catch title changes
using SetWindowTextW_t = decltype(&SetWindowTextW);
SetWindowTextW_t SetWindowTextW_Original;

BOOL WINAPI SetWindowTextW_Hook(HWND hWnd, LPCWSTR lpString) {
    BOOL result = SetWindowTextW_Original(hWnd, lpString);
    
    if (result && lpString) {
        if (wcsstr(lpString, L"Büyüteç") || 
            wcsstr(lpString, L"Magnifier")) {
            ShowWindow(hWnd, SW_HIDE);
        }
    }
    
    return result;
}

// Hook ShowWindow to prevent magnifier from showing itself
using ShowWindow_t = decltype(&ShowWindow);
ShowWindow_t ShowWindow_Original;

BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    if (IsMagnifierWindow(hWnd) && nCmdShow != SW_HIDE) {
        return ShowWindow_Original(hWnd, SW_HIDE);
    }
    return ShowWindow_Original(hWnd, nCmdShow);
}

struct {
    void** pOriginalFunction;
    void* hookFunction;
    const wchar_t* symbolName;
} hooks[] = {
    {(void**)&CreateWindowExW_Original, (void*)CreateWindowExW_Hook, L"CreateWindowExW"},
    {(void**)&SetWindowTextW_Original, (void*)SetWindowTextW_Hook, L"SetWindowTextW"},
    {(void**)&ShowWindow_Original, (void*)ShowWindow_Hook, L"ShowWindow"}
};

BOOL Wh_ModBeforeSymbolLoading() {
    for (auto& hook : hooks) {
        if (!Wh_SetFunctionHook(
            (void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), 
            (LPCSTR)hook.symbolName),
            hook.hookFunction,
            hook.pOriginalFunction)) {
            return FALSE;
        }
    }
    return TRUE;
}
