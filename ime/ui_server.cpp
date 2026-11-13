// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// UI server window class. IMM32 requires the class name returned by
// ImeInquire to be registered. At M1 the window is deliberately inert:
// WM_IME_* handling, composition window, candidate window and status window
// are M2/M3 work (see ui/README.md).

#include "rospinyin.h"

ATOM RegisterUIServerClass(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_IME;
    wc.lpfnWndProc = UIServerWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInst;
    wc.hIcon = nullptr;
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = rospinyin::kUIServerClassName;
    wc.hIconSm = nullptr;
    return RegisterClassExW(&wc);
}

void UnregisterUIServerClass(HINSTANCE hInst) {
    if (!hInst) {
        return;
    }
    UnregisterClassW(rospinyin::kUIServerClassName, hInst);
}

LRESULT CALLBACK UIServerWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        // No WM_IME_* handling at M1: never paint, never allocate, never
        // touch the input context. Keep the logon path quiet.
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}
