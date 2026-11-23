// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// M3 shift fix: the Win32 vk -> character mapping, as a Windows-free pure
// function. key_translate.cpp itself cannot be host-tested (it includes
// <windows.h>), so the entire 13-key x 2-state table lives here in imfw and
// the platform layer only calls it. Without this test the shift semantics are
// invisible to every build that does not boot ReactOS.

#include <imfw/key_char.hpp>

#include <cstdio>

namespace {

int failures = 0;

void expect(unsigned vk, bool shift, char32_t want, const char* what) {
    const char32_t got = imfw::key_char(vk, shift);
    if (got != want) {
        std::fprintf(stderr,
                     "key char test failed: %s (vk=0x%02X shift=%d -> "
                     "U+%04X, expected U+%04X)\n",
                     what, vk, shift ? 1 : 0, static_cast<unsigned>(got),
                     static_cast<unsigned>(want));
        ++failures;
    }
}

}  // namespace

int main() {
    // Digit row (US layout): the shifted symbols are what Chinese punctuation
    // consumption needs (e.g. shift+6 -> '^' -> ……).
    expect(0x30, false, U'0', "0");
    expect(0x30, true, U')', "shift+0");
    expect(0x31, false, U'1', "1");
    expect(0x31, true, U'!', "shift+1 -> !");
    expect(0x32, false, U'2', "2");
    expect(0x32, true, U'@', "shift+2 -> @");
    expect(0x33, false, U'3', "3");
    expect(0x33, true, U'#', "shift+3 -> #");
    expect(0x34, false, U'4', "4");
    expect(0x34, true, U'$', "shift+4 -> $");
    expect(0x35, false, U'5', "5");
    expect(0x35, true, U'%', "shift+5 -> %");
    expect(0x36, false, U'6', "6");
    expect(0x36, true, U'^', "shift+6 -> ^");
    expect(0x37, false, U'7', "7");
    expect(0x37, true, U'&', "shift+7 -> &");
    expect(0x38, false, U'8', "8");
    expect(0x38, true, U'*', "shift+8 -> *");
    expect(0x39, false, U'9', "9");
    expect(0x39, true, U'(', "shift+9 -> (");

    // OEM punctuation row. Missing entries here are what made shift+/ drop
    // out as ch=0 and always fall through to the application.
    expect(0xBD, false, U'-', "OEM_MINUS");
    expect(0xBD, true, U'_', "shift+OEM_MINUS -> _");
    expect(0xBB, false, U'=', "OEM_PLUS");
    expect(0xBB, true, U'+', "shift+OEM_PLUS -> +");
    expect(0xBC, false, U',', "OEM_COMMA");
    expect(0xBC, true, U'<', "shift+OEM_COMMA -> < (book title)");
    expect(0xBE, false, U'.', "OEM_PERIOD");
    expect(0xBE, true, U'>', "shift+OEM_PERIOD -> > (book title)");
    expect(0xBA, false, U';', "OEM_1");
    expect(0xBA, true, U':', "shift+OEM_1 -> :");
    expect(0xBF, false, U'/', "OEM_2");
    expect(0xBF, true, U'?', "shift+OEM_2 -> ?");
    expect(0xC0, false, U'`', "OEM_3");
    expect(0xC0, true, U'~', "shift+OEM_3 -> ~");
    expect(0xDB, false, U'[', "OEM_4");
    expect(0xDB, true, U'{', "shift+OEM_4 -> {");
    expect(0xDC, false, U'\\', "OEM_5");
    expect(0xDC, true, U'|', "shift+OEM_5 -> |");
    expect(0xDD, false, U']', "OEM_6");
    expect(0xDD, true, U'}', "shift+OEM_6 -> }");
    expect(0xDE, false, U'\'', "OEM_7");
    expect(0xDE, true, U'"', "shift+OEM_7 -> \"");

    // Keys the framework claims regardless of shift (numpad keeps its old
    // mapping; space is a selection key and never a punctuation source).
    expect(0x20, false, U' ', "space");
    expect(0x20, true, U' ', "shift+space");
    expect(0x6D, false, U'-', "numpad minus");
    expect(0x6D, true, U'-', "shift+numpad minus");
    expect(0x6B, false, U'=', "numpad plus");
    expect(0x6B, true, U'=', "shift+numpad plus");

    // An unmapped key is 0 in both states: wants_key() leaves it alone.
    expect(0x41, false, 0, "letter A has no framework char");
    expect(0x41, true, 0, "shift+letter A has no framework char");
    expect(0x08, false, 0, "Backspace has no framework char");

    if (failures != 0) {
        std::fprintf(stderr, "%d key char check(s) failed\n", failures);
        return 1;
    }
    std::printf("key char: all checks passed\n");
    return 0;
}
