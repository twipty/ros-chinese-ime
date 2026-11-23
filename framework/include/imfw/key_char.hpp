// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// M3 shift fix: Win32 virtual-key -> character, as a Windows-free pure
// function. key_translate.cpp lives under platform/win32 and includes
// <windows.h>, so it cannot be exercised by the host test suite; keeping the
// mapping here (same pattern as punctuation.hpp) makes every vk x shift pair
// unit-testable on Linux. The numeric vk values are Win32's, but no Windows
// header is needed -- this is a data table, not a platform call.
//
// US layout only. A user switching the system keyboard layout will get a
// different shifted symbol; that is documented, not probed at runtime.

#pragma once

#include <cstdint>

namespace imfw {

inline char32_t key_char(std::uint32_t vk, bool shift) noexcept {
    // Digit row: the shifted symbols are the whole reason this function exists
    // (shift+6 -> '^' -> …… in the shared punctuation table).
    if (vk >= 0x30 && vk <= 0x39) {
        if (!shift) return static_cast<char32_t>(U'0' + (vk - 0x30));
        switch (vk) {
        case 0x30: return U')';
        case 0x31: return U'!';
        case 0x32: return U'@';
        case 0x33: return U'#';
        case 0x34: return U'$';
        case 0x35: return U'%';
        case 0x36: return U'^';
        case 0x37: return U'&';
        case 0x38: return U'*';
        case 0x39: return U'(';
        }
    }

    // OEM punctuation row. `,` / `.` keep their unshifted characters so the
    // existing paging policy (composing -> page, idle -> punctuation) holds;
    // shifted they become the book-title bracket sources.
    switch (vk) {
    case 0x20: return U' ';  // VK_SPACE
    case 0xBD: return shift ? U'_' : U'-';  // OEM_MINUS
    case 0xBB: return shift ? U'+' : U'=';  // OEM_PLUS
    case 0xBC: return shift ? U'<' : U',';  // OEM_COMMA
    case 0xBE: return shift ? U'>' : U'.';  // OEM_PERIOD
    case 0xBA: return shift ? U':' : U';';  // OEM_1
    case 0xBF: return shift ? U'?' : U'/';  // OEM_2
    case 0xC0: return shift ? U'~' : U'`';  // OEM_3
    case 0xDB: return shift ? U'{' : U'[';  // OEM_4
    case 0xDC: return shift ? U'|' : U'\\'; // OEM_5
    case 0xDD: return shift ? U'}' : U']';  // OEM_6
    case 0xDE: return shift ? U'"' : U'\''; // OEM_7
    case 0x6D: return U'-';  // numpad subtract: no shift variant
    case 0x6B: return U'=';  // numpad add: no shift variant
    default:   return 0;
    }
}

}  // namespace imfw
