// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// M0 red line #1: a bare LoadLibrary of the assembled .ime must be harmless,
// including during winlogon/CSRSS startup. DllMain therefore ONLY registers
// the UI window class. No registry reads, no dictionary loading, no window
// creation, no threads, no IMM32 calls. tools/check_m1.py enforces this.
//
// The UI server class name comes from the Scheme; constructing that static
// object here is pure static initialization (string constants + a vtable),
// not loader work.

#include "ime_host.h"

#include <imfw/scheme.hpp>

namespace imfw {

HINSTANCE g_hInst = nullptr;

}  // namespace imfw

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason,
                               LPVOID lpvReserved) {
    (void)lpvReserved;

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        imfw::g_hInst = hInstDLL;
        DisableThreadLibraryCalls(hInstDLL);
        if (!RegisterUIServerClass(hInstDLL)) {
            imfw::g_hInst = nullptr;
            return FALSE;
        }
        break;

    case DLL_PROCESS_DETACH:
        UnregisterUIServerClass(imfw::g_hInst);
        imfw::g_hInst = nullptr;
        break;

    default:
        break;
    }
    return TRUE;
}
