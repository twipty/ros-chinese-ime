// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// X2-c: the routing decision, tested on its own.
//
// Before this file, "what does this key mean" could only be tested by driving
// handle_key() with a fake engine *and* a fake host and then reading the
// resulting call log -- an entire smoke test to ask whether '5' selects the
// fifth candidate. route_key() answers with a value, so this file needs a fake
// Scheme and nothing else: no engine, no host, no windows, no session.
//
// Three things are pinned here:
//   * every IntentKind is reachable, and its payload (code point / page-relative
//     slot) is what handle_key() will act on;
//   * the keys are the *scheme's*, not the framework's -- a scheme with 'q' for
//     selection and 'j'/'k' for paging gets exactly that, while '1' means
//     nothing to it. This is G4 stated as a test.
//   * the branch order (selection before paging before text) survives, because
//     a scheme is allowed to declare one key as both and the selection branch
//     has always won.

#include <imfw/intent.hpp>
#include <imfw/scheme.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::fprintf(stderr, "key route test failed: %s\n", what.c_str());
        ++failures;
    }
}

const char* kind_name(imfw::IntentKind kind) {
    switch (kind) {
    case imfw::IntentKind::None: return "None";
    case imfw::IntentKind::Input: return "Input";
    case imfw::IntentKind::Select: return "Select";
    case imfw::IntentKind::PagePrev: return "PagePrev";
    case imfw::IntentKind::PageNext: return "PageNext";
    case imfw::IntentKind::Punctuation: return "Punctuation";
    case imfw::IntentKind::Backspace: return "Backspace";
    case imfw::IntentKind::Escape: return "Escape";
    case imfw::IntentKind::RawCommit: return "RawCommit";
    }
    return "?";
}

imfw::KeyEvent plain(std::uint32_t vk, char32_t ch = 0) {
    imfw::KeyEvent event;
    event.vk = vk;
    event.ch = ch;
    return event;
}

// The classic policy: letters are text, '1'..'9' and space select, ',.-=' page.
//
// One deliberate oddity: vk 222 (the apostrophe key) maps to U'\'' but
// is_input_char() rejects it. A fake that accepts everything cannot tell "the
// scheme has no code point for this key" apart from "the scheme has one and
// will not take it", and those are the two halves of the guard at the bottom
// of route_key().
class ClassicScheme : public imfw::Scheme {
public:
    const wchar_t* ui_server_class() const override { return L"RouteUISrv"; }
    const wchar_t* composition_class() const override { return L"RouteComp"; }
    const wchar_t* candidate_class() const override { return L"RouteCand"; }
    const wchar_t* toolbar_class() const override { return L"RouteBar"; }
    const wchar_t* toolbar_label() const override { return L"rt"; }

    char32_t input_char(std::uint32_t vk) const override {
        if (vk >= 'A' && vk <= 'Z') {
            return static_cast<char32_t>('a' + (vk - 'A'));
        }
        if (vk == 222) return U'\'';
        return 0;
    }
    bool is_input_char(char32_t cp) const override {
        return cp >= U'a' && cp <= U'z';
    }
    std::unique_ptr<imfw::Engine> create_engine() const override {
        return nullptr;  // the router never builds an engine
    }
};

// A scheme that reassigns both policies, to prove the router asks instead of
// assuming. '1' is not a selection key here, so getting Select back for 'q' and
// None for '1' can only come from the scheme.
class CustomScheme final : public imfw::Scheme {
public:
    const wchar_t* ui_server_class() const override { return L"CustomUISrv"; }
    const wchar_t* composition_class() const override { return L"CustomComp"; }
    const wchar_t* candidate_class() const override { return L"CustomCand"; }
    const wchar_t* toolbar_class() const override { return L"CustomBar"; }
    const wchar_t* toolbar_label() const override { return L"cu"; }

    char32_t input_char(std::uint32_t vk) const override {
        if (vk >= 'A' && vk <= 'Z') {
            return static_cast<char32_t>('a' + (vk - 'A'));
        }
        return 0;
    }
    bool is_input_char(char32_t cp) const override {
        return cp >= U'a' && cp <= U'z';
    }
    bool is_selection_key(char32_t ch) const override { return ch == U'q'; }
    int page_key(char32_t ch) const override {
        if (ch == U'j') return -1;
        if (ch == U'k') return 1;
        return 0;
    }
    std::unique_ptr<imfw::Engine> create_engine() const override {
        return nullptr;
    }
};

