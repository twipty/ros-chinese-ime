// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Shared declarations for the IMM32 host layer (framework/host). The
// assembling IME module provides branding (ime/rospinyin.h) separately.

#pragma once

#include <windows.h>

#include "imm32_abi.h"

namespace imfw {

// Owned by dllmain.cpp.
extern HINSTANCE g_hInst;

}  // namespace imfw

// UI server window (ui_server.cpp). Its class name comes from the Scheme
// (see imfw/scheme.hpp); it exists so IMM32 has a valid registered class.
ATOM RegisterUIServerClass(HINSTANCE hInst);
void UnregisterUIServerClass(HINSTANCE hInst);
LRESULT CALLBACK UIServerWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam);
