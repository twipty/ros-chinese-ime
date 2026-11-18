// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Platform-independent pinyin normalization (M2, decision D2=A + v-key rule).
// No Windows headers, no IMM32, no locale dependency: this is shared by the
// mkdat compiler (build time) and the pinyin engine (run time) so that both
// agree on one canonical representation.
//
// Canonical form rules (see docs/milestones/m2/M2-DATA-PLAN.md section 7):
//   * tone marks become a digit immediately AFTER the toned vowel:
//       zhong-with-macron-o -> "zho1ng", "guo2", not "zhong1"
//   * the entire u-umlaut family collapses to 'v':
//       n + u-umlaut-with-caron -> "nv3", l + u-umlaut-with-grave -> "lv4"
//       l + u-umlaut + e-with-grave -> "lve4"
//   * syllabic nasals: m-with-acute -> "m2", n-with-grave -> "n4",
//     n-with-caron -> "n3" (e.g. "ng-with-grave" -> "n4g")
//   * ASCII letters are lowercased; ASCII apostrophe (syllable separator)
//     is preserved.
//
// Why 'v' and not 'u:': the Owner decided that in actual typing the 'v' key
// represents the u-umlaut sound. Neither Unihan nor hanziDB contains a
// literal 'v', so the mapping is lossless in both directions.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace rospinyin {
namespace pinyin {

// Convert a tone-marked reading (Unihan kMandarin or hanziDB pinyin column)
// into canonical numeric form, e.g. "zh\u014dng" -> "zho1ng", "n\u01d6" -> "nv3".
// Returns an empty string if the input contains an invalid UTF-8 sequence or
// a non-ASCII character that is not a known tone mark (data-integrity signal).
std::string marked_to_numeric(std::string_view marked);

// Strip tone digits 1-4 to obtain the toneless syllable used as the
// segmentation key, e.g. "zho1ng" -> "zhong", "nv3" -> "nv", "n4g" -> "ng".
std::string syllable_of(std::string_view numeric);

// Extract the tone (0 = neutral/no digit, 1..4 = the four tones).
int tone_of(std::string_view numeric) noexcept;

// A canonical reading split into its toneless syllable and tone.
struct Reading {
    std::string syllable;
    int tone = 0;
};
Reading split(std::string_view numeric);

// True when the string only contains [a-z0-9'] and no digit outside 1-4.
// Used by mkdat to assert that no raw u-umlaut / tone mark leaked through.
bool is_canonical(std::string_view s) noexcept;

// Named alias for call sites that read hanziDB's pinyin column.
inline std::string canonical(std::string_view pinyin_text) {
    return marked_to_numeric(pinyin_text);
}

}  // namespace pinyin
}  // namespace rospinyin
