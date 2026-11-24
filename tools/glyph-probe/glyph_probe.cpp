// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// P1 verification-stage GDI font-fallback probe. NOT product code. Double-click
// in the guest to answer two questions that cannot be inferred on the host:
//   Q1: can a single face name (e.g. "WenJin Mincho Plane 2") draw Ext B/C?
//   Q2: does GDI auto-fall back ACROSS face names (Plane0 face drawing a
//       Plane2 codepoint), or must the candidate window SelectObject per plane?
//
// For every (codepoint x face name) cell we ask GDI for a glyph index and draw
// the character into a window so a screenshot is possible. Results are written
// to glyph-probe-result.txt next to the exe (UTF-8, pipe-friendly columns).

#include <windows.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include "glyph_probe_logic.hpp"

namespace {

struct TestCp {
    std::uint32_t cp;
    const wchar_t* label;
};

// One representative per plane plus boundaries and a known BMP control.
const TestCp kTestPoints[] = {
    {0x4E2D, L"U+4E2D BMP 中"},
    {0x3400, L"U+3400 ExtA start"},
    {0x20000, L"U+20000 ExtB start"},
    {0x2A6E0, L"U+2A6E0 ExtB"},
    {0x2B81E, L"U+2B81E ExtD"},
    {0x30000, L"U+30000 ExtG start"},
    {0x31350, L"U+31350 ExtH start"},
};

struct Face {
    const wchar_t* name;   // nullptr == system default ("MS Shell Dlg")
    const wchar_t* label;
};

const Face kFaces[] = {
    {nullptr, L"SystemDefault"},
    {L"WenJin Mincho Plane 0", L"WJ-P0"},
    {L"WenJin Mincho Plane 2", L"WJ-P2"},
    {L"WenJin Mincho Plane 3", L"WJ-P3"},
};

enum class Verdict { HasGlyph, NoGlyph, ApiUnavailable, Unknown };

struct Cell {
    Verdict verdict = Verdict::ApiUnavailable;
    std::uint16_t glyph = 0;
    bool using_fallback = false;
    const wchar_t* via = L"GlyphIndices";  // measurement method for this cell
};

// get_glyph_index via GetGlyphIndicesW is accurate for BMP but on ReactOS
// explicitly does NOT combine surrogate pairs (freetype.c:7642). Advance width
// is no help either: WenJin is monospaced (every glyph, .notdef included, is
// 1000/1000 em), so widths are identical whether a glyph exists or not.
// The reliable signal is PIXELS: render the codepoint and a known-absent
// noncharacter (U+10FFFF) with the same font into two offscreen bitmaps and
// compare. ReactOS TextOutW goes through the surrogate-pair-aware paint path
// (freetype.c:6949 Utf32FromSurrogatePair), so real Ext B/C glyphs do draw.
constexpr int kGlyphTile = 40;  // 40x40 px offscreen tile per render

// Render one text run into a 40x40 32-bpp DIB section using the given font.
// Returns raw BGRA bytes; blank background is 0. Empty on failure.
std::vector<std::uint8_t> render_tile(const wchar_t* text, int units, HFONT font) {
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(kGlyphTile) * kGlyphTile * 4, 0);
    HDC screen = GetDC(nullptr);
    if (!screen) return pixels;
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = kGlyphTile;
    bi.bmiHeader.biHeight = kGlyphTile;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ old_bmp = SelectObject(mem, bmp);
        HGDIOBJ old_font = SelectObject(mem, font);
        RECT r{0, 0, kGlyphTile, kGlyphTile};
        FillRect(mem, &r, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, RGB(0, 0, 0));
        TextOutW(mem, 2, 2, text, units);
        GdiFlush();
        std::memcpy(pixels.data(), bits, pixels.size());
        SelectObject(mem, old_font);
        SelectObject(mem, old_bmp);
        DeleteObject(bmp);
    }
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return pixels;
}

// Dynamically resolve GetGlyphIndicesW; ReactOS may or may not export it.
using GetGlyphIndicesFn = UINT(WINAPI*)(HDC, LPCWSTR, int, LPWORD, DWORD);

struct GdiApi {
    HMODULE gdi32 = nullptr;
    GetGlyphIndicesFn get_glyph_indices = nullptr;
    bool unicode_ranges_ok = false;

