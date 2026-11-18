// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Host-only stub of the IMM32 CLIENT API, for the Linux syntax check.
//
// Two consumers, and they use different halves of the API:
//   * install-rospinyin.exe -> ImmInstallIMEW (installs the layout)
//   * the IME host layer    -> ImmGetOpenStatus / ImmSetOpenStatus, called by
//                              AlignOpenStatus() to keep the IMC's open flag in
//                              sync with the IME's own state.
//
// The IME-side ABI (the 16 exports) is declared by
// framework/platform/win32/imm32/imm32_abi.h instead, so the two headers
// cannot disagree about what this DLL exports. What imm32_abi.h does *not*
// provide is the client half above — it deliberately keeps HIMC as
// `rpimm::InputContext = void*` so it needs nothing but <windows.h>. The host
// layer converts explicitly at the call site.
//
// Keep these signatures identical to the real <imm.h>: this file exists so
// `g++ -fsyntax-only` can typecheck Windows code on Linux, and a wrong
// signature here would silently weaken that gate instead of failing it.

#pragma once
#include <windows.h>

// Opaque input-context handle. One pointer, exactly like rpimm::InputContext.
using HIMC = HANDLE;

extern "C" {
HKL WINAPI ImmInstallIMEW(LPCWSTR lpszIMEFileName, LPCWSTR lpszLayoutText);
BOOL WINAPI ImmGetOpenStatus(HIMC);
BOOL WINAPI ImmSetOpenStatus(HIMC, BOOL);
}
