// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// The key policy, with the host taken out.
//
// Before X2-a this logic was process_key() inside
// framework/platform/win32/imm32/imm_entry.cpp: it read a 256-byte Win32 key
// state array, called ImmGetOpenStatus, painted our windows and wrote into a
// TRANSMSGLIST. That is four platform concerns wrapped around a decision, and
// it meant the decision itself had **zero** host-side test coverage — the
// single most-executed code path in the IME could only be exercised by typing
// into a ReactOS guest.
//
// The split now is:
//   * the host adapter (platform/win32) translates one native message into a
//     KeyEvent — which virtual key is Backspace, which character a key
//     produces, which modifiers are down — and turns the returned
//     KeyResult::commit back into WM_IME_CHAR messages;
//   * this layer decides, and calls Host for the four things it cannot do
//     alone (open flag, two popup windows, glyph-mode state, trace channel).
//
// Nothing below includes <windows.h>; tests/host_key_test.cpp is the proof.

#pragma once

#include <imfw/candidate_pager.hpp>
#include <imfw/engine.hpp>
#include <imfw/session_state.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace imfw {

// Which framework-managed key this message is. The adapter decides (it is the
// only layer that knows the platform's key numbers); the policy below compares
// kinds, so it never spells a virtual-key constant.
enum class KeyKind : std::uint8_t {
    Other = 0,
    Backspace,
    Escape,
    Shift,
    ModeToggle,     // Ctrl+Shift+G: flip [快]/[全], owned by the framework
    LanguageToggle, // Ctrl+Shift+Space: backup hotkey for Chinese/English
                    // (the primary Ctrl+Space is unreliable — CORE-19455)
};

// One key message, already translated.
//
//   vk       the scheme's own key code (Scheme::input_char). The adapter passes
//            the platform virtual key through untouched: every scheme maps
//            ASCII letters itself, and a scheme that wants another key knows
//            its own numbers.
//   ch       the character the *framework* translates this key into (digits,
//            space, `+-.,`): those are the policy keys checked against
//            Scheme::is_selection_key / page_key. 0 when there is none.
//   kind     Backspace / Escape / Shift / Other, for the keys the framework
//            owns itself.
//   key_up   transition bit from ImeProcessKey's lParam. ImeToAsciiEx gets no
//            lParam, so this is the only place the bit can enter the policy.
//   ctrl/alt modifier state. Modifiers are never consumed, so application
//            shortcuts keep working.
struct KeyEvent {
    std::uint32_t vk = 0;
    char32_t ch = 0;
    KeyKind kind = KeyKind::Other;
    bool key_up = false;
    bool ctrl = false;
    bool alt = false;
};

// What the policy wants inserted. Empty means "nothing to emit"; the adapter
// turns it into WM_IME_CHAR messages through imfw/emit_text.hpp.
struct KeyResult {
    std::u32string commit;
};

// Everything the key policy needs from outside the engine. The Windows host
// implements it over ImmGetOpenStatus / ImmSetOpenStatus and
// framework/platform/win32/ui; a test substitutes a spy and needs neither
// <windows.h> nor a GUI.
class Host {
public:
    virtual ~Host() = default;

    // Chinese/English state: stored by IMM32 (client API), owned by the IME.
    virtual bool is_open() = 0;
    virtual void set_open(bool open) = 0;

    // Composition + candidate windows for the current input context.
    // `composition` is UTF-8 — engines may put CJK in the composition, not just
    // ASCII codes (G4) — and is converted to whatever the window layer paints
    // inside the adapter, so this interface stays free of wide-character
    // decisions.
    virtual void show(const std::string& composition,
                      const std::vector<std::string>& segments,
                      const CandidatePager& pager) = 0;
    virtual void hide() = 0;

    // P1-2 used to ask the font per code point. Grading now happens once at
    // build time (Candidate::glyph_visible); the host only exposes the
    // session-scoped [快]/[全] choice. The state lives in ImeSessionState --
    // Host is an accessor, never the owner (FR-2/FR-5).
    virtual bool show_all_glyphs() const = 0;
    virtual void toggle_glyph_mode() = 0;

    // ---- diagnostics (optional) ------------------------------------------
    // The Win32 host writes these lines to C:\rospinyin-trace.log when
    // ROSPINYIN_TRACE is defined; tests leave the defaults alone. They exist as
    // hooks instead of a trace macro because the numbers belong to the host
    // (window moves / repaints), which this layer cannot see.
    virtual void on_feed(char32_t input, const Result& result) {
        (void)input;
        (void)result;
    }
    // `kept_all` distinguishes the two ways a filter run ends: everything was
    // drawable, or so little was that the font itself is suspect and the list
    // is kept anyway.
    virtual void on_candidates_filtered(std::size_t dropped, std::size_t total,
                                        bool kept_all) {
        (void)dropped;
        (void)total;
        (void)kept_all;
    }
};

// ImeProcessKey's decision: does this IME own the key?
//
// Takes a mutable session because of lone-Shift: a Shift key-up with no other
// key in between toggles Chinese/English *here*, which is the only place the
// transition bit is visible. Every other key clears the latch. For every other
// key this function has no side effect.
bool wants_key(ImeSessionState& session, const KeyEvent& event, Host& host);

// ImeToAsciiEx's work: apply the key to the session, return what to insert.
KeyResult handle_key(ImeSessionState& session, const KeyEvent& event, Host& host);

}  // namespace imfw