    GdiApi() {
        gdi32 = GetModuleHandleW(L"gdi32.dll");
        if (gdi32) {
            get_glyph_indices = reinterpret_cast<GetGlyphIndicesFn>(
                GetProcAddress(gdi32, "GetGlyphIndicesW"));
        }
        // GetFontUnicodeRanges is a GDI standard too; assume available and let
        // the call fail gracefully per-cell if not.
        unicode_ranges_ok = (gdi32 != nullptr);
    }
};

std::size_t kTestPointsCount() { return sizeof(kTestPoints) / sizeof(kTestPoints[0]); }

// Self-test for the pixel-render path: U+4E2D in Plane-0 font MUST render
// differently from the U+10FFFF noncharacter baseline. If offscreen rendering
// is broken on this OS (e.g. CreateDIBSection/TextOutW), both are blank and the
// comparison would falsely say "no glyph" for every cell. Detect that here and
// surface it in the result header so the data is never silently trusted.
bool pixel_path_selftest_ok() {
    HFONT font = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"WenJin Mincho Plane 0");
    if (!font) return false;
    const wchar_t known[2] = {0x4E2D, 0};  // HAS on P0 in prior guest run
    std::uint16_t nd[2] = {0, 0};
    const int nd_n = glyph_probe::encode_utf16(0x10FFFF, nd);
    const wchar_t ndc[3] = {static_cast<wchar_t>(nd[0]), static_cast<wchar_t>(nd[1]), 0};
    const auto a = render_tile(known, 1, font);
    const auto b = render_tile(ndc, nd_n, font);
    DeleteObject(font);
    return glyph_probe::render_verdict(a.data(), b.data(), a.size(), true) ==
           glyph_probe::ProbeVerdict::HasGlyph;
}

// Fallback: does the selected font's cmap claim the codepoint?
// GLYPHSET layout returned by GetFontUnicodeRanges:
//   DWORD cbSize; DWORD cRanges; then cRanges inline WCRANGE { WORD wcLow; WORD cGlyphs; }
// We parse bytes by hand because MinGW's headers don't declare GLYPHRANGE here.
bool unicode_range_claims(HDC dc, std::uint32_t cp) {
    using Fn = DWORD(WINAPI*)(HDC, LPVOID);
    HMODULE gdi32 = GetModuleHandleW(L"gdi32.dll");
    if (!gdi32) return false;
    Fn fn = reinterpret_cast<Fn>(GetProcAddress(gdi32, "GetFontUnicodeRanges"));
    if (!fn) return false;
    DWORD bytes = fn(dc, nullptr);  // query required size
    if (bytes < 8) return false;
    std::vector<BYTE> buf(bytes, 0);
    if (fn(dc, buf.data()) == 0) return false;  // fill the buffer
    const DWORD count = *reinterpret_cast<const DWORD*>(buf.data() + 4);
    for (DWORD i = 0; i < count; ++i) {
        const std::size_t off = 8 + static_cast<std::size_t>(i) * 4;
        if (off + 4 > buf.size()) break;
        const WORD low = *reinterpret_cast<const WORD*>(buf.data() + off);
        const WORD len = *reinterpret_cast<const WORD*>(buf.data() + off + 2);
        // WCRANGE.wcLow is 16-bit: GetFontUnicodeRanges only describes BMP
        // ranges, so supplementary-plane cps are never claimed here. This is
        // an API limit; the primary path (GetGlyphIndicesW) handles those and
        // is available on both guest OSes in this round.
        if (cp >= low && cp < static_cast<std::uint32_t>(low) + len) {
            return true;
        }
    }
    return false;
}

