// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Composition (pre-edit) window, WBS 3.3.1. Shows the raw canonical pinyin
// string with a thin underline under every recognized segment (the
// engine's lookup keys, e.g. pinyin syllables or shape codes).

#pragma once

#include "ui_window.h"

#include <cstddef>
#include <string>
#include <vector>

namespace imfw {
namespace ui {

class CompositionWindow final : public UiWindow {
public:
    // class_name comes from the Scheme; text is the raw composition;
    // segments holds the engine's lookup keys in input order (their
    // cumulative lengths mark the underline boundaries). Empty text hides
    // the window.
    void update(HINSTANCE inst, const wchar_t* class_name, const wchar_t* text,
                const std::vector<std::string>& segments);

protected:
    void on_paint(HDC dc, const RECT& client) override;

private:
    std::wstring text_;
    std::vector<std::size_t> syllable_ends_;  // cumulative character offsets
};

}  // namespace ui
}  // namespace imfw
