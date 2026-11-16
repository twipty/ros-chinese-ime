// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT

#include <imfw/candidate_pager.hpp>

namespace imfw {

void CandidatePager::reset(std::vector<Candidate> candidates) {
    candidates_ = std::move(candidates);
    page_ = 0;
}

std::size_t CandidatePager::page_count() const noexcept {
    if (candidates_.empty()) return 0;
    return (candidates_.size() + kPageSize - 1) / kPageSize;
}

bool CandidatePager::next_page() noexcept {
    if (page_ + 1 >= page_count()) return false;
    ++page_;
    return true;
}

bool CandidatePager::prev_page() noexcept {
    if (page_ == 0) return false;
    --page_;
    return true;
}

const Candidate* CandidatePager::select(std::size_t digit) const noexcept {
    if (digit >= kPageSize) return nullptr;
    const std::size_t index = page_ * kPageSize + digit;
    if (index >= candidates_.size()) return nullptr;
    return &candidates_[index];
}

const Candidate* CandidatePager::page_begin() const noexcept {
    if (candidates_.empty()) return nullptr;
    return &candidates_[page_ * kPageSize];
}

std::size_t CandidatePager::page_size() const noexcept {
    if (candidates_.empty()) return 0;
    const std::size_t begin = page_ * kPageSize;
    const std::size_t remaining = candidates_.size() - begin;
    return remaining < kPageSize ? remaining : kPageSize;
}

}  // namespace imfw
