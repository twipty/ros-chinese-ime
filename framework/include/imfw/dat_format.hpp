// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Binary layout of rospinyin.dat, produced by mkdat.
// Shared by the compiler, the host-side checker and the run-time engine.
//
// Design goals:
//   * mmap-friendly: a fixed-size header, four fixed-size record arrays, then
//     one NUL-terminated string pool. No pointers, no relocation needed.
//   * little-endian, 32-bit fields only (target is 32-bit ReactOS x86).
//   * deterministic: given the same inputs, byte-for-byte identical output.
//   * self-describing: the header states the record sizes it was written with,
//     so a half-upgraded file is rejected instead of misread.
//
// Layout (all offsets are absolute file offsets; string-pool offsets are
// relative to the start of the pool):
//
//   [Header][CharEntry * entry_count][SylEntry * code_count]
//           [LexEntry * lex_entry_count][LexCode * lex_code_count][pool]
//
// One file carries both the single-character table and the word list. The two
// tables share a record shape but not a meaning: in the character table slot 0
// is a code point and slot 2 a syllable id, in the word table slot 0 is a pool
// offset and slot 2 a syllable count. No tag distinguishes them -- which table
// you are walking decides it -- so the two counts must never be conflated.
//
// Char entries are sorted by (syl_id, rank, codepoint), so the characters of
// one syllable form a contiguous range [first_entry, first_entry+entry_count)
// inside which rank ascends. Unknown-rank entries sort after ranked ones.
//
// Version note: version 2 is the only version this header describes and the
// only one the loader accepts. Version 1 tables (52-byte header, 20-byte
// entries) are rejected outright -- they predate the tier bit and the word
// table, and an IME that silently accepted them would filter candidates on a
// bit that is not there. Rebuild the table with mkdat instead.
//
// The authoritative text is xingma/docs/glyph-tier/UNIFIED-DAT-FORMAT.md; the
// pinyin reading of it is docs/plan/DAT-FORMAT-V2.md. RosXingMa's shape.dat
// uses the same skeleton under another magic.

#pragma once

#include <cstddef>
#include <cstdint>

