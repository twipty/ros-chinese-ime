// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Lazy data loading for the host side. The scheme's .dat file is an optional,
// separately updatable file in the system directory; when it is missing or
// corrupt the scheme's fallback table keeps the IME usable on a bare
// installation.

#pragma once

#include <imfw/code_table.hpp>
#include <imfw/scheme.hpp>

namespace imfw {

// Try <SystemDirectory>\<dat_filename>. Returns true when at least one entry
// was loaded. Never called from DllMain.
bool load_system_dat(CodeTable& out, const wchar_t* dat_filename);

// Preferred entry point: try the scheme's dat file, then its fallback.
// Returns true when the real dat was used.
bool load_ime_table(CodeTable& out, const Scheme& scheme);

}  // namespace imfw
