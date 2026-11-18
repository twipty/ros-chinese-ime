// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// UI server window class. IMM32 requires the class name returned by
// ImeInquire to be registered; the name itself comes from the Scheme so the
// same framework code serves any input method. The window stays inert:
// composition / candidate / toolbar windows own their own message handling.

#include "ime_host.h"

#include <imfw/scheme.hpp>

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
    wc.lpszClassName = imfw::scheme_instance().ui_server_class();
    wc.hIconSm = nullptr;
    return RegisterClassExW(&wc);
}

void UnregisterUIServerClass(HINSTANCE hInst) {
    if (!hInst) {
        return;
    }
    UnregisterClassW(imfw::scheme_instance().ui_server_class(), hInst);
}

LRESULT CALLBACK UIServerWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        // No WM_IME_* handling here: never paint, never allocate, never
        // touch the input context. Keep the logon path quiet.
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}
