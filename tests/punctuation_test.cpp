// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// M3: the shared ASCII -> fullwidth Chinese punctuation table. This is a
// language-level fact, not pinyin-specific: the future shape IME calls the
// same function. It lives in the framework (no <windows.h>, no data file) so
// both schemes share ONE table -- a second copy would drift.
//
// Every mapped key is pinned, and the deliberate non-mappings (digits, space,
// letters, NUL) are pinned too: "no fullwidth form" is a contract, not an
// oversight, and must keep returning 0 so route_key leaves those keys alone.

#include <imfw/punctuation.hpp>

#include <cstdio>
#include <cstdint>
#include <string>

namespace {

int failures = 0;

std::string describe(const std::u32string& text) {
    std::string out = "[";
    for (char32_t cp : text) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "U+%04X ", static_cast<unsigned>(cp));
        out += buf;
    }
    out += "]";
    return out;
}

void check_result(char32_t input, const std::u32string& got,
                  const std::u32string& want, const char* what) {
    if (got != want) {
        std::fprintf(stderr,
                     "punctuation test failed: %s (U+%04X -> %s, expected %s)\n",
                     what, static_cast<unsigned>(input), describe(got).c_str(),
                     describe(want).c_str());
        ++failures;
    }
}

// Single-code-point mapping (the common case) stays readable as char32_t.
void expect(char32_t input, char32_t mapped, const char* what) {
    const std::u32string want =
        mapped == 0 ? std::u32string() : std::u32string(1, mapped);
    check_result(input, imfw::fullwidth_punctuation(input), want, what);
}

// Multi-code-point mapping: '^' -> '……' cannot be expressed as one char32_t.
void expect_text(char32_t input, const char32_t* mapped, const char* what) {
    check_result(input, imfw::fullwidth_punctuation(input),
                 std::u32string(mapped), what);
}

void expect_none(char32_t input, const char* what) {
    expect(input, 0, what);
}

}  // namespace

int main() {
    // The 23-key table (S0 assumption item, pinned in S1). Comma/period also
    // page -- the composing flag, not this table, decides which meaning wins.
    expect(U',', U'\uFF0C', "comma -> fullwidth comma");
    expect(U'.', U'\u3002', "period -> ideographic full stop");
    expect(U'?', U'\uFF1F', "question -> fullwidth question");
    expect(U'!', U'\uFF01', "bang -> fullwidth exclamation");
    expect(U':', U'\uFF1A', "colon -> fullwidth colon");
    expect(U';', U'\uFF1B', "semicolon -> fullwidth semicolon");
    expect(U'(', U'\uFF08', "open paren -> fullwidth");
    expect(U')', U'\uFF09', "close paren -> fullwidth");
    expect(U'[', U'\u3010', "open bracket -> lenticular");
    expect(U']', U'\u3011', "close bracket -> lenticular");
    expect_none(U'{', "open brace: pure fullwidth form, falls back to ASCII");
    expect_none(U'}', "close brace: pure fullwidth form, falls back to ASCII");
    expect(U'<', U'\u300A', "less -> book title open");
    expect(U'>', U'\u300B', "greater -> book title close");
    expect_text(U'^', U"\u2026\u2026", "caret -> double ellipsis");
    expect_none(U'/', "slash: pure fullwidth form, falls back to ASCII");
    expect(U'\\', U'\u3001', "backslash -> ideographic comma (顿号)");
    expect_text(U'"', U"\u201C\u201D", "double quote -> open/close pair");
    expect_text(U'\'', U"\u2018\u2019", "apostrophe -> open/close pair");
    expect(U'~', U'\uFF5E', "tilde -> fullwidth (confirmed CJK punctuation)");
    expect_none(U'-', "minus: pure fullwidth form, falls back to ASCII");
    expect_text(U'_', U"\u2014\u2014", "underscore -> double em dash (破折号)");
    expect_none(U'+', "plus: pure fullwidth form, falls back to ASCII");
    expect_none(U'=', "equal: pure fullwidth form, falls back to ASCII");
    expect(U'$', U'\uFFE5', "dollar -> fullwidth yen/yuan sign");
    expect(U'`', U'\u00B7', "backtick -> middle dot (间隔号)");

    // Deliberate non-mappings: digits stay ASCII (S0 acceptance 4), letters
    // are the input alphabet, space is a selection key, NUL means "no key".
    expect_none(U'1', "digit 1 stays halfwidth");
    expect_none(U'9', "digit 9 stays halfwidth");
    expect_none(U'0', "digit 0 stays halfwidth");
    expect_none(U' ', "space stays halfwidth");
    expect_none(U'a', "letter stays halfwidth");
    expect_none(U'z', "letter stays halfwidth");
    expect_none(U'\0', "null key has no mapping");

    if (failures != 0) {
        std::fprintf(stderr, "%d punctuation check(s) failed\n", failures);
        return 1;
    }
    std::printf("punctuation: all checks passed\n");
    return 0;
}
