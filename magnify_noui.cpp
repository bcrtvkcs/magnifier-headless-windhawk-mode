// ==WindhawkMod==
// @id              magnifier-headless-mode
// @name            Magnifier Headless Mode
// @description     Hides the Magnifier interface window named "Büyüteç" or "Magnifier"
// @version         0.1
// @author          BCRTVKCS
// @github          bcrtvkcs
// @twitter         bcrtvkcs
// @homepage        https://grdigital.pro
// @include         magnify.exe
// @exclude         ^(?!.*magnify.exe)
// @compilerOptions -luser32 -lkernel32
// ==/WindhawkMod==

#include <windhawk.h>
#include <windows.h>

static bool ShouldHideTitle(LPCWSTR title) {
    if (!title) {
        return false;
    }
    return wcscmp(title, L"Magnifier") == 0 || wcscmp(title, L"Büyüteç") == 0;
}

static void HideWindow(HWND hwnd) {
    if (IsWindow(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
    }
}

static void CheckAndHide(HWND hwnd) {
    WCHAR title[256];
    if (GetWindowTextW(hwnd, title, ARRAYSIZE(title))) {
        if (ShouldHideTitle(title)) {
            HideWindow(hwnd);
        }
    }
}

static BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lParam) {
    CheckAndHide(hwnd);
    return TRUE;
}

typedef HWND (WINAPI *CreateWindowExW_t)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int,
                                         HWND, HMENU, HINSTANCE, LPVOID);
CreateWindowExW_t pCreateWindowExW;

HWND WINAPI CreateWindowExW_Hook(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName,
                                 DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                                 HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    HWND hwnd = pCreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight,
                                 hWndParent, hMenu, hInstance, lpParam);
    if (hwnd) {
        if (ShouldHideTitle(lpWindowName)) {
            HideWindow(hwnd);
        } else {
            CheckAndHide(hwnd);
        }
    }
    return hwnd;
}

typedef BOOL (WINAPI *SetWindowTextW_t)(HWND, LPCWSTR);
SetWindowTextW_t pSetWindowTextW;

BOOL WINAPI SetWindowTextW_Hook(HWND hwnd, LPCWSTR lpString) {
    if (ShouldHideTitle(lpString)) {
        HideWindow(hwnd);
    }
    return pSetWindowTextW(hwnd, lpString);
}

BOOL Wh_ModInit(void) {
    EnumWindows(EnumProc, 0);
    Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_Hook, (void**)&pCreateWindowExW);
    Wh_SetFunctionHook((void*)SetWindowTextW, (void*)SetWindowTextW_Hook, (void**)&pSetWindowTextW);
    return TRUE;
}

void Wh_ModUninit(void) {
}

