// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// P1 implementation stage: the pure "UTF-16 text -> per-plane runs" logic the
// candidate / composition windows use to pick a WenJin plane font per glyph.
// Windows-free on purpose (char16_t is 16-bit everywhere, unlike wchar_t which
// is 16-bit on Win32 but 32-bit on this Linux host), so the exact segmentation
// decision is pinned here and the Win32 UI just consumes it.

#include <imfw/glyph_plane.hpp>

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* label) {
    if (!ok) {
        std::fprintf(stderr, "glyph_plane test failed: %s\n", label);
        ++failures;
    }
}

// Append a codepoint as UTF-16 units the way Win32 wchar_t would hold it.
void append_cp(std::u16string& out, std::uint32_t cp) {
    if (cp <= 0xFFFF) {
        out.push_back(static_cast<char16_t>(cp));
        return;
    }
    const std::uint32_t v = cp - 0x10000u;
    out.push_back(static_cast<char16_t>(0xD800u + (v >> 10)));
    out.push_back(static_cast<char16_t>(0xDC00u + (v & 0x3FFu)));
}

// plane_face_name returns wchar_t* (what Win32 GDI consumes). On this Linux
// host wchar_t is 32-bit; the face names are ASCII, so widen each unit into a
// char16_t string for comparison without relying on a platform-specific cast.
std::u16string face_as_u16(const wchar_t* name) {
    std::u16string out;
    if (name == nullptr) return out;
    for (const wchar_t* p = name; *p != L'\0'; ++p) {
        out.push_back(static_cast<char16_t>(*p));
    }
    return out;
}

void expect_runs(const std::u16string& text,
                 const std::vector<imfw::GlyphRun>& want,
                 const char* label) {
    const auto got = imfw::split_into_runs(text);
    if (got.size() != want.size()) {
        std::fprintf(stderr,
                     "glyph_plane test failed: %s run count got %zu expected %zu\n",
                     label, got.size(), want.size());
        ++failures;
        return;
    }
    for (std::size_t i = 0; i < got.size(); ++i) {
        if (got[i].offset != want[i].offset || got[i].length != want[i].length ||
            got[i].plane != want[i].plane) {
            std::fprintf(stderr,
                         "glyph_plane test failed: %s run[%zu] "
                         "got(off=%zu,len=%zu,plane=%d) "
                         "want(off=%zu,len=%zu,plane=%d)\n",
                         label, i, got[i].offset, got[i].length,
                         static_cast<int>(got[i].plane), want[i].offset,
                         want[i].length, static_cast<int>(want[i].plane));
            ++failures;
            return;
        }
    }
}

}  // namespace

int main() {
    using imfw::WenJinPlane;

    // The plane mapping itself is unchanged from the verification stage; pin
    // the boundaries here too so the product-side head cannot drift.
    check(imfw::pick_plane_for_codepoint(U'中') == WenJinPlane::Plane0, "BMP 中 -> P0");
    check(imfw::pick_plane_for_codepoint(0x3400) == WenJinPlane::Plane0, "ExtA -> P0");
    check(imfw::pick_plane_for_codepoint(0x20000) == WenJinPlane::Plane2, "ExtB -> P2");
    check(imfw::pick_plane_for_codepoint(0x2A700) == WenJinPlane::Plane2, "ExtC -> P2");
    check(imfw::pick_plane_for_codepoint(0x2B740) == WenJinPlane::Plane2, "ExtD -> P2");
    check(imfw::pick_plane_for_codepoint(0x30000) == WenJinPlane::Plane3, "ExtG -> P3");
    check(imfw::pick_plane_for_codepoint(0x31350) == WenJinPlane::Plane3, "ExtH -> P3");
    check(imfw::pick_plane_for_codepoint(L'A') == WenJinPlane::None, "ASCII -> None");

    // Family names are language facts; a typo here silently selects the wrong
    // (or a substituted) font on the guest.
    check(face_as_u16(imfw::plane_face_name(WenJinPlane::Plane0)) ==
              u"WenJin Mincho Plane 0",
          "P0 face name");
    check(face_as_u16(imfw::plane_face_name(WenJinPlane::Plane2)) ==
              u"WenJin Mincho Plane 2",
          "P2 face name");
    check(face_as_u16(imfw::plane_face_name(WenJinPlane::Plane3)) ==
              u"WenJin Mincho Plane 3",
          "P3 face name");
    check(imfw::plane_face_name(WenJinPlane::None) == nullptr, "None has no face");

    // One plain BMP line: the "1. " numbering is None (baseline font), the
    // candidate glyph is Plane0. Two runs, contiguous offsets.
    expect_runs(u"1. \u4E2D",
                {{0, 3, WenJinPlane::None}, {3, 1, WenJinPlane::Plane0}},
                "numbering + BMP candidate");

    // A supplementary-plane glyph occupies TWO UTF-16 units but is ONE code
    // point; the run length must count units (2) while the plane decision uses
    // the decoded codepoint (Plane2).
    std::u16string extb;
    append_cp(extb, 0x20000);
    expect_runs(extb, {{0, 2, WenJinPlane::Plane2}}, "ExtB surrogate pair");

    // Mixed line: ASCII prefix, one Plane2 surrogate pair, one BMP glyph.
    std::u16string mixed = u"A";
    append_cp(mixed, 0x2A700);  // ExtC -> P2
    mixed.push_back(u'\u4E2D');  // BMP -> P0
    expect_runs(mixed,
                {{0, 1, WenJinPlane::None},
                 {1, 2, WenJinPlane::Plane2},
                 {3, 1, WenJinPlane::Plane0}},
                "mixed ASCII + ExtC + BMP");

    // Consecutive codepoints on the same plane coalesce into a single run, so
    // the UI selects each font once per run instead of once per glyph.
    expect_runs(u"\u4E2D\u6587", {{0, 2, WenJinPlane::Plane0}}, "same-plane coalesce");

    std::u16string extg;
    append_cp(extg, 0x30000);
    append_cp(extg, 0x31350);
    expect_runs(extg, {{0, 4, WenJinPlane::Plane3}}, "ExtG+ExtH coalesce in P3");

    // Punctuation / page footer uses the baseline font (None), not any WenJin
    // plane -- otherwise the ASCII numbering would render with a CJK face.
    expect_runs(u"  -  1/9  +", {{0, 11, WenJinPlane::None}}, "footer is None");

    // Nothing assigned in any WenJin plane -> a single None run, no crash.
    expect_runs(u"", {}, "empty text");

    if (failures != 0) {
        std::fprintf(stderr, "%d glyph_plane check(s) failed\n", failures);
        return 1;
    }
    std::puts("glyph_plane: all checks passed");
    return 0;
}
