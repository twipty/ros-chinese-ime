// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// See emit_text.hpp. Platform-independent (part of imfw-engine).

#include <imfw/emit_text.hpp>

namespace imfw {

std::uint32_t emit_text(TransMsgSlot* slots, std::uint32_t capacity,
                        const std::u32string& text) {
    if (slots == nullptr || capacity == 0 || text.empty()) return 0;

    std::uint32_t written = 0;
    for (const char32_t cp : text) {
        if (cp <= 0xFFFF) {
            if (written >= capacity) break;
            slots[written].message = kImeCharMessage;
            slots[written].wparam = static_cast<std::uintptr_t>(cp);
            slots[written].lparam = 0;
            ++written;
        } else {
            // Never split a surrogate pair across the capacity boundary.
            if (written + 2 > capacity) break;
            const std::uint32_t v =
                static_cast<std::uint32_t>(cp) - 0x10000u;
            slots[written].message = kImeCharMessage;
            slots[written].wparam =
                static_cast<std::uintptr_t>(0xD800u + (v >> 10));
            slots[written].lparam = 0;
            ++written;
            slots[written].message = kImeCharMessage;
            slots[written].wparam =
                static_cast<std::uintptr_t>(0xDC00u + (v & 0x3FFu));
            slots[written].lparam = 0;
            ++written;
        }
    }
    return written;
}

}  // namespace imfw
