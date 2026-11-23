// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// PinyinScheme contract tests: the plugin interface (imfw/scheme.hpp) is the
// seam where future input methods plug into the framework, so its behaviour
// is pinned here on the host.

#include <imfw/code_table.hpp>
#include <imfw/engine.hpp>
#include <imfw/punctuation.hpp>
#include <rospinyin/dat_source.hpp>
#include <rospinyin/pinyin_session.hpp>
#include <rospinyin/scheme.hpp>

#include <cstdio>
#include <memory>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "scheme test failed: %s\n", what);
        ++failures;
    }
}

}  // namespace

int main() {
    const rospinyin::PinyinScheme scheme;

    // Identity and resources. dat_filename/load_fallback are gone: data
    // ownership moved fully into the schemes (framework gap G2 disposition).
    check(std::wcslen(scheme.ui_server_class()) > 0, "ui server class set");
    check(std::wcslen(scheme.composition_class()) > 0, "composition class set");
    check(std::wcslen(scheme.candidate_class()) > 0, "candidate class set");
    check(std::wcslen(scheme.toolbar_class()) > 0, "toolbar class set");
    check(std::wcscmp(scheme.toolbar_label(), L"拼") == 0, "toolbar label 拼");
    check(std::wcscmp(scheme.ui_server_class(), scheme.composition_class()) != 0,
          "window classes must be distinct");

    // Key policy: letters pass through lowercased, the apostrophe key is the
    // syllable separator, framework keys (digits/space/paging) are not input.
    check(scheme.input_char('A') == U'a' && scheme.input_char('Z') == U'z',
          "uppercase virtual keys map to lowercase");
    check(scheme.input_char('a') == U'a', "lowercase letters accepted");
    check(scheme.input_char(0xDE) == U'\'', "VK_OEM_7 maps to apostrophe");
    check(scheme.input_char('1') == 0, "digits are not pinyin input");
    check(scheme.input_char(' ') == 0, "space is not pinyin input");
    check(scheme.is_input_char(U'a') && scheme.is_input_char(U'\''),
          "a-z and apostrophe are input chars");
    check(!scheme.is_input_char(U'0') && !scheme.is_input_char(U' '),
          "digits and space are not input chars");

    // G4: selection / paging key policy defaults (a scheme may override).
    check(scheme.is_selection_key(U' ') && scheme.is_selection_key(U'1') &&
              scheme.is_selection_key(U'9'),
          "space and digits 1-9 are selection keys by default");
    check(!scheme.is_selection_key(U'0') && !scheme.is_selection_key(U'-'),
          "0 and - are not selection keys");
    check(scheme.page_key(U'-') == -1 && scheme.page_key(U',') == -1,
          "- and , page back by default");
    check(scheme.page_key(U'=') == 1 && scheme.page_key(U'.') == 1,
          "= and . page forward by default");
    check(scheme.page_key(U' ') == 0 && scheme.page_key(U'x') == 0,
          "non-paging keys report 0");
    check(!scheme.is_selection_key(0) && scheme.page_key(0) == 0,
          "null key reports neither selection nor paging");

    // M3: PinyinScheme enables Chinese punctuation by forwarding to the ONE
    // shared table. Asserting equality with the public function (rather than
    // hardcoding mappings again) is the anti-fork guard: if the plugin ever
    // grows its own private table, this test fails.
    check(scheme.punctuation(U',') == imfw::fullwidth_punctuation(U',') &&
              scheme.punctuation(U',') == std::u32string(1, U'\uFF0C'),
          "scheme punctuation forwards the shared table (comma)");
    check(scheme.punctuation(U'.') == std::u32string(1, U'\u3002') &&
              scheme.punctuation(U'?') == std::u32string(1, U'\uFF1F'),
          "scheme punctuation forwards the shared table (period/question)");
    check(scheme.punctuation(U'^') == std::u32string(U"\u2026\u2026"),
          "scheme punctuation forwards the multi-codepoint ellipsis");
    check(scheme.punctuation(U'1').empty() &&
              scheme.punctuation(U' ').empty(),
          "scheme leaves digits and space to the framework");

    // G3: the configure hook defaults to declining (ImeConfigure then keeps
    // returning FALSE); schemes with configuration UI override it.
    check(!scheme.configure(nullptr),
          "configure() declines by default");

    // G2 final disposition: the scheme owns its data (dat_filename /
    // load_fallback are gone from the contract). Engines are built with no
    // table argument against the scheme's own table; the engine must reject
    // input until selected (the imm_entry regression guard depends on this
    // contract).
    std::unique_ptr<imfw::Engine> engine = scheme.create_engine();
    check(engine != nullptr, "create_engine (no table argument) returns an engine");
    check(engine->feed(U'n').action == imfw::Action::Passthrough,
          "engine must passthrough before on_select");
    engine->on_select(true);
    check(engine->feed(U'n').action == imfw::Action::Absorb,
          "engine must absorb input after on_select");

    // The builtin fallback loader is also directly testable. Under the
    // BUILTIN_TABLE=OFF release configuration the loader exists but empties
    // the table (scheme.cpp #else branch), so the coverage assertions only
    // hold when the table is actually compiled in — hence the guard.
#if defined(ROSPINYIN_BUILTIN_TABLE)
    imfw::CodeTable fallback;
    rospinyin::load_builtin_table(fallback);
    check(fallback.char_count() > 100, "fallback table covers common characters");
    check(fallback.is_code("nv") && fallback.is_code("lv") && fallback.is_code("lve"),
          "v-key syllables present in fallback table");
    check(fallback.is_prefix("zho"), "fallback table exposes prefixes");
#endif

    // Reload swaps in a fresh table generation without breaking anything.
    scheme.reload_data();
    std::unique_ptr<imfw::Engine> reloaded = scheme.create_engine();
    check(reloaded != nullptr, "engine builds after reload_data");
    reloaded->on_select(true);
    check(reloaded->feed(U'n').action == imfw::Action::Absorb,
          "engine works after reload_data");

    if (failures == 0) {
        std::printf("pinyin scheme contract OK\n");
        return 0;
    }
    return 1;
}
