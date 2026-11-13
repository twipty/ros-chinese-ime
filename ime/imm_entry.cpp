// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// M1 "passthrough IME": all 16 IMM32 entry points are implemented with the
// smallest correct contract, and ImeProcessKey consumes NO key at all. The
// module can be installed, selected and switched, but keystrokes go straight
// to the application. This is the M1 exit criterion (WBS 2.4) and the
// vehicle for isolating registration/activation crashes from engine bugs.
//
// No Japanese conversion logic, romaji table, kana mapping or dictionary is
// present: the M1 tree is a new implementation, not a copy of the reference.

#include "rospinyin.h"
#include <cwchar>

extern "C" {

BOOL WINAPI ImeInquire(rpimm::ImeInfo* lpIMEInfo, LPWSTR lpszClassName,
                       DWORD dwSystemInfoFlags) {
    if (!lpIMEInfo || !lpszClassName) {
        return FALSE;
    }

    lpIMEInfo->dwPrivateDataSize = 0;  // no per-HIMC state at M1
    lpIMEInfo->fdwProperty = rpimm::kPropUnicode;
    lpIMEInfo->fdwConversionCaps = 0;
    lpIMEInfo->fdwSentenceCaps = 0;
    lpIMEInfo->fdwUICaps = rpimm::kUICap2700;
    lpIMEInfo->fdwSCSCaps = 0;
    lpIMEInfo->fdwSelectCaps = 0;

    // IMM32 guarantees a 64-TCHAR class-name buffer.
    std::wcsncpy(lpszClassName, rospinyin::kUIServerClassName, 63);
    lpszClassName[63] = L'\0';

    // Intentionally ignore IME_SYSINFO_WINLOGON: doing anything extra on the
    // logon path is exactly what made the reference IME blue-screen (0xF4).
    (void)dwSystemInfoFlags;
    return TRUE;
}

DWORD WINAPI ImeConversionList(rpimm::InputContext hIMC, LPCWSTR lpSource,
                               const rpimm::CandidateList* lpCandList,
                               DWORD dwBufLen, UINT uFlags) {
    (void)hIMC; (void)lpSource; (void)lpCandList; (void)dwBufLen; (void)uFlags;
    return 0;
}

BOOL WINAPI ImeConfigure(HKL hKL, HWND hWnd, DWORD dwMode, LPVOID lpData) {
    (void)hKL; (void)hWnd; (void)dwMode; (void)lpData;
    return FALSE;  // no configuration dialog until M6
}

BOOL WINAPI ImeDestroy(UINT uForce) {
    (void)uForce;
    return TRUE;
}

LRESULT WINAPI ImeEscape(rpimm::InputContext hIMC, UINT uSubFunc, LPVOID lpData) {
    (void)hIMC; (void)uSubFunc; (void)lpData;
    return 0;
}

BOOL WINAPI ImeSelect(rpimm::InputContext hIMC, BOOL fSelect) {
    (void)hIMC; (void)fSelect;
    return TRUE;  // select/deselect must never fail during a layout switch
}

BOOL WINAPI ImeSetActiveContext(rpimm::InputContext hIMC, BOOL fFlag) {
    (void)hIMC; (void)fFlag;
    return TRUE;
}

BOOL WINAPI ImeProcessKey(rpimm::InputContext hIMC, UINT uVirtKey, LPARAM lParam,
                          const BYTE* lpbKeyState) {
    (void)hIMC; (void)uVirtKey; (void)lParam; (void)lpbKeyState;
    return FALSE;  // M1: consume nothing, let every key reach the app
}

UINT WINAPI ImeToAsciiEx(UINT uVirtKey, UINT uScanCode, const BYTE* lpbKeyState,
                         rpimm::TransMsg* lpTransMsg, UINT fuState, rpimm::InputContext hIMC) {
    (void)uVirtKey; (void)uScanCode; (void)lpbKeyState; (void)lpTransMsg;
    (void)fuState; (void)hIMC;
    return 0;  // zero generated messages
}

BOOL WINAPI NotifyIME(rpimm::InputContext hIMC, DWORD dwAction, DWORD dwIndex, DWORD dwValue) {
    (void)hIMC; (void)dwAction; (void)dwIndex; (void)dwValue;
    return FALSE;  // there is no composition state to notify about
}

BOOL WINAPI ImeRegisterWord(LPCWSTR lpszReading, DWORD dwStyle,
                            LPCWSTR lpszString) {
    (void)lpszReading; (void)dwStyle; (void)lpszString;
    return FALSE;
}

BOOL WINAPI ImeUnregisterWord(LPCWSTR lpszReading, DWORD dwStyle,
                              LPCWSTR lpszString) {
    (void)lpszReading; (void)dwStyle; (void)lpszString;
    return FALSE;
}

UINT WINAPI ImeGetRegisterWordStyle(UINT nItem, rpimm::StyleBuf* lpStyleBuf) {
    (void)nItem; (void)lpStyleBuf;
    return 0;
}

UINT WINAPI ImeEnumRegisterWord(rpimm::EnumRegisterWordProc lpfnEnumProc,
                                LPCWSTR lpszReading, DWORD dwStyle,
                                LPCWSTR lpszString, LPVOID lpData) {
    (void)lpfnEnumProc; (void)lpszReading; (void)dwStyle; (void)lpszString;
    (void)lpData;
    return 0;
}

BOOL WINAPI ImeSetCompositionString(rpimm::InputContext hIMC, DWORD dwIndex,
                                    LPCVOID lpComp, DWORD dwCompLen,
                                    LPCVOID lpRead, DWORD dwReadLen) {
    (void)hIMC; (void)dwIndex; (void)lpComp; (void)dwCompLen;
    (void)lpRead; (void)dwReadLen;
    return FALSE;
}

DWORD WINAPI ImeGetImeMenuItems(rpimm::InputContext hIMC, DWORD dwFlags, DWORD dwType,
                                rpimm::ImeMenuItemInfo* lpImeParentMenu,
                                rpimm::ImeMenuItemInfo* lpImeMenu,
                                DWORD dwSize) {
    (void)hIMC; (void)dwFlags; (void)dwType; (void)lpImeParentMenu;
    (void)lpImeMenu; (void)dwSize;
    return 0;
}

}  // extern "C"
