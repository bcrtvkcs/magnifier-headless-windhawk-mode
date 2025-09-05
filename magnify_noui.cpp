// ==WindhawkMod==
// @id              magnifier-headless-mode
// @name            Magnifier Headless Mode
// @description     Blocks all Magnifier window creation, keeping only zoom functionality
// @version         0.0.1test
// @author          BCRTVKCS
// @github          bcrtvkcs
// @twitter         bcrtvkcs
// @homepage        https://grdigital.pro
// @include         magnify.exe
// @exclude         ^(?!.*magnify.exe)
// @compilerOptions -luser32 -lkernel32
// ==/WindhawkMod==

#include <windows.h>
#include <windhawk.h>

// Store handle of the Magnifier window so we can block attempts to show it
static HWND g_magnifierWindow = NULL;

using CreateWindowExW_t = decltype(&CreateWindowExW);
static CreateWindowExW_t pCreateWindowExW;

// Intercept window creation and hide the Magnifier UI window ("Büyüteç" or "Magnifier")
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
    LPVOID lpParam) {
    // Call the original function first
    HWND hwnd = pCreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle,
                                X, Y, nWidth, nHeight, hWndParent, hMenu,
                                hInstance, lpParam);

    if (hwnd && lpWindowName &&
        (wcscmp(lpWindowName, L"Büyüteç") == 0 ||
         wcscmp(lpWindowName, L"Magnifier") == 0)) {
        g_magnifierWindow = hwnd;
        // Ensure the window stays hidden
        ShowWindow(hwnd, SW_HIDE);
    }

    return hwnd;
}

using ShowWindow_t = decltype(&ShowWindow);
static ShowWindow_t pShowWindow;

// Prevent the window from being shown later on
BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    if (hWnd == g_magnifierWindow) {
        nCmdShow = SW_HIDE;
    }
    return pShowWindow(hWnd, nCmdShow);
}

void Wh_ModInit() {
    Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_Hook,
                       (void**)&pCreateWindowExW);
    Wh_SetFunctionHook((void*)ShowWindow, (void*)ShowWindow_Hook,
                       (void**)&pShowWindow);
}

void Wh_ModUninit() {
    g_magnifierWindow = NULL;
}