Cell probe_cell(HDC dc, const Face& face, std::uint32_t cp, const GdiApi& api) {
    Cell out;
    // CreateFontW with the face name; if nullptr, use system default.
    HFONT font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE,
                             face.name ? face.name : L"MS Shell Dlg");
    HGDIOBJ old = SelectObject(dc, font);

    // UTF-16 encode: non-BMP codepoints (Ext B/C/... U+20000+) need a
    // surrogate pair. Passing a truncated static_cast<wchar_t>(cp) here was
    // the false-NONE bug in the first guest result matrix.
    std::uint16_t units[2] = {0, 0};
    const int n = glyph_probe::encode_utf16(cp, units);
    wchar_t text[3] = {static_cast<wchar_t>(units[0]),
                       static_cast<wchar_t>(units[1]), 0};

    if (cp <= 0xFFFF) {
        // BMP: GetGlyphIndicesW is accurate (verified: U+4E2D/U+3400 -> P0 HAS).
        if (api.get_glyph_indices) {
            WORD index = 0;
            UINT got = api.get_glyph_indices(dc, text, n, &index, 0);
            if (got == n && index != 0 && index != 0xFFFF) {
                out.verdict = Verdict::HasGlyph;
                out.glyph = index;
            } else if (got == n) {
                out.verdict = Verdict::NoGlyph;
                out.glyph = index;
            } else {
                out.verdict = Verdict::ApiUnavailable;
            }
        } else if (unicode_range_claims(dc, cp)) {
            out.verdict = Verdict::HasGlyph;
            out.using_fallback = true;
            out.via = L"UnicodeRanges";
        } else {
            out.verdict = Verdict::NoGlyph;
            out.using_fallback = true;
            out.via = L"UnicodeRanges";
        }
    } else {
        // Non-BMP: neither GetGlyphIndicesW (no surrogate support,
        // freetype.c:7642) nor advance width (WenJin is monospaced) can answer.
        // Render the surrogate pair and U+10FFFF (noncharacter, absent from
        // every font) with THIS font into offscreen tiles and compare pixels.
        // TextOutW combines surrogate pairs on ReactOS (freetype.c:6949).
        std::uint16_t nd_units[2] = {0, 0};
        const int nd_n = glyph_probe::encode_utf16(0x10FFFF, nd_units);
        const wchar_t nd_text[3] = {static_cast<wchar_t>(nd_units[0]),
                                    static_cast<wchar_t>(nd_units[1]), 0};
        const std::vector<std::uint8_t> target_px = render_tile(text, n, font);
        const std::vector<std::uint8_t> notdef_px = render_tile(nd_text, nd_n, font);
        const glyph_probe::ProbeVerdict pv = glyph_probe::render_verdict(
            target_px.data(), notdef_px.data(), target_px.size(), true);
        out.verdict = pv == glyph_probe::ProbeVerdict::HasGlyph   ? Verdict::HasGlyph
                      : pv == glyph_probe::ProbeVerdict::NoGlyph ? Verdict::NoGlyph
                                                                 : Verdict::Unknown;
        out.via = L"Pixels";
    }
    // Note: glyph drawing happens in WM_PAINT with per-face fonts, not here.
    // This measurement DC must not paint (0,0) -- that was a redundant draw.

    SelectObject(dc, old);
    DeleteObject(font);
    return out;
}

std::wstring cell_text(Verdict v) {
    switch (v) {
        case Verdict::HasGlyph: return L"HAS";
        case Verdict::NoGlyph: return L"NONE";
        case Verdict::ApiUnavailable: return L"API?";
        case Verdict::Unknown: return L"UNK?";
    }
    return L"?";
}

std::wstring exe_dir_file(const wchar_t* name) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path);
    std::size_t slash = p.find_last_of(L"\\/");
    if (slash != std::wstring::npos) p.resize(slash + 1);
    return p + name;
}

// Append a UTF-16 string to an ASCII/UTF-8 buffer as UTF-8 bytes. Kept local
// so the result file needs no CRT wide-stream functions (see write_result).
void append_utf8(std::string& out, const wchar_t* s) {
    if (!s) return;
    const int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return;  // empty string (only NUL)
    const std::size_t start = out.size();
    out.resize(start + static_cast<std::size_t>(n) - 1);  // drop source NUL
    WideCharToMultiByte(CP_UTF8, 0, s, -1, &out[start], n - 1, nullptr, nullptr);
}

