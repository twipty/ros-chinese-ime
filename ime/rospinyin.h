// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
#pragma once

#include <windows.h>
#include "imm32_abi.h"

namespace rospinyin {

// ImeInquire copies this name into the 64-TCHAR buffer supplied by IMM32.
inline constexpr wchar_t kUIServerClassName[] = L"RosPinyinUIServer";

// Must stay byte-identical across ime/rospinyin.h, ime/rospinyin_res.rc and
// tools/install-rospinyin/install.cpp; tools/check_m1.py enforces this.
inline constexpr wchar_t kLayoutText[] = L"中文(简体) - RosPinyin";
inline constexpr wchar_t kImeFileName[] = L"rospinyin.ime";

// Where install-rospinyin.exe records the HKL returned by ImmInstallIMEW.
inline constexpr wchar_t kRegInstall[] = L"SOFTWARE\\RosPinyin\\Install";
inline constexpr wchar_t kRegHklValue[] = L"HKL";

}  // namespace rospinyin

// Owned by dllmain.cpp.
extern HINSTANCE g_hInst;

// UI server window (ui_server.cpp). At M1 it only exists so IMM32 has a
// valid window class; composition/candidate/state windows arrive in M2/M3.
ATOM RegisterUIServerClass(HINSTANCE hInst);
void UnregisterUIServerClass(HINSTANCE hInst);
LRESULT CALLBACK UIServerWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam);
