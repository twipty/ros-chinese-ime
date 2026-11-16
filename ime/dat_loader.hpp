// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Lazy data loading for the IME side. rospinyin.dat is an optional, separately
// updatable file; when it is missing or corrupt we fall back to a compact
// built-in table so the IME remains usable in a bare ReactOS installation.

#pragma once

#include <rospinyin/table.hpp>

namespace rospinyin {

// Try <SystemDirectory>\rospinyin.dat. Returns true when at least one entry
// was loaded. Never called from DllMain.
bool load_system_dat(PinyinTable& out);

// A small but useful table of common syllables/characters used as a fallback
// when rospinyin.dat is unavailable. Ranked by approximate frequency.
void load_builtin_table(PinyinTable& out);

// Preferred entry point: try the dat file, then the built-in table.
// Returns true when the real dat was used.
bool load_ime_table(PinyinTable& out);

}  // namespace rospinyin
