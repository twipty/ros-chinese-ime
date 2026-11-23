// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// M3: the shared ASCII -> fullwidth Chinese punctuation table.
//
// This is a language-level fact, not a pinyin fact: any Chinese input method
// (pinyin today, a shape IME tomorrow) turns ',' into '，' when composing in
// Chinese. Keeping ONE function here means schemes only opt in -- they never
// copy the table, so the two plugins cannot drift apart. A scheme's
// punctuation() virtual simply forwards to this function.
//
// M3 shift fix: the return type is std::u32string, not char32_t. Most keys map
// to one code point, but '^' maps to '……' (U+2026 twice) and '_' to '——'
// (U+2014 twice); paired quotes ('"' and '\'') return OPEN+CLOSE so the
// commit layer can alternate them by session parity. A single char32_t cannot
// express any of these, and hiding the extra code point in the commit adapter
// would put a language fact in the wrong layer. An empty string means "no
// Chinese fullwidth form" (digits, letters, space, NUL, and ASCII keys with no
// Chinese-standard symbol); callers use empty() exactly where they used
// `== 0` before.
//
// Windows-free and data-free: builds and is unit-tested on the Linux host
// (tests/punctuation_test.cpp).

#pragma once

#include <cstdint>
#include <string>

namespace imfw {

inline std::u32string fullwidth_punctuation(char32_t ch) {
    // M3 v6: this table holds only REAL Chinese punctuation, not mechanical
    // fullwidth forms of ASCII. Keys with no Chinese-standard code point
    // (`{` `}` `/` `-` `+` `=`) return empty: wants_key then leaves them to
    // the system layout, which emits the halfwidth ASCII glyph. Their
    // fullwidth forms are reserved for a later, independent width feature.
    switch (ch) {
    case U',': return U"\uFF0C";  // ， fullwidth comma
    case U'.': return U"\u3002";  // 。 ideographic full stop
    case U'?': return U"\uFF1F";  // ？ fullwidth question mark
    case U'!': return U"\uFF01";  // ！ fullwidth exclamation mark
    case U':': return U"\uFF1A";  // ： fullwidth colon
    case U';': return U"\uFF1B";  // ； fullwidth semicolon
    case U'(': return U"\uFF08";  // （ fullwidth left paren
    case U')': return U"\uFF09";  // ） fullwidth right paren
    case U'[': return U"\u3010";  // 【 left lenticular bracket
    case U']': return U"\u3011";  // 】 right lenticular bracket
    case U'<': return U"\u300A";  // 《 book title open
    case U'>': return U"\u300B";  // 》 book title close
    case U'^': return U"\u2026\u2026";  // …… double ellipsis, TWO code points
    case U'_': return U"\u2014\u2014";  // —— double em dash, TWO code points
    case U'\\': return U"\u3001"; // 、 ideographic comma (顿号)
    case U'$': return U"\uFFE5";  // ￥ fullwidth yen/yuan sign
    case U'`': return U"\u00B7";  // · middle dot (间隔号)
    // Paired quotes return OPEN+CLOSE; handle_key picks one by session parity.
    case U'"': return U"\u201C\u201D";  // “”
    case U'\'': return U"\u2018\u2019"; // ‘’
    case U'~': return U"\uFF5E";  // ～ fullwidth tilde (confirmed CJK punctuation)
    default:   return {};
    }
}

}  // namespace imfw
