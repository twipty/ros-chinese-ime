// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Generic code table: a read-only view over an imfw .dat blob (dat_format.hpp).
// Platform independent: the caller supplies the bytes (file read, resource,
// embedded blob — the IME layer decides). No Windows headers, no file I/O.
//
// "Code" here is the generic lookup key of an input method. For pinyin it is
// the toneless syllable ("zhong"); for a shape-based IME it is the shape code
// ("wqvb"). The dat file carries its own key table (dat::SylEntry names), so
// the table never needs pinyin-specific knowledge to index itself.
//
// Polyphonic characters carry extra readings in CharEntry::alt_off (space
// separated, e.g. "zho4ng"). Those are only usable when the caller supplies a
// key deriver, because turning a reading into a lookup key is scheme policy:
// pinyin strips the tone digits, a shape IME would do something else.

#pragma once

#include <imfw/dat_format.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace imfw {

struct TableEntry {
    char32_t codepoint = 0;
    std::uint32_t rank = dat::kUnknownRank;
    std::string reading;  // full canonical reading, e.g. "zho1ng" / "wqvb"
    // Glyph tier copied verbatim from the record's slot-5 bit1:
    // 0 = the bundled font can draw it, 1 = an extra font is required.
    // The loader reads the bit but does not filter on it; an engine turns
    // this into Candidate::glyph_visible (glyph_visible = (tier == 0)).
    std::uint8_t tier = 0;
};

// A multi-syllable word from the optional lexicon dat (M4). The text is
// UTF-8-decoded so engines can place it straight into Candidate::text.
struct TableWord {
    std::u32string text;
    std::uint32_t rank = dat::kUnknownRank;
    std::uint32_t syl_count = 0;
    std::uint8_t tier = 0;
};

// Maps an alternative reading ("zho4ng") to the lookup key it should answer to
// ("zhong"). Must be a plain function pointer: the IME PE avoids std::function.
using KeyOfReading = std::string (*)(std::string_view);

class CodeTable {
public:
    // Parse a .dat blob. Returns the number of loaded characters, or 0 when
    // the header/offsets are invalid (caller keeps ownership of the bytes;
    // all strings are copied into this object).
    //
    // `key_of_reading` (optional) makes the alt readings reachable: every
    // character is then also indexed under each of them, so 谁 answers to both
    // "shei" and "shui". Pass nullptr to ignore alt readings entirely.
    std::size_t load_dat(std::string_view bytes,
                         KeyOfReading key_of_reading = nullptr);

    // Small-builder API used by host tests, fallback tables and embedded
    // data. `key` is the lookup key (the Scheme decides how to derive it
    // from `reading`, e.g. pinyin::syllable_of). Call build_index() after
    // adding all entries. `tier` defaults to 0 (bundled font), so callers
    // that do not grade glyphs are unaffected.
    void add(char32_t codepoint, std::uint32_t rank, std::string reading,
             std::string key, std::uint8_t tier = 0);
    // M4: words are indexed by the same toneless key the syllables use
    // ("nihao" matches the LexCode key "ni3hao3" once a deriver strips tones).
    void add_word(std::u32string text, std::uint32_t rank,
                  std::uint32_t syl_count, std::string key,
                  std::uint8_t tier = 0);
    void clear();
    void build_index();

    // Characters sharing one key, ordered by frequency rank (lower first),
    // then codepoint for a stable tie-break.
    std::vector<TableEntry> lookup(std::string_view key) const;
    std::vector<TableWord> lookup_words(std::string_view key) const;

    // Load a word-only dat on top of an already loaded character table.
    // Returns the number of words, or 0 if the optional file is invalid.
    // Existing characters are untouched; a previous word list is replaced.
    std::size_t load_lexicon_dat(std::string_view bytes,
                                 KeyOfReading key_of_reading = nullptr);

    bool is_code(std::string_view key) const;
    bool is_prefix(std::string_view prefix) const;

    std::size_t char_count() const noexcept { return entries_.size(); }
    std::size_t code_count() const noexcept { return by_code_.size(); }
    std::size_t word_count() const noexcept { return words_.size(); }

private:
    struct WordEntry {
        std::u32string text;
        std::uint32_t rank = dat::kUnknownRank;
        std::uint32_t syl_count = 0;
        std::uint8_t tier = 0;
        std::string key;
    };

    // Appends words from a validated lexicon; shared by load_dat and the
    // optional word-only dat. `key_of_reading` turns "ni3hao3" into "nihao".
    bool parse_lexicon(std::string_view bytes, const dat::Header& header,
                       KeyOfReading key_of_reading);

    struct Entry {
        char32_t codepoint = 0;
        std::uint32_t rank = dat::kUnknownRank;
        std::string reading;
        std::string key;
        // dat slot-5 bit1, preserved so lookup() can hand it to the engine.
        std::uint8_t tier = 0;
        // Additional keys this character answers to, derived from the dat's
        // alt readings. Empty unless load_dat() was given a key deriver.
        std::vector<std::string> alt_keys;
    };

    std::vector<Entry> entries_;
    std::vector<WordEntry> words_;
    std::map<std::string, std::vector<std::size_t>> by_code_;
    std::map<std::string, std::vector<std::size_t>> by_word_;
    std::set<std::string> codes_;
    std::set<std::string> prefixes_;
};

}  // namespace imfw
