// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// The only place a native IMM32 message becomes framework vocabulary, and the
// only place finished text comes back out as TRANSMSGLIST messages.
//
// Nothing here decides anything: ownership is imfw::wants_key() and dispatch is
// imfw::handle_key() (framework/src/host_key.cpp, compiled and tested on the
// build host), and "what does this key mean" is imfw::route_key()
// (framework/src/key_route.cpp). This file is the platform edge -- Win32
// virtual-key codes and buffer layouts stop here.

#pragma once

#include "imm32_abi.h"

#include <imfw/host.hpp>

#include <string>

namespace imfw {
namespace win32 {

// One native message -> one KeyEvent. lParam carries the only key-up bit
// anywhere in the IME path; ImeToAsciiEx has no lParam and passes 0, which is
// correct because ImeProcessKey never claims a key-up (see wants_key()).
KeyEvent key_event_from(UINT vk, unsigned long lParam, const BYTE* lpbKeyState);

// Commit text as WM_IME_CHAR TRANSMESSAGEs, honouring the capacity IMM32
// pre-filled in list->uMsgCount. Returns the number of messages written.
UINT emit_commit(rpimm::TransMsgList* lpTransMsgList, const std::u32string& text);

}  // namespace win32
}  // namespace imfw
