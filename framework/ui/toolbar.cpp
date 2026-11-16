// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// See toolbar.h.

#include "toolbar.h"

#include <windows.h>

#ifndef SWP_NOSIZE
#define SWP_NOSIZE 0x0001
#endif
#ifndef SWP_NOZORDER
#define SWP_NOZORDER 0x0004
#endif
#ifndef SWP_NOACTIVATE
#define SWP_NOACTIVATE 0x0010
#endif
#ifndef SWP_SHOWWINDOW
#define SWP_SHOWWINDOW 0x0040
#endif
#ifndef TRANSPARENT
#define TRANSPARENT 1
#endif
#ifndef LTGRAY_BRUSH
#define LTGRAY_BRUSH 1
#endif
#ifndef SM_CXSCREEN
#define SM_CXSCREEN 0
#endif
#ifndef SM_CYSCREEN
#define SM_CYSCREEN 1
#endif

namespace imfw {
namespace ui {

namespace {

constexpr int kToolbarWidth = 44;
constexpr int kToolbarHeight = 22;
constexpr int kScreenMargin = 8;

}  // namespace

void Toolbar::update(HINSTANCE inst, const wchar_t* class_name,
                     const wchar_t* label) {
    label_ = label != nullptr ? label : L"";

    if (!ensure_created(inst, class_name, L"", kToolbarWidth, kToolbarHeight)) {
        return;
    }

    InvalidateRect(hwnd_, nullptr, TRUE);
    dock_bottom_right();
    show();
}

void Toolbar::dock_bottom_right() {
    if (hwnd_ == nullptr) return;

    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int x = 0;
    int y = 0;
    if (screen_w > 0) x = screen_w - kToolbarWidth - kScreenMargin;
    if (screen_h > 0) y = screen_h - kToolbarHeight - kScreenMargin;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void Toolbar::on_paint(HDC dc, const RECT& client) {
    FillRect(dc, &client,
             reinterpret_cast<HBRUSH>(GetStockObject(LTGRAY_BRUSH)));
    if (label_.empty()) return;

    const HGDIOBJ old_font = font_ != nullptr ? SelectObject(dc, font_) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));

    SIZE size{};
    if (GetTextExtentPoint32W(dc, label_.c_str(),
                              static_cast<int>(label_.size()), &size)) {
        const int x = (client.right - client.left - size.cx) / 2;
        const int y = (client.bottom - client.top - size.cy) / 2;
        TextOutW(dc, x > 0 ? x : 0, y > 0 ? y : 0, label_.c_str(),
                 static_cast<int>(label_.size()));
    }

    if (old_font != nullptr) SelectObject(dc, old_font);
}

}  // namespace ui
}  // namespace imfw
