// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// X2-a: process_key() moved out of framework/platform/win32/imm32/imm_entry.cpp
// and renamed handle_key(), with its ownership sibling should_consume_key()
// renamed wants_key(). The bodies are the same decisions in the same order —
// what changed is that the platform calls became imfw::Host calls, so the whole
// file compiles and is tested on the build host (tests/host_key_test.cpp).
//
// The rule that makes this a move instead of a rewrite: **no decision was
// touched.** Every `if` below existed in imm_entry.cpp with the same operands.
// If a branch ever needs to change, it changes here, with a test.

#include <imfw/host.hpp>

#include <imfw/scheme.hpp>

#include <vector>

namespace imfw {
namespace {

// Composition is UTF-8 (G4 connected conclusion: engines may show CJK in the
// composition window, not just ASCII codes); the host converts it.
void refresh_ui(Host& host, ImeSessionState& state) {
    host.show(state.composition, state.segments, state.pager);
}

void finish_composition(Host& host, ImeSessionState& state) {
    state.engine->cancel();
    state.pager.reset({});
    state.composing = false;
    state.composition.clear();
    state.segments.clear();
    host.hide();
}

// Chinese <-> English. Called only from the Shift gesture in wants_key().
//
// Closing must drop an in-flight composition: leaving the letters on screen
// while every following key goes to the application produces a popup that
// never updates (the exact "frozen composition window" symptom).
void set_ime_open(Host& host, ImeSessionState& state, bool open) {
    if (open == host.is_open()) return;
    host.set_open(open);
    if (!open && state.composing) finish_composition(host, state);
}

// Track the last engine output so paging/refresh need no engine call.
void adopt_result(ImeSessionState& state, const Result& result) {
    state.composition = result.composition;
    state.segments = result.segments;
}

// P1-2: keep only candidates the candidate font can draw. Without this,
// `shi` offered a ranked extension-A character that painted as a tofu box and
// inserted a box into the document when picked.
//
// Two guards make a wrong font harmless:
//   * everything dropped (or more than half) means the font has no CJK at all
//     — that is a font problem, not a candidate problem, so keep the list;
//   * the probe fails open (see UiCanRender), so a GDI without
//     GetGlyphIndicesW behaves exactly as before.
std::vector<Candidate> renderable_candidates(Host& host,
                                             const std::vector<Candidate>& input) {
    if (input.empty()) return input;

    std::vector<Candidate> kept;
    kept.reserve(input.size());
    for (const auto& candidate : input) {
        bool drawable = true;
        for (const char32_t codepoint : candidate.text) {
            if (codepoint != 0 && !host.can_render(codepoint)) {
                drawable = false;
                break;
            }
        }
        if (drawable) kept.push_back(candidate);
    }

    const std::size_t dropped = input.size() - kept.size();
    if (dropped == 0) return input;
    if (kept.empty() || kept.size() * 2 < input.size()) {
        host.on_candidates_filtered(dropped, input.size(), true);
        return input;
    }
    host.on_candidates_filtered(dropped, input.size(), false);
    return kept;
}

// Emit one picked character and either keep composing or close the session.
//
// The pinyin engine converts left to right, so committing 中 out of
// "zhongguo" leaves "guo" in the composition (Result::composition, candidates
// and segments are all filled in). Dropping that remainder — what an
// unconditional finish_composition() used to do — threw away everything the
// user had typed after the first syllable. A stateless engine returns an empty
// composition here, so this branch is the plain "commit and close" path.
std::u32string apply_commit(Host& host, ImeSessionState& state,
                            const Result& result) {
    if (result.commit_text.empty()) return {};
    const std::u32string text = result.commit_text;

    if (result.composition.empty()) {
        finish_composition(host, state);
    } else {
        adopt_result(state, result);
        state.pager.reset(renderable_candidates(host, result.candidates));
        state.composing = true;
        refresh_ui(host, state);
    }
    return text;
}

}  // namespace

bool wants_key(ImeSessionState& session, const KeyEvent& event, Host& host) {
    // Shift owns Chinese/English. ReactOS gives us every key message, including
    // VK_SHIFT (verified in the trace: `processkey vk=0x10 -> 0`), so the IME
    // implements the toggle itself -- imm32 only stores fOpen and no hot key is
    // registered for Shift.
    //
    // Lone-Shift semantics: a latch cleared by any other key means Shift+A is
    // still a capital A rather than "switch to English, then type a". The
    // switch happens on key-UP, which is where a user expects it.
    if (event.kind == KeyKind::Shift) {
        if (!event.key_up) {
            session.shift_latched = true;
        } else {
            const bool lone = session.shift_latched;
            session.shift_latched = false;
            if (lone) set_ime_open(host, session, !host.is_open());
        }
        return false;
    }

    // A key-UP must never be claimed. This is the only place the decision can
    // be made: ReactOS routes every key message through IntImmProcessKey
    // (co_IntProcessKeyboardMessage sets bKeyUpDown for WM_KEYUP as well) and
    // imm32!ImmProcessKey ignores the transition bit. Claiming a key-up sets
    // pIC->bNeedsTrans, and the following ImmGenerateMessage calls
    // ImeToAsciiEx — which receives no lParam, so it cannot filter either.
    //
    // Symptom when it is wrong: every keystroke reaches the engine twice.
    // "n" becomes "nn", "a" becomes "aa". Doubled letters that still form a
    // legal pinyin prefix are kept (extra characters); the rest are rejected
    // by can_extend() and vanish (missing characters). Both halves of the
    // "extra / lost characters" report come from this one line.
    if (event.key_up) return false;

    session.shift_latched = false;  // Shift was a modifier

    // Closed = English: every key belongs to the application. This is the whole
    // point of the Shift toggle; without it "switching to English" would leave
    // the IME swallowing letters and inserting Chinese.
    if (!host.is_open()) return false;
    if (event.ctrl || event.alt) return false;

    const Scheme& scheme = scheme_instance();
    const char32_t input = scheme.input_char(event.vk);
    if (input != 0 && scheme.is_input_char(input)) return true;

    if (event.kind == KeyKind::Backspace || event.kind == KeyKind::Escape) {
        return session.composing;
    }
    if (event.ch == 0) return false;
    // Selection and paging keys (policy owned by the Scheme, G4) are only
    // meaningful mid-composition with candidates on screen; otherwise they
    // pass through to the application (e.g. space in an empty editor).
    if (scheme.is_selection_key(event.ch) || scheme.page_key(event.ch) != 0) {
        return session.composing && !session.pager.empty();
    }
    return false;
}

KeyResult handle_key(ImeSessionState& session, const KeyEvent& event, Host& host) {
    if (session.engine == nullptr) return {};

    const Scheme& scheme = scheme_instance();
    const char32_t ch = event.ch;

    // Digit / space selection is resolved by the pager so page 2 digit 1
    // selects candidate 10, not candidate 1. Which keys select is the
    // Scheme's policy (G4); digits map to page-relative indices, any other
    // selection key picks the first candidate.
    if (ch != 0 && scheme.is_selection_key(ch)) {
        std::size_t digit = 0;
        if (ch >= U'1' && ch <= U'9') {
            digit = static_cast<std::size_t>(ch - U'1');
        }
        const Candidate* chosen = session.pager.select(digit);
        if (chosen == nullptr || chosen->text.empty()) return {};
        // The pager holds a *filtered* copy of the candidate list (P1-2 drops
        // characters the font cannot draw), so its index means nothing to the
        // engine — hand the text back instead and let the engine decide what
        // is left to convert.
        return {apply_commit(host, session,
                             session.engine->commit_text(chosen->text))};
    }

    // Paging direction is likewise the Scheme's policy (G4).
    if (ch != 0) {
        const int page = scheme.page_key(ch);
        if (page != 0) {
            const bool moved = page < 0 ? session.pager.prev_page()
                                        : session.pager.next_page();
            if (moved) refresh_ui(host, session);
            return {};
        }
    }

    if (event.kind == KeyKind::Backspace) {
        const Result result = session.engine->backspace();
        adopt_result(session, result);
        session.pager.reset(renderable_candidates(host, result.candidates));
        session.composing = !result.composition.empty();
        if (session.composing) refresh_ui(host, session);
        else host.hide();
        return {};
    }

    if (event.kind == KeyKind::Escape) {
        finish_composition(host, session);
        return {};
    }

    const char32_t input = scheme.input_char(event.vk);
    if (input == 0 || !scheme.is_input_char(input)) return {};

    const Result result = session.engine->feed(input);
    // One line per fed character. The composition string after each feed is
    // the fastest way to separate "the key arrived twice" (letters doubled:
    // `nn`, `aa`) from "the key arrived once and was rejected by can_extend"
    // (composition unchanged). Without it both look like a wrong result.
    // The host adds the window counters it owns (moves / repaints).
    host.on_feed(input, result);
    switch (result.action) {
    case Action::Commit:
        return {apply_commit(host, session, result)};
    case Action::Absorb:
        adopt_result(session, result);
        session.pager.reset(renderable_candidates(host, result.candidates));
        session.composing = !result.composition.empty();
        refresh_ui(host, session);
        break;
    case Action::Passthrough:
    default:
        break;
    }
    return {};
}

}  // namespace imfw
