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

}  // namespace glyph_probe

#endif  // ROSPINYIN_GLYPH_PROBE_LOGIC_HPP
