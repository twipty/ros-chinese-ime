// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Shared Win32 window scaffolding for the M2 composition / candidate windows.
// The two UI classes are registered lazily on first use, never in DllMain.

#pragma once

#include <imfw/glyph_plane.hpp>

#include <windows.h>

#include <cstddef>  // std::size_t
#include <cstdint>  // std::intptr_t/uintptr_t: MinGW's windows.h does not pull it in

namespace imfw {
namespace ui {

// Shared Win32 window scaffolding for the composition / candidate / toolbar
// windows. Window classes are registered lazily on first use, never in
// DllMain, with per-IME names supplied by the Scheme.

// Pick the first installed CJK-capable face from a candidate list, verifying
// with GetTextFaceW because GDI silently substitutes missing faces. Returns a
// system-default font when none of the candidates is present.
HFONT CreateUiFont(int pixel_height);

// Idempotent per class name: registers a window class once per process even
// when two assembled IME DLLs are loaded at the same time (their Scheme
// supplies distinct class names).
bool RegisterUiWindowClass(HINSTANCE inst, const wchar_t* class_name);

// Window-operation counters. One composition of n letters used to issue 3
// SetWindowPos/ShowWindow/invalidate sets per keystroke (the notify storm
// from IMM32 plus our own unconditional repaint), which is what the user
// perceives as the composition window jumping. These counters let a trace
// line state how many of those calls actually changed something.
struct UiStats {
    unsigned place_calls = 0;    // apply_geometry() invocations
    unsigned place_changes = 0;  // ...of which the geometry really changed
    unsigned show_real = 0;      // ShowWindow(SW_SHOW*) that were not a no-op
    unsigned invalidates = 0;    // InvalidateRect() calls
};

UiStats ui_stats();

// Internal: bump UiStats::invalidates. Called by the paint-driven windows so
// one counter covers every repaint request.
void note_invalidate();

// Base class owning one topmost, no-activate popup window. Window creation is
// driven by ensure_created(); destruction happens in the destructor or when
// Windows sends WM_CLOSE/WM_DESTROY.
class UiWindow {
public:
    UiWindow() = default;
    UiWindow(const UiWindow&) = delete;
    UiWindow& operator=(const UiWindow&) = delete;
    virtual ~UiWindow();

    bool ensure_created(HINSTANCE inst, const wchar_t* class_name,
                        const wchar_t* window_name, int width, int height);
    void show();
    void hide();
    void move_to(int x, int y);

    // Place and size at once, skipping the Win32 call when nothing changed.
    // "Changed" is compared against what was last applied, not against the
    // requested values, so repeated identical updates cost nothing.
    void apply_geometry(int x, int y, int width, int height);

    bool visible() const noexcept { return hwnd_ != nullptr && visible_; }

    HWND handle() const noexcept { return hwnd_; }
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }

    // Public because RegisterUiWindowClasses() installs it as the class proc.
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

protected:
    virtual void on_paint(HDC dc, const RECT& client) = 0;

    static UiWindow* from_handle(HWND hwnd);

    // P1 [全] mode: draw one UTF-16 line by splitting it into WenJin plane
    // runs and selecting the matching cached font per run. When `full_glyph`
    // is false this is byte-for-byte the old single-font TextOutW path.
    // Returns the advance width actually drawn (used to advance x between
    // composition segments). Only call with a live HDC (WM_PAINT or measure).
    int draw_text_runs(HDC dc, int x, int y, const wchar_t* text,
                       std::size_t len, bool full_glyph);
    // Width of the same text with the same plane split, without drawing.
    int measure_text_runs(HDC dc, const wchar_t* text, std::size_t len,
                          bool full_glyph);

private:
    HFONT font_for_plane(imfw::WenJinPlane plane) const;

protected:
    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;              // baseline / None-plane / numbering
    HFONT plane_fonts_[4] = {};         // indexed by WenJinPlane value (0 unused)
    int width_ = 0;
    int height_ = 0;

private:
    bool visible_ = false;

    // Last geometry handed to SetWindowPos. -1/0 means "never applied", so the
    // first apply_geometry() always goes through.
    int applied_x_ = -1;
    int applied_y_ = -1;
    int applied_w_ = -1;
    int applied_h_ = -1;
};

}  // namespace ui
}  // namespace imfw
