// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// M2 single-character pinyin session. Implements the stable imfw::Engine
// contract; the framework host only has to translate keys and render Result.
//
// Typed letters are accumulated as canonical pinyin (lowercase, 'v' for the
// u-umlaut sound). An apostrophe is a hard syllable separator (xian / xi'an).
// Invalid pinyin prefixes are rejected rather than accumulating garbage.

#pragma once

#include <imfw/code_table.hpp>
#include <imfw/engine.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace rospinyin {

// Longest-match-first segmentation with backtracking. Returns an empty vector
// when the string cannot be split entirely into known syllables.
std::vector<std::string> segment_syllables(const std::string& letters,
                                           const imfw::CodeTable& table);

// Convenience builder: add one reading to a table, deriving the lookup key
// with pinyin::syllable_of. Host tests and the built-in fallback table use
// this so neither needs to know the key derivation twice.
void add_reading(imfw::CodeTable& table, char32_t codepoint,
                 std::uint32_t rank, const std::string& reading);

class PinyinSession final : public imfw::Engine {
public:
    // Holds a shared reference: the owning PinyinScheme may rebuild its
    // table (reload_data) while old sessions from a previous generation
    // stay valid through the shared_ptr.
    explicit PinyinSession(std::shared_ptr<const imfw::CodeTable> table)
        : table_(std::move(table)) {}

    imfw::Result feed(char32_t codepoint) override;
    imfw::Result backspace() override;
    imfw::Result cancel() override;
    imfw::Result commit_text(const std::u32string& text) override;
    void on_select(bool selected) override;

    const std::string& composition() const noexcept { return composition_; }
    const std::vector<imfw::Candidate>& candidates() const noexcept {
        return candidates_;
    }
    const std::vector<std::string>& syllables() const noexcept {
        return syllables_;
    }

    // Characters with no frequency rank come from Unihan alone: 34,457 of the
    // 44,355 entries, mostly CJK Ext-A/B. Most have no glyph in the bundled
    // font subset, so rendering them only produces boxes. They stay out of the
    // candidate list by default; the syllable still works because the filter
    // falls back to the full list when it would leave nothing to choose from.
    void set_show_rare(bool show) noexcept { show_rare_ = show; }
    bool show_rare() const noexcept { return show_rare_; }

private:
    void recompute();
    imfw::Result commit(std::size_t index);

    std::shared_ptr<const imfw::CodeTable> table_;
    std::string composition_;
    std::vector<imfw::Candidate> candidates_;
    std::vector<std::string> syllables_;
    bool active_ = false;
    bool show_rare_ = false;
};

}  // namespace rospinyin
