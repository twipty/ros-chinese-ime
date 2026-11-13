// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// M0 red line #1: a bare LoadLibrary of rospinyin.ime must be harmless,
// including during winlogon/CSRSS startup. DllMain therefore ONLY registers
// the UI window class. No registry reads, no dictionary loading, no window
// creation, no threads, no IMM32 calls. tools/check_m1.py enforces this.

#include "rospinyin.h"

HINSTANCE g_hInst = nullptr;

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason,
                               LPVOID lpvReserved) {
    (void)lpvReserved;

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        g_hInst = hInstDLL;
        DisableThreadLibraryCalls(hInstDLL);
        if (!RegisterUIServerClass(hInstDLL)) {
            g_hInst = nullptr;
            return FALSE;
        }
        break;

    case DLL_PROCESS_DETACH:
        UnregisterUIServerClass(g_hInst);
        g_hInst = nullptr;
        break;

    default:
        break;
    }
    return TRUE;
}
