// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// X2-c: the routing decision, on its own. Every `if` here is the same test, in
// the same order, on the same operands as the chain it was lifted out of
// handle_key() — the difference is that this function returns a value instead
// of doing the work, so the order (selection before paging before text) is
// visible in one place and pinnable by tests/key_route_test.cpp.
//
// The order is not incidental: a scheme may declare one key as both a
// selection key and a paging key, and the selection branch has always run
// first. X2-c's contract is behaviour-identical, so it stays first.

#include <imfw/intent.hpp>

#include <imfw/scheme.hpp>

namespace imfw {

Intent route_key(const Scheme& scheme, const KeyEvent& event, bool composing) {
    const char32_t ch = event.ch;

    // M3 punctuation runs FIRST, and only while idle. A key like ',' is both
    // a fullwidth-comma source and a page key; when there is no composition
    // the punctuation meaning wins, and mid-composition the !composing guard
    // drops through to the paging branch below. Selection keys (space, 1-9)
    // have no fullwidth mapping, so they are unaffected.
    if (!composing && ch != 0) {
        const std::u32string punct = scheme.punctuation(ch);
        if (!punct.empty()) {
            Intent intent;
            intent.kind = IntentKind::Punctuation;
            intent.ch = ch;        // source key, for diagnostics only
            intent.text = punct;   // complete text: '^' commits TWO code points
            return intent;
        }
    }

    // Selection before paging before text: the order the branches have always
    // run in, and the one a scheme's overlapping declarations depend on.
    if (ch != 0 && scheme.is_selection_key(ch)) {
        Intent intent;
        intent.kind = IntentKind::Select;
        intent.ch = ch;
        // Space (and any non-digit selection key) selects the first candidate
        // on the page; digits map to page-relative indices. Which keys select
        // at all is the Scheme's policy (G4).
        if (ch >= U'1' && ch <= U'9') {
            intent.slot = static_cast<std::size_t>(ch - U'1');
        }
        return intent;
    }

    if (ch != 0) {
        const int page = scheme.page_key(ch);
        if (page != 0) {
            Intent intent;
            intent.kind = page < 0 ? IntentKind::PagePrev : IntentKind::PageNext;
            intent.ch = ch;
            return intent;
        }
    }

    if (event.kind == KeyKind::Backspace) {
        Intent intent;
        intent.kind = IntentKind::Backspace;
        return intent;
    }
    if (event.kind == KeyKind::Escape) {
        Intent intent;
        intent.kind = IntentKind::Escape;
        return intent;
    }

    // Text last: a key that is not a policy key is only ours if the scheme's
    // alphabet claims it.
    const char32_t input = scheme.input_char(event.vk);
    if (input == 0 || !scheme.is_input_char(input)) return {};
    Intent intent;
    intent.kind = IntentKind::Input;
    intent.ch = input;
    return intent;
}

}  // namespace imfw
