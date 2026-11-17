// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Non-Windows dat_source: host tests and any non-Windows build only ever see
// the built-in fallback table. On Windows the assembled .ime instead links
// pinyin/host/dat_source_win.cpp, which provides the real system-directory
// read; these definitions are weak so they silently yield to it.

#include <rospinyin/dat_source.hpp>

#if defined(__GNUC__) || defined(__clang__)
#define RPY_WEAK __attribute__((weak))
#else
#define RPY_WEAK
#endif

namespace rospinyin {

RPY_WEAK TablePtr load_system_table() { return nullptr; }

TablePtr load_pinyin_table() {
    if (auto table = load_system_table()) return table;
    auto table = std::make_shared<imfw::CodeTable>();
    load_builtin_table(*table);
    return table;
}

}  // namespace rospinyin
