// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// The 16 IMM32 export entry points. Everything else this file used to hold
// moved next door in X2-d, because a single 600-line translation unit made
// "where does a key get decided" a question you answered by reading all of it:
//
//   trace_line.{h,cpp}     the ROSPINYIN_TRACE channel (one line per call site)
//   key_translate.{h,cpp}  native message <-> imfw vocabulary, commit -> messages
//   win32_host.{h,cpp}     Win32Host: the platform half of the key policy
//   imm_entry.cpp          this file: entry points + activation ordering
//
// The key policy itself is not here at all (X2-a moved it to
// framework/src/host_key.cpp, routing in X2-c to framework/src/key_route.cpp);
// both are compiled and tested on the build host, so this file no longer
// contains a single `if` that decides what a key means.
//
// Design notes:
//   * ImeProcessKey only decides ownership; ImeToAsciiEx does the work.
//   * Modifiers (Ctrl/Alt) are never consumed, so application shortcuts work.
//   * Committed text is emitted as WM_IME_CHAR TRANSMESSAGEs. Candidate and
//     composition display is handled by our own UI windows
//     (framework/platform/win32/ui), which avoids depending on IMM32's
//     DDK-only candidate list structures.
//   * Nothing here runs in DllMain; the shared table and per-HIMC state are
//     created lazily on the first key (see session_store.cpp).

#include "ime_host.h"
#include "ime_ui.h"
#include "key_translate.h"
#include "session_store.hpp"
#include "trace_line.h"
#include "win32_host.h"

// IMM32 client API (ImmGetOpenStatus / ImmSetOpenStatus). The IME-side ABI in
// imm32_abi.h deliberately does not pull this header, but the *client* half is
// needed here for the open-status alignment described at AlignOpenStatus().
#include <imm.h>

#include <imfw/scheme.hpp>

#include <cstdint>
#include <cwchar>

namespace {

using imfw::ImeSessionState;

// P0-2 (M2-REMAINING): align IMM32's own "IME is open" flag with reality.
//
// ReactOS creates the input context with pIC->fOpen = FALSE
// (imm32_imm.c, IntCreateInputContext) and nothing in the layout-activation
// path sets it back: ImmSetActiveContext only reads it to feed
// NtUserNotifyIMEStatus. Consequences of the stale FALSE:
//   * ImmGetOpenStatus() reports "closed" for an IME that is processing keys
//     — this is what the trace line `open=0` was showing; the `-1` that
//     imhost printed came from its own `g_himc == NULL` sentinel, not from
//     IMM32, so the two readings never contradicted each other;
//   * the shell never sees 中/英 state (UserSetImeConversionKeyState is
//     called with ALPHANUMERIC);
//   * the next Ctrl+Space takes imm32_keymsg.c!Imm32CImeNonImeToggle's
//     "closed" branch, so it opens instead of switching to the EN layout.
// Setting it once per session makes ImmGetOpenStatus agree with what the
// user sees, and gives the Shift toggle a well-defined starting point.
//
// "Once per session", not "once per activation": re-aligning on every
// activation would silently undo an English toggle the moment the user clicks
// into another window and back, which is exactly what a Chinese/English switch
// must survive.
void AlignOpenStatus(rpimm::InputContext hIMC, ImeSessionState& state) {
    HIMC imc = reinterpret_cast<HIMC>(hIMC);
    const int before = ImmGetOpenStatus(imc) ? 1 : 0;
    int after = before;
    if (!state.open_initialized) {
        state.open_initialized = true;
        if (before == 0 && ImmSetOpenStatus(imc, TRUE)) {
            after = ImmGetOpenStatus(imc) ? 1 : 0;
        }
    }
    IMFW_TRACE("openalign before=").d(before).s(" after=").d(after)
        .s(" once=").d(state.open_initialized ? 1 : 0);
}

// The single activation path (X2-d).
//
// ImeSelect and ImeSetActiveContext used to carry the same three steps in
// *opposite orders* — one called on_select() then aligned the flag, the other
// aligned then called on_select(). That is the shape that produced U1 ("the
// very first switch to this layout arrives closed") in the first place: two
// copies of a sequence that has to happen in one order, with nothing pinning
// the order. The order below is what both entry points need:
//   * AlignOpenStatus first — it decides what the toolbar is about to show;
//   * on_select(true) next — the engine contract requires it before any feed();
//   * UiToolbarShow last — it renders the state the first step established.
//
// The two deactivation paths are deliberately *not* merged: ImeSelect(FALSE)
// tears the session down (UiDestroy + release_session) while
// ImeSetActiveContext(FALSE) only hides the windows, so the input context
// survives a focus change. Merging those would be a behaviour change, not a
// refactor.
void activate_session(rpimm::InputContext hIMC) {
    ImeSessionState* state = imfw::acquire_session(hIMC);
    IMFW_TRACE("activate state=").d(state != nullptr)
        .s(" engine=").d(state != nullptr && state->engine != nullptr);
    if (state != nullptr) AlignOpenStatus(hIMC, *state);
    // The engine contract requires on_select(true) before feed() does
    // anything; without this the IME would silently swallow every key.
    if (state != nullptr && state->engine != nullptr) {
        state->engine->on_select(true);
    }
    imfw::UiToolbarShow(hIMC);
}

}  // namespace

