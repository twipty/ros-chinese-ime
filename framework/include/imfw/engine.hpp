// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Platform-independent input-engine contract (no Windows headers, no IMM32).
// M1 ships a passthrough implementation; M2 replaces it with the pinyin
// syllable engine while keeping this API stable. The IME layer translates
// WM_KEYDOWN / IMM32 calls into feed()/backspace()/cancel() and renders the
// returned Result.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace imfw {

enum class Action : std::uint8_t {
    Passthrough,  // let the key reach the application untouched
    Absorb,       // key was used to update the composition
    Commit,       // key produced text to be inserted into the application
};

struct Candidate {
    std::u32string text;
    int score = 0;
};

struct Result {
    Action action = Action::Passthrough;
    // Raw input shown to the user in the composition window. UTF-8 — ASCII
    // input works unchanged, but an engine may also emit CJK text (framework
    // gap G4 used to limit compositions to ASCII by byte-wise conversion).
    std::string composition;
    std::vector<Candidate> candidates;
    // How the engine split the composition into lookup units (pinyin:
    // syllables [zhong][guo]; shape IMEs: typed codes), UTF-8 like the
    // composition. The composition window underlines these boundaries.
    std::vector<std::string> segments;
    std::u32string commit_text;          // text to insert on Commit
};

class Engine {
public:
    virtual ~Engine() = default;
    virtual Result feed(char32_t codepoint) = 0;
    virtual Result backspace() = 0;
    virtual Result cancel() = 0;
    // Called on ImeSelect / ImeSetActiveContext. Implementations must be
    // idempotent: IMM32 issues these repeatedly during layout switches.
    virtual void on_select(bool selected) = 0;
};

// Build identifier supplied by the linked engine library. Tests use it to
// prove the library is actually linked rather than header-only by accident.
const char* engine_build_id() noexcept;

}  // namespace imfw
