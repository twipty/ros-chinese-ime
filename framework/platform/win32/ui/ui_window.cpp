// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// See ui_window.h. All window classes are registered on first use (the IME
// entry points), never from DllMain.

#include "ui_window.h"

#include <imfw/glyph_plane.hpp>

#include <windows.h>

#include <cwchar>
#include <set>
#include <string>     // std::u16string lives here (MinGW has no <u16string> header)
#include <vector>

namespace imfw {
namespace ui {
namespace {

// ReactOS ships DroidSansFallback.ttf (covers ~16.5k Hanzi) but only mounts
// the CJK font registry keys under a CJK locale. Try the common faces in order
// and keep the one GDI actually resolved (GetTextFaceW reports substitutions).
const wchar_t* const kFontFaces[] = {
    L"Droid Sans Fallback",
    L"Noto Sans SC",
    L"Microsoft YaHei",
    L"SimSun",
    L"Arial Unicode MS",
};

bool ascii_iequal(const wchar_t* a, const wchar_t* b) {
    while (*a != L'\0' && *b != L'\0') {
        wchar_t ca = *a;
        wchar_t cb = *b;
        if (ca >= L'A' && ca <= L'Z') ca = static_cast<wchar_t>(ca - L'A' + L'a');
        if (cb >= L'A' && cb <= L'Z') cb = static_cast<wchar_t>(cb - L'A' + L'a');
        if (ca != cb) return false;
        ++a;
        ++b;
    }
    return *a == L'\0' && *b == L'\0';
}

// On Win32 wchar_t is 16-bit, so a wstring is already a UTF-16 unit sequence.
// Copy to char16_t for the Windows-free split_into_runs() (char16_t is 16-bit
// on every platform, which is what lets the host test pin the exact runs).
std::u16string as_utf16(const wchar_t* text, std::size_t len) {
    std::u16string out;
    out.reserve(len);
    for (std::size_t i = 0; i < len; ++i) {
        out.push_back(static_cast<char16_t>(text[i]));
    }
    return out;
}

// Per-process window-class registry. CRITICAL_SECTION instead of std::mutex
// keeps libwinpthread out of the IME PE (see session_store.cpp); a set keyed
// by class name lets two assembled IME DLLs (distinct Scheme class names)
// coexist in one process.
class WindowRegistry {
public:
    WindowRegistry() { InitializeCriticalSection(&cs_); }
    ~WindowRegistry() { DeleteCriticalSection(&cs_); }

    bool register_once(HINSTANCE inst, const wchar_t* class_name) {
        EnterCriticalSection(&cs_);
        bool result = true;
        if (seen_.insert(std::wstring(class_name)).second) {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style = 0;
            wc.lpfnWndProc = &UiWindow::wnd_proc;
            wc.hInstance = inst;
            wc.hbrBackground = nullptr;
            wc.lpszMenuName = nullptr;
            wc.lpszClassName = class_name;
            result = RegisterClassExW(&wc) != 0;
        }
        LeaveCriticalSection(&cs_);
        return result;
    }

private:
    CRITICAL_SECTION cs_;
    std::set<std::wstring> seen_;
};

WindowRegistry& window_registry() {
    static WindowRegistry instance;  // first use, never in DllMain
    return instance;
}

UiStats g_stats;

}  // namespace

UiStats ui_stats() { return g_stats; }

void note_invalidate() { ++g_stats.invalidates; }

HFONT CreateUiFont(int pixel_height) {
    for (const wchar_t* face : kFontFaces) {
        HFONT font = CreateFontW(pixel_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                 FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, face);
        if (font == nullptr) continue;

        HDC dc = GetDC(nullptr);
        HGDIOBJ old = SelectObject(dc, font);
        wchar_t actual[LF_FACESIZE] = {};
        GetTextFaceW(dc, LF_FACESIZE, actual);
        SelectObject(dc, old);
        ReleaseDC(nullptr, dc);

        if (ascii_iequal(actual, face)) return font;
        DeleteObject(font);
    }
    // Last resort: let GDI pick any font. At least Latin pinyin stays visible
    // on a bare ReactOS installation without a CJK font registry entry.
    return CreateFontW(pixel_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, nullptr);
}

bool RegisterUiWindowClass(HINSTANCE inst, const wchar_t* class_name) {
    if (inst == nullptr || class_name == nullptr || class_name[0] == L'\0') {
        return false;
    }
    return window_registry().register_once(inst, class_name);
}

UiWindow::~UiWindow() {
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (font_ != nullptr) {
        DeleteObject(font_);
        font_ = nullptr;
    }
    for (HFONT& plane_font : plane_fonts_) {
        if (plane_font != nullptr) {
            DeleteObject(plane_font);
            plane_font = nullptr;
        }
    }
}

bool UiWindow::ensure_created(HINSTANCE inst, const wchar_t* class_name,
                              const wchar_t* window_name, int width,
                              int height) {
    if (hwnd_ != nullptr) return true;
    RegisterUiWindowClass(inst, class_name);

    width_ = width;
    height_ = height;
    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                            class_name, window_name, WS_POPUP, 0, 0, width,
                            height, nullptr, nullptr, inst, nullptr);
    if (hwnd_ == nullptr) return false;

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));
    font_ = CreateUiFont(16);

    // P1 [全] mode: cache one HFONT per WenJin plane at creation time, never
    // in WM_PAINT. Per the owner's decision we do NOT probe with GetTextFaceW:
    // CreateFontW on a missing face returns a usable handle that GDI silently
    // substitutes. Worst case is still tofu, no worse than the status quo, and
    // the installer/release text warns the user all three TTFs are required.
    for (imfw::WenJinPlane plane : {imfw::WenJinPlane::Plane0,
                                    imfw::WenJinPlane::Plane2,
                                    imfw::WenJinPlane::Plane3}) {
        const wchar_t* face = imfw::plane_face_name(plane);
        if (face == nullptr) continue;
        plane_fonts_[static_cast<int>(plane)] = CreateFontW(
            16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, face);
    }
    return true;
}

