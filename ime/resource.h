// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
#pragma once

// Referenced by the "Layout Display Name" registry value as
// @%SystemRoot%\system32\rospinyin.ime,-1024
#define IDS_LAYOUT_TEXT 1024
#define IDS_IME_NAME    1025

// Tray icon for kbswitch. ReactOS kbswitch does NOT draw a [CH]/[EN] letter
// badge for IME layouts: for layouts with an "IME File" value it calls
// FakeExtractIcon()->EnumResourceNames(RT_GROUP_ICON) on the .ime itself and
// returns NULL when no icon resource exists. Providing IDI_ROSPINYIN here is
// what makes the language bar show an icon at all.
#define IDI_ROSPINYIN   1
