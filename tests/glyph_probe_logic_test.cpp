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
    expect(0x2B740, pick_plane_for_codepoint(0x2B740), WenJinPlane::Plane2, "Ext D start");
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
        {0x2A700,  2, 0xD869, 0xDF00},  // Ext C start (assigned; replaced U+2A6E0 gap)
        {0x2B740,  2, 0xD86D, 0xDF40},  // Ext D start (assigned; replaced U+2B81E gap)
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

    // Render-based verdict for non-BMP: GetGlyphIndicesW does NOT combine
    // surrogate pairs on ReactOS (freetype.c:7642), and advance width cannot
    // distinguish glyphs either because WenJin is monospaced (every glyph,
    // including .notdef, has advance 1000/1000 em). The only reliable signal is
    // PIXELS: render the codepoint and a known-absent noncharacter (U+10FFFF)
    // with the same font into equally-sized buffers; if the pixels are
    // identical the codepoint fell back to .notdef (no glyph), otherwise a
    // real glyph was drawn. Pin the pixel-comparison + verdict rule as a host
    // pure function (Win32 rendering stays in the GUI tool).
    using glyph_probe::render_verdict;
    using glyph_probe::ProbeVerdict;
    {
        const std::uint8_t a[4] = {0, 0, 0, 0};  // blank / .notdef render
        const std::uint8_t same[4] = {0, 0, 0, 0};
        const std::uint8_t diff[4] = {0, 1, 0, 0};
        const bool capable = true;  // TextOutW combines pairs on ReactOS
        if (render_verdict(a, same, 4, capable) != ProbeVerdict::NoGlyph) {
            std::fprintf(stderr, "glyph_probe_logic test failed: identical pixels must be NoGlyph\n");
            ++failures;
        }
        if (render_verdict(a, diff, 4, capable) != ProbeVerdict::HasGlyph) {
            std::fprintf(stderr, "glyph_probe_logic test failed: differing pixels must be HasGlyph\n");
            ++failures;
        }
        if (render_verdict(a, a, 4, false) != ProbeVerdict::Unknown) {
            std::fprintf(stderr, "glyph_probe_logic test failed: non-surrogate-capable must be Unknown\n");
            ++failures;
        }
        // Empty/zero-length render is unusable -> Unknown, never NoGlyph.
        if (render_verdict(a, same, 0, capable) != ProbeVerdict::Unknown) {
            std::fprintf(stderr, "glyph_probe_logic test failed: empty buffer must be Unknown\n");
            ++failures;
        }
        // A different pixel beyond the first word still counts.
        const std::uint8_t diff_late[4] = {0, 0, 0, 9};
        if (render_verdict(a, diff_late, 4, capable) != ProbeVerdict::HasGlyph) {
            std::fprintf(stderr, "glyph_probe_logic test failed: must scan whole buffer\n");
            ++failures;
        }
    }

    // Probe matrix points must all be Unicode-assigned CJK codepoints. An
    // unassigned point (e.g. U+2A6E0 in the ExtB/ExtC gap, U+2B81E past ExtD's
    // end) yields a meaningless NONE and makes AC1(a) unanswerable. Pin both
    // directions: real extension starts are assigned, the old gap points are not.
    using glyph_probe::is_assigned_cjk;
    using glyph_probe::kProbePoints;
    for (const auto& p : kProbePoints) {
        if (!is_assigned_cjk(p.cp)) {
            std::fprintf(stderr,
                         "glyph_probe_logic test failed: probe point U+%04X is not an "
                         "assigned CJK codepoint (label: %ls)\n",
                         static_cast<unsigned>(p.cp), p.label);
            ++failures;
        }
    }
    if (!is_assigned_cjk(0x2A700) || !is_assigned_cjk(0x2B740)) {
        std::fprintf(stderr, "glyph_probe_logic test failed: ExtC/ExtD starts must be assigned\n");
        ++failures;
    }
    if (is_assigned_cjk(0x2A6E0) || is_assigned_cjk(0x2B81E)) {
        std::fprintf(stderr, "glyph_probe_logic test failed: old gap points must read unassigned\n");
        ++failures;
    }

    if (failures) {
        std::fprintf(stderr, "%d glyph_probe_logic check(s) failed\n", failures);
        return 1;
    }
    std::printf("glyph_probe_logic: all checks passed\n");
    return 0;
}
