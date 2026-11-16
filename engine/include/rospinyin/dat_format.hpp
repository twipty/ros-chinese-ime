// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Binary layout of rospinyin.dat (version 1), produced by mkdat.
// Shared by the compiler, the host-side checker and the run-time engine.
//
// Design goals:
//   * mmap-friendly: a fixed-size header, two fixed-size record arrays, then
//     one NUL-terminated string pool. No pointers, no relocation needed.
//   * little-endian, 32-bit fields only (target is 32-bit ReactOS x86).
//   * deterministic: given the same inputs, byte-for-byte identical output.
//
// Layout (all offsets are absolute file offsets unless stated otherwise):
//   [Header][CharEntry * char_count][SylEntry * syl_count][string pool]
//
// CharEntry records are sorted by (syl_id, rank, codepoint), so the chars of
// any one syllable form a contiguous range [first_char, first_char+count)
// inside which rank is ascending. Unknown-rank entries sort after ranked ones.
// String-pool offsets inside records are relative to the start of the pool.

#pragma once

#include <cstddef>
#include <cstdint>

namespace rospinyin {
namespace dat {

constexpr char kMagic[8] = {'R', 'O', 'S', 'P', 'Y', 'D', 'A', 'T'};
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kUnknownRank = 0xFFFFFFFFu;

struct Header {
    char magic[8];
    std::uint32_t version;
    std::uint32_t flags;          // reserved, 0
    std::uint32_t char_count;
    std::uint32_t syl_count;
    std::uint32_t char_table_off;
    std::uint32_t syl_table_off;
    std::uint32_t pool_off;
    std::uint32_t pool_size;
    std::uint32_t reserved[3];    // reserved, 0
};

struct CharEntry {
    std::uint32_t codepoint;      // Unicode code point (UTF-32)
    std::uint32_t rank;           // 1-based frequency rank; kUnknownRank if none
    std::uint32_t syl_id;         // index into the syllable table
    std::uint32_t reading_off;    // pool offset: canonical reading "zho1ng"
    std::uint32_t alt_off;        // pool offset: space-separated alt readings, 0 if none
};

struct SylEntry {
    std::uint32_t name_off;       // pool offset: toneless syllable "zhong"
    std::uint32_t first_char;     // first index into the char table
    std::uint32_t char_count;     // number of chars in this contiguous range
    std::uint32_t reserved;
};

static_assert(sizeof(Header) == 52, "dat::Header must be exactly 52 bytes");
static_assert(sizeof(CharEntry) == 20, "dat::CharEntry must be exactly 20 bytes");
static_assert(sizeof(SylEntry) == 16, "dat::SylEntry must be exactly 16 bytes");

constexpr std::size_t kHeaderSize = sizeof(Header);
constexpr std::size_t kCharEntrySize = sizeof(CharEntry);
constexpr std::size_t kSylEntrySize = sizeof(SylEntry);

}  // namespace dat
}  // namespace rospinyin
