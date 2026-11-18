// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Floating input-method toolbar (输入法工具条), reusable by every assembled
// IME. A small always-on-top strip docked to the bottom-right of the screen
// that shows the scheme's label ("拼" for RosPinyin) while the IME is
// selected, so users get a persistent hint that a Chinese IME is active even
// when no composition is open. Later milestones can extend it with a mode
// menu; the window itself is framework-owned and needs no per-IME code.

#pragma once

#include "ui_window.h"

#include <string>

namespace imfw {
namespace ui {

class Toolbar final : public UiWindow {
public:
    // class_name and label come from the Scheme.
    void update(HINSTANCE inst, const wchar_t* class_name,
                const wchar_t* label);

    // Dock the toolbar to the bottom-right corner of the primary screen.
    void dock_bottom_right();

protected:
    void on_paint(HDC dc, const RECT& client) override;

private:
    std::wstring label_;
};

}  // namespace ui
}  // namespace imfw
