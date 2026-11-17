// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Scheme-owned data loading (framework gap G2, framework-side disposition:
// the framework no longer loads, holds or passes a code table — the scheme
// owns its data end to end).
//
// Two layers:
//   * load_builtin_table()    — platform-independent, fills the embedded
//                               fallback table (pinyin data lives in the
//                               plugin, not the framework).
//   * load_system_table()     — reads <SystemDirectory>\rospinyin.dat and
//                               parses it through the generic imfw::CodeTable.
//                               The file read needs Win32, so the
//                               Windows-only implementation lives in
//                               pinyin/host/dat_source_win.cpp (compiled into
//                               the assembled .ime); host tests and any
//                               non-Windows build fall back to the weak
//                               definition below in pinyin/src/dat_source.cpp.
//
// Loading strategy: prefer the .dat; if it is missing or corrupt, fall back
// to the built-in table so the IME stays usable on a bare installation.

#pragma once

#include <imfw/code_table.hpp>

#include <memory>

namespace rospinyin {

// Read-only view of the scheme's current data. Reloads replace the pointer;
// sessions created earlier keep their shared reference alive.
using TablePtr = std::shared_ptr<const imfw::CodeTable>;

// Fill `out` with the embedded fallback table. Always succeeds (the table is
// compiled in). Defined in pinyin/src/scheme.cpp next to the data.
void load_builtin_table(imfw::CodeTable& out);

// Try the system-directory .dat (Windows implementation in
// pinyin/host/dat_source_win.cpp). Returns nullptr when the file is missing,
// unreadable or invalid.
TablePtr load_system_table();

// The scheme's load path: .dat first, builtin fallback otherwise.
TablePtr load_pinyin_table();

}  // namespace rospinyin
