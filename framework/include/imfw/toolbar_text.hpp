// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// The toolbar's caption, composed from the Scheme.
//
// Why this is a separate, portable function: the toolbar window only exists
// inside the guest (GDI, a real HWND, a real font), so the one part of it that
// can be wrong in a way a user notices -- the exact text, "拼:开" vs "拼:关" --
// had no host-side test at all. Composing the string here keeps that decision
// out of <windows.h> and pins it in tests/toolbar_text_test.cpp.

#pragma once

#include <imfw/scheme.hpp>

#include <string>

namespace imfw {

// "拼:开" (open) / "拼:关" (closed), or the bare label when the scheme declares
// no state text. A scheme with no state is not an error: a shape IME with no
// Chinese/English mode simply shows "형" the whole time.
std::wstring toolbar_text(const Scheme& scheme, bool open);

// The same caption plus the session's glyph-mode suffix ("[快]" / "[全]").
// A separate function keeps the existing caption and its pinned host tests
// untouched; only the Win32 toolbar, which is where the mode is visible, opts
// in. `full_glyph` is ImeSessionState::full_glyph, NOT the Chinese/English
// `open` flag -- two independent booleans must not share a parameter slot.
std::wstring toolbar_text_with_mode(const Scheme& scheme, bool open,
                                    bool full_glyph);

}  // namespace imfw
