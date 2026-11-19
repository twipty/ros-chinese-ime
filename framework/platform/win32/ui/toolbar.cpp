// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// See toolbar.h.

#include "toolbar.h"

#include <windows.h>

namespace imfw {
namespace ui {

namespace {

// Floor for the measured width, and the padding on each side of the text. The
// floor is what the label-less toolbar used to be; text decides the rest.
constexpr int kMinWidth = 44;
constexpr int kTextPadX = 6;
constexpr int kToolbarHeight = 22;
constexpr int kScreenMargin = 8;

}  // namespace

void Toolbar::update(HINSTANCE inst, const wchar_t* class_name,
                     const wchar_t* label) {
    const std::wstring text = label != nullptr ? label : L"";
    // Already on screen with this exact text: nothing to paint, nothing to
    // move. See the header for why this early return is load-bearing.
    if (hwnd_ != nullptr && visible() && text == label_) return;

    label_ = text;
    if (!ensure_created(inst, class_name, L"", kMinWidth, kToolbarHeight)) {
        return;
    }

    // Order matters: the width is measured with the font created above, and
    // InvalidateRect last, so the resize and the repaint arrive together
    // instead of the old text being stretched across the new width.
    dock_bottom_right();
    show();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

int Toolbar::measure_width() const {
    if (label_.empty() || font_ == nullptr) return kMinWidth;

    HDC dc = CreateCompatibleDC(nullptr);
    if (dc == nullptr) return kMinWidth;

    const HGDIOBJ old_font = SelectObject(dc, font_);
    SIZE size{};
    int width = kMinWidth;
    if (GetTextExtentPoint32W(dc, label_.c_str(),
                              static_cast<int>(label_.size()), &size) &&
        size.cx > 0) {
        width = size.cx + 2 * kTextPadX;
        if (width < kMinWidth) width = kMinWidth;
    }
    if (old_font != nullptr) SelectObject(dc, old_font);
    DeleteDC(dc);
    return width;
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

    const int width = measure_width();
    int x = work.right - width - kScreenMargin;
    int y = work.bottom - kToolbarHeight - kScreenMargin;
    if (x < work.left) x = work.left;
    if (y < work.top) y = work.top;

    // apply_geometry, not SetWindowPos: it skips the call when the rectangle is
    // already right, and the counters the trace prints stay meaningful. The
    // window is already visible (show() above / SW_SHOWNOACTIVATE), so the
    // SWP_SHOWWINDOW this used to pass is no longer part of the deal.
    apply_geometry(x, y, width, kToolbarHeight);
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
