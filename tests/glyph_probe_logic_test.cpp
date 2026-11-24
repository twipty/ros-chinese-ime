// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// P1 verification stage: the pure "codepoint -> WenJin plane" mapping used by
// the guest GDI probe tool. No <windows.h>, no IME, no data file: this is a
// language-level fact about WenJin Mincho v2.100's three plane-split TTFs, so
// it is host-testable. The probe exe consumes the same header so the mapping
// shown in the guest matrix is the one pinned here.
//
// Plane coverage (WenJinMincho v2.100, measured against our 44,355-entry dat):
//   Plane 0 (P0): BMP basic CJK U+4E00-U+9FFF + Ext A U+3400-U+4DBF
//   Plane 2 (P2): Ext B U+20000-U+2A6DF, C/D/E/F/I up to U+2EE5D
//   Plane 3 (P3): Ext G U+30000-U+3134A, Ext H U+31350-U+323AF

#include <glyph_probe_logic.hpp>

#include <cstdio>
#include <cstdint>
#include <string>

namespace {

int failures = 0;

void expect(char32_t cp, glyph_probe::WenJinPlane got,
            glyph_probe::WenJinPlane want, const char* label) {
    if (got != want) {
        std::fprintf(stderr,
                     "glyph_probe_logic test failed: %s U+%04X "
                     "(got plane %d, expected %d)\n",
                     label, static_cast<unsigned>(cp),
                     static_cast<int>(got), static_cast<int>(want));
        ++failures;
    }
}

}  // namespace

int main() {
    using glyph_probe::WenJinPlane;
    using glyph_probe::pick_plane_for_codepoint;
    using glyph_probe::plane_face_name;

    // BMP basic + Ext A -> Plane 0
    expect(U'中', pick_plane_for_codepoint(U'中'), WenJinPlane::Plane0, "BMP 中");
    expect(0x4E00, pick_plane_for_codepoint(0x4E00), WenJinPlane::Plane0, "BMP start");
    expect(0x9FFF, pick_plane_for_codepoint(0x9FFF), WenJinPlane::Plane0, "BMP end");
    expect(0x3400, pick_plane_for_codepoint(0x3400), WenJinPlane::Plane0, "Ext A start");
    expect(0x4DBF, pick_plane_for_codepoint(0x4DBF), WenJinPlane::Plane0, "Ext A end");

    // Ext B..F/I -> Plane 2 (the [全] mode's main tofu range)
    expect(0x20000, pick_plane_for_codepoint(0x20000), WenJinPlane::Plane2, "Ext B start");
    expect(0x2A6DF, pick_plane_for_codepoint(0x2A6DF), WenJinPlane::Plane2, "Ext B end");
    expect(0x2A700, pick_plane_for_codepoint(0x2A700), WenJinPlane::Plane2, "Ext C start");
    expect(0x2B81E, pick_plane_for_codepoint(0x2B81E), WenJinPlane::Plane2, "Ext D (v2.100 new)");
    expect(0x2CEAF, pick_plane_for_codepoint(0x2CEAF), WenJinPlane::Plane2, "Ext E end");
    expect(0x2EE5D, pick_plane_for_codepoint(0x2EE5D), WenJinPlane::Plane2, "Ext I end");

    // Ext G/H -> Plane 3
    expect(0x30000, pick_plane_for_codepoint(0x30000), WenJinPlane::Plane3, "Ext G start");
    expect(0x3134A, pick_plane_for_codepoint(0x3134A), WenJinPlane::Plane3, "Ext G end");
    expect(0x31350, pick_plane_for_codepoint(0x31350), WenJinPlane::Plane3, "Ext H start");
    expect(0x323AF, pick_plane_for_codepoint(0x323AF), WenJinPlane::Plane3, "Ext H end");

    // Non-CJK and gaps -> None (no WenJin plane claims these)
    expect(U'A', pick_plane_for_codepoint(U'A'), WenJinPlane::None, "ASCII");
    expect(0x3000, pick_plane_for_codepoint(0x3000), WenJinPlane::None, "CJK punctuation");
    expect(0x4DC0, pick_plane_for_codepoint(0x4DC0), WenJinPlane::None, "Yijing gap after Ext A");
    expect(0x2EE5E, pick_plane_for_codepoint(0x2EE5E), WenJinPlane::None, "gap before Ext G");
    expect(0x323B0, pick_plane_for_codepoint(0x323B0), WenJinPlane::None, "beyond Ext H");
    expect(0x0000, pick_plane_for_codepoint(0x0000), WenJinPlane::None, "NUL");

    // The exact GDI family names the probe must query (measured from name table).
    if (std::wstring(plane_face_name(WenJinPlane::Plane0)) != L"WenJin Mincho Plane 0" ||
        std::wstring(plane_face_name(WenJinPlane::Plane2)) != L"WenJin Mincho Plane 2" ||
        std::wstring(plane_face_name(WenJinPlane::Plane3)) != L"WenJin Mincho Plane 3") {
        std::fprintf(stderr, "glyph_probe_logic test failed: GDI face names drifted\n");
        ++failures;
    }

    // UTF-16 encoding: Win32 wchar_t is 16-bit, so non-BMP codepoints MUST be
    // surrogate pairs. A plain static_cast<wchar_t>(cp) truncates U+20000 to
    // U+0000 (NUL) and makes every Ext B/C probe a false NONE. This is the
    // exact bug that produced the first guest result matrix -- pin the fix.
    using glyph_probe::encode_utf16;
    struct Utf16Case { std::uint32_t cp; int len; std::uint16_t u0; std::uint16_t u1; };
    const Utf16Case utf16_cases[] = {
        {0x4E2D,   1, 0x4E2D, 0x0000},  // BMP: one unit
        {0x3400,   1, 0x3400, 0x0000},  // Ext A is still BMP
        {0x20000,  2, 0xD840, 0xDC00},  // Ext B start -> surrogate pair
        {0x2A6E0,  2, 0xD869, 0xDEE0},  // Ext B
        {0x2B81E,  2, 0xD86E, 0xDC1E},  // Ext D
        {0x30000,  2, 0xD880, 0xDC00},  // Ext G start
        {0x31350,  2, 0xD884, 0xDF50},  // Ext H start
    };
    for (const auto& t : utf16_cases) {
        std::uint16_t units[2] = {0, 0};
        const int len = encode_utf16(t.cp, units);
        if (len != t.len || units[0] != t.u0 || units[1] != t.u1) {
            std::fprintf(stderr,
                         "glyph_probe_logic test failed: UTF-16 U+%04X "
                         "got len=%d units={%04X,%04X}, expected len=%d {%04X,%04X}\n",
                         static_cast<unsigned>(t.cp), len, units[0], units[1],
                         t.len, t.u0, t.u1);
            ++failures;
        }
        // Round-trip: decoding the pair must return the original codepoint.
        if (glyph_probe::decode_utf16(units, len) != t.cp) {
            std::fprintf(stderr, "glyph_probe_logic test failed: UTF-16 round-trip U+%04X\n",
                         static_cast<unsigned>(t.cp));
            ++failures;
        }
    }

    if (failures) {
        std::fprintf(stderr, "%d glyph_probe_logic check(s) failed\n", failures);
        return 1;
    }
    std::printf("glyph_probe_logic: all checks passed\n");
    return 0;
}
