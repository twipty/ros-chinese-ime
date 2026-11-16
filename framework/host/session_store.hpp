// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Per-input-context state. One CodeTable is shared process-wide (loaded
// lazily on first key); each HIMC owns an Engine (built by the Scheme), a
// CandidatePager and the last composition/segmentation view. No state is
// touched in DllMain (M0 red line #1).

#pragma once

#include "imm32_abi.h"

#include <imfw/candidate_pager.hpp>
#include <imfw/engine.hpp>
#include <imfw/scheme.hpp>

#include <memory>
#include <string>
#include <vector>

namespace imfw {

struct ImeSessionState {
    std::unique_ptr<Engine> engine;
    CandidatePager pager;
    bool composing = false;  // true while there is an active composition
    std::string composition;             // last engine composition
    std::vector<std::string> segments;    // last engine segmentation
};

// Returns the shared table, loading the scheme's dat (or fallback) on first
// use. Thread-safe.
const CodeTable& shared_table();

// Get or create the state for hIMC. Never returns nullptr after the table
// has been loaded. Thread-safe; callers must not hold the returned pointer
// across a release_session() for the same HIMC.
ImeSessionState* acquire_session(rpimm::InputContext hIMC);

// Drop the state for hIMC (called on ImeSelect(FALSE)).
void release_session(rpimm::InputContext hIMC);

// Framework gap G2: forget the loaded table and every per-HIMC session so
// the next key reloads data (scheme .dat rewritten, packs swapped by a
// configuration tool — see ImeConfigure). Thread-safe, never called from
// DllMain. Same hazard note as release_session(): a thread already inside
// process_key keeps its engine until the key completes; IMM32 serializes
// calls per input context, so only a cross-HIMC reload can race, in which
// case the next key for that context transparently rebuilds its session.
void reload_sessions();

}  // namespace imfw
