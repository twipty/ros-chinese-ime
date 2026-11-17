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
    // adding all entries.
    void add(char32_t codepoint, std::uint32_t rank, std::string reading,
             std::string key);
    void clear();
    void build_index();

    // Characters sharing one key, ordered by frequency rank (lower first),
    // then codepoint for a stable tie-break.
    std::vector<TableEntry> lookup(std::string_view key) const;

    bool is_code(std::string_view key) const;
    bool is_prefix(std::string_view prefix) const;

    std::size_t char_count() const noexcept { return entries_.size(); }
    std::size_t code_count() const noexcept { return by_code_.size(); }

private:
    struct Entry {
        char32_t codepoint = 0;
        std::uint32_t rank = dat::kUnknownRank;
        std::string reading;
        std::string key;
        // Additional keys this character answers to, derived from the dat's
        // alt readings. Empty unless load_dat() was given a key deriver.
        std::vector<std::string> alt_keys;
    };

    std::vector<Entry> entries_;
    std::map<std::string, std::vector<std::size_t>> by_code_;
    std::set<std::string> codes_;
    std::set<std::string> prefixes_;
};

}  // namespace imfw
