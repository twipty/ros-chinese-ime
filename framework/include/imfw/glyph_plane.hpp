// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// P1 implementation stage: the pure "codepoint -> WenJin plane -> text runs"
// logic shared by the Win32 UI windows. No <windows.h> on purpose -- char16_t
// is 16-bit on every host while wchar_t is 16-bit only on Win32, so pinning the
// segmentation here lets tests/glyph_plane_test.cpp verify the exact runs the
// candidate / composition windows will draw.
//
// WenJin Mincho v2.100 ships three plane-split TTFs with DIFFERENT GDI family
// names (measured from each font's name table, nameID=1 platform=3):
//   WenJinMinchoP0-Regular.ttf -> "WenJin Mincho Plane 0"
//   WenJinMinchoP2-Regular.ttf -> "WenJin Mincho Plane 2"
//   WenJinMinchoP3-Regular.ttf -> "WenJin Mincho Plane 3"
// The P1 verification stage proved GDI does NOT fall back across these names,
// so the UI must pick the plane explicitly per glyph run.

#ifndef ROSPINYIN_IMFW_GLYPH_PLANE_HPP
#define ROSPINYIN_IMFW_GLYPH_PLANE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace imfw {

enum class WenJinPlane : std::uint8_t {
    None = 0,
    Plane0 = 1,  // BMP basic CJK + Ext A
    Plane2 = 2,  // Ext B/C/D/E/F/I
    Plane3 = 3,  // Ext G/H
};

// Which WenJin plane carries the given codepoint, or None if the codepoint is
// outside WenJin's CJK coverage (ASCII, punctuation, unassigned gaps).
inline WenJinPlane pick_plane_for_codepoint(std::uint32_t cp) {
    if ((cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF)) {
        return WenJinPlane::Plane0;
    }
    if (cp >= 0x20000 && cp <= 0x2EE5D) {
        return WenJinPlane::Plane2;
    }
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

// One contiguous stretch of UTF-16 units that all resolve to the same plane.
// offset/length are UTF-16 code-unit indices into the input string (a
// supplementary code point therefore has length 2), matching what TextOutW
// consumes on Win32.
struct GlyphRun {
    std::size_t offset = 0;
    std::size_t length = 0;
    WenJinPlane plane = WenJinPlane::None;
};

// Split a UTF-16 string into runs of constant plane. Consecutive codepoints on
// the same plane coalesce so the UI selects each font once per run, not once
// per glyph. Surrogate pairs are decoded as one codepoint before the plane
// lookup; an unpaired surrogate is treated as None (baseline font) rather than
// crashing or silently truncating.
inline std::vector<GlyphRun> split_into_runs(const std::u16string& text) {
    std::vector<GlyphRun> runs;
    std::size_t i = 0;
    while (i < text.size()) {
        const std::uint16_t unit = text[i];
        std::uint32_t cp = unit;
        std::size_t units = 1;

        if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < text.size()) {
            const std::uint16_t low = text[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000u +
                     ((static_cast<std::uint32_t>(unit) - 0xD800u) << 10) +
                     (static_cast<std::uint32_t>(low) - 0xDC00u);
                units = 2;
            }
        }

        const WenJinPlane plane = pick_plane_for_codepoint(cp);
        if (!runs.empty() && runs.back().plane == plane) {
            runs.back().length += units;
        } else {
            runs.push_back(GlyphRun{i, units, plane});
        }
        i += units;
    }
    return runs;
}

}  // namespace imfw

#endif  // ROSPINYIN_IMFW_GLYPH_PLANE_HPP
