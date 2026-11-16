// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Generic IMM32 entry points. The 16 exports keep the same ABI for every
// assembled IME; key handling routes through the Scheme (imfw/scheme.hpp):
//   * the framework owns paging (- = , .), selection (digits/space),
//     backspace and Esc, plus commit via WM_IME_CHAR;
//   * the scheme owns the input alphabet and the engine behind it.
//
// Design notes:
//   * ImeProcessKey only decides ownership; ImeToAsciiEx does the work.
//   * Modifiers (Ctrl/Alt) are never consumed, so application shortcuts work.
//   * Committed text is emitted as WM_IME_CHAR TRANSMESSAGEs. Candidate and
//     composition display is handled by our own UI windows (framework/ui),
//     which avoids depending on IMM32's DDK-only candidate list structures.
//   * Nothing here runs in DllMain; the shared table and per-HIMC state are
//     created lazily on the first key (see session_store.cpp).

#include "ime_host.h"
#include "ime_ui.h"
#include "session_store.hpp"

#include <imfw/candidate_pager.hpp>
#include <imfw/scheme.hpp>

#include <algorithm>
#include <cstdint>
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

using imfw::CandidatePager;
using imfw::ImeSessionState;

constexpr BYTE kCtrlBit = 0x80;
constexpr BYTE kAltBit = 0x80;

enum class KeyIndex { Shift = 0x10, Control = 0x11, Alt = 0x12 };

bool modifier_down(const BYTE* lpbKeyState, KeyIndex index, BYTE bit) {
    return lpbKeyState != nullptr &&
           (lpbKeyState[static_cast<int>(index)] & bit) != 0;
}

