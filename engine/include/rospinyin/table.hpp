// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Read-only view over rospinyin.dat (M2). Platform independent: the caller
// supplies the bytes (mmap'd file, resource, embedded blob - the IME layer
// decides). No Windows headers, no file I/O here.

#pragma once

#include <rospinyin/dat_format.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace rospinyin {

struct TableEntry {
    char32_t codepoint = 0;
    std::uint32_t rank = dat::kUnknownRank;
    std::string reading;  // canonical numeric form, e.g. "zho1ng"
    int tone = 0;
};

class PinyinTable {
public:
    // Parse a rospinyin.dat blob. Returns the number of loaded characters,
    // or 0 when the header/offsets are invalid (caller keeps ownership of
    // the bytes; all strings are copied into this object).
    std::size_t load_dat(std::string_view bytes);

    // Small-builder API used by host tests and by a future embedded fallback
    // table. Call build_index() after adding all entries.
    void add(char32_t codepoint, std::uint32_t rank, std::string reading);
    void clear();
    void build_index();

    // Characters sharing a toneless syllable, ordered by frequency rank
    // (lower first), then codepoint for a stable tie-break.
    std::vector<TableEntry> lookup(std::string_view syllable) const;

    bool is_syllable(std::string_view syllable) const;
    bool is_prefix(std::string_view prefix) const;

    std::size_t char_count() const noexcept { return entries_.size(); }
    std::size_t syllable_count() const noexcept { return syllables_.size(); }

private:
    std::vector<TableEntry> entries_;
    std::map<std::string, std::vector<std::size_t>> by_syllable_;
    std::set<std::string> syllables_;
    std::set<std::string> prefixes_;
};

}  // namespace rospinyin
