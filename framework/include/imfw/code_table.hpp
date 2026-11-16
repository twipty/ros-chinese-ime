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

class CodeTable {
public:
    // Parse a .dat blob. Returns the number of loaded characters, or 0 when
    // the header/offsets are invalid (caller keeps ownership of the bytes;
    // all strings are copied into this object).
    std::size_t load_dat(std::string_view bytes);

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
    };

    std::vector<Entry> entries_;
    std::map<std::string, std::vector<std::size_t>> by_code_;
    std::set<std::string> codes_;
    std::set<std::string> prefixes_;
};

}  // namespace imfw