// Framework-managed keys: candidate selection and paging. Input-alphabet
// keys are translated by the Scheme.
char32_t framework_key_char(UINT vk) {
    if (vk >= '0' && vk <= '9') return static_cast<char32_t>(vk);
    switch (vk) {
    case VK_SPACE:      return U' ';
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

bool is_selection_key(char32_t ch) {
    return ch == U' ' || (ch >= U'1' && ch <= U'9');
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

    const imfw::Scheme& scheme = imfw::scheme_instance();
    const char32_t input = scheme.input_char(vk);
    if (input != 0 && scheme.is_input_char(input)) return true;

    const char32_t ch = framework_key_char(vk);
    if (ch == 0) {
        return vk == VK_BACK || vk == VK_ESCAPE ? state->composing : false;
    }
    // Selection and paging keys are only meaningful mid-composition.
    return state->composing && !state->pager.empty();
}

// Generate WM_IME_CHAR TRANSMESSAGEs for a committed text (framework gap G1:
// single-codepoint commits made word candidates impossible). One message per
// UTF-16 code unit keeps non-BMP characters correct (surrogate pairs). The
// IMM32 DDK contract allows up to 256 messages per ImeToAsciiEx call; cap
// well below that — no pinyin/shape candidate comes anywhere near.
UINT emit_text(rpimm::TransMsg* messages, const std::u32string& text) {
    constexpr UINT kMaxEmitMessages = 32;
    if (messages == nullptr || text.empty()) return 0;

    UINT written = 0;
    for (const char32_t cp : text) {
        if (cp <= 0xFFFF) {
            if (written >= kMaxEmitMessages) break;
            messages[written].uMsg = WM_IME_CHAR;
            messages[written].wParam = static_cast<WPARAM>(cp);
            messages[written].lParam = 0;
            ++written;
        } else {
            if (written + 2 > kMaxEmitMessages) break;
            const std::uint32_t v = static_cast<std::uint32_t>(cp) - 0x10000u;
            messages[written].uMsg = WM_IME_CHAR;
            messages[written].wParam = static_cast<WPARAM>(0xD800u + (v >> 10));
            messages[written].lParam = 0;
            ++written;
            messages[written].uMsg = WM_IME_CHAR;
            messages[written].wParam = static_cast<WPARAM>(0xDC00u + (v & 0x3FFu));
            messages[written].lParam = 0;
            ++written;
        }
    }
    return written;
}

void refresh_ui(rpimm::InputContext hIMC, ImeSessionState& state) {
    const std::wstring text = to_wide(state.composition);
    imfw::UiShow(hIMC, text.c_str(), state.segments, state.pager);
}

void finish_composition(rpimm::InputContext hIMC, ImeSessionState& state) {
    state.engine->cancel();
    state.pager.reset({});
    state.composing = false;
    state.composition.clear();
    state.segments.clear();
    imfw::UiHide(hIMC);
}

// Track the last engine output so paging/refresh need no engine call.
void adopt_result(ImeSessionState& state, const imfw::Result& result) {
    state.composition = result.composition;
    state.segments = result.segments;
}

UINT process_key(rpimm::InputContext hIMC, UINT vk, const BYTE* lpbKeyState,
                 rpimm::TransMsg* lpTransMsg) {
    ImeSessionState* state = imfw::acquire_session(hIMC);
    if (state == nullptr || state->engine == nullptr) return 0;

    const char32_t ch = framework_key_char(vk);

    // Digit / space selection is resolved by the pager so page 2 digit 1
    // selects candidate 10, not candidate 1.
    if (is_selection_key(ch)) {
        std::size_t digit = 0;
        if (ch >= U'1' && ch <= U'9') {
            digit = static_cast<std::size_t>(ch - U'1');
        }
        const imfw::Candidate* chosen = state->pager.select(digit);
        if (chosen == nullptr || chosen->text.empty()) return 0;
        const std::u32string commit_text = chosen->text;
        finish_composition(hIMC, *state);
        return emit_text(lpTransMsg, commit_text);
    }

    if (is_page_key(ch)) {
        const bool moved = (ch == U'-' || ch == U',')
                               ? state->pager.prev_page()
                               : state->pager.next_page();
        if (moved) refresh_ui(hIMC, *state);
        return 0;
    }

    if (vk == VK_BACK) {
        const imfw::Result result = state->engine->backspace();
        adopt_result(*state, result);
        state->pager.reset(result.candidates);
        state->composing = !result.composition.empty();
        if (state->composing) refresh_ui(hIMC, *state);
        else imfw::UiHide(hIMC);
        return 0;
    }

    if (vk == VK_ESCAPE) {
        finish_composition(hIMC, *state);
        return 0;
    }

    const imfw::Scheme& scheme = imfw::scheme_instance();
    const char32_t input = scheme.input_char(vk);
    if (input == 0 || !scheme.is_input_char(input)) return 0;

    const imfw::Result result = state->engine->feed(input);
    switch (result.action) {
    case imfw::Action::Commit:
        if (!result.commit_text.empty()) {
            const std::u32string commit_text = result.commit_text;
            finish_composition(hIMC, *state);
            return emit_text(lpTransMsg, commit_text);
        }
        break;
    case imfw::Action::Absorb:
        adopt_result(*state, result);
        state->pager.reset(result.candidates);
        state->composing = !result.composition.empty();
        refresh_ui(hIMC, *state);
        break;
    case imfw::Action::Passthrough:
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

    std::wcsncpy(lpszClassName, imfw::scheme_instance().ui_server_class(), 63);
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
        // The engine contract requires on_select(true) before feed() does
        // anything; without this the IME would silently swallow every key.
        ImeSessionState* state = imfw::acquire_session(hIMC);
        if (state != nullptr && state->engine != nullptr) {
            state->engine->on_select(true);
        }
        imfw::UiToolbarShow(hIMC);
    } else if (hIMC != nullptr) {
        imfw::UiToolbarHide(hIMC);
        imfw::UiDestroy(hIMC);
        imfw::release_session(hIMC);
    }
    return TRUE;  // select/deselect must never fail during a layout switch
}

BOOL WINAPI ImeSetActiveContext(rpimm::InputContext hIMC, BOOL fFlag) {
    if (hIMC == nullptr) return TRUE;
    if (fFlag) {
        // Some IMM32 implementations only deliver ImeSetActiveContext for a
        // newly focused HIMC; activating here as well is idempotent.
        ImeSessionState* state = imfw::acquire_session(hIMC);
        if (state != nullptr && state->engine != nullptr) {
            state->engine->on_select(true);
        }
        imfw::UiToolbarShow(hIMC);
    } else {
        imfw::UiToolbarHide(hIMC);
        imfw::UiHide(hIMC);
    }
    return TRUE;
}

BOOL WINAPI ImeProcessKey(rpimm::InputContext hIMC, UINT uVirtKey, LPARAM lParam,
                          const BYTE* lpbKeyState) {
    (void)lParam;
    ImeSessionState* state = imfw::acquire_session(hIMC);
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
