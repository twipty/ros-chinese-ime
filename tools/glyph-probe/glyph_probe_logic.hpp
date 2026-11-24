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

#include <cstdint>

namespace glyph_probe {

enum class WenJinPlane : std::uint8_t {
    None = 0,
    Plane0 = 1,  // BMP basic CJK + Ext A
    Plane2 = 2,  // Ext B/C/D/E/F/I
    Plane3 = 3,  // Ext G/H
};

// Which WenJin plane carries the given codepoint, or None if the codepoint is
// outside WenJin's CJK coverage (ASCII, punctuation, unassigned gaps).
inline WenJinPlane pick_plane_for_codepoint(std::uint32_t cp) {
    // Basic multilingual plane: CJK unified ideographs + Ext A.
    if ((cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF)) {
        return WenJinPlane::Plane0;
    }
    // Supplementary plane 2: Ext B (0x20000-0x2A6DF), C (0x2A700-0x2B73F),
    // D (0x2B740-0x2B81F), E (0x2B820-0x2CEAF), F (0x2CEB0-0x2EBEF),
    // I (0x2EBF0-0x2EE5D). Contiguous in WenJin's cmap, so one range covers it.
    if (cp >= 0x20000 && cp <= 0x2EE5D) {
        return WenJinPlane::Plane2;
    }
    // Supplementary plane 3: Ext G (0x30000-0x3134A), H (0x31350-0x323AF).
    if (cp >= 0x30000 && cp <= 0x323AF) {
        return WenJinPlane::Plane3;
    }
    return WenJinPlane::None;
}

// Exact GDI family name for a plane; nullptr for None. These strings are
// language facts pinned by the host test -- do not "simplify" to one family.
inline const wchar_t* plane_face_name(WenJinPlane plane) {
    switch (plane) {
        case WenJinPlane::Plane0: return L"WenJin Mincho Plane 0";
        case WenJinPlane::Plane2: return L"WenJin Mincho Plane 2";
        case WenJinPlane::Plane3: return L"WenJin Mincho Plane 3";
        case WenJinPlane::None:
        default: return nullptr;
    }
}

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
