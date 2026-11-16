// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Composition (pre-edit) window, WBS 3.3.1. Shows the raw canonical pinyin
// string with a thin underline under every recognized syllable.

#pragma once

#include "ui_window.h"

#include <cstddef>
#include <string>
#include <vector>

namespace rospinyin {
namespace ui {

class CompositionWindow final : public UiWindow {
public:
    // text is the raw pinyin; syllables holds canonical syllable strings in
    // input order. An empty text hides the window.
    void update(HINSTANCE inst, const wchar_t* text,
                const std::vector<std::string>& syllables);

protected:
    void on_paint(HDC dc, const RECT& client) override;

private:
    std::wstring text_;
    std::vector<std::size_t> syllable_ends_;  // cumulative character offsets
};

}  // namespace ui
}  // namespace rospinyin
