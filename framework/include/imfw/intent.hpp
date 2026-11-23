// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// X2-c: one key message, reduced to one word.
//
// Until now the question "what does this key mean?" was answered inline in
// handle_key() as a chain of `if`s that each did the work as they matched: the
// selection branch committed text, the paging branch moved the pager, the
// backspace branch called the engine. Reading a key cost a call into the
// engine; testing one meant standing up a fake engine *and* a fake host, so the
// only tests that covered the routing were whole-sequence smoke tests.
//
// route_key() splits that: it answers with a value, and handle_key() does the
// work. Same decisions, same order — but now the routing can be tested with a
// fake Scheme alone (tests/key_route_test.cpp), and the vocabulary survives a
// frontend change: a second frontend can consume Intent without inheriting any
// of our threading, windowing or engine assumptions.
//
// status: draft  — see docs/plan/PLAN-MODULARIZATION.md §4 (M3). Draft because
// the vocabulary is still growing: M3's punctuation, full/half width and
// temporary-English keys will each add a kind, and freezing an enum before its
// additions are known is how enums end up with `Misc2`.
//
// Deliberately absent: ToggleLanguage. Shift does switch Chinese/English, but
// the decision that runs it is an *ownership* decision — ImeProcessKey must
// answer "not mine" for the key-up that performs the toggle, and the lone-Shift
// latch only exists in that function (see wants_key). An intent that can only
// be produced by the layer that promised never to dispatch anything is a trap
// for the next reader, so the toggle stays in wants_key() until a second
// frontend actually needs to ask for it.

#pragma once

#include <imfw/host.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace imfw {

class Scheme;

enum class IntentKind : std::uint8_t {
    None = 0,   // nothing to do; the key was owned but produces no action
    Input,      // feed `ch` to the engine
    Select,     // pick the candidate at page-relative index `slot`
    PagePrev,   // pager.previous_page()
    PageNext,   // pager.next_page()
    Punctuation,// commit `ch`, an already-mapped fullwidth code point (M3)
    Backspace,  // engine.backspace()
    Escape,     // discard the composition
};

struct Intent {
    IntentKind kind = IntentKind::None;
    // Input: the code point to feed (already through Scheme::input_char).
    // Select: the key that selected, kept for diagnostics only.
    char32_t ch = 0;
    // Select only: 0-based, relative to the current page (so digit 1 on page 2
    // means candidate 10). Resolved by the pager, not here.
    std::size_t slot = 0;
    // Punctuation only: the complete text to commit. `ch` remains the source
    // key for diagnostics, but one char32_t cannot express '……' (U+2026 twice).
    std::u32string text;
};

// What this key means, or IntentKind::None.
//
// Precondition: the caller has already established ownership through
// wants_key(). This function does not look at key_up / ctrl / alt / fOpen —
// those are ownership questions and answering them twice is how the two
// answers drift apart.
//
// `composing` is the session's active-composition flag: a key that is both a
// punctuation key and a page key (`,` `.`) yields Punctuation only when idle,
// and PagePrev/PageNext mid-composition. It is a value, not a session -- the
// router stays pure and the decision stays testable on its own.
//
// Pure: no session, no engine, no host. The only inputs are the key, the
// composing flag and the scheme's policy, which is why every branch below is
// testable on its own.
Intent route_key(const Scheme& scheme, const KeyEvent& event, bool composing);

}  // namespace imfw