// Returns the cached font for a plane run. None -> baseline font_ (the
// numbering / Latin / punctuation path), which keeps [快] and the pure-ASCII
// parts of [全] byte-for-byte identical to the old single-font rendering.
HFONT UiWindow::font_for_plane(imfw::WenJinPlane plane) const {
    if (plane == imfw::WenJinPlane::None) return font_;
    const int idx = static_cast<int>(plane);
    if (idx > 0 && idx < 4 && plane_fonts_[idx] != nullptr) {
        return plane_fonts_[idx];
    }
    return font_;  // defensive: never hand a null HFONT to SelectObject
}

int UiWindow::draw_text_runs(HDC dc, int x, int y, const wchar_t* text,
                             std::size_t len, bool full_glyph) {
    if (!full_glyph || text == nullptr || len == 0) {
        // Old path, unchanged: one font, one TextOutW, one measurement.
        const HGDIOBJ old = font_ != nullptr ? SelectObject(dc, font_) : nullptr;
        if (text != nullptr && len != 0) {
            TextOutW(dc, x, y, text, static_cast<int>(len));
        }
        SIZE size{};
        int width = 0;
        if (text != nullptr && len != 0 &&
            GetTextExtentPoint32W(dc, text, static_cast<int>(len), &size)) {
            width = size.cx;
        }
        if (old != nullptr) SelectObject(dc, old);
        return width;
    }

    const std::u16string utf16 = as_utf16(text, len);
    const std::vector<imfw::GlyphRun> runs = imfw::split_into_runs(utf16);
    int advance = 0;
    for (const auto& run : runs) {
        HFONT font = font_for_plane(run.plane);
        const HGDIOBJ old = font != nullptr ? SelectObject(dc, font) : nullptr;
        const wchar_t* piece = text + run.offset;
        const int n = static_cast<int>(run.length);
        TextOutW(dc, x + advance, y, piece, n);
        SIZE size{};
        if (GetTextExtentPoint32W(dc, piece, n, &size)) advance += size.cx;
        if (old != nullptr) SelectObject(dc, old);
    }
    return advance;
}

int UiWindow::measure_text_runs(HDC dc, const wchar_t* text, std::size_t len,
                                bool full_glyph) {
    if (!full_glyph || text == nullptr || len == 0) {
        const HGDIOBJ old = font_ != nullptr ? SelectObject(dc, font_) : nullptr;
        SIZE size{};
        int width = 0;
        if (text != nullptr && len != 0 &&
            GetTextExtentPoint32W(dc, text, static_cast<int>(len), &size)) {
            width = size.cx;
        }
        if (old != nullptr) SelectObject(dc, old);
        return width;
    }

    const std::u16string utf16 = as_utf16(text, len);
    const std::vector<imfw::GlyphRun> runs = imfw::split_into_runs(utf16);
    int advance = 0;
    for (const auto& run : runs) {
        HFONT font = font_for_plane(run.plane);
        const HGDIOBJ old = font != nullptr ? SelectObject(dc, font) : nullptr;
        SIZE size{};
        if (GetTextExtentPoint32W(dc, text + run.offset,
                                  static_cast<int>(run.length), &size)) {
            advance += size.cx;
        }
        if (old != nullptr) SelectObject(dc, old);
    }
    return advance;
}

void UiWindow::show() {
    if (hwnd_ == nullptr) return;
    // ShowWindow on an already-visible window still sends WM_SHOWWINDOW in
    // some implementations and always forces a z-order pass; with one call per
    // engine update that is a visible flicker. Only do it when hidden.
    if (!visible_) {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        visible_ = true;
        ++g_stats.show_real;
    }
}

void UiWindow::hide() {
    if (hwnd_ == nullptr) return;
    if (visible_) {
        ShowWindow(hwnd_, SW_HIDE);
        visible_ = false;
    }
}

void UiWindow::move_to(int x, int y) {
    apply_geometry(x, y, width_, height_);
}

void UiWindow::apply_geometry(int x, int y, int width, int height) {
    if (hwnd_ == nullptr) return;
    ++g_stats.place_calls;

    // P0-3: the composition window used to grow its cached width_/height_ but
    // never tell Win32, so "zhongguo" was clipped to the width measured for
    // "zhong". Sizing belongs here, together with the position, and both are
    // skipped when unchanged so a caret that did not move costs no messages.
    if (width <= 0) width = width_;
    if (height <= 0) height = height_;
    if (x == applied_x_ && y == applied_y_ && width == applied_w_ &&
        height == applied_h_) {
        return;
    }

    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    applied_x_ = x;
    applied_y_ = y;
    applied_w_ = width;
    applied_h_ = height;
    width_ = width;
    height_ = height;
    ++g_stats.place_changes;
}

UiWindow* UiWindow::from_handle(HWND hwnd) {
    if (hwnd == nullptr) return nullptr;
    return reinterpret_cast<UiWindow*>(
        static_cast<std::uintptr_t>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)));
}

LRESULT CALLBACK UiWindow::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UiWindow* self = from_handle(hwnd);
    switch (msg) {
    case WM_CREATE:
        return 0;
    case WM_ERASEBKGND:
        return 1;  // on_paint fills the whole client area
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_PAINT: {
        if (self == nullptr) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        self->on_paint(dc, client);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        if (self != nullptr) self->hwnd_ = nullptr;
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

}  // namespace ui
}  // namespace imfw
