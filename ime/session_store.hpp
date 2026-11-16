// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Per-input-context M2 state. One PinyinTable is shared process-wide (loaded
// lazily on first key); each HIMC owns a PinyinSession and a CandidatePager.
// No state is touched in DllMain (M0 red line #1).

#pragma once

#include "imm32_abi.h"
#include <rospinyin/candidate_pager.hpp>
#include <rospinyin/pinyin_session.hpp>
#include <rospinyin/table.hpp>

namespace rospinyin {

struct ImeSessionState {
    explicit ImeSessionState(const PinyinTable& table) : session(table) {}

    PinyinSession session;
    CandidatePager pager;
    bool composing = false;  // true while there is an active composition
};

// Returns the shared table, loading rospinyin.dat (or the built-in fallback)
// on first use. Thread-safe.
const PinyinTable& shared_table();

// Get or create the state for hIMC. Never returns nullptr after the table has
// been loaded. Thread-safe; callers must not hold the returned pointer across
// a release_session() for the same HIMC.
ImeSessionState* acquire_session(rpimm::InputContext hIMC);

// Drop the state for hIMC (called on ImeSelect(FALSE)).
void release_session(rpimm::InputContext hIMC);

}  // namespace rospinyin
