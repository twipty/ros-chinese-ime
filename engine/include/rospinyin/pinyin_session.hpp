// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// M2 single-character pinyin session. Implements the stable engine.hpp
// contract; the IME layer only has to translate keys and render Result.
//
// Typed letters are accumulated as canonical pinyin (lowercase, 'v' for the
// u-umlaut sound). An apostrophe is a hard syllable separator (xian / xi'an).
// Invalid pinyin prefixes are rejected rather than accumulating garbage.

#pragma once

#include <rospinyin/engine.hpp>
#include <rospinyin/table.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace rospinyin {

// Longest-match-first segmentation with backtracking. Returns an empty vector
// when the string cannot be split entirely into known syllables.
std::vector<std::string> segment_syllables(const std::string& letters,
                                           const PinyinTable& table);

class PinyinSession final : public Engine {
public:
    explicit PinyinSession(const PinyinTable& table) : table_(table) {}

    Result feed(char32_t codepoint) override;
    Result backspace() override;
    Result cancel() override;
    void on_select(bool selected) override;

    const std::string& composition() const noexcept { return composition_; }
    const std::vector<Candidate>& candidates() const noexcept {
        return candidates_;
    }
    const std::vector<std::string>& syllables() const noexcept {
        return syllables_;
    }

private:
    void recompute();
    Result commit(std::size_t index);

    const PinyinTable& table_;
    std::string composition_;
    std::vector<Candidate> candidates_;
    std::vector<std::string> syllables_;
    bool active_ = false;
};

}  // namespace rospinyin
