// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// See ui_window.h. All window classes are registered on first use (the IME
// entry points), never from DllMain.

#include "ui_window.h"

#include <windows.h>

#include <cwchar>

#ifndef GWLP_USERDATA
#define GWLP_USERDATA (-21)
#endif
#ifndef WS_POPUP
#define WS_POPUP 0x80000000L
#endif
#ifndef WS_EX_TOOLWINDOW
#define WS_EX_TOOLWINDOW 0x00000080L
#endif
#ifndef WS_EX_TOPMOST
#define WS_EX_TOPMOST 0x00000008L
#endif
#ifndef WS_EX_NOACTIVATE
#define WS_EX_NOACTIVATE 0x08000000L
#endif
#ifndef SW_HIDE
#define SW_HIDE 0
#endif
#ifndef SW_SHOWNOACTIVATE
#define SW_SHOWNOACTIVATE 4
#endif
#ifndef SWP_NOSIZE
#define SWP_NOSIZE 0x0001
#endif
#ifndef SWP_NOMOVE
#define SWP_NOMOVE 0x0002
#endif
#ifndef SWP_NOZORDER
#define SWP_NOZORDER 0x0004
#endif
#ifndef SWP_NOACTIVATE
#define SWP_NOACTIVATE 0x0010
#endif
#ifndef HWND_TOPMOST
#define HWND_TOPMOST reinterpret_cast<HWND>(static_cast<std::intptr_t>(-1))
#endif
#ifndef WM_PAINT
#define WM_PAINT 0x000F
#endif
#ifndef WM_ERASEBKGND
#define WM_ERASEBKGND 0x0014
#endif
#ifndef WM_MOUSEACTIVATE
#define WM_MOUSEACTIVATE 0x0021
#endif
#ifndef MA_NOACTIVATE
#define MA_NOACTIVATE 0x0004
#endif
#ifndef TRANSPARENT
#define TRANSPARENT 1
#endif
#ifndef WHITE_BRUSH
#define WHITE_BRUSH 0
#endif
#ifndef LTGRAY_BRUSH
#define LTGRAY_BRUSH 1
#endif
#ifndef BLACK_BRUSH
#define BLACK_BRUSH 4
#endif
#ifndef FW_NORMAL
#define FW_NORMAL 400
#endif
#ifndef DEFAULT_CHARSET
#define DEFAULT_CHARSET 1
#endif
#ifndef OUT_DEFAULT_PRECIS
#define OUT_DEFAULT_PRECIS 0
#endif
#ifndef CLIP_DEFAULT_PRECIS
#define CLIP_DEFAULT_PRECIS 0
#endif
#ifndef DEFAULT_QUALITY
#define DEFAULT_QUALITY 1
#endif
#ifndef DEFAULT_PITCH
#define DEFAULT_PITCH 8
#endif
#ifndef FF_DONTCARE
#define FF_DONTCARE 0
#endif
#ifndef LF_FACESIZE
#define LF_FACESIZE 32
#endif

namespace rospinyin {
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

}  // namespace

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

void RegisterUiWindowClasses(HINSTANCE inst) {
    static bool done = false;
    if (done) return;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = &UiWindow::wnd_proc;
    wc.hInstance = inst;
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;

    wc.lpszClassName = kCompositionWindowClass;
    RegisterClassExW(&wc);
    wc.lpszClassName = kCandidateWindowClass;
    RegisterClassExW(&wc);

    done = true;
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
    RegisterUiWindowClasses(inst);

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
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    visible_ = true;
}

void UiWindow::hide() {
    if (hwnd_ == nullptr) return;
    ShowWindow(hwnd_, SW_HIDE);
    visible_ = false;
}

void UiWindow::move_to(int x, int y) {
    if (hwnd_ == nullptr) return;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
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
}  // namespace rospinyin
