// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Candidate list window, WBS 3.3.2/3.3.3. Renders the current CandidatePager
// page with 1..9 numbering and a prev/next page hint.

#pragma once

#include "ui_window.h"
#include <rospinyin/candidate_pager.hpp>

#include <string>
#include <vector>

namespace rospinyin {
namespace ui {

class CandidateWindow final : public UiWindow {
public:
    // Rebuilds the page view. An empty pager hides the window.
    void update(HINSTANCE inst, const CandidatePager& pager);

protected:
    void on_paint(HDC dc, const RECT& client) override;

private:
    std::vector<std::wstring> lines_;
    std::size_t line_height_ = 18;
};

}  // namespace ui
}  // namespace rospinyin
