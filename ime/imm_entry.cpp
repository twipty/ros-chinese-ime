// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// M2 IMM32 entry points. The 16 exports keep the same ABI as M1; the key
// handling now drives the platform-independent pinyin engine in engine/.
//
// Design notes:
//   * ImeProcessKey only decides ownership; ImeToAsciiEx does the work.
//   * Modifiers (Ctrl/Alt) are never consumed, so application shortcuts work.
//   * Committed text is emitted as WM_IME_CHAR TRANSMESSAGEs. Candidate and
//     composition display is handled by our own UI windows (ui/ime_ui.cpp),
//     which avoids depending on IMM32's DDK-only candidate list structures.
//   * Nothing here runs in DllMain; the shared table and per-HIMC state are
//     created lazily on the first key (see session_store.cpp).

#include "rospinyin.h"
#include "ime_ui.h"
#include "session_store.hpp"

#include <rospinyin/candidate_pager.hpp>
#include <rospinyin/pinyin_session.hpp>

#include <cwchar>
#include <string>

#ifndef WM_IME_CHAR
#define WM_IME_CHAR 0x0286
#endif

#ifndef VK_OEM_PLUS
#define VK_OEM_PLUS 0xBB
#endif
#ifndef VK_OEM_COMMA
#define VK_OEM_COMMA 0xBC
#endif
#ifndef VK_OEM_MINUS
#define VK_OEM_MINUS 0xBD
#endif
#ifndef VK_OEM_PERIOD
#define VK_OEM_PERIOD 0xBE
#endif

