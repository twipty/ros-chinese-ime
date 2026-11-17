// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Minimal IME-side ABI for the 16 exports IMM32 calls on an IME DLL.
//
// Why this header exists: the Win32 SDK <imm.h> only declares the *client*
// side (ImmGetContext and friends); the IME-side prototypes live in the DDK
// <immdev.h>, which MinGW-w64 does not always ship. Declaring the ABI here
// keeps the DLL buildable with MinGW-w64, MSVC and the ReactOS build
// environment without importing third-party headers. Layouts below follow
// the published IMM32 contract; binary width is all that matters on the ABI
// boundary, and all pointer-shaped parameters are 32-bit on our x86 target.

#pragma once

#include <windows.h>

namespace rpimm {

// HIMC is declared by the *client* header <imm.h>, which we deliberately do
// not include here. Define our own pointer-shaped handle instead so the IME
// layer only depends on <windows.h> and cannot clash with whatever the
// toolchain's imm.h declares. ABI-wise this is still one pointer parameter.
using InputContext = void*;

// IMEINFO / LPIMEINFO as consumed by ImeInquire.
struct ImeInfo {
    DWORD dwPrivateDataSize;
    DWORD fdwProperty;
    DWORD fdwConversionCaps;
    DWORD fdwSentenceCaps;
    DWORD fdwUICaps;
    DWORD fdwSCSCaps;
    DWORD fdwSelectCaps;
};

// TRANSMSG / LPTRANSMSG as consumed by ImeToAsciiEx.
struct TransMsg {
    UINT   uMsg;
    WPARAM wParam;
    LPARAM lParam;
};

// TRANSMSGLIST / LPTRANSMSGLIST (audit 2026-11-17 §2.1, DDK <immdev.h>).
// ImeToAsciiEx does NOT receive a bare TRANSMSG array: it receives this
// list, whose uMsgCount field is pre-filled by IMM32 with the capacity of
// the buffer that follows. ReactOS win32ss/user/imm32/keymsg.c reads the
// messages back from ->TransMsg[0] using the count we return, so writing
// at offset 0 would overwrite the capacity header and shift every message
// by one slot. The message array therefore starts right after the header.
// The member named `TransMsg` (DDK spelling, keep it) shadows the struct
// name inside this scope, which trips GCC's -Wchanges-meaning when both
// are declared in the same block. Silence just that diagnostic here —
// the layout itself is what we must match.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wchanges-meaning"
struct TransMsgList {
    UINT     uMsgCount;              // in: capacity pre-filled by IMM32
    TransMsg TransMsg[1];            // out: messages written here, count == return value
};
#pragma GCC diagnostic pop

// Opaque at M1: no conversion list, no candidate list, no menu items.
struct CandidateList;
struct StyleBuf;
struct ImeMenuItemInfo;

// REGISTERWORDENUMPROCW shape (callback used by ImeEnumRegisterWord).
typedef BOOL(CALLBACK* EnumRegisterWordProc)(LPCWSTR, DWORD, LPCWSTR, LPVOID);

// Property / capability bits (values from the IMM32 IME-side header).
constexpr DWORD kPropUnicode   = 0x00080000;  // IME_PROP_UNICODE
constexpr DWORD kPropAtCaret   = 0x00010000;  // IME_PROP_AT_CARET
constexpr DWORD kUICap2700     = 0x00000001;  // UI_CAP_2700
constexpr DWORD kSysInfoWinlogon = 0x0001;    // IME_SYSINFO_WINLOGON

}  // namespace rpimm

// The 16 mandatory IME exports. Declared once so every implementation file
// is checked against the same signatures at compile time.
extern "C" {

BOOL WINAPI ImeInquire(rpimm::ImeInfo* lpIMEInfo, LPWSTR lpszClassName,
                       DWORD dwSystemInfoFlags);

DWORD WINAPI ImeConversionList(rpimm::InputContext hIMC, LPCWSTR lpSource,
                               const rpimm::CandidateList* lpCandList,
                               DWORD dwBufLen, UINT uFlags);

BOOL WINAPI ImeConfigure(HKL hKL, HWND hWnd, DWORD dwMode, LPVOID lpData);
BOOL WINAPI ImeDestroy(UINT uForce);
LRESULT WINAPI ImeEscape(rpimm::InputContext hIMC, UINT uSubFunc, LPVOID lpData);
BOOL WINAPI ImeSelect(rpimm::InputContext hIMC, BOOL fSelect);
BOOL WINAPI ImeSetActiveContext(rpimm::InputContext hIMC, BOOL fFlag);

BOOL WINAPI ImeProcessKey(rpimm::InputContext hIMC, UINT uVirtKey, LPARAM lParam,
                          const BYTE* lpbKeyState);

UINT WINAPI ImeToAsciiEx(UINT uVirtKey, UINT uScanCode, const BYTE* lpbKeyState,
                         rpimm::TransMsgList* lpTransMsgList, UINT fuState, rpimm::InputContext hIMC);

BOOL WINAPI NotifyIME(rpimm::InputContext hIMC, DWORD dwAction, DWORD dwIndex, DWORD dwValue);

BOOL WINAPI ImeRegisterWord(LPCWSTR lpszReading, DWORD dwStyle,
                            LPCWSTR lpszString);
BOOL WINAPI ImeUnregisterWord(LPCWSTR lpszReading, DWORD dwStyle,
                              LPCWSTR lpszString);
UINT WINAPI ImeGetRegisterWordStyle(UINT nItem, rpimm::StyleBuf* lpStyleBuf);
UINT WINAPI ImeEnumRegisterWord(rpimm::EnumRegisterWordProc lpfnEnumProc,
                                LPCWSTR lpszReading, DWORD dwStyle,
                                LPCWSTR lpszString, LPVOID lpData);

BOOL WINAPI ImeSetCompositionString(rpimm::InputContext hIMC, DWORD dwIndex,
                                    LPCVOID lpComp, DWORD dwCompLen,
                                    LPCVOID lpRead, DWORD dwReadLen);

DWORD WINAPI ImeGetImeMenuItems(rpimm::InputContext hIMC, DWORD dwFlags, DWORD dwType,
                                rpimm::ImeMenuItemInfo* lpImeParentMenu,
                                rpimm::ImeMenuItemInfo* lpImeMenu,
                                DWORD dwSize);

}  // extern "C"
