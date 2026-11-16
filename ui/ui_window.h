// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Shared Win32 window scaffolding for the M2 composition / candidate windows.
// The two UI classes are registered lazily on first use, never in DllMain.

#pragma once

#include <windows.h>

namespace rospinyin {
namespace ui {

inline constexpr wchar_t kCompositionWindowClass[] = L"RosPinyinComposition";
inline constexpr wchar_t kCandidateWindowClass[] = L"RosPinyinCandidate";

// Pick the first installed CJK-capable face from a candidate list, verifying
// with GetTextFaceW because GDI silently substitutes missing faces. Returns a
// system-default font when none of the candidates is present.
HFONT CreateUiFont(int pixel_height);

// Idempotent: registers both UI window classes once per process.
void RegisterUiWindowClasses(HINSTANCE inst);

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
    bool visible() const noexcept { return hwnd_ != nullptr && visible_; }

    HWND handle() const noexcept { return hwnd_; }
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }

    // Public because RegisterUiWindowClasses() installs it as the class proc.
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

protected:
    virtual void on_paint(HDC dc, const RECT& client) = 0;

    static UiWindow* from_handle(HWND hwnd);

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    int width_ = 0;
    int height_ = 0;

private:
    bool visible_ = false;
};

}  // namespace ui
}  // namespace rospinyin