// Both policies claim '5'. Selection has always won; if the order ever flips,
// this is the only place that notices.
class OverlapScheme final : public ClassicScheme {
public:
    int page_key(char32_t ch) const override {
        if (ch == U'5') return 1;
        return ClassicScheme::page_key(ch);
    }
};

// M3: a scheme that declares Chinese punctuation. `,` and `.` are ALSO page
// keys in the classic policy -- the whole point of the test is that the router
// picks punctuation when idle and paging when composing, on the SAME key.
class PunctuationScheme final : public ClassicScheme {
public:
    std::u32string punctuation(char32_t ch) const override {
        if (ch == U',') return U"\uFF0C";
        if (ch == U'.') return U"\u3002";
        if (ch == U'^') return U"\u2026\u2026";  // multi-code-point case
        return {};
    }
};

// Printed with the payload attached: a failure that says "expected Select, got
// Select" is the one thing a wrong slot looks like, and it costs a rebuild to
// find out which of the two differed.
std::string describe(imfw::IntentKind kind, char32_t ch, std::size_t slot,
                     const std::u32string& text) {
    char buf[96];
    std::snprintf(buf, sizeof buf, " ch=U+%04X slot=%zu text_size=%zu",
                  static_cast<unsigned>(ch), slot, text.size());
    return std::string(kind_name(kind)) + buf;
}

void expect(const std::string& desc, const imfw::Scheme& scheme,
            const imfw::KeyEvent& event, imfw::IntentKind kind,
            char32_t ch = 0, std::size_t slot = 0, bool composing = false,
            const std::u32string& text = {}) {
    const imfw::Intent intent = imfw::route_key(scheme, event, composing);
    if (intent.kind != kind || intent.ch != ch || intent.slot != slot ||
        intent.text != text) {
        check(false, desc + ": expected " + describe(kind, ch, slot, text) +
                         ", got " + describe(intent.kind, intent.ch,
                                             intent.slot, intent.text));
    }
}

}  // namespace