extern "C" {

BOOL WINAPI ImeInquire(rpimm::ImeInfo* lpIMEInfo, LPWSTR lpszClassName,
                       DWORD dwSystemInfoFlags) {
    IMFW_TRACE("inquire sysinfo=").h(dwSystemInfoFlags);
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
    IMFW_TRACE("inquire prop=").h(lpIMEInfo->fdwProperty)
        .s(" caps=").h(lpIMEInfo->fdwUICaps)
        .s(" priv=").d(lpIMEInfo->dwPrivateDataSize)
        .s(" -> 1");
    return TRUE;
}

DWORD WINAPI ImeConversionList(rpimm::InputContext hIMC, LPCWSTR lpSource,
                               const rpimm::CandidateList* lpCandList,
                               DWORD dwBufLen, UINT uFlags) {
    (void)hIMC; (void)lpSource; (void)lpCandList; (void)dwBufLen; (void)uFlags;
    return 0;
}

BOOL WINAPI ImeConfigure(HKL hKL, HWND hWnd, DWORD dwMode, LPVOID lpData) {
    (void)hKL; (void)dwMode; (void)lpData;
    // Framework gap G3: ImeConfigure used to decline unconditionally, so
    // users had to reach the configuration tool from the start menu or the
    // command line. A Scheme that provides configuration handling now gets
    // the call; after a successful configure the data and all sessions are
    // reloaded (G2 hot path: the tool may have swapped packs or rewritten
    // the .dat). Schemes without a handler keep the old behaviour.
    if (imfw::scheme_instance().configure(hWnd)) {
        imfw::reload_sessions();
        return TRUE;
    }
    return FALSE;
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
    IMFW_TRACE("select imc=").h(static_cast<unsigned long>(
        reinterpret_cast<std::uintptr_t>(hIMC)))
        .s(" f=").d(fSelect);
    if (fSelect) {
        // ReactOS can deliver ImeSelect without a preceding
        // ImeSetActiveContext, and that is exactly how the very first switch
        // to this layout ended up "closed" (U1). Both entry points therefore
        // go through the same activation path.
        activate_session(hIMC);
    } else if (hIMC != nullptr) {
        imfw::UiToolbarHide(hIMC);
        imfw::UiDestroy(hIMC);
        imfw::release_session(hIMC);
    }
    return TRUE;  // select/deselect must never fail during a layout switch
}

BOOL WINAPI ImeSetActiveContext(rpimm::InputContext hIMC, BOOL fFlag) {
    IMFW_TRACE("active f=").d(fFlag)
        .s(" imc=").h(static_cast<unsigned long>(
            reinterpret_cast<std::uintptr_t>(hIMC)));
    if (hIMC == nullptr) return TRUE;
    if (fFlag) {
        // Some IMM32 implementations only deliver ImeSetActiveContext for a
        // newly focused HIMC; activating here as well is idempotent.
        activate_session(hIMC);
    } else {
        imfw::UiToolbarHide(hIMC);
        imfw::UiHide(hIMC);
    }
    return TRUE;
}

BOOL WINAPI ImeProcessKey(rpimm::InputContext hIMC, UINT uVirtKey, LPARAM lParam,
                          const BYTE* lpbKeyState) {
    // Ownership question, not work: the policy is wants_key() (imfw-key).
    // Everything platform-shaped is in the two lines that build the event —
    // the decision itself is compiled and tested on the build host.
    ImeSessionState* state = imfw::acquire_session(hIMC);
    imfw::win32::Win32Host host(hIMC);
    const imfw::KeyEvent event = imfw::win32::key_event_from(
        uVirtKey, static_cast<unsigned long>(lParam), lpbKeyState);

    if (event.kind == imfw::KeyKind::Shift) {
        // Lone-Shift toggle: the transition bit is only visible here, and the
        // answer is always "not ours" (see wants_key).
        if (state != nullptr) imfw::wants_key(*state, event, host);
        IMFW_TRACE("processkey vk=").h(uVirtKey)
            .s(event.key_up ? " shift-up" : " shift-down")
            .s(" lone=").d(state != nullptr && state->shift_latched ? 1 : 0)
            .s(" open=").d(host.is_open() ? 1 : 0)
            .s(" -> 0");
        return FALSE;
    }

    if (event.key_up) {
        IMFW_TRACE("processkey vk=").h(uVirtKey).s(" up -> 0");
        return FALSE;
    }

    const bool consume = state != nullptr && imfw::wants_key(*state, event, host);
#if IMFW_TRACE_ON
    // open= is now the gate itself (Chinese/English), so a persistent 0 here
    // means "closed", not "IMM32 is broken". The earlier P0-2 reading -- open
    // is an observation, never a gate -- was only valid while nothing set the
    // flag; see AlignOpenStatus() and set_open().
    IMFW_TRACE("processkey vk=").h(uVirtKey)
        .s(" state=").d(state != nullptr)
        .s(" open=").d(host.is_open() ? 1 : 0)
        .s(" pf=").h(static_cast<unsigned long>(HIWORD(lParam)))
        .s(" -> ").d(consume ? 1 : 0);
#endif
    return consume ? TRUE : FALSE;
}

UINT WINAPI ImeToAsciiEx(UINT uVirtKey, UINT uScanCode, const BYTE* lpbKeyState,
                         rpimm::TransMsgList* lpTransMsgList, UINT fuState,
                         rpimm::InputContext hIMC) {
    (void)uScanCode; (void)fuState;
    IMFW_TRACE("toascii vk=").h(uVirtKey);
    ImeSessionState* state = imfw::acquire_session(hIMC);
    if (state == nullptr) return 0;
    imfw::win32::Win32Host host(hIMC);
    // No lParam in this ABI: key_up is always false here, which is correct
    // because ImeProcessKey never claims a key-up (see wants_key).
    const imfw::KeyEvent event = imfw::win32::key_event_from(uVirtKey, 0, lpbKeyState);
    const imfw::KeyResult result = imfw::handle_key(*state, event, host);
    return imfw::win32::emit_commit(lpTransMsgList, result.commit);
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
    (void)hIMC; (void)dwIndex; (void)lpComp; (void)dwCompLen; (void)lpRead; (void)dwReadLen;
    return FALSE;  // we render the composition ourselves
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
