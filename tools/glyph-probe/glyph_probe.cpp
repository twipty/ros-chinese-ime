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

enum class Verdict { HasGlyph, NoGlyph, ApiUnavailable };

struct Cell {
    Verdict verdict = Verdict::ApiUnavailable;
    std::uint16_t glyph = 0;
    bool using_fallback = false;
};

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

    wchar_t text[2] = {static_cast<wchar_t>(cp), 0};
    if (api.get_glyph_indices) {
        WORD index = 0;
        UINT got = api.get_glyph_indices(dc, text, 1, &index, 0);
        if (got == 1 && index != 0 && index != 0xFFFF) {
            out.verdict = Verdict::HasGlyph;
            out.glyph = index;
        } else if (got == 1) {
            out.verdict = Verdict::NoGlyph;
            out.glyph = index;
        } else {
            out.verdict = Verdict::ApiUnavailable;
        }
    } else {
        out.verdict = unicode_range_claims(dc, cp) ? Verdict::HasGlyph : Verdict::NoGlyph;
        out.using_fallback = true;
    }

    // Draw the glyph into the window (screenshot evidence). Wide-char path.
    TextOutW(dc, 0, 0, text, 1);

    SelectObject(dc, old);
    DeleteObject(font);
    return out;
}

std::wstring cell_text(Verdict v) {
    switch (v) {
        case Verdict::HasGlyph: return L"HAS";
        case Verdict::NoGlyph: return L"NONE";
        case Verdict::ApiUnavailable: return L"API?";
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

void write_result(const std::vector<std::vector<Cell>>& grid, const GdiApi& api) {
    FILE* f = nullptr;
    _wfopen_s(&f, exe_dir_file(L"glyph-probe-result.txt").c_str(), L"w, ccs=UTF-8");
    if (!f) return;
    std::fwprintf(f, L"# glyph-probe-result v1\n");
    std::fwprintf(f, L"# api.GetGlyphIndicesW=%s fallback=%s\n",
                  api.get_glyph_indices ? L"yes" : L"no",
                  api.get_glyph_indices ? L"no" : L"UnicodeRanges");
    std::fwprintf(f, L"codepoint\tface\tverdict\tglyph_index\n");
    for (std::size_t r = 0; r < kTestPointsCount(); ++r) {
        for (std::size_t c = 0; c < sizeof(kFaces) / sizeof(kFaces[0]); ++c) {
            const Cell& cell = grid[r][c];
            std::fwprintf(f, L"U+%04X\t%s\t%s\t%u\n",
                          kTestPoints[r].cp,
                          kFaces[c].label,
                          cell_text(cell.verdict).c_str(),
                          static_cast<unsigned>(cell.glyph));
        }
    }
    std::fclose(f);
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
            write_result(grid, api);
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
                    wchar_t ch = static_cast<wchar_t>(kTestPoints[r].cp);
                    TextOutW(dc, x, y, &ch, 1);
                    TextOutW(dc, x + 24, y, cell_text(grid[r][c].verdict).c_str(), 4);
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
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
