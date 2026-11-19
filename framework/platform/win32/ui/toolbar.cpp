// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// See toolbar.h.

#include "toolbar.h"

#include <windows.h>

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

    // U3 (2026-11-18 真机): dock inside the *work area*, not the screen rect.
    // The taskbar lies inside the screen but outside the work area, and on
    // ReactOS HWND_TOPMOST does not win against it -- the taskbar is topmost
    // too and keeps the higher z-order, so the toolbar sat under the tray and
    // only its top few pixels were visible.
    RECT work{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0) ||
        work.right <= work.left || work.bottom <= work.top) {
        // Fall back to the full screen, which is what this always used to do.
        work.left = 0;
        work.top = 0;
        work.right = GetSystemMetrics(SM_CXSCREEN);
        work.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    int x = work.right - kToolbarWidth - kScreenMargin;
    int y = work.bottom - kToolbarHeight - kScreenMargin;
    if (x < work.left) x = work.left;
    if (y < work.top) y = work.top;

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
