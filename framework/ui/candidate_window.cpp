// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// See candidate_window.h.

#include "candidate_window.h"

#include <windows.h>

#include <cstddef>
#include <string>

#ifndef SWP_NOMOVE
#define SWP_NOMOVE 0x0002
#endif
#ifndef SWP_NOZORDER
#define SWP_NOZORDER 0x0004
#endif
#ifndef SWP_NOACTIVATE
#define SWP_NOACTIVATE 0x0010
#endif
#ifndef TRANSPARENT
#define TRANSPARENT 1
#endif
#ifndef LTGRAY_BRUSH
#define LTGRAY_BRUSH 1
#endif

namespace imfw {
namespace ui {
namespace {

std::wstring utf32_to_wide(const std::u32string& text) {
    std::wstring out;
    out.reserve(text.size());
    for (const char32_t cp : text) {
        if (cp == 0) break;
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<wchar_t>(cp));
        } else {
            const std::uint32_t v = static_cast<std::uint32_t>(cp) - 0x10000u;
            out.push_back(static_cast<wchar_t>(0xD800u + (v >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00u + (v & 0x3FFu)));
        }
    }
    return out;
}

std::wstring small_number(std::size_t value) {
    wchar_t buf[16] = {};
    int pos = 15;
    if (value == 0) {
        buf[--pos] = L'0';
    } else {
        while (value != 0 && pos > 0) {
            buf[--pos] = static_cast<wchar_t>(L'0' + (value % 10));
            value /= 10;
        }
    }
    return std::wstring(buf + pos, buf + 16);
}

}  // namespace

void CandidateWindow::update(HINSTANCE inst, const wchar_t* class_name,
                             const CandidatePager& pager) {
    if (pager.empty()) {
        lines_.clear();
        hide();
        return;
    }

    std::vector<std::wstring> next;
    const Candidate* begin = pager.page_begin();
    const std::size_t count = pager.page_size();
    next.reserve(count + 1);
    for (std::size_t i = 0; i < count; ++i) {
        std::wstring line;
        line.push_back(static_cast<wchar_t>(L'1' + i));
        line += L". ";
        line += utf32_to_wide(begin[i].text);
        next.push_back(std::move(line));
    }

    std::wstring footer = L"  -  ";
    footer += small_number(pager.page() + 1);
    footer += L"/";
    footer += small_number(pager.page_count());
    footer += L"  +";
    next.push_back(std::move(footer));

    // Same dedupe as the composition window: an engine update that produces
    // the identical page (key repeat, IMM32 re-entry) must not repaint.
    const bool content_changed = next != lines_;
    lines_.swap(next);

    if (!ensure_created(inst, class_name, L"", 160,
                        static_cast<int>(lines_.size() * line_height_) + 8)) {
        return;
    }

    if (content_changed) {
        HDC dc = GetDC(hwnd_);
        if (dc != nullptr) {
            const HGDIOBJ old = font_ != nullptr ? SelectObject(dc, font_) : nullptr;
            int max_width = 0;
            for (const auto& line : lines_) {
                SIZE size{};
                if (GetTextExtentPoint32W(dc, line.c_str(),
                                          static_cast<int>(line.size()), &size)) {
                    if (size.cx > max_width) max_width = size.cx;
                    if (size.cy > 0) line_height_ = static_cast<std::size_t>(size.cy + 4);
                }
            }
            if (old != nullptr) SelectObject(dc, old);
            ReleaseDC(hwnd_, dc);

            const int width = max_width + 20;
            const int height = static_cast<int>(lines_.size() * line_height_) + 8;
            if (width > 16 && height > 16) {
                width_ = width;
                height_ = height;
            }
        }
        // The size is applied together with the position by place_windows().
        InvalidateRect(hwnd_, nullptr, TRUE);
        note_invalidate();
    }

    show();
}

void CandidateWindow::on_paint(HDC dc, const RECT& client) {
    FillRect(dc, &client,
             reinterpret_cast<HBRUSH>(GetStockObject(LTGRAY_BRUSH)));
    if (lines_.empty()) return;

    const HGDIOBJ old_font = font_ != nullptr ? SelectObject(dc, font_) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));

    int y = 4;
    for (const auto& line : lines_) {
        TextOutW(dc, 8, y, line.c_str(), static_cast<int>(line.size()));
        y += static_cast<int>(line_height_);
    }

    if (old_font != nullptr) SelectObject(dc, old_font);
}

}  // namespace ui
}  // namespace imfw
