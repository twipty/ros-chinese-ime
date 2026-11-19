// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Floating input-method toolbar (输入法工具条), reusable by every assembled
// IME. A small always-on-top strip docked to the bottom-right of the screen
// that shows the scheme's label plus its state ("拼:开") while the IME is
// selected, so users get a persistent hint that a Chinese IME is active even
// when no composition is open -- and, after Shift, that it is currently in
// English mode.
//
// The toolbar is the only surface that can report the state: the shell tray
// indicator (red 中) follows the keyboard *layout*, which Shift must not
// change.

#pragma once

#include "ui_window.h"

#include <string>

namespace imfw {
namespace ui {

class Toolbar final : public UiWindow {
public:
    // class_name and label come from the Scheme (imfw::toolbar_text).
    //
    // Idempotent on purpose: the label carries fOpen, which changes without
    // any window message telling us, so this is called from the key entry
    // points as well. Identical text therefore has to cost zero Win32 calls --
    // an InvalidateRect per keystroke here is exactly the repaint storm P0-3
    // spent a milestone removing.
    void update(HINSTANCE inst, const wchar_t* class_name,
                const wchar_t* label);

    // Dock the toolbar to the bottom-right corner of the primary screen and
    // size it to the current label. The right edge stays put and the strip
    // grows leftwards, so "拼:关" cannot run off the screen edge.
    void dock_bottom_right();

protected:
    void on_paint(HDC dc, const RECT& client) override;

private:
    // Width that fits the label in the font on_paint() uses, with padding and a
    // floor of kMinWidth. Measured, not assumed: "拼:开" is 3 glyphs in a font
    // we do not ship, and a fixed 44 px clipped it.
    int measure_width() const;

    std::wstring label_;
};

}  // namespace ui
}  // namespace imfw
