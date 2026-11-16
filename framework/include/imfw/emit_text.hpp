// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Framework gap G1, closed here: commits used to emit a single WM_IME_CHAR
// (commit_text[0]), which made multi-codepoint word candidates impossible —
// the candidate window could show a word but selecting it typed only its
// first character. This module emits one WM_IME_CHAR TRANSMESSAGE per UTF-16
// code unit, so words commit whole and non-BMP characters survive as
// surrogate pairs.
//
// Windows-free on purpose: the UTF-16 conversion logic is exactly the part
// that regressed historically, so it lives in imfw-engine and is unit-tested
// on the Linux host like everything else in framework/src.

#pragma once

#include <cstdint>
#include <string>

namespace imfw {

// Layout-compatible with the IMM32 TRANSMESSAGE
//   {UINT uMsg; WPARAM wParam; LPARAM lParam;}
// The host layer reinterpret_casts its real TRANSMESSAGE array here
// (guarded by a static_assert) so no Windows header is needed to test.
struct TransMsgSlot {
    std::uint32_t message;
    std::uintptr_t wparam;
    std::intptr_t lparam;
};

constexpr std::uint32_t kImeCharMessage = 0x0286;  // WM_IME_CHAR

// The IMM32 DDK contract allows up to 256 TRANSMESSAGEs per ImeToAsciiEx
// call; cap well below that — no pinyin/shape candidate comes anywhere near.
constexpr std::uint32_t kMaxEmitMessages = 32;

// Emit committed text as WM_IME_CHAR messages: one message per UTF-16 code
// unit (non-BMP code points become a high + low surrogate pair). Returns the
// number of slots written; stops early (with everything written so far kept
// well-formed — no dangling half surrogate) when capacity runs out.
std::uint32_t emit_text(TransMsgSlot* slots, std::uint32_t capacity,
                        const std::u32string& text);

}  // namespace imfw