int main() {
    const ClassicScheme classic;
    const CustomScheme custom;
    const OverlapScheme overlap;

    // ---- text -------------------------------------------------------------
    expect("A is text 'a'", classic, plain('A', U'a'), imfw::IntentKind::Input,
           U'a');
    expect("Z is text 'z' (the scheme normalizes case)", classic,
           plain('Z', U'z'), imfw::IntentKind::Input, U'z');
    expect("a key with no code point is None", classic, plain(0x70),
           imfw::IntentKind::None);
    expect("a code point the scheme rejects is None", classic, plain(222),
           imfw::IntentKind::None);

    // Text is the last branch, so it must survive a non-zero `ch` that is not
    // a policy key.
    expect("text with a non-policy character is still text", classic,
           plain('M', U'm'), imfw::IntentKind::Input, U'm');

    // ---- selection --------------------------------------------------------
    expect("'1' selects slot 0", classic, plain('1', U'1'),
           imfw::IntentKind::Select, U'1', 0);
    expect("'5' selects slot 4", classic, plain('5', U'5'),
           imfw::IntentKind::Select, U'5', 4);
    expect("'9' selects slot 8", classic, plain('9', U'9'),
           imfw::IntentKind::Select, U'9', 8);
    expect("space selects the first candidate", classic, plain(' ', U' '),
           imfw::IntentKind::Select, U' ', 0);
    expect("'0' is not a selection key", classic, plain('0', U'0'),
           imfw::IntentKind::None);

    // ---- paging -----------------------------------------------------------
    expect("',' pages back", classic, plain(',', U','),
           imfw::IntentKind::PagePrev, U',');
    expect("'-' pages back", classic, plain('-', U'-'),
           imfw::IntentKind::PagePrev, U'-');
    expect("'.' pages forward", classic, plain('.', U'.'),
           imfw::IntentKind::PageNext, U'.');
    expect("'=' pages forward", classic, plain('=', U'='),
           imfw::IntentKind::PageNext, U'=');

    // ---- framework-owned keys ---------------------------------------------
    {
        imfw::KeyEvent event = plain(0x08);
        event.kind = imfw::KeyKind::Backspace;
        expect("Backspace is Backspace", classic, event,
               imfw::IntentKind::Backspace);
    }
    {
        imfw::KeyEvent event = plain(0x1B);
        event.kind = imfw::KeyKind::Escape;
        expect("Escape is Escape", classic, event, imfw::IntentKind::Escape);
    }

    // ---- the scheme owns the keys (G4) ------------------------------------
    expect("a scheme picks its own selection key", custom, plain('Q', U'q'),
           imfw::IntentKind::Select, U'q', 0);
    expect("a scheme picks its own back-page key", custom, plain('J', U'j'),
           imfw::IntentKind::PagePrev, U'j');
    expect("a scheme picks its own forward-page key", custom, plain('K', U'k'),
           imfw::IntentKind::PageNext, U'k');
    expect("the default digits mean nothing to a scheme that did not ask",
           custom, plain('1', U'1'), imfw::IntentKind::None);
    expect("the default space means nothing either", custom, plain(' ', U' '),
           imfw::IntentKind::None);
    expect("the default page keys mean nothing either", custom,
           plain('.', U'.'), imfw::IntentKind::None);

    // ---- branch order -----------------------------------------------------
    expect("a key claimed by both policies selects, not pages", overlap,
           plain('5', U'5'), imfw::IntentKind::Select, U'5', 4);

    // ---- M3 punctuation: same key, two meanings by composing state --------
    const PunctuationScheme punct;
    expect("idle ',' yields a fullwidth comma", punct, plain(',', U','),
           imfw::IntentKind::Punctuation, U',', 0, false,
           std::u32string(1, U'\uFF0C'));
    expect("idle '.' yields an ideographic full stop", punct, plain('.', U'.'),
           imfw::IntentKind::Punctuation, U'.', 0, false,
           std::u32string(1, U'\u3002'));
    expect("idle '^' carries TWO code points in Intent.text", punct,
           plain(0x36, U'^'), imfw::IntentKind::Punctuation, U'^', 0, false,
           std::u32string(U"\u2026\u2026"));
    expect("composing ',' still pages back", punct, plain(',', U','),
           imfw::IntentKind::PagePrev, U',', 0, true);
    expect("composing '.' still pages forward", punct, plain('.', U'.'),
           imfw::IntentKind::PageNext, U'.', 0, true);
    expect("a key with no punctuation mapping stays a page key", punct,
           plain('-', U'-'), imfw::IntentKind::PagePrev, U'-', 0, false);
    expect("digits select even when idle (halfwidth, no punct mapping)", punct,
           plain('1', U'1'), imfw::IntentKind::Select, U'1', 0, false);
    expect("a scheme with no punctuation policy pages as before", classic,
           plain(',', U','), imfw::IntentKind::PagePrev, U',', 0, false);

    // ---- S2: Enter is RawCommit, but only mid-composition ------------------
    {
        imfw::KeyEvent enter;
        enter.vk = 0x0D;  // VK_RETURN
        enter.kind = imfw::KeyKind::Enter;
        expect("Enter while composing routes to RawCommit", classic, enter,
               imfw::IntentKind::RawCommit, 0, 0, true);
        // Idle Enter is not an intent here: wants_key() already lets it reach
        // the application, so the router simply finds no text for VK_RETURN.
        expect("Enter while idle is not RawCommit", classic, enter,
               imfw::IntentKind::None, 0, 0, false);
    }

    // ---- the default Intent is None ---------------------------------------
    {
        const imfw::Intent fresh;
        check(fresh.kind == imfw::IntentKind::None && fresh.ch == 0 &&
                  fresh.slot == 0 && fresh.text.empty(),
              "a default-constructed Intent means nothing to do");
    }

    if (failures != 0) {
        std::fprintf(stderr, "%d key route checks failed\n", failures);
        return 1;
    }
    std::printf("key route: all checks passed\n");
    return 0;
}
