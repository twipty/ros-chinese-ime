// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// M2 candidate paging logic (WBS 3.3.2/3.3.3). Pure C++, no Win32: the UI
// layer renders the current page and the host tests can verify paging,
// digit selection and edge cases without a GUI.

#pragma once

#include <imfw/engine.hpp>

#include <cstddef>
#include <vector>

namespace imfw {

class CandidatePager {
public:
    static constexpr std::size_t kPageSize = 9;

    // Replace the candidate set and reset to the first page.
    void reset(std::vector<Candidate> candidates);

    // Replace the candidate set but keep the current page when it still
    // exists, clamping down when the new list has fewer pages (X2-w3).
    // Typing another letter must not drag the user back to page 1.
    void replace(std::vector<Candidate> candidates);

    std::size_t total() const noexcept { return candidates_.size(); }
    std::size_t page() const noexcept { return page_; }
    std::size_t page_count() const noexcept;
    bool empty() const noexcept { return candidates_.empty(); }

    bool next_page() noexcept;
    bool prev_page() noexcept;

    // Digit 0..8 selects the corresponding candidate on the current page.
    // Returns nullptr when the digit is out of range.
    const Candidate* select(std::size_t digit) const noexcept;

    // Current page view for rendering: page_begin() + page_size().
    const Candidate* page_begin() const noexcept;
    std::size_t page_size() const noexcept;

private:
    std::vector<Candidate> candidates_;
    std::size_t page_ = 0;
};

}  // namespace imfw
