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
#include "trace_stamp.h"

#include <imfw/scheme.hpp>

namespace imfw {

HINSTANCE g_hInst = nullptr;

}  // namespace imfw

// ---- diagnostic trace (ROSPINYIN_TRACE) ------------------------------
// The other probes live in imm_entry.cpp and can only fire once IMM32 starts
// calling the IME. This one answers the earlier question: was the .ime loaded
// at all? IMM32 silently falls back to plain key handling when an IME fails
// to load, which is indistinguishable from "keys are not intercepted" at the
// application. Only compiled in with ROSPINYIN_TRACE (see ime/CMakeLists.txt).
//
// It stays a single open/append/close with no allocation and no library call
// beyond kernel32, so the "bare LoadLibrary must be harmless" red line still
// holds in the shipping build, where this block does not exist at all.
#if defined(ROSPINYIN_TRACE)
namespace {

// Why pid/tid/module path: "did anything load us" was unanswerable from a trace
// line that only said "attach". With pid+tid we can tell IMM32's load apart from
// our own probe's LoadLibrary, and the module path tells apart rospinyin.ime /
// rospinyin2.ime / rospinyin3.ime — three files have been installed at various
// points during bring-up.
void TraceAppend(char* line, unsigned& n, const char* text) {
    for (const char* p = text; p != nullptr && *p != '\0' && n + 2 < 200; ++p) {
        line[n++] = *p;
    }
}

void TraceAppendUint(char* line, unsigned& n, unsigned long value) {
    char tmp[12];
    unsigned len = 0;
    if (value == 0) {
        TraceAppend(line, n, "0");
        return;
    }
    while (value != 0 && len < sizeof(tmp)) {
        tmp[len++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    while (len > 0 && n + 2 < 200) {
        line[n++] = tmp[--len];
    }
}

void TraceAttach(const char* what, HINSTANCE hInst) {
    const HANDLE file = CreateFileA(imfw::trace::kTracePath, FILE_APPEND_DATA,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;

    char line[200];
    unsigned n = imfw::trace::stamp(line, 0, sizeof(line));
    TraceAppend(line, n, "dllmain pid=");
    TraceAppendUint(line, n, static_cast<unsigned long>(GetCurrentProcessId()));
    TraceAppend(line, n, " tid=");
    TraceAppendUint(line, n, static_cast<unsigned long>(GetCurrentThreadId()));
    TraceAppend(line, n, " hInst=");
    {
        char hex[12];
        unsigned long v = static_cast<unsigned long>(
            reinterpret_cast<unsigned long long>(hInst) & 0xFFFFFFFFull);
        for (int i = 7; i >= 0; --i) {
            const unsigned nib = static_cast<unsigned>((v >> (i * 4)) & 0xF);
            hex[7 - i] = static_cast<char>(nib < 10 ? '0' + nib : 'a' + nib - 10);
        }
        hex[8] = '\0';
        TraceAppend(line, n, hex);
    }
    TraceAppend(line, n, " ");
    TraceAppend(line, n, what);

    if (hInst != nullptr) {
        char module[MAX_PATH];
        module[0] = '\0';
        if (GetModuleFileNameA(hInst, module, MAX_PATH) > 0) {
            module[MAX_PATH - 1] = '\0';
            TraceAppend(line, n, " module=");
            TraceAppend(line, n, module);
        }
    }
    line[n++] = '\n';

    // Seek to end explicitly: FILE_APPEND_DATA did not append reliably here,
    // and a DllMain trace is exactly the evidence we cannot afford to lose.
    SetFilePointer(file, 0, nullptr, FILE_END);
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(n), &written, nullptr);
    CloseHandle(file);
}

}  // namespace
#endif

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason,
                               LPVOID lpvReserved) {
    (void)lpvReserved;

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
#if defined(ROSPINYIN_TRACE)
        TraceAttach("process attach: entered", hInstDLL);
#endif
        imfw::g_hInst = hInstDLL;
        DisableThreadLibraryCalls(hInstDLL);
        if (!RegisterUIServerClass(hInstDLL)) {
#if defined(ROSPINYIN_TRACE)
            TraceAttach("process attach: RegisterUIServerClass FAILED, unload", hInstDLL);
#endif
            imfw::g_hInst = nullptr;
            return FALSE;
        }
#if defined(ROSPINYIN_TRACE)
        TraceAttach("process attach: ok", hInstDLL);
#endif
        break;

    case DLL_PROCESS_DETACH:
#if defined(ROSPINYIN_TRACE)
        TraceAttach("process detach", hInstDLL);
#endif
        UnregisterUIServerClass(imfw::g_hInst);
        imfw::g_hInst = nullptr;
        break;

    default:
        break;
    }
    return TRUE;
}
