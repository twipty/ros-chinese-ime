/* RosPinyin - Simplified Chinese pinyin IME for ReactOS
 * SPDX-License-Identifier: MIT
 *
 * workarea_probe - diagnostic only. Prints the guest's real values for every
 * Win32 API the IME toolbar depends on:
 *   - SPI_GETWORKAREA  (toolbar docks against this rect; a bad value sends it
 *                       to the full-screen fallback at y=570)
 *   - CreateUiFont / GetTextFaceW  (which CJK face GDI actually resolves)
 *   - GetTextExtentPoint32W  (measure_width(); a failure pins the strip at
 *                            kMinWidth=44, clipping "拼:开[快]" to "拼")
 *
 * Output is plain ASCII on stdout, one line per probe. Nothing is written.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

static const wchar_t* const kFontFaces[] = {
    L"Droid Sans Fallback",
    L"Noto Sans SC",
    L"Microsoft YaHei",
    L"SimSun",
    L"Arial Unicode MS",
};

static int ascii_iequal(const wchar_t* a, const wchar_t* b) {
    while (*a && *b) {
        wchar_t ca = *a, cb = *b;
        if (ca >= L'A' && ca <= L'Z') ca = (wchar_t)(ca - L'A' + L'a');
        if (cb >= L'A' && cb <= L'Z') cb = (wchar_t)(cb - L'A' + L'a');
        if (ca != cb) return 0;
        ++a; ++b;
    }
    return *a == 0 && *b == 0;
}

int main(void) {
    RECT work;
    memset(&work, 0, sizeof(work));
    BOOL ok = SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    printf("SPI_GETWORKAREA ret=%d l=%ld t=%ld r=%ld b=%ld\n",
           (int)ok, (long)work.left, (long)work.top,
           (long)work.right, (long)work.bottom);
    printf("SM_CXSCREEN=%d SM_CYSCREEN=%d\n",
           GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

    HFONT chosen = NULL;
    int i;
    for (i = 0; i < (int)(sizeof(kFontFaces) / sizeof(kFontFaces[0])); ++i) {
        HFONT f = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, kFontFaces[i]);
        if (!f) { printf("font[%d] CREATE_FAIL\n", i); continue; }
        HDC dc = GetDC(NULL);
        HGDIOBJ old = SelectObject(dc, f);
        wchar_t actual[LF_FACESIZE] = {0};
        int got = GetTextFaceW(dc, LF_FACESIZE, actual);
        SelectObject(dc, old);
        ReleaseDC(NULL, dc);
        printf("font[%d] want=%ls got=%ls (ret=%d)\n", i, kFontFaces[i], actual, got);
        if (ascii_iequal(actual, kFontFaces[i])) { chosen = f; break; }
        DeleteObject(f);
    }
    if (!chosen) {
        chosen = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, NULL);
        printf("font FALLBACK chosen=%d\n", chosen ? 1 : 0);
    } else {
        printf("font CHOSEN idx=%d\n", i);
    }

    HDC mdc = CreateCompatibleDC(NULL);
    printf("CreateCompatibleDC=%d font=%d\n", mdc ? 1 : 0, chosen ? 1 : 0);
    if (mdc && chosen) {
        HGDIOBJ old = SelectObject(mdc, chosen);
        const wchar_t* samples[] = {L"拼", L"拼:开", L"拼:开[快]", L"ABC",
                                    L"[", L"[快]", L"快", L"]", L"[全]"};
        int j;
        for (j = 0; j < 9; ++j) {
            SIZE sz; memset(&sz, 0, sizeof(sz));
            BOOL r = GetTextExtentPoint32W(mdc, samples[j],
                                           (int)wcslen(samples[j]), &sz);
            printf("measure[%d] ret=%d cx=%ld cy=%ld\n", j, (int)r,
                   (long)sz.cx, (long)sz.cy);
        }
        SelectObject(mdc, old);
        DeleteDC(mdc);
    }
    return 0;
}
