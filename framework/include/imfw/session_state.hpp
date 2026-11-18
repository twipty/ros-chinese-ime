// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Per-input-context state, with the Windows part removed.
//
// Why it moved here (X2-a): the struct used to sit next to the IMM32 session
// store, and it is the only reason the key policy could not be compiled
// against the host. Nothing in it is platform-specific — an engine, a pager,
// two strings and three flags — so it belongs to the portable core. What stays
// platform-specific is the store that hands out one instance per HIMC
// (framework/platform/win32/imm32/session_store.hpp), because its key type
// rpimm::InputContext is an IMM32 handle.

#pragma once

#include <imfw/candidate_pager.hpp>
#include <imfw/engine.hpp>

#include <memory>
#include <string>
#include <vector>

namespace imfw {

struct ImeSessionState {
    std::unique_ptr<Engine> engine;
    CandidatePager pager;
    bool composing = false;  // true while there is an active composition
    std::string composition;             // last engine composition
    std::vector<std::string> segments;   // last engine segmentation
    // Set on VK_SHIFT down and cleared by any other key, so only a *lone*
    // Shift toggles Chinese/English (Shift+A must stay a capital A).
    bool shift_latched = false;
    // The OS-side fOpen was set once for this session (see AlignOpenStatus).
    // After that the flag belongs to the user: re-aligning would undo an
    // English toggle on the next focus change.
    bool open_initialized = false;
};

}  // namespace imfw
