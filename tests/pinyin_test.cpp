// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// M2 unit tests for the shared pinyin normalization (D2=A + v-key rule).
// These are the exact examples recorded in docs/milestones/m2/M2-DATA-PLAN.md section 7.

#include <rospinyin/pinyin.hpp>

#include <cstdio>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "pinyin test failed: %s\n", what);
        ++failures;
    }
}

void check_equal(const std::string& actual, const char* expected,
                 const char* what) {
    if (actual != expected) {
        std::fprintf(stderr,
                     "pinyin test failed: %s (expected '%s', got '%s')\n",
                     what, expected, actual.c_str());
        ++failures;
    }
}

using rospinyin::pinyin::canonical;
using rospinyin::pinyin::is_canonical;
using rospinyin::pinyin::marked_to_numeric;
using rospinyin::pinyin::syllable_of;
using rospinyin::pinyin::split;
using rospinyin::pinyin::tone_of;

}  // namespace

int main() {
    // Tone digit goes immediately after the toned vowel, never at the end.
    check_equal(marked_to_numeric("zhōng"), "zho1ng", "zhong tone placement");
    check_equal(marked_to_numeric("guó"), "guo2", "guo tone placement");
    check_equal(marked_to_numeric("yī"), "yi1", "yi tone placement");

    // u-umlaut family collapses to the 'v' key in the data layer.
    check_equal(marked_to_numeric("nǚ"), "nv3", "nü -> nv3");
    check_equal(marked_to_numeric("lǜ"), "lv4", "lü -> lv4");
    check_equal(marked_to_numeric("lüè"), "lve4", "lüe -> lve4");
    check_equal(marked_to_numeric("nüè"), "nve4", "nüe -> nve4");
    check_equal(marked_to_numeric("ǹg"), "n4g", "syllabic n (grave)");
    check_equal(marked_to_numeric("ń"), "n2", "syllabic n (acute, hanziDB)");
    check_equal(marked_to_numeric("ň"), "n3", "syllabic n (caron)");
    check_equal(marked_to_numeric("ḿ"), "m2", "syllabic m");

    // Neutral tone and ASCII passthrough / lowercasing.
    check_equal(marked_to_numeric("de"), "de", "neutral tone");
    check_equal(marked_to_numeric("ZHONG"), "zhong", "ASCII lowercase");
    check_equal(canonical("nǚ"), "nv3", "canonical alias");

    // Strict rejection: invalid UTF-8 and unknown non-tone characters.
    check(marked_to_numeric(std::string("\xC3", 1)).empty(),
          "truncated UTF-8 must be rejected");
    check(marked_to_numeric("ê").empty(),
          "unknown non-tone character must be rejected");

    // Toneless syllable keys used by the segmenter must be 'v'-based.
    check_equal(syllable_of("zho1ng"), "zhong", "syllable of zhong");
    check_equal(syllable_of("nv3"), "nv", "syllable of nv");
    check_equal(syllable_of("lve4"), "lve", "syllable of lve");
    check_equal(syllable_of("n4g"), "ng", "syllable of ng");
    check_equal(syllable_of(marked_to_numeric("nǚ")), "nv",
                "input 'nv' matches data 'nǚ'");
    check_equal(syllable_of(marked_to_numeric("lüè")), "lve",
                "input 'lve' matches data 'lüè'");

    // Tone extraction and split.
    check(tone_of("zho1ng") == 1, "tone of zho1ng");
    check(tone_of("lv4") == 4, "tone of lv4");
    check(tone_of("de") == 0, "neutral tone of de");
    const auto reading = split("lve4");
    check_equal(reading.syllable, "lve", "split syllable");
    check(reading.tone == 4, "split tone");

    // Canonical-form assertion used by mkdat to catch data regressions.
    check(is_canonical("zho1ng"), "zho1ng is canonical");
    check(is_canonical("nv3"), "nv3 is canonical");
    check(!is_canonical("nǚ"), "raw umlaut is not canonical");
    check(!is_canonical("zhong5"), "digit 5 is not a tone");
    check(!is_canonical("zho0ng"), "digit 0 is not a tone");

    if (failures != 0) {
        std::fprintf(stderr, "%d pinyin test(s) failed\n", failures);
        return 1;
    }
    std::printf("RosPinyin pinyin normalization OK (tone/v-key rules)\n");
    return 0;
}
