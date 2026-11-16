// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// PinyinScheme contract tests: the plugin interface (imfw/scheme.hpp) is the
// seam where future input methods plug into the framework, so its behaviour
// is pinned here on the host.

#include <imfw/code_table.hpp>
#include <imfw/engine.hpp>
#include <rospinyin/pinyin_session.hpp>
#include <rospinyin/scheme.hpp>

#include <cstdio>
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

    // Identity and resources.
    check(std::wcscmp(scheme.dat_filename(), L"rospinyin.dat") == 0,
          "dat filename must be rospinyin.dat");
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

    // G3: the configure hook defaults to declining (ImeConfigure then keeps
    // returning FALSE); schemes with configuration UI override it.
    check(!scheme.configure(nullptr),
          "configure() declines by default");

    // G2 documentation contract: a scheme that owns its data returns a
    // dat filename; the pinyin scheme uses the framework dat.
    check(scheme.dat_filename() != nullptr, "pinyin uses the framework dat");

    // Engine factory: the engine must reject input until selected (the
    // imm_entry regression guard depends on this contract).
    imfw::CodeTable table;
    check(scheme.load_fallback(table), "fallback table must load");
    check(table.char_count() > 100, "fallback table covers common characters");
    check(table.is_code("nv") && table.is_code("lv") && table.is_code("lve"),
          "v-key syllables present in fallback table");
    check(table.is_prefix("zho"), "fallback table exposes prefixes");

    std::unique_ptr<imfw::Engine> engine = scheme.create_engine(table);
    check(engine != nullptr, "create_engine returns an engine");
    check(engine->feed(U'n').action == imfw::Action::Passthrough,
          "engine must passthrough before on_select");
    engine->on_select(true);
    check(engine->feed(U'n').action == imfw::Action::Absorb,
          "engine must absorb input after on_select");

    if (failures == 0) {
        std::printf("pinyin scheme contract OK\n");
        return 0;
    }
    return 1;
}
