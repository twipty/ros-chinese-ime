// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// The toolbar caption ("拼:开" / "拼:关").
//
// This is the one part of the state indicator that can be pinned on the build
// host: the window itself needs GDI and a real HWND, but the *text* is a
// decision, and until now nothing tested it. What the user reported after the
// first Shift -- the tray's red 中 staying put and our toolbar showing a
// constant "拼" -- is exactly a caption bug, so it gets a caption test.

#include <imfw/scheme.hpp>
#include <imfw/toolbar_text.hpp>

#include <rospinyin/scheme.hpp>

#include <cstdio>
#include <memory>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "toolbar text test failed: %s\n", what);
        ++failures;
    }
}

void check_text(const std::wstring& actual, const wchar_t* expected,
                const char* what) {
    if (actual != expected) {
        std::fprintf(stderr, "toolbar text test failed: %s (got \"%ls\")\n",
                     what, actual.c_str());
        ++failures;
    }
}

// The label/state half of the Scheme contract, and nothing else -- this test
// has no engine, no data and no key policy.
class TextScheme final : public imfw::Scheme {
public:
    TextScheme(const wchar_t* label, const wchar_t* open, const wchar_t* closed)
        : label_(label), open_(open), closed_(closed) {}

    const wchar_t* ui_server_class() const override { return L"TextUi"; }
    const wchar_t* composition_class() const override { return L"TextComp"; }
    const wchar_t* candidate_class() const override { return L"TextCand"; }
    const wchar_t* toolbar_class() const override { return L"TextBar"; }
    const wchar_t* toolbar_label() const override { return label_; }
    const wchar_t* toolbar_state_label(bool open) const override {
        return open ? open_ : closed_;
    }

    char32_t input_char(std::uint32_t) const override { return 0; }
    bool is_input_char(char32_t) const override { return false; }
    std::unique_ptr<imfw::Engine> create_engine() const override {
        return nullptr;
    }

private:
    const wchar_t* label_;
    const wchar_t* open_;
    const wchar_t* closed_;
};

// A scheme that keeps the default toolbar_state_label(): no state to show.
class StatelessScheme final : public imfw::Scheme {
public:
    const wchar_t* ui_server_class() const override { return L"StatelessUi"; }
    const wchar_t* composition_class() const override { return L"StatelessComp"; }
    const wchar_t* candidate_class() const override { return L"StatelessCand"; }
    const wchar_t* toolbar_class() const override { return L"StatelessBar"; }
    const wchar_t* toolbar_label() const override { return L"형"; }

    char32_t input_char(std::uint32_t) const override { return 0; }
    bool is_input_char(char32_t) const override { return false; }
    std::unique_ptr<imfw::Engine> create_engine() const override {
        return nullptr;
    }
};

}  // namespace

int main() {
    // The shipped pinyin scheme: the strings the user sees in the guest.
    const rospinyin::PinyinScheme pinyin;
    check_text(imfw::toolbar_text(pinyin, true), L"拼:开", "pinyin open");
    check_text(imfw::toolbar_text(pinyin, false), L"拼:关", "pinyin closed");
    // The property behind the whole change: the caption must actually differ,
    // otherwise "the toolbar shows a state" is a claim with no observable.
    check(imfw::toolbar_text(pinyin, true) != imfw::toolbar_text(pinyin, false),
          "open and closed captions differ");

    // Any scheme, any words: nothing about 开/关 is hardcoded in the framework.
    const TextScheme text(L"仓", L"ON", L"OFF");
    check_text(imfw::toolbar_text(text, true), L"仓:ON", "custom open state");
    check_text(imfw::toolbar_text(text, false), L"仓:OFF", "custom closed state");

    // Default state label (nullptr): bare label, and specifically no dangling
    // "형:" separator, which would look like a broken render rather than a
    // scheme that has no mode.
    const StatelessScheme stateless;
    check_text(imfw::toolbar_text(stateless, true), L"형", "stateless open");
    check_text(imfw::toolbar_text(stateless, false), L"형", "stateless closed");

    // Glyph mode is a second, independent boolean: [快]/[全] must not follow
    // the Chinese/English flag, and a stateless scheme gets the suffix too.
    check_text(imfw::toolbar_text_with_mode(pinyin, true, false), L"拼:开·中[快]",
               "[快] + Chinese on an open pinyin session");
    check_text(imfw::toolbar_text_with_mode(pinyin, true, true), L"拼:开·中[全]",
               "[全] on an open pinyin session");
    check_text(imfw::toolbar_text_with_mode(pinyin, false, true), L"拼:关·英[全]",
               "mode suffix is independent, English state shown on closed");
    check_text(imfw::toolbar_text_with_mode(pinyin, false, false), L"拼:关·英[快]",
               "closed pinyin shows English with [快]");
    check_text(imfw::toolbar_text_with_mode(stateless, false, false), L"형[快]",
               "stateless scheme still shows the mode, no language marker");

    if (failures != 0) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    std::printf("toolbar text: all checks passed\n");
    return 0;
}
