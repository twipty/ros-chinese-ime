// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Per-input-context state. Each HIMC owns an Engine (built by the Scheme
// against the scheme's own data), a CandidatePager and the last
// composition/segmentation view. No state is touched in DllMain
// (M0 red line #1).

#pragma once

#include "imm32_abi.h"

#include <imfw/session_state.hpp>

namespace imfw {

// The state itself is portable (imfw/session_state.hpp) because the key policy
// reads and writes it on the host; only the per-HIMC store below is IMM32-side.

// Get or create the state for hIMC. Never returns nullptr. Thread-safe;
// callers must not hold the returned pointer across a release_session()
// for the same HIMC.
ImeSessionState* acquire_session(rpimm::InputContext hIMC);

// Drop the state for hIMC (called on ImeSelect(FALSE)).
void release_session(rpimm::InputContext hIMC);

// Forget every per-HIMC session so the next key rebuilds engines against
// fresh data. Calls scheme_instance().reload_data() first, then clears the
// sessions (used after a successful ImeConfigure — a configuration tool may
// have swapped packs or rewritten a data file). Thread-safe, never called
// from DllMain. Same hazard note as release_session(): a thread already
// inside handle_key keeps its engine until the key completes; IMM32
// serializes calls per input context, so only a cross-HIMC reload can race,
// in which case the next key for that context transparently rebuilds its
// session.
void reload_sessions();

}  // namespace imfw
