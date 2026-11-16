// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Branding constants for the assembled rospinyin.ime (installer, resources,
// registry). The IMM32 host layer is framework-owned (framework/host); the
// UI server class name now comes from PinyinScheme (pinyin/).

#pragma once

namespace rospinyin {

// Must stay byte-identical across ime/rospinyin.h, ime/rospinyin_res.rc and
// tools/install-rospinyin/install.cpp; tools/check_m1.py enforces this.
inline constexpr wchar_t kLayoutText[] = L"中文(简体) - RosPinyin";
inline constexpr wchar_t kImeFileName[] = L"rospinyin.ime";

// Where install-rospinyin.exe records the HKL returned by ImmInstallIMEW.
inline constexpr wchar_t kRegInstall[] = L"SOFTWARE\\RosPinyin\\Install";
inline constexpr wchar_t kRegHklValue[] = L"HKL";

}  // namespace rospinyin
