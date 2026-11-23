// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Win32 -> imfw translation (see key_translate.h). Split out of imm_entry.cpp
// in X2-d so that the file holding the 16 exports contains no key semantics at
// all -- only the jump from a native message to a framework call.

#include "key_translate.h"

#include <imfw/key_char.hpp>
#include <imfw/emit_text.hpp>

#include <cstddef>

namespace imfw {
namespace win32 {
namespace {

constexpr BYTE kCtrlBit = 0x80;
constexpr BYTE kAltBit = 0x80;
constexpr BYTE kShiftBit = 0x80;
constexpr UINT kModeToggleVk = 'G';  // Ctrl+Shift+G -> [快]/[全]
constexpr UINT kLanguageToggleVk = VK_SPACE;  // Ctrl+Shift+Space -> 中/英 backup

// lParam bit 31 of WM_KEYDOWN/WM_KEYUP: 1 = the key is being released. The
// same lParam is handed to ImeProcessKey, and it is the only transition
// information available anywhere in the IME path (ImeToAsciiEx gets none).
constexpr unsigned long kKeyUpBit = 0x80000000UL;

enum class KeyIndex { Shift = 0x10, Control = 0x11, Alt = 0x12 };

bool modifier_down(const BYTE* lpbKeyState, KeyIndex index, BYTE bit) {
    return lpbKeyState != nullptr &&
           (lpbKeyState[static_cast<int>(index)] & bit) != 0;
}

// The vk -> char table moved to imfw/key_char.hpp (M3 shift fix). Besides
// being Windows-free and host-testable, the important change is the shift
// argument: the old signature could not express '?' (shift+/), '<' (shift+,)
// or '^' (shift+6), so those keys produced ch=0 / the unshifted character and
// never reached the Chinese punctuation router. The policy deciding which of
// these are selection / paging keys still lives in the Scheme (G4).

}  // namespace

KeyEvent key_event_from(UINT vk, unsigned long lParam,
                       const BYTE* lpbKeyState) {
    KeyEvent event;
    event.vk = vk;
    // Modifiers first: the character table needs `shift`. In the old order
    // event.ch was computed before shift was even read, which is exactly why
    // shifted punctuation could never be represented.
    const bool ctrl = modifier_down(lpbKeyState, KeyIndex::Control, kCtrlBit);
    const bool alt = modifier_down(lpbKeyState, KeyIndex::Alt, kAltBit);
    const bool shift = modifier_down(lpbKeyState, KeyIndex::Shift, kShiftBit);
    event.ch = key_char(vk, shift);
    // Ctrl+Shift+G is the framework-owned mode switch (FR-D5). Alt+G and a
    // bare Shift+G are ordinary application keys and stay untouched.
    const bool mode_toggle =
        vk == kModeToggleVk && ctrl && shift && !alt;
    // Ctrl+Shift+Space is the backup Chinese/English hotkey. Requires shift so
    // it cannot be confused with the primary Ctrl+Space (CORE-19455 unreliable)
    // or with plain Space (a selection key). Alt+Shift combos stay untouched.
    const bool language_toggle =
        vk == kLanguageToggleVk && ctrl && shift && !alt;
    event.kind = mode_toggle ? KeyKind::ModeToggle
                 : language_toggle ? KeyKind::LanguageToggle
                 : vk == VK_BACK     ? KeyKind::Backspace
                 : vk == VK_ESCAPE   ? KeyKind::Escape
                 : vk == VK_SHIFT    ? KeyKind::Shift
                                     : KeyKind::Other;
    event.key_up = (lParam & kKeyUpBit) != 0;
    event.ctrl = ctrl;
    event.alt = alt;
    return event;
}

// G1: one WM_IME_CHAR per UTF-16 code unit, surrogate pairs for non-BMP. The
// UTF-16 logic lives in imfw-engine (imfw/emit_text.hpp) so it is unit-tested
// on the host; the slot struct is layout-compatible with TRANSMESSAGE,
// guarded below.
//
// Audit 2026-11-17 §2.1 (P0): the buffer ImeToAsciiEx receives is a
// TRANSMSGLIST whose uMsgCount header is pre-filled by IMM32 with the buffer
// capacity. Messages must be written at list->TransMsg[0] (offset
// sizeof(UINT)), never at offset 0 -- that would clobber the header and shift
// every message by one slot, so IMM32 would read garbage and drop the last
// message. The capacity we honor is min(uMsgCount, kMaxEmit).
UINT emit_commit(rpimm::TransMsgList* lpTransMsgList,
                 const std::u32string& text) {
    static_assert(sizeof(imfw::TransMsgSlot) == sizeof(rpimm::TransMsg),
                  "TRANSMESSAGE must stay layout-compatible with "
                  "imfw::TransMsgSlot");
    using rpimm_list = rpimm::TransMsgList;
#if defined(_WIN32) || defined(__i686__) || defined(__i386__)
    // Win32 (ILP32) ABI: TRANSMESSAGE is 4-byte packed (UINT/WPARAM/LPARAM
    // all 4 bytes), so the array follows the header with no padding. On
    // LP64 hosts the pointer-sized fields pad the slot and the offset grows
    // accordingly -- the runtime buffer is only ever seen on Win32.
    static_assert(offsetof(rpimm_list, TransMsg) == sizeof(UINT),
                  "TRANSMSGLIST.TransMsg must follow uMsgCount directly "
                  "(no padding) on the Win32 ABI");
#endif
    if (lpTransMsgList == nullptr || lpTransMsgList->uMsgCount == 0) return 0;
    const UINT capacity = lpTransMsgList->uMsgCount < imfw::kMaxEmitMessages
                              ? lpTransMsgList->uMsgCount
                              : imfw::kMaxEmitMessages;
    return imfw::emit_text(
        reinterpret_cast<imfw::TransMsgSlot*>(lpTransMsgList->TransMsg),
        capacity, text);
}

}  // namespace win32
}  // namespace imfw
