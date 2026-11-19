// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Binary layout of rospinyin.dat, produced by mkdat.
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
//
// Two versions are alive at once:
//
//   v1 (kVersion below) is what ships today. It has no glyph-visibility bit,
//   so every entry reads as tier 0 ("renderable by the bundled font"). The
//   loader must keep accepting it: built-in tables, CI artefacts and the VM
//   images all carry v1 blobs.
//
//   v2 (kVersionV2) is the unified skeleton frozen on 2026-11-20, shared with
//   RosXingMa's shape.dat (ROSSHDAT) -- same 64-byte header and 24-byte entry,
//   the magic selects the meaning of the shared slots. The authoritative text
//   is xingma/docs/glyph-tier/UNIFIED-DAT-FORMAT.md; the pinyin reading of it
//   (including the three pinyin-only validation red lines: ROSPYDAT-only magic,
//   kUnknownRank is legal, slot 4 belongs to alt_off) is docs/plan/
//   DAT-FORMAT-V2.md. The structs below are that contract's machine-checked
//   half; the loader switch lands with F-T1.

#pragma once

#include <cstddef>
#include <cstdint>

namespace imfw {
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

// ---------------------------------------------------------------------------
// v2 — unified skeleton (frozen 2026-11-20). See the file header comment and
// docs/plan/DAT-FORMAT-V2.md before touching anything here.
// ---------------------------------------------------------------------------

constexpr std::uint32_t kVersionV2 = 2;

// RosXingMa's tag on the very same skeleton. The pinyin loader must reject it:
// reading a shape table as a pinyin table yields plausible-looking garbage
// (a pool offset as a code point, a byte length as a syllable id) instead of
// an honest failure. Shared validation takes the expected magic as a parameter.
constexpr char kMagicShape[8] = {'R', 'O', 'S', 'S', 'H', 'D', 'A', 'T'};

// Entry slot 5, shared by both products: bit1 = tier, 0 = the bundled font
// (Droid Sans Fallback) can draw the glyph, 1 = an extra font is required.
// Every other bit is reserved and must be zero; a reader rejects a non-zero
// reserved bit rather than guessing at a newer format.
constexpr std::uint32_t kFlagTier = 0x00000002u;
constexpr std::uint32_t kFlagsMask = kFlagTier;

struct HeaderV2 {
    char magic[8];                // ROSPYDAT (pinyin) / ROSSHDAT (shape)
    std::uint32_t version;        // 2
    std::uint32_t flags;          // reserved, 0
    std::uint32_t entry_count;    // v1's char_count
    std::uint32_t code_count;     // v1's syl_count
    std::uint32_t entry_table_off;  // must equal sizeof(HeaderV2)
    std::uint32_t code_table_off;   // entry_table_off + entry_count * 24
    std::uint32_t pool_off;         // code_table_off + code_count * 16
    std::uint32_t pool_size;
    std::uint32_t name_off;       // shape only; pinyin writes 0
    std::uint32_t alphabet_off;   // shape only; pinyin writes 0
    std::uint32_t source_off;     // shape only; pinyin writes 0
    std::uint8_t max_code_len;    // shape only; pinyin writes 0
    std::uint8_t auto_commit;     // shape only; pinyin writes 0
    std::uint8_t reserved[10];    // reserved, 0
};

// 24 bytes: the v1 CharEntry plus slot 5. Slots 0-4 keep v1's offsets, which is
// what makes a v1 blob readable with "flags == 0, stride 20" (docs §6).
struct CharEntryV2 {
    std::uint32_t codepoint;      // slot 0: UTF-32 (pinyin)
    std::uint32_t rank;           // slot 1: shared; kUnknownRank is legal
    std::uint32_t syl_id;         // slot 2: syllable-table index (pinyin)
    std::uint32_t reading_off;    // slot 3: pool offset, canonical reading
    std::uint32_t alt_off;        // slot 4: pool offset, alt readings, 0 if none
    std::uint32_t flags;          // slot 5: shared; bit1 = tier (see kFlagTier)
};

// v2 reuses SylEntry unchanged: slots are name_off / first_char / char_count /
// reserved. In the unified skeleton slot 3 is named `flags` (shape stores its
// short-code marks there); pinyin must write 0 and the loader checks it.
static_assert(sizeof(HeaderV2) == 64, "dat::HeaderV2 must be exactly 64 bytes");
static_assert(sizeof(CharEntryV2) == 24, "dat::CharEntryV2 must be exactly 24 bytes");

constexpr std::size_t kHeaderV2Size = sizeof(HeaderV2);
constexpr std::size_t kCharEntryV2Size = sizeof(CharEntryV2);

}  // namespace dat
}  // namespace imfw