namespace imfw {
namespace dat {

constexpr char kMagic[8] = {'R', 'O', 'S', 'P', 'Y', 'D', 'A', 'T'};

// RosXingMa's tag on the very same skeleton. The pinyin loader must reject it:
// reading a shape table as a pinyin table yields plausible-looking garbage
// (a pool offset as a code point, a byte length as a syllable id) instead of
// an honest failure. Shared validation takes the expected magic as a
// parameter, so each product's loader only ever accepts its own tag.
constexpr char kMagicShape[8] = {'R', 'O', 'S', 'S', 'H', 'D', 'A', 'T'};

constexpr std::uint32_t kVersion = 2;

// Rank sentinel: the character exists in the data but has no frequency order
// (34,457 of 44,355 entries). A loader that demands rank > 0 would drop every
// one of them, so the sentinel is a legal value, not a corruption.
constexpr std::uint32_t kUnknownRank = 0xFFFFFFFFu;

// Entry slot 5, shared by both products: bit1 = tier, 0 = the bundled font
// (Droid Sans Fallback) can draw the glyph, 1 = an extra font is required.
// Every other bit is reserved and must be zero; a reader rejects a non-zero
// reserved bit rather than guessing at a newer format.
constexpr std::uint32_t kFlagTier = 0x00000002u;
constexpr std::uint32_t kFlagsMask = kFlagTier;

struct Header {
    char magic[8];                 // ROSPYDAT (pinyin) / ROSSHDAT (shape)
    std::uint32_t version;         // 2
    std::uint32_t flags;           // reserved, 0
    std::uint32_t entry_count;     // CharEntry records (characters)
    std::uint32_t code_count;      // SylEntry records (syllables)
    std::uint32_t lex_entry_count;  // LexEntry records (words); 0 = no word list
    std::uint32_t lex_code_count;   // LexCode records (syllable-sequence keys)
    std::uint32_t entry_table_off;      // must equal sizeof(Header)
    std::uint32_t code_table_off;       // entry_table_off + entry_count * entry_size
    std::uint32_t lex_entry_table_off;  // code_table_off + code_count * code_size
    std::uint32_t lex_code_table_off;   // lex_entry_table_off + lex_entry_count * entry_size
    std::uint32_t pool_off;             // lex_code_table_off + lex_code_count * code_size
    std::uint32_t pool_size;            // pool_off + pool_size <= file size
    std::uint32_t name_off;        // shape only; pinyin writes 0
    std::uint32_t alphabet_off;    // shape only; pinyin writes 0
    std::uint32_t source_off;      // shape only; pinyin writes 0
    std::uint32_t entry_size;      // must equal sizeof(CharEntry) / sizeof(LexEntry)
    std::uint32_t code_size;       // must equal sizeof(SylEntry) / sizeof(LexCode)
    std::uint8_t max_code_len;     // shape only; pinyin writes 0
    std::uint8_t auto_commit;      // shape only; pinyin writes 0
    std::uint8_t reserved[2];      // reserved, 0
};

// A character. Six slots, all of them spoken for.
struct CharEntry {
    std::uint32_t codepoint;      // slot 0: UTF-32
    std::uint32_t rank;           // slot 1: 1-based; kUnknownRank if none
    std::uint32_t syl_id;         // slot 2: index into the syllable table
    std::uint32_t reading_off;    // slot 3: pool offset, canonical reading "zho1ng"
    std::uint32_t alt_off;        // slot 4: pool offset, space-separated alt readings, 0 if none
    std::uint32_t flags;          // slot 5: bit1 = tier (see kFlagTier)
};

// A syllable. Slot 3 is named `flags` in the unified skeleton (shape stores its
// short-code marks there); pinyin writes 0 and the loader checks it.
struct SylEntry {
    std::uint32_t name_off;       // slot 0: pool offset, toneless syllable "zhong"
    std::uint32_t first_entry;    // slot 1: first index into the char table
    std::uint32_t entry_count;    // slot 2: characters in this contiguous range
    std::uint32_t flags;          // slot 3: reserved for pinyin, 0
};

// A word. Same 24-byte shape as CharEntry, read as a different table: slot 0 is
// where the text lives, not a code point, and slot 2 counts syllables over the
// whole word. The single/word distinction is carried by which table the record
// sits in, so the magic stays unambiguous and no per-record tag is needed.
struct LexEntry {
    std::uint32_t text_off;       // slot 0: pool offset, UTF-8 word, NUL-terminated
    std::uint32_t rank;           // slot 1: 1-based; kUnknownRank if none
    std::uint32_t syl_count;      // slot 2: syllables in the word, >= 2
    std::uint32_t reading_off;    // slot 3: pool offset "ni3 hao3"; 0 = derive from key
    std::uint32_t alt_off;        // slot 4: reserved, must be 0
    std::uint32_t flags;          // slot 5: bit1 = tier; bit0 and the rest must be 0
};

// A word's lookup key: the concatenated toned syllables ("ni3hao3"). Keys are
// strings rather than syllable ids so the word list does not depend on the
// character table's syllable numbering -- either table can be rebuilt alone.
// Records are sorted by key bytes, which lets the runtime binary-search the
// pool without a second index.
struct LexCode {
    std::uint32_t key_off;        // slot 0: pool offset, key string
    std::uint32_t first_entry;    // slot 1: first LexEntry of this run
    std::uint32_t entry_count;    // slot 2: run length
    std::uint32_t flags;          // slot 3: reserved, 0
};

static_assert(sizeof(Header) == 80, "dat::Header must be exactly 80 bytes");
static_assert(sizeof(CharEntry) == 24, "dat::CharEntry must be exactly 24 bytes");
static_assert(sizeof(SylEntry) == 16, "dat::SylEntry must be exactly 16 bytes");
static_assert(sizeof(LexEntry) == 24, "dat::LexEntry must be exactly 24 bytes");
static_assert(sizeof(LexCode) == 16, "dat::LexCode must be exactly 16 bytes");
// The header carries one entry_size and one code_size for all four tables, so
// the character and word records must stay the same width. Widening either
// needs a new version, not a second size field.
static_assert(sizeof(CharEntry) == sizeof(LexEntry),
              "char and word records share the header's entry_size");
static_assert(sizeof(SylEntry) == sizeof(LexCode),
              "syllable and word-key records share the header's code_size");

constexpr std::size_t kHeaderSize = sizeof(Header);
constexpr std::size_t kCharEntrySize = sizeof(CharEntry);
constexpr std::size_t kSylEntrySize = sizeof(SylEntry);
constexpr std::size_t kLexEntrySize = sizeof(LexEntry);
constexpr std::size_t kLexCodeSize = sizeof(LexCode);

}  // namespace dat
}  // namespace imfw
