// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Host-only stub of the IMM32 CLIENT API used by install-rospinyin.exe.
// The IME-side ABI is declared by ime/imm32_abi.h instead, so there is no
// chance of these two headers disagreeing about the 16 IME exports.

#pragma once
#include <windows.h>

extern "C" {
HKL WINAPI ImmInstallIMEW(LPCWSTR lpszIMEFileName, LPCWSTR lpszLayoutText);
}