namespace {

using rospinyin::ImeSessionState;
using rospinyin::CandidatePager;

constexpr BYTE kShiftBit = 0x80;
constexpr BYTE kCtrlBit = 0x80;
constexpr BYTE kAltBit = 0x80;

enum class KeyIndex { Shift = 0x10, Control = 0x11, Alt = 0x12 };

bool modifier_down(const BYTE* lpbKeyState, KeyIndex index, BYTE bit) {
    return lpbKeyState != nullptr &&
           (lpbKeyState[static_cast<int>(index)] & bit) != 0;
}

// Translate a virtual key to the canonical pinyin character. Uppercase
// letters are lowercased: the engine works on lowercase canonical strings.
char32_t virtkey_to_char(UINT vk) {
    if (vk >= 'A' && vk <= 'Z') return static_cast<char32_t>(vk - 'A' + 'a');
    if (vk >= 'a' && vk <= 'z') return static_cast<char32_t>(vk);
    if (vk >= '0' && vk <= '9') return static_cast<char32_t>(vk);
    switch (vk) {
    case VK_SPACE:      return U' ';
    case VK_OEM_7:      return U'\'';  // hard syllable separator
    case VK_OEM_MINUS:  return U'-';   // previous page
    case VK_OEM_PLUS:   return U'=';   // next page
    case VK_OEM_COMMA:  return U',';   // previous page
    case VK_OEM_PERIOD: return U'.';   // next page
    case VK_SUBTRACT:   return U'-';   // numpad minus
    case VK_ADD:        return U'=';   // numpad plus
    default:            return 0;
    }
}

bool is_page_key(char32_t ch) {
    return ch == U'-' || ch == U'=' || ch == U',' || ch == U'.';
}

bool is_selection_key(UINT vk, char32_t ch) {
    return ch == U' ' || (ch >= U'1' && ch <= U'9') ||
           (vk >= '0' && vk <= '9' && ch == static_cast<char32_t>(vk));
}

std::wstring to_wide(const std::string& text) {
    std::wstring result;
    result.reserve(text.size());
    for (const char c : text) {
        result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
    }
    return result;
}

// Decide whether the IME should consume this key. Called by IMM32 before
// ImeToAsciiEx; keeping it side-effect free is important.
bool should_consume_key(ImeSessionState* state, UINT vk,
                        const BYTE* lpbKeyState) {
    if (state == nullptr) return false;
    if (modifier_down(lpbKeyState, KeyIndex::Control, kCtrlBit) ||
        modifier_down(lpbKeyState, KeyIndex::Alt, kAltBit)) {
        return false;
    }

    const char32_t ch = virtkey_to_char(vk);
    if (ch == 0) return false;

    if (is_page_key(ch)) return state->composing && !state->pager.empty();
    if (is_selection_key(vk, ch)) {
        return state->composing && !state->pager.empty();
    }
    if (vk == VK_BACK || vk == VK_ESCAPE) return state->composing;

    // Pinyin letters and the apostrophe separator.
    return (ch >= U'a' && ch <= U'z') || ch == U'\'';
}

// Generate a WM_IME_CHAR message for a committed code point.
UINT emit_char(rpimm::TransMsg* messages, UINT count, char32_t codepoint) {
    if (messages == nullptr || count == 0) return 0;
    // All M2 candidates are BMP; every built-in / dat character is BMP too.
    messages[0].uMsg = WM_IME_CHAR;
    messages[0].wParam = static_cast<WPARAM>(codepoint);
    messages[0].lParam = 0;
    return 1;
}

void refresh_ui(rpimm::InputContext hIMC, ImeSessionState& state) {
    const std::wstring text = to_wide(state.session.composition());
    rospinyin::UiShow(hIMC, text.c_str(), state.session.syllables(),
                      state.pager);
}

void finish_composition(rpimm::InputContext hIMC, ImeSessionState& state,
                        char32_t commit_text) {
    state.session.cancel();
    state.pager.reset({});
    state.composing = false;
    rospinyin::UiHide(hIMC);
    (void)commit_text;  // emitted by the caller
}

UINT process_key(rpimm::InputContext hIMC, UINT vk, const BYTE* lpbKeyState,
                 rpimm::TransMsg* lpTransMsg) {
    ImeSessionState* state = rospinyin::acquire_session(hIMC);
    if (state == nullptr) return 0;

    const char32_t ch = virtkey_to_char(vk);

    // Digit / space selection is resolved by the pager so page 2 digit 1
    // selects candidate 10, not candidate 1.
    if (is_selection_key(vk, ch)) {
        std::size_t digit = 0;
        if (ch >= U'1' && ch <= U'9') {
            digit = static_cast<std::size_t>(ch - U'1');
        }
        const rospinyin::Candidate* chosen = state->pager.select(digit);
        if (chosen == nullptr || chosen->text.empty()) return 0;
        const char32_t commit_text = chosen->text[0];
        finish_composition(hIMC, *state, commit_text);
        return emit_char(lpTransMsg, 1, commit_text);
    }

    if (is_page_key(ch)) {
        const bool moved = (ch == U'-' || ch == U',')
                               ? state->pager.prev_page()
                               : state->pager.next_page();
        if (moved) refresh_ui(hIMC, *state);
        return 0;
    }

    if (vk == VK_BACK) {
        const rospinyin::Result result = state->session.backspace();
        (void)result;
        state->pager.reset(state->session.candidates());
        state->composing = !state->session.composition().empty();
        if (state->composing) refresh_ui(hIMC, *state);
        else rospinyin::UiHide(hIMC);
        return 0;
    }

    if (vk == VK_ESCAPE) {
        finish_composition(hIMC, *state, 0);
        return 0;
    }

    const rospinyin::Result result = state->session.feed(ch);
    switch (result.action) {
    case rospinyin::Action::Commit:
        if (!result.commit_text.empty()) {
            const char32_t commit_text = result.commit_text[0];
            finish_composition(hIMC, *state, commit_text);
            return emit_char(lpTransMsg, 1, commit_text);
        }
        break;
    case rospinyin::Action::Absorb:
        state->pager.reset(state->session.candidates());
        state->composing = !state->session.composition().empty();
        refresh_ui(hIMC, *state);
        break;
    case rospinyin::Action::Passthrough:
    default:
        break;
    }
    return 0;
}

}  // namespace

