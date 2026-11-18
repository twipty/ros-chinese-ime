// RosPinyin / toy scheme test — SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// The toy scheme is not decoration: it is the second consumer of the plugin
// contract, and the only one this repository can build on a host with no CJK
// data. Two things are pinned here:
//
//   * it reaches the framework through the *seam* (scheme_factory -> registry),
//     so the sample cannot rot into a folder nobody links;
//   * it obeys the contract details that cost real debugging time before —
//     especially the 15-character UI class name, which imm32 truncates before
//     looking the class up (see imfw/scheme.hpp).

#include <imfw/engine.hpp>
#include <imfw/scheme.hpp>
#include <imfw/scheme_registry.hpp>
#include <toy_scheme.hpp>

#include <cstdio>
#include <cwchar>
#include <memory>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "toy scheme test failed: %s\n", what);
        ++failures;
    }
}

}  // namespace

// The assembly half of -DROSPINYIN_SCHEME=toy, reduced to its essentials:
// ime/scheme_reg_toy.cpp does exactly this.
namespace imfw {

Scheme* scheme_factory() {
    static toy::ToyScheme scheme;
    return &scheme;
}

}  // namespace imfw

int main() {
    // Reach the scheme the way the framework does, not by constructing one
    // directly: this is what proves the seam returns the *assembled* scheme.
    const imfw::Scheme& scheme = imfw::scheme_instance();

    check(std::wcscmp(scheme.toolbar_label(), L"toy") == 0,
          "the seam resolves to the toy scheme");
    check(std::wcslen(scheme.ui_server_class()) <= 15,
          "UI server class fits imm32's 16-char buffer");
    check(std::wcscmp(scheme.ui_server_class(), scheme.composition_class()) != 0,
          "window classes are distinct");

    check(scheme.input_char('A') == U'a' && scheme.input_char('Z') == U'z',
          "letters map to lowercase");
    check(scheme.input_char('1') == 0 && scheme.input_char(' ') == 0,
          "digits and space are not input");
    check(scheme.is_input_char(U'a') && !scheme.is_input_char(U'0'),
          "input alphabet is a-z");

    std::unique_ptr<imfw::Engine> engine = scheme.create_engine();
    check(engine != nullptr, "create_engine returns an engine");
    if (engine == nullptr) return 1;

    imfw::Result result = engine->feed(U'a');
    check(result.action == imfw::Action::Absorb, "letters are absorbed");
    check(result.composition == "a", "composition mirrors the typed letters");
    check(result.segments.size() == 1, "one underline segment per letter");
    check(result.candidates.size() == 2, "two candidates per keystroke");

    result = engine->feed(U'b');
    check(result.composition == "ab" && result.segments.size() == 2,
          "composition grows");
    check(result.candidates.size() == 2 &&
              result.candidates[0].text == U"ab" &&
              result.candidates[1].text == U"ab!",
          "candidates are the text and the marked text");

    // Selecting a candidate: the host hands the *text* back (its list is
    // filtered, so indices mean nothing to the engine).
    const imfw::Result committed = engine->commit_text(result.candidates[1].text);
    check(committed.action == imfw::Action::Commit &&
              committed.commit_text == U"ab!",
          "commit_text emits the picked candidate");

    // Editing.
    result = engine->feed(U'c');
    check(result.composition == "abc", "composition before backspace");
    result = engine->backspace();
    check(result.action == imfw::Action::Absorb && result.composition == "ab",
          "backspace shortens the composition");
    result = engine->backspace();
    result = engine->backspace();
    check(result.composition.empty(), "backspace empties the composition");
    check(engine->backspace().action == imfw::Action::Passthrough,
          "backspace on an empty composition is not ours");
    check(engine->cancel().action == imfw::Action::Passthrough,
          "cancel produces no text");
    engine->on_select(true);

    if (failures == 0) {
        std::printf("toy scheme contract OK\n");
        return 0;
    }
    return 1;
}
