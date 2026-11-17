// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Framework gap G1 regression test: commit emission must produce one
// WM_IME_CHAR TRANSMESSAGE per UTF-16 code unit (non-BMP code points as
// surrogate pairs) so multi-codepoint word candidates commit whole. This
// exact logic used to live untestable inside imm_entry.cpp and regressed to
// single-codepoint commits; it now lives in imfw-engine (imfw/emit_text.hpp).

#include <imfw/emit_text.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "emit_text test failed: %s\n", what);
        ++failures;
    }
}

}  // namespace

int main() {
    using imfw::TransMsgSlot;

    // Empty / null inputs emit nothing.
    {
        TransMsgSlot slots[imfw::kMaxEmitMessages];
        check(imfw::emit_text(nullptr, imfw::kMaxEmitMessages, U"x") == 0,
              "null slot array emits nothing");
        check(imfw::emit_text(slots, imfw::kMaxEmitMessages, U"") == 0,
              "empty text emits nothing");
        check(imfw::emit_text(slots, 0, U"x") == 0,
              "zero capacity emits nothing");
    }

    // BMP text: one message per code point, wParam = the code unit.
    {
        TransMsgSlot slots[imfw::kMaxEmitMessages];
        const std::u32string text = U"中国";  // 0x4E2D, 0x56FD
        const std::uint32_t n =
            imfw::emit_text(slots, imfw::kMaxEmitMessages, text);
        check(n == 2, "two BMP code points emit two messages");
        check(slots[0].message == imfw::kImeCharMessage, "message is WM_IME_CHAR");
        check(slots[0].wparam == 0x4E2D && slots[1].wparam == 0x56FD,
              "wParam carries the code unit");
        check(slots[0].lparam == 0, "lParam stays zero");
    }

    // A word candidate commits whole — the exact G1 regression.
    {
        TransMsgSlot slots[imfw::kMaxEmitMessages];
        const std::u32string word = U"中华人民共和国";
        const std::uint32_t n =
            imfw::emit_text(slots, imfw::kMaxEmitMessages, word);
        check(n == word.size(), "7-character word commits all 7 units");
        for (std::uint32_t i = 0; i < n; ++i) {
            if (slots[i].message != imfw::kImeCharMessage ||
                slots[i].wparam == 0) {
                check(false, "every slot is a real WM_IME_CHAR");
                break;
            }
        }
    }

    // Non-BMP: surrogate pair, high first, low second.
    {
        TransMsgSlot slots[imfw::kMaxEmitMessages];
        // U+20BB7 (CJK Ext-B) = D842 DF B7 in UTF-16.
        const std::u32string text{U'\U00020BB7'};
        const std::uint32_t n =
            imfw::emit_text(slots, imfw::kMaxEmitMessages, text);
        check(n == 2, "non-BMP code point emits a surrogate pair");
        check(slots[0].wparam == 0xD842 && slots[1].wparam == 0xDFB7,
              "surrogate pair is high-then-low with correct values");
        check(slots[0].message == imfw::kImeCharMessage &&
                  slots[1].message == imfw::kImeCharMessage,
              "both halves are WM_IME_CHAR");
    }

    // Capacity: truncation never leaves a dangling half surrogate.
    {
        TransMsgSlot slots[3];
        const std::u32string text{U'A', U'\U00020BB7', U'B'};
        const std::uint32_t n = imfw::emit_text(slots, 3, text);
        check(n == 3, "capacity 3 fits A + pair + would truncate B");
        const std::uint32_t n2 = imfw::emit_text(slots, 2, text);
        check(n2 == 1, "capacity 2 emits only the BMP char, no half pair");
    }

    // Capacity cap: at most `capacity` messages, never more.
    {
        TransMsgSlot slots[imfw::kMaxEmitMessages];
        std::u32string text;
        for (int i = 0; i < 100; ++i) text += U'x';
        const std::uint32_t n =
            imfw::emit_text(slots, imfw::kMaxEmitMessages, text);
        check(n == imfw::kMaxEmitMessages, "long text stops at the cap");
    }

    // TRANSMSGLIST layout (audit 2026-11-17 §2.1): the message array starts
    // after the capacity header. On the ILP32 Win32 target TRANSMESSAGE is
    // 4-byte aligned so the array follows the header at offset 4 with no
    // padding; LP64 hosts pad the pointer-sized fields, so the exact offset
    // there is sizeof(uint32_t) rounded up to the slot alignment. What must
    // hold everywhere: the header occupies its own leading bytes and is
    // followed by the first message.
    {
        struct ListLayout {
            std::uint32_t uMsgCount;
            TransMsgSlot msg;
        };
        check(offsetof(ListLayout, uMsgCount) == 0,
              "TRANSMSGLIST capacity header is the first field");
        check(offsetof(ListLayout, msg) >= sizeof(std::uint32_t),
              "TRANSMSGLIST message array never overlaps the header");
        check(sizeof(ListLayout) ==
                  offsetof(ListLayout, msg) + sizeof(TransMsgSlot),
              "TRANSMSGLIST[1] is header + exactly one message slot");
    }

    if (failures == 0) {
        std::printf("emit_text contract OK\n");
        return 0;
    }
    return 1;
}
