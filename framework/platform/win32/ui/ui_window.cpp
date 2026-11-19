// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// See ui_window.h. All window classes are registered on first use (the IME
// entry points), never from DllMain.

#include "ui_window.h"

#include <windows.h>

#include <cwchar>
#include <set>
#include <string>

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
    return true;
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