// Build the whole report in memory, then write it with CreateFileW/WriteFile.
// Rationale: _wfopen_s is a C11 Annex K function that ReactOS msvcrt may not
// export, and fwprintf pulls CRT locale state. Pure Win32 removes both risks
// and keeps the result file byte-identical across host and guest.
void write_result(const std::vector<std::vector<Cell>>& grid, const GdiApi& api,
                  bool selftest_ok) {
    std::string text;
    text += "# glyph-probe-result v2\n";
    text += "# bmp=GetGlyphIndicesW nonbmp=TextOutW-pixels(notdef=U+10FFFF)\n";
    text += "# pixel_selftest=";
    text += selftest_ok ? "OK (U+4E2D@P0 != U+10FFFF)"
                        : "FAIL (offscreen render unreliable; non-BMP verdicts invalid)";
    text += "\n";
    text += "# api.GetGlyphIndicesW=";
    text += api.get_glyph_indices ? "yes" : "no";
    text += " (BMP only; non-BMP via pixel render)\n";
    text += "codepoint\tface\tverdict\tglyph_index\tvia\n";
    for (std::size_t r = 0; r < kTestPointsCount(); ++r) {
        for (std::size_t c = 0; c < sizeof(kFaces) / sizeof(kFaces[0]); ++c) {
            const Cell& cell = grid[r][c];
            char line[256];
            std::snprintf(line, sizeof(line), "U+%04X\t", kTestPoints[r].cp);
            text += line;
            append_utf8(text, kFaces[c].label);
            text += "\t";
            append_utf8(text, cell_text(cell.verdict).c_str());
            char via_ascii[32] = {0};
            WideCharToMultiByte(CP_UTF8, 0, cell.via, -1, via_ascii, sizeof(via_ascii),
                                nullptr, nullptr);
            std::snprintf(line, sizeof(line), "\t%u\t%s\n",
                          static_cast<unsigned>(cell.glyph), via_ascii);
            text += line;
        }
    }

    HANDLE h = CreateFileW(exe_dir_file(L"glyph-probe-result.txt").c_str(),
                           GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(h);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    static std::vector<std::vector<Cell>> grid;
    switch (msg) {
        case WM_CREATE: {
            GdiApi api;
            HDC dc = GetDC(hwnd);
            const std::size_t rows = kTestPointsCount();
            const std::size_t cols = sizeof(kFaces) / sizeof(kFaces[0]);
            grid.assign(rows, std::vector<Cell>(cols));
            for (std::size_t r = 0; r < rows; ++r) {
                for (std::size_t c = 0; c < cols; ++c) {
                    grid[r][c] = probe_cell(dc, kFaces[c], kTestPoints[r].cp, api);
                }
            }
            ReleaseDC(hwnd, dc);
            const bool selftest_ok = pixel_path_selftest_ok();
            write_result(grid, api, selftest_ok);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            const int colw = 150;
            const int rowh = 34;
            const int x0 = 10;
            const int y0 = 40;
            SetBkMode(dc, TRANSPARENT);
            const wchar_t* hint = L"RosPinyin GDI font-fallback probe | see glyph-probe-result.txt";
            TextOutW(dc, x0, 8, hint, static_cast<int>(wcslen(hint)));
            for (std::size_t c = 0; c < sizeof(kFaces) / sizeof(kFaces[0]); ++c) {
                TextOutW(dc, x0 + static_cast<int>(c) * colw, y0 - 20,
                         kFaces[c].label, static_cast<int>(wcslen(kFaces[c].label)));
            }
            for (std::size_t r = 0; r < kTestPointsCount(); ++r) {
                int y = y0 + static_cast<int>(r) * rowh;
                TextOutW(dc, x0, y, kTestPoints[r].label,
                         static_cast<int>(wcslen(kTestPoints[r].label)));
                for (std::size_t c = 0; c < sizeof(kFaces) / sizeof(kFaces[0]); ++c) {
                    int x = x0 + 130 + static_cast<int>(c) * colw;
                    // Re-draw glyph with that face for the screenshot.
                    const Face& face = kFaces[c];
                    HFONT font = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                             DEFAULT_PITCH | FF_DONTCARE,
                                             face.name ? face.name : L"MS Shell Dlg");
                    HGDIOBJ old = SelectObject(dc, font);
                    // Same UTF-16 surrogate-pair fix as probe_cell: draw the
                    // real Ext B/C character, not its truncated low 16 bits.
                    std::uint16_t units[2] = {0, 0};
                    const int n = glyph_probe::encode_utf16(kTestPoints[r].cp, units);
                    wchar_t text[3] = {static_cast<wchar_t>(units[0]),
                                       static_cast<wchar_t>(units[1]), 0};
                    TextOutW(dc, x, y, text, n);
                    const std::wstring vt = cell_text(grid[r][c].verdict);
                    TextOutW(dc, x + 24, y, vt.c_str(), static_cast<int>(vt.size()));
                    SelectObject(dc, old);
                    DeleteObject(font);
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

}  // namespace

// -municode entry point, matching the installer GUI. This keeps the CRT
// startup symbols (__wgetmainargs/__p__wcmdln) identical to the installer, so
// "installer runs on the guest" implies "probe runs on the guest".
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"RosPinyinGlyphProbe";
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
                                L"RosPinyin GDI font-fallback probe (P1 verification)",
                                WS_OVERLAPPEDWINDOW, 40, 40, 780, 360,
                                nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
