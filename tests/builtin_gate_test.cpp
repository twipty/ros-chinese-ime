// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// builtin_gate_test: the missing-.dat abnormal state, pinned.
//
// Decision 2026-11-19 (21:2x correction): release builds compile without the
// built-in fallback table (ROSPINYIN_BUILTIN_TABLE=OFF). The installer's
// /step1 copies .ime + .dat as one size-verified payload, so on the supported
// path a missing .dat means a failed install. The design goal for that state
// is *simplicity*: nothing here is worth a special code path beyond keeping
// the session alive. What is pinned:
//
//   1. letters accumulate in the composition — the user sees what they typed
//      at the caret instead of the IME silently eating keys;
//   2. the candidate list stays empty — there is nothing to choose from;
//   3. digit / space are no-ops — nothing to select, nothing committed;
//   4. Backspace shrinks and Esc clears — those are the two exits;
//   5. no crash, no hang, engine keeps answering; deactivate/reselect does
//      not wedge it either.
//
// The empty table here is built by hand (fresh CodeTable + build_index)
// rather than through load_builtin_table(): that keeps the test honest in
// both configurations — with the switch ON it still pins the empty-table
// contract, which is the reference behaviour release builds ship.
//
// No <windows.h>, no IMM32, no GUI — same rules as the other engine tests.

#include <rospinyin/pinyin_session.hpp>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "builtin gate test failed: %s\n", what);
        ++failures;
    }
}

// The release missing-.dat table: empty, but a fully valid CodeTable.
// Built by hand (add nothing, then index) rather than through
// load_builtin_table(): that symbol only exists under the BUILTIN_TABLE
// configuration, and this test must pin the empty-table behaviour in *both*
// configurations (ON builds must pass it too).
std::shared_ptr<const imfw::CodeTable> empty_table() {
    auto table = std::make_shared<imfw::CodeTable>();
    table->build_index();
    return table;
}

}  // namespace

int main() {
    auto table = empty_table();
    check(table->char_count() == 0, "gate table is empty");
    check(table->lookup("de").empty(), "gate table lookup is empty");
    check(!table->is_prefix("zh"), "gate table has no prefixes");

    rospinyin::PinyinSession session(table);

    // 1. Letters accumulate in the composition (decision 21:2x: show what
    //    the user typed instead of eating keys); candidates stay empty.
    session.on_select(true);
    for (char32_t cp = U'a'; cp <= U'z'; ++cp) {
        (void)session.feed(cp);
    }
    (void)session.feed(U'\'');
    for (char32_t cp = U'a'; cp <= U'z'; ++cp) {
        (void)session.feed(cp);
    }
    check(session.composition() ==
              "abcdefghijklmnopqrstuvwxyz'abcdefghijklmnopqrstuvwxyz",
          "letters accumulate in composition on an empty table");
    check(session.candidates().empty(),
          "candidate list stays empty on an empty table");

    // 2. Digit / space over zero candidates are no-ops: nothing selected,
    //    nothing committed, composition untouched.
    (void)session.feed(U'2');
    (void)session.feed(U' ');
    check(session.candidates().empty(),
          "digit/space over zero candidates select nothing");
    check(session.composition() ==
              "abcdefghijklmnopqrstuvwxyz'abcdefghijklmnopqrstuvwxyz",
          "digit/space over zero candidates commit nothing");

    // 3. Backspace shrinks, Esc clears — the two exits both work.
    (void)session.backspace();
    check(session.composition().size() == 52,
          "backspace removes one letter");
    (void)session.cancel();
    check(session.composition().empty(), "Esc clears the composition");

    // 4. Engine keeps answering after the gauntlet: one more type/clear.
    (void)session.feed(U'n');
    check(session.composition() == "n",
          "engine still answers after the whole gauntlet");
    (void)session.cancel();

    // 5. Deactivate/reselect cycle: the release path where imm32 flips fOpen
    //    while the .dat is missing must not wedge the session either.
    session.on_select(false);
    (void)session.feed(U'x');  // passthrough expected; must simply return
    session.on_select(true);
    check(session.composition().empty(),
          "reselect starts a fresh composition");

    if (failures == 0) {
        std::printf("builtin gate OK\n");
        return 0;
    }
    return 1;
}
