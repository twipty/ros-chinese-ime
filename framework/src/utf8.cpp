// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// See utf8.hpp. Platform-independent (part of imfw-engine).

#include <imfw/utf8.hpp>

#include <cstddef>
#include <cstdint>

namespace imfw {
namespace {

// On Windows wchar_t is UTF-16 (surrogate pairs for non-BMP); on Linux hosts
// it is UTF-32 (direct code points).
constexpr bool kWideIsUtf16 = sizeof(wchar_t) == 2;

void append_code_point(std::wstring& out, char32_t cp) {
    if constexpr (kWideIsUtf16) {
        if (cp > 0xFFFF) {
            const std::uint32_t v =
                static_cast<std::uint32_t>(cp) - 0x10000u;
            out.push_back(static_cast<wchar_t>(0xD800 + (v >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (v & 0x3FF)));
            return;
        }
    }
    out.push_back(static_cast<wchar_t>(cp));
}

}  // namespace

std::wstring utf8_to_wide(const std::string& utf8) {
    std::wstring out;
    out.reserve(utf8.size());

    std::size_t i = 0;
    while (i < utf8.size()) {
        const unsigned char b0 = static_cast<unsigned char>(utf8[i]);
        std::size_t len = 0;
        char32_t cp = 0;

        if (b0 < 0x80) {
            len = 1;
            cp = b0;
        } else if ((b0 & 0xE0) == 0xC0) {
            len = 2;
            cp = b0 & 0x1F;
        } else if ((b0 & 0xF0) == 0xE0) {
            len = 3;
            cp = b0 & 0x0F;
        } else if ((b0 & 0xF8) == 0xF0) {
            len = 4;
            cp = b0 & 0x07;
        } else {
            // Malformed lead byte: keep it as its own code point (the
            // historical byte-wise conversion) so nothing is lost.
            append_code_point(out, b0);
            ++i;
            continue;
        }

        bool ok = (i + len <= utf8.size());
        for (std::size_t k = 1; ok && k < len; ++k) {
            const unsigned char bk =
                static_cast<unsigned char>(utf8[i + k]);
            if ((bk & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (bk & 0x3F);
        }

        // Reject overlong encodings and out-of-range code points.
        if (ok && ((len == 2 && cp < 0x80) ||
                   (len == 3 && cp < 0x800) ||
                   (len == 4 && cp < 0x10000) || cp > 0x10FFFF)) {
            ok = false;
        }

        if (ok) {
            append_code_point(out, cp);
            i += len;
        } else {
            append_code_point(out, b0);
            ++i;
        }
    }
    return out;
}

std::size_t utf8_length(const std::string& utf8) {
    std::size_t count = 0;
    std::size_t i = 0;
    while (i < utf8.size()) {
        const unsigned char b0 = static_cast<unsigned char>(utf8[i]);
        std::size_t len = 0;
        if (b0 < 0x80) {
            len = 1;
        } else if ((b0 & 0xE0) == 0xC0) {
            len = 2;
        } else if ((b0 & 0xF0) == 0xE0) {
            len = 3;
        } else if ((b0 & 0xF8) == 0xF0) {
            len = 4;
        } else {
            len = 1;  // malformed lead byte counts on its own
        }
        if (i + len > utf8.size()) len = 1;
        i += len;
        ++count;
    }
    return count;
}

}  // namespace imfw
