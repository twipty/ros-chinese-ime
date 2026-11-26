// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Non-Windows dat_source: host tests and any non-Windows build only ever see
// the built-in fallback table.
//
// On Windows the real read is host/dat_source_win.cpp, now a member of this
// same library (pinyin/CMakeLists.txt, X2-e2). It defines load_system_table()
// strongly, so the stub below is weak and yields to it instead of colliding.
//
// A Windows build also defines ROSPINYIN_REQUIRE_SYSTEM_TABLE, which compiles
// the stub out entirely. That is the point of the flag: while the stub was
// unconditional, "nobody compiled the real data source" produced a perfectly
// linkable .ime that quietly used the built-in table (measured: 942,989 B vs
// 948,481 B, and no other symptom). With the flag, the same mistake is an
// undefined reference to rospinyin::load_system_table().

#include <rospinyin/dat_source.hpp>

#if defined(__GNUC__) || defined(__clang__)
#define RPY_WEAK __attribute__((weak))
#else
#define RPY_WEAK
#endif

namespace rospinyin {

#if !defined(ROSPINYIN_REQUIRE_SYSTEM_TABLE)
// Missing, unreadable or invalid .dat is not an error: the built-in table keeps
// the IME usable on a bare installation (see dat_source.hpp).
RPY_WEAK TablePtr load_system_table() { return nullptr; }
#endif

TablePtr load_pinyin_table() {
    if (auto table = load_system_table()) return table;

#if defined(ROSPINYIN_REQUIRE_SYSTEM_TABLE)
    // M4 (owner 2026-11-26): in the release/Windows build the system table is
    // mandatory. A nullptr here means either the .dat or the now-required
    // word list failed validation — that is an explicit rejection, not a
    // reason to fall back to the built-in table (which has no phrases and
    // would silently recreate the "installed but no 你好" symptom).
    // Return an EMPTY table instead: the IME still loads and stays alive, but
    // produces no candidates. This is the intersection of the M1 no-table
    // contract (never crash) and the S0 "refuse to load" decision.
    return std::make_shared<imfw::CodeTable>();
#else
    // Host / diagnostic builds keep the built-in fallback unchanged so both
    // ctest configurations and the no-table abnormal-state test stay green.
    auto table = std::make_shared<imfw::CodeTable>();
    load_builtin_table(*table);
    return table;
#endif
}

}  // namespace rospinyin
