// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Framework gap G4 (connected conclusion): compositions were limited to
// ASCII because the host converted bytes to wide chars one byte at a time.
// The contract is now UTF-8 with byte-wise fallback for malformed input;
// these tests pin both halves.

#include <imfw/utf8.hpp>

#include <cstdio>
#include <cstdint>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "utf8 test failed: %s\n", what);
        ++failures;
    }
}

}  // namespace

int main() {
    // ASCII decodes byte-wise (unchanged historical behaviour).
    {
        const std::wstring w = imfw::utf8_to_wide("zhong'guo");
        check(w == L"zhong'guo", "ASCII composition decodes as itself");
        check(imfw::utf8_length("zhong'guo") == 9, "ASCII length");
    }

    // CJK composition now works — the constraint the shape IME hit.
    {
        // "中国" in UTF-8: E4 B8 AD E5 9B BD.
        const std::string utf8 =
            std::string("\xE4\xB8\xAD") + std::string("\xE5\x9B\xBD");
        const std::wstring w = imfw::utf8_to_wide(utf8);
        check(w.size() == 2, "two CJK code points decode to two wide chars");
        check(static_cast<std::uint32_t>(w[0]) == 0x4E2D &&
                  static_cast<std::uint32_t>(w[1]) == 0x56FD,
              "CJK code points decode correctly");
        check(imfw::utf8_length(utf8) == 2, "CJK byte length vs code point count");
    }

    // Non-BMP: surrogate pair on 16-bit wchar_t hosts, direct on 32-bit.
    {
        const std::string utf8 = "\xF0\xA0\xAE\xB7";  // U+20BB7
        const std::wstring w = imfw::utf8_to_wide(utf8);
        if (sizeof(wchar_t) == 2) {
            check(w.size() == 2 &&
                      static_cast<std::uint32_t>(w[0]) == 0xD842 &&
                      static_cast<std::uint32_t>(w[1]) == 0xDFB7,
                  "non-BMP becomes a surrogate pair on UTF-16 hosts");
        } else {
            check(w.size() == 1 &&
                      static_cast<std::uint32_t>(w[0]) == 0x20BB7,
                  "non-BMP decodes directly on 32-bit wchar_t hosts");
        }
        check(imfw::utf8_length(utf8) == 1, "non-BMP counts as one code point");
    }

    // Malformed input falls back byte-wise: nothing is dropped or garbled
    // beyond the unavoidable, and no truncation happens.
    {
        const std::string bad = "\xE4\xB8";  // truncated 3-byte sequence
        const std::wstring w = imfw::utf8_to_wide(bad);
        check(w.size() == 2, "truncated sequence falls back to two code points");
        check(static_cast<unsigned char>(w[0]) == 0xE4 &&
                  static_cast<unsigned char>(w[1]) == 0xB8,
              "fallback keeps the raw bytes as code points");
    }
    {
        const std::wstring w = imfw::utf8_to_wide("\xFF");
        check(w.size() == 1 && static_cast<unsigned char>(w[0]) == 0xFF,
              "invalid lead byte falls back to its own code point");
    }
    {
        // Overlong encoding of 'A' (C1 81) is rejected byte-wise.
        const std::wstring w = imfw::utf8_to_wide("\xC1\x81");
        check(w.size() == 2, "overlong encoding rejected, not merged");
    }

    // Mixed content: segment lengths feed composition-window underlines, so
    // byte strings and text strings must agree after decoding.
    {
        const std::string mixed = "ab" + std::string("\xE4\xB8\xAD") + "cd";
        const std::wstring w = imfw::utf8_to_wide(mixed);
        check(w.size() == 5, "mixed ASCII+CJK decodes to 5 wide chars");
        check(w.compare(0, 2, L"ab") == 0 && w.compare(3, 2, L"cd") == 0,
              "mixed decoding preserves ASCII runs");
    }

    if (failures == 0) {
        std::printf("utf8 contract OK\n");
        return 0;
    }
    return 1;
}
