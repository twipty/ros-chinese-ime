// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Win32 -> imfw translation (see key_translate.h). Split out of imm_entry.cpp
// in X2-d so that the file holding the 16 exports contains no key semantics at
// all -- only the jump from a native message to a framework call.

#include "key_translate.h"

#include <imfw/emit_text.hpp>

#include <cstddef>

namespace imfw {
namespace win32 {
namespace {

constexpr BYTE kCtrlBit = 0x80;
constexpr BYTE kAltBit = 0x80;

// lParam bit 31 of WM_KEYDOWN/WM_KEYUP: 1 = the key is being released. The
// same lParam is handed to ImeProcessKey, and it is the only transition
// information available anywhere in the IME path (ImeToAsciiEx gets none).
constexpr unsigned long kKeyUpBit = 0x80000000UL;

enum class KeyIndex { Shift = 0x10, Control = 0x11, Alt = 0x12 };

bool modifier_down(const BYTE* lpbKeyState, KeyIndex index, BYTE bit) {
    return lpbKeyState != nullptr &&
           (lpbKeyState[static_cast<int>(index)] & bit) != 0;
}

// Translate the framework-managed virtual keys to their character. The policy
// deciding which of these are selection / paging keys lives in the Scheme
// (G4); the input alphabet is also translated by the Scheme.
char32_t framework_key_char(UINT vk) {
    if (vk >= '0' && vk <= '9') return static_cast<char32_t>(vk);
    switch (vk) {
    case VK_SPACE:      return U' ';
    case VK_OEM_MINUS:  return U'-';
    case VK_OEM_PLUS:   return U'=';
    case VK_OEM_COMMA:  return U',';
    case VK_OEM_PERIOD: return U'.';
    case VK_SUBTRACT:   return U'-';   // numpad minus
    case VK_ADD:        return U'=';   // numpad plus
    default:            return 0;
    }
}

}  // namespace

KeyEvent key_event_from(UINT vk, unsigned long lParam,
                       const BYTE* lpbKeyState) {
    KeyEvent event;
    event.vk = vk;
    event.ch = framework_key_char(vk);
    event.kind = vk == VK_BACK     ? KeyKind::Backspace
                 : vk == VK_ESCAPE ? KeyKind::Escape
                 : vk == VK_SHIFT  ? KeyKind::Shift
                                   : KeyKind::Other;
    event.key_up = (lParam & kKeyUpBit) != 0;
    event.ctrl = modifier_down(lpbKeyState, KeyIndex::Control, kCtrlBit);
    event.alt = modifier_down(lpbKeyState, KeyIndex::Alt, kAltBit);
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
