// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// See composition_window.h.

#include "composition_window.h"

#include <imfw/utf8.hpp>

#include <windows.h>

#ifndef TRANSPARENT
#define TRANSPARENT 1
#endif
#ifndef WHITE_BRUSH
#define WHITE_BRUSH 0
#endif
#ifndef BLACK_BRUSH
#define BLACK_BRUSH 4
#endif

namespace imfw {
namespace ui {

void CompositionWindow::update(HINSTANCE inst, const wchar_t* class_name,
                               const wchar_t* text,
                               const std::vector<std::string>& segments) {
    if (text == nullptr || *text == L'\0') {
        text_.clear();
        syllable_ends_.clear();
        hide();
        return;
    }

    text_ = text;
    syllable_ends_.clear();
    std::size_t end = 0;
    for (const auto& seg : segments) {
        // Segments are UTF-8 (engine.hpp); underline ends are indexes into
        // the wide composition text, so accumulate the decoded width, not
        // the byte count (G4: CJK segments must underline correctly).
        end += imfw::utf8_to_wide(seg).size();
        if (end <= text_.size()) syllable_ends_.push_back(end);
    }

    // Rough width: 8 px per latin char is enough for a 16 px font; refine in
    // WM_PAINT where the real font metrics are available.
    const int width = static_cast<int>(text_.size() * 9) + 16;
    if (!ensure_created(inst, class_name, L"", width, 24)) return;

    HDC dc = GetDC(hwnd_);
    if (dc != nullptr) {
        HGDIOBJ old = font_ != nullptr ? SelectObject(dc, font_) : nullptr;
        SIZE size{};
        if (GetTextExtentPoint32W(dc, text_.c_str(),
                                  static_cast<int>(text_.size()), &size)) {
            const int w = size.cx + 16;
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
    show();
}

void CompositionWindow::on_paint(HDC dc, const RECT& client) {
    FillRect(dc, &client,
             reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    if (text_.empty()) return;

    const HGDIOBJ old_font = font_ != nullptr ? SelectObject(dc, font_) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    TextOutW(dc, 4, 3, text_.c_str(), static_cast<int>(text_.size()));

    // Underline each recognized syllable so the user can see where the engine
    // split the stream (e.g. [zhong][guo]).
    std::size_t begin = 0;
    for (const std::size_t end : syllable_ends_) {
        if (end <= begin || end > text_.size()) continue;
        SIZE head{};
        SIZE full{};
        GetTextExtentPoint32W(dc, text_.c_str(), static_cast<int>(begin), &head);
        GetTextExtentPoint32W(dc, text_.c_str(), static_cast<int>(end), &full);
        RECT line{};
        line.left = 4 + head.cx;
        line.right = 4 + full.cx;
        line.top = 4 + (client.bottom - client.top) - 7;
        line.bottom = line.top + 1;
        if (line.right > line.left) {
            FillRect(dc, &line,
                     reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        }
        begin = end;
    }

    if (old_font != nullptr) SelectObject(dc, old_font);
}

}  // namespace ui
}  // namespace imfw
