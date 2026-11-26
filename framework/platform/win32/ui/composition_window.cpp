// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// See composition_window.h.

#include "composition_window.h"

#include <imfw/utf8.hpp>

#include <windows.h>

namespace imfw {
namespace ui {

void CompositionWindow::update(HINSTANCE inst, const wchar_t* class_name,
                               const wchar_t* text,
                               const std::vector<std::string>& segments,
                               bool full_glyph) {
    if (text == nullptr || *text == L'\0') {
        text_.clear();
        syllable_ends_.clear();
        hide();
        return;
    }

    // Build the new view first so it can be compared with the current one:
    // IMM32 re-enters the engine for the same composition on key repeats and
    // on IMN_SETCOMPOSITIONWINDOW (1-3 times per keystroke), and every one of
    // those used to force a full repaint.
    std::wstring next_text(text);
    std::vector<std::size_t> next_ends;
    std::size_t end = 0;
    for (const auto& seg : segments) {
        // Segments are UTF-8 (engine.hpp); underline ends are indexes into
        // the wide composition text, so accumulate the decoded width, not
        // the byte count (G4: CJK segments must underline correctly).
        end += imfw::utf8_to_wide(seg).size();
        if (end <= next_text.size()) next_ends.push_back(end);
    }

    const bool content_changed =
        next_text != text_ || next_ends != syllable_ends_ ||
        full_glyph != full_glyph_;
    text_.swap(next_text);
    syllable_ends_.swap(next_ends);
    full_glyph_ = full_glyph;

    // Rough width: 8 px per latin char is enough for a 16 px font; refine in
    // WM_PAINT where the real font metrics are available.
    const int rough = static_cast<int>(text_.size() * 9) + 16;
    if (!ensure_created(inst, class_name, L"", rough, 24)) return;

    if (content_changed) {
        HDC dc = GetDC(hwnd_);
        if (dc != nullptr) {
            const int cx = measure_text_runs(dc, text_.c_str(),
                                             text_.size(), full_glyph_);
            const HGDIOBJ old = font_ != nullptr ? SelectObject(dc, font_) : nullptr;
            SIZE size{};
            if (GetTextExtentPoint32W(dc, text_.c_str(),
                                      static_cast<int>(text_.size()), &size)) {
                const int w = cx + 16;
                const int h = size.cy + 8;
                if (w > 8 && h > 8) {
                    width_ = w;
                    height_ = h;
                }
            }
            if (old != nullptr) SelectObject(dc, old);
            ReleaseDC(hwnd_, dc);
        }
        InvalidateRect(hwnd_, nullptr, TRUE);
        note_invalidate();
    }

    // Position and size are applied by ime_ui's place_windows(), which owns
    // the caret anchor and the screen-edge clamping.
    show();
}

void CompositionWindow::on_paint(HDC dc, const RECT& client) {
    FillRect(dc, &client,
             reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    if (text_.empty()) return;

    SetBkMode(dc, TRANSPARENT);

    // The engine converts left to right: the first syllable is the one the
    // candidate window is currently offering (recompute() looks up
    // syllables().front()), everything after it is still pending. Drawing the
    // pending part grey is what makes that legible — before this, "zhongguo"
    // looked like one live 8-letter choice while the candidates silently
    // belonged to the last two letters.
    const bool has_segments = !syllable_ends_.empty();
    const std::size_t active_end =
        has_segments ? syllable_ends_.front() : text_.size();

    const int underline_y = 4 + (client.bottom - client.top) - 7;

    // Walk the segments; x always comes from a prefix measurement so the
    // letters never overlap even if one GetTextExtentPoint32W call fails.
    std::size_t begin = 0;
    for (std::size_t i = 0; i <= syllable_ends_.size(); ++i) {
        const bool last = i == syllable_ends_.size();
        const std::size_t end = last ? text_.size() : syllable_ends_[i];
        if (end <= begin || end > text_.size()) continue;

        // Prefix measurements are run-aware in [全] so a wide-plane glyph
        // before this segment cannot shift the underline off by a code point.
        const int from_cx = measure_text_runs(dc, text_.c_str(), begin,
                                              full_glyph_);
        const int to_cx = measure_text_runs(dc, text_.c_str(), end,
                                            full_glyph_);

        const bool active = begin < active_end;
        SetTextColor(dc, active ? RGB(0, 0, 0) : RGB(130, 130, 130));
        draw_text_runs(dc, 4 + from_cx, 3, text_.c_str() + begin,
                       end - begin, full_glyph_);

        if (has_segments && to_cx > from_cx) {
            RECT line{};
            line.left = 4 + from_cx;
            line.right = 4 + to_cx;
            line.top = underline_y;
            line.bottom = underline_y + 1;
            FillRect(dc, &line,
                     reinterpret_cast<HBRUSH>(GetStockObject(
                         active ? BLACK_BRUSH : LTGRAY_BRUSH)));
        }
        begin = end;
    }
}

}  // namespace ui
}  // namespace imfw
