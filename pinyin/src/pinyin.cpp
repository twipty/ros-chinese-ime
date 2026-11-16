// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// See engine/include/rospinyin/pinyin.hpp for the canonical-form contract.
// This file is compiled into rospinyin-engine and is used both at build time
// (tools/mkdat) and at run time (pinyin engine), so there is exactly one
// implementation of the tone/umlaut mapping.

#include <rospinyin/pinyin.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>

namespace rospinyin {
namespace pinyin {
namespace {

struct ToneMark {
    std::uint32_t codepoint;
    const char* replacement;
};

// Every tone-marked character observed in Unihan 18.0.0 kMandarin plus the
// missing 1st-tone u-umlaut for completeness. Sorted by codepoint.
// The digit is placed immediately after the base vowel (zho1ng, not zhong1).
constexpr ToneMark kToneMarks[] = {
    {0x00E0, "a4"},  // à
    {0x00E1, "a2"},  // á
    {0x00E8, "e4"},  // è
    {0x00E9, "e2"},  // é
    {0x00EC, "i4"},  // ì
    {0x00ED, "i2"},  // í
    {0x00F2, "o4"},  // ò
    {0x00F3, "o2"},  // ó
    {0x00F9, "u4"},  // ù
    {0x00FA, "u2"},  // ú
    {0x00FC, "v"},   // ü  (neutral u-umlaut -> v key)
    {0x0101, "a1"},  // ā
    {0x0113, "e1"},  // ē
    {0x011B, "e3"},  // ě
    {0x012B, "i1"},  // ī
    {0x0144, "n2"},  // ń  (syllabic n, 2nd tone; used by hanziDB for 嗯)
    {0x0148, "n3"},  // ň  (syllabic n, 3rd tone)
    {0x014D, "o1"},  // ō
    {0x016B, "u1"},  // ū
    {0x01CE, "a3"},  // ǎ
    {0x01D0, "i3"},  // ǐ
    {0x01D2, "o3"},  // ǒ
    {0x01D4, "u3"},  // ǔ
    {0x01D6, "v1"},  // ǖ
    {0x01D8, "v2"},  // ǘ
    {0x01DA, "v3"},  // ǚ
    {0x01DC, "v4"},  // ǜ
    {0x01F9, "n4"},  // ǹ  (syllabic n, 4th tone)
    {0x1E3F, "m2"},  // ḿ  (syllabic m, 2nd tone)
};

const char* replacement_for(std::uint32_t cp) noexcept {
    const auto it = std::lower_bound(
        std::begin(kToneMarks), std::end(kToneMarks), cp,
        [](const ToneMark& mark, std::uint32_t value) {
            return mark.codepoint < value;
        });
    if (it != std::end(kToneMarks) && it->codepoint == cp) {
        return it->replacement;
    }
    return nullptr;
}

// Minimal strict UTF-8 decoder. Returns false on any malformed sequence,
// overlong encoding, surrogate or out-of-range code point.
bool decode_utf8(std::string_view s, std::size_t& i,
                 std::uint32_t& cp) noexcept {
    const auto b0 = static_cast<unsigned char>(s[i]);
    if (b0 < 0x80) {
        cp = b0;
        ++i;
        return true;
    }

    std::size_t length = 0;
    std::uint32_t value = 0;
    if ((b0 & 0xE0) == 0xC0) {
        length = 2;
        value = b0 & 0x1Fu;
    } else if ((b0 & 0xF0) == 0xE0) {
        length = 3;
        value = b0 & 0x0Fu;
    } else if ((b0 & 0xF8) == 0xF0) {
        length = 4;
        value = b0 & 0x07u;
    } else {
        return false;
    }

    if (length > s.size() - i) {
        return false;
    }
    for (std::size_t k = 1; k < length; ++k) {
        const auto b = static_cast<unsigned char>(s[i + k]);
        if ((b & 0xC0) != 0x80) {
            return false;
        }
        value = (value << 6) | (b & 0x3Fu);
    }

    if ((length == 2 && value < 0x80) ||
        (length == 3 && value < 0x800) ||
        (length == 4 && value < 0x10000)) {
        return false;  // overlong
    }
    if (value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        return false;
    }

    cp = value;
    i += length;
    return true;
}

bool is_tone_digit(char c) noexcept {
    return c >= '1' && c <= '4';
}

}  // namespace

std::string marked_to_numeric(std::string_view marked) {
    std::string out;
    out.reserve(marked.size() + 8);

    std::size_t i = 0;
    while (i < marked.size()) {
        std::uint32_t cp = 0;
        if (!decode_utf8(marked, i, cp)) {
            return {};
        }
        if (cp < 0x80) {
            const char c = static_cast<char>(cp);
            out.push_back(c >= 'A' && c <= 'Z'
                              ? static_cast<char>(c - 'A' + 'a')
                              : c);
            continue;
        }
        const char* replacement = replacement_for(cp);
        if (replacement == nullptr) {
            return {};  // unknown non-tone character: let the caller reject it
        }
        out.append(replacement);
    }
    return out;
}

std::string syllable_of(std::string_view numeric) {
    std::string out;
    out.reserve(numeric.size());
    for (const char c : numeric) {
        if (!is_tone_digit(c)) {
            out.push_back(c);
        }
    }
    return out;
}

int tone_of(std::string_view numeric) noexcept {
    for (const char c : numeric) {
        if (is_tone_digit(c)) {
            return c - '0';
        }
    }
    return 0;
}

Reading split(std::string_view numeric) {
    Reading result;
    result.syllable.reserve(numeric.size());
    for (const char c : numeric) {
        if (is_tone_digit(c)) {
            if (result.tone == 0) {
                result.tone = c - '0';
            }
        } else {
            result.syllable.push_back(c);
        }
    }
    return result;
}

bool is_canonical(std::string_view s) noexcept {
    for (const char c : s) {
        if ((c >= 'a' && c <= 'z') || c == '\'') {
            continue;
        }
        if (is_tone_digit(c)) {
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace pinyin
}  // namespace rospinyin
