// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// P1 verification stage: the pure "codepoint -> WenJin Mincho plane" mapping.
// No <windows.h> here on purpose -- the host CTest and the guest GDI probe
// compile this same header, so the mapping pinned by tests is the one the
// guest matrix actually uses.
//
// WenJin Mincho v2.100 ships three plane-split TTFs with DIFFERENT GDI family
// names (measured from each font's name table, nameID=1 platform=3):
//   WenJinMinchoP0-Regular.ttf -> "WenJin Mincho Plane 0"
//   WenJinMinchoP2-Regular.ttf -> "WenJin Mincho Plane 2"
//   WenJinMinchoP3-Regular.ttf -> "WenJin Mincho Plane 3"
// The probe exists to find out whether GDI auto-falls back across these names
// or whether the candidate window must SelectObject per plane.

#ifndef ROSPINYIN_GLYPH_PROBE_LOGIC_HPP
#define ROSPINYIN_GLYPH_PROBE_LOGIC_HPP

// P1 implementation stage: the plane mapping / family names moved to the
// framework's Windows-free layer so the product UI and this probe tool share
// ONE source of truth. Re-export them under the historical glyph_probe:: names
// so the guest tool and its pinned tests need no edits at all.
#include <imfw/glyph_plane.hpp>

#include <cstdint>

namespace glyph_probe {

using imfw::WenJinPlane;
using imfw::pick_plane_for_codepoint;
using imfw::plane_face_name;

// Win32 wchar_t is 16-bit: any codepoint above the BMP must be passed to GDI
// as a UTF-16 surrogate pair. Truncating with static_cast<wchar_t>(cp) turns
// U+20000 into U+0000 and silently makes every Ext B/C probe a false NONE.
// Returns the number of units written (1 for BMP, 2 for supplementary).
inline int encode_utf16(std::uint32_t cp, std::uint16_t* out) {
    if (cp <= 0xFFFF) {
        out[0] = static_cast<std::uint16_t>(cp);
        out[1] = 0;
        return 1;
    }
    const std::uint32_t v = cp - 0x10000;
    out[0] = static_cast<std::uint16_t>(0xD800 + (v >> 10));
    out[1] = static_cast<std::uint16_t>(0xDC00 + (v & 0x3FF));
    return 2;
}

// Inverse of encode_utf16, for the round-trip test in the host suite.
inline std::uint32_t decode_utf16(const std::uint16_t* units, int len) {
    if (len == 1) return units[0];
    if (len == 2 && units[0] >= 0xD800 && units[0] <= 0xDBFF &&
        units[1] >= 0xDC00 && units[1] <= 0xDFFF) {
        return 0x10000 + ((static_cast<std::uint32_t>(units[0]) - 0xD800) << 10) +
               (static_cast<std::uint32_t>(units[1]) - 0xDC00);
    }
    return 0xFFFFFFFFu;  // invalid sequence
}

// Single source of truth for the guest probe matrix. Both the Win32 GUI tool
// and the host test consume this table, so a test point can never drift out of
// sync between what is displayed and what is asserted.
struct ProbePoint {
    std::uint32_t cp;
    const wchar_t* label;
};

// Unicode 15.1 assigned CJK unified-ideograph ranges. A probe point outside
// these ranges is unassigned: TextOutW can still map it to some cmap entry, but
// the result is meaningless as a coverage signal (and often reads as NONE),
// which is exactly why U+2A6E0 / U+2B81E produced an unanswerable matrix.
inline bool is_assigned_cjk(std::uint32_t cp) {
    if (cp >= 0x4E00 && cp <= 0x9FFF) return true;   // basic
    if (cp >= 0x3400 && cp <= 0x4DBF) return true;   // Ext A
    if (cp >= 0x20000 && cp <= 0x2A6DF) return true; // Ext B
    if (cp >= 0x2A700 && cp <= 0x2B73F) return true; // Ext C
    if (cp >= 0x2B740 && cp <= 0x2B81D) return true; // Ext D
    if (cp >= 0x2B820 && cp <= 0x2CEAF) return true; // Ext E
    if (cp >= 0x2CEB0 && cp <= 0x2EBEF) return true; // Ext F
    if (cp >= 0x2EBF0 && cp <= 0x2EE5D) return true; // Ext I
    if (cp >= 0x30000 && cp <= 0x3134A) return true; // Ext G
    if (cp >= 0x31350 && cp <= 0x323AF) return true; // Ext H
    return false;
}

inline constexpr ProbePoint kProbePoints[] = {
    {0x4E2D,  L"U+4E2D BMP 中"},
    {0x3400,  L"U+3400 ExtA start"},
    {0x20000, L"U+20000 ExtB start"},
    {0x2A700, L"U+2A700 ExtC start"},
    {0x2B740, L"U+2B740 ExtD start"},
    {0x30000, L"U+30000 ExtG start"},
    {0x31350, L"U+31350 ExtH start"},
};

// Verdict for the extent-based probe (the non-BMP-capable replacement for
// GetGlyphIndicesW). Unknown is distinct from NoGlyph: it means "the API used
// cannot answer for this codepoint", never "the font lacks the glyph".
enum class ProbeVerdict : std::uint8_t {
    HasGlyph = 0,
    NoGlyph = 1,
    Unknown = 2,
};

// Decide a glyph-availability verdict from two rendered pixel buffers.
//
// Advance width is NOT a usable signal for WenJin Mincho: it is monospaced, so
// every glyph including .notdef advances 1000/1000 em. The reliable signal is
// the rendered pixels themselves. The tool renders the codepoint under test
// and a known-absent noncharacter (U+10FFFF) with the same font into two equally
// sized buffers; this function compares them.
//
//   a, b              - pixel buffers (raw BITMAPINFO RGB bytes) of equal size
//   len               - number of bytes to compare; 0 => Unknown
//   surrogate_capable - true when the render path combines UTF-16 surrogate
//                       pairs (TextOutW on ReactOS does, per freetype.c:6949).
//
// Identical pixels => the codepoint rendered as .notdef => NoGlyph.
// Any differing pixel => a real glyph was drawn => HasGlyph.
inline ProbeVerdict render_verdict(const std::uint8_t* a, const std::uint8_t* b,
                                   std::size_t len, bool surrogate_capable) {
    if (!surrogate_capable || len == 0 || !a || !b) return ProbeVerdict::Unknown;
    for (std::size_t i = 0; i < len; ++i) {
        if (a[i] != b[i]) return ProbeVerdict::HasGlyph;
    }
    return ProbeVerdict::NoGlyph;
}

}  // namespace glyph_probe

#endif  // ROSPINYIN_GLYPH_PROBE_LOGIC_HPP
