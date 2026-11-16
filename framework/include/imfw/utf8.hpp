// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// UTF-8 support for engine output (framework gap G4, connected conclusion).
// Engines used to be effectively limited to ASCII compositions because the
// host converted composition bytes to wide chars one byte at a time. With
// this module the contract is: Result::composition and Result::segments are
// UTF-8 (ASCII input works unchanged), and the composition window can render
// any text — e.g. a shape IME showing its code's character inline.
//
// Invalid bytes never throw or drop data: each malformed byte is mapped to
// its own code point (the historical byte-wise behaviour), so a truncated or
// binary-ish composition still renders instead of vanishing.
//
// Windows-free on purpose so Linux CI unit-tests it with everything else.

#pragma once

#include <cstddef>
#include <string>

namespace imfw {

// Decode UTF-8 into the platform's wchar_t string. On Windows (16-bit
// wchar_t, UTF-16) non-BMP code points become surrogate pairs; on wide-hosts
// (32-bit wchar_t) each code point maps directly. Invalid bytes map to
// U+0000..U+00FF one-to-one.
std::wstring utf8_to_wide(const std::string& utf8);

// Number of code points in a UTF-8 string, with invalid bytes counted
// individually. Cheap sanity helper for engines and tests.
std::size_t utf8_length(const std::string& utf8);

}  // namespace imfw
