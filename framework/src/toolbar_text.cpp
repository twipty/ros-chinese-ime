// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// See imfw/toolbar_text.hpp.

#include <imfw/toolbar_text.hpp>

namespace imfw {

std::wstring toolbar_text(const Scheme& scheme, bool open) {
    const wchar_t* label = scheme.toolbar_label();
    std::wstring text = label != nullptr ? label : L"";

    const wchar_t* state = scheme.toolbar_state_label(open);
    // No state text: the bare label, which is what every scheme showed before
    // the toolbar learned about fOpen. Deliberately not "拼:" -- a dangling
    // separator reads like a rendering bug.
    if (state == nullptr || state[0] == L'\0') return text;

    text += L':';
    text += state;
    return text;
}

std::wstring toolbar_text_with_mode(const Scheme& scheme, bool open,
                                    bool full_glyph) {
    std::wstring text = toolbar_text(scheme, open);
    // M3: for a scheme that declares a state (the pinyin "拼:开/关"), add the
    // explicit Chinese/English marker the user asked for: "拼:开·中[快]".
    // A stateless scheme (no toolbar_state_label) keeps its bare label + mode
    // suffix -- inventing a language marker for a scheme with no language
    // state would be a lie. The marker is derived from `open`, not a new flag:
    // fOpen IS the Chinese/English state, and punctuation follows it.
    const wchar_t* state = scheme.toolbar_state_label(open);
    if (state != nullptr && state[0] != L'\0') {
        text += open ? L"·中" : L"·英";
    }
    return text + (full_glyph ? L"[全]" : L"[快]");
}

}  // namespace imfw
