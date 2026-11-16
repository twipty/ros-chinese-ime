// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Candidate list window, WBS 3.3.2/3.3.3. Renders the current CandidatePager
// page with 1..9 numbering and a prev/next page hint.

#pragma once

#include "ui_window.h"
#include <imfw/candidate_pager.hpp>

#include <string>
#include <vector>

namespace imfw {
namespace ui {

class CandidateWindow final : public UiWindow {
public:
    // class_name comes from the Scheme. Rebuilds the page view; an empty
    // pager hides the window.
    void update(HINSTANCE inst, const wchar_t* class_name,
                const CandidatePager& pager);

protected:
    void on_paint(HDC dc, const RECT& client) override;

private:
    std::vector<std::wstring> lines_;
    std::size_t line_height_ = 18;
};

}  // namespace ui
}  // namespace imfw