extern "C" {

BOOL WINAPI ImeInquire(rpimm::ImeInfo* lpIMEInfo, LPWSTR lpszClassName,
                       DWORD dwSystemInfoFlags) {
    if (!lpIMEInfo || !lpszClassName) return FALSE;

    lpIMEInfo->dwPrivateDataSize = 0;  // we keep state in session_store
    lpIMEInfo->fdwProperty = rpimm::kPropUnicode | rpimm::kPropAtCaret;
    lpIMEInfo->fdwConversionCaps = 0;
    lpIMEInfo->fdwSentenceCaps = 0;
    lpIMEInfo->fdwUICaps = rpimm::kUICap2700;
    lpIMEInfo->fdwSCSCaps = 0;
    lpIMEInfo->fdwSelectCaps = 0;

    std::wcsncpy(lpszClassName, rospinyin::kUIServerClassName, 63);
    lpszClassName[63] = L'\0';

    // Ignore IME_SYSINFO_WINLOGON: extra work on the logon path is exactly
    // what made the reference IME blue-screen (0xF4).
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
    return FALSE;  // configuration UI is M6
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
    if (fSelect) {
        // The engine rejects feed() until on_select(true); without this the
        // IME would consume pinyin keys but never build a composition.
        ImeSessionState* state = rospinyin::acquire_session(hIMC);
        if (state != nullptr) state->session.on_select(true);
    } else if (hIMC != nullptr) {
        rospinyin::UiDestroy(hIMC);
        rospinyin::release_session(hIMC);
    }
    return TRUE;  // select/deselect must never fail during a layout switch
}

BOOL WINAPI ImeSetActiveContext(rpimm::InputContext hIMC, BOOL fFlag) {
    if (hIMC == nullptr) return TRUE;
    if (fFlag) {
        // Some IMM32 implementations only deliver ImeSetActiveContext for a
        // newly focused HIMC; activating here as well is idempotent.
        ImeSessionState* state = rospinyin::acquire_session(hIMC);
        if (state != nullptr) state->session.on_select(true);
    } else {
        rospinyin::UiHide(hIMC);
    }
    return TRUE;
}

BOOL WINAPI ImeProcessKey(rpimm::InputContext hIMC, UINT uVirtKey, LPARAM lParam,
                          const BYTE* lpbKeyState) {
    (void)lParam;
    ImeSessionState* state = rospinyin::acquire_session(hIMC);
    return should_consume_key(state, uVirtKey, lpbKeyState) ? TRUE : FALSE;
}

UINT WINAPI ImeToAsciiEx(UINT uVirtKey, UINT uScanCode, const BYTE* lpbKeyState,
                         rpimm::TransMsg* lpTransMsg, UINT fuState,
                         rpimm::InputContext hIMC) {
    (void)uScanCode; (void)fuState;
    return process_key(hIMC, uVirtKey, lpbKeyState, lpTransMsg);
}

BOOL WINAPI NotifyIME(rpimm::InputContext hIMC, DWORD dwAction, DWORD dwIndex,
                      DWORD dwValue) {
    (void)hIMC; (void)dwAction; (void)dwIndex; (void)dwValue;
    return FALSE;  // our UI windows drive themselves
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
    return FALSE;  // we render the composition ourselves
}

DWORD WINAPI ImeGetImeMenuItems(rpimm::InputContext hIMC, DWORD dwFlags, DWORD dwType,
                                rpimm::ImeMenuItemInfo* lpImeParentMenu,
                                rpimm::ImeMenuItemInfo* lpImeMenu, DWORD dwSize) {
    (void)hIMC; (void)dwFlags; (void)dwType; (void)lpImeParentMenu;
    (void)lpImeMenu; (void)dwSize;
    return 0;
}

}  // extern "C"
