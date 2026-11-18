// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Generic IMM32 entry points. The 16 exports keep the same ABI for every
// assembled IME; key handling routes through the Scheme (imfw/scheme.hpp):
//   * the framework owns backspace and Esc plus commit via WM_IME_CHAR
//     (multi-codepoint, see imfw/emit_text.hpp) and resolves selection /
//     paging keys using the Scheme's key policy (G4) — only while a
//     composition with candidates is on screen, otherwise those keys reach
//     the application;
//   * the scheme owns the input alphabet, the key policy and the engine.
//
// Design notes:
//   * ImeProcessKey only decides ownership; ImeToAsciiEx does the work. Since
//     X2-a neither decision lives here: they are imfw::wants_key() and
//     imfw::handle_key() in framework/src/host_key.cpp, compiled and tested on
//     the build host. This file translates a native message into an
//     imfw::KeyEvent and the answer back into TRANSMSGLIST messages.
//   * Modifiers (Ctrl/Alt) are never consumed, so application shortcuts work.
//   * Committed text is emitted as WM_IME_CHAR TRANSMESSAGEs. Candidate and
//     composition display is handled by our own UI windows (framework/platform/win32/ui),
//     which avoids depending on IMM32's DDK-only candidate list structures.
//   * Nothing here runs in DllMain; the shared table and per-HIMC state are
//     created lazily on the first key (see session_store.cpp).

#include "ime_host.h"
#include "ime_ui.h"
#include "session_store.hpp"
#include "trace_stamp.h"

// IMM32 client API (ImmGetOpenStatus / ImmSetOpenStatus). The IME-side ABI in
// imm32_abi.h deliberately does not pull this header, but the *client* half is
// needed here for the open-status alignment described at AlignOpenStatus().
#include <imm.h>

#include <imfw/candidate_pager.hpp>
#include <imfw/emit_text.hpp>
#include <imfw/host.hpp>
#include <imfw/scheme.hpp>
#include <imfw/utf8.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>

namespace {

using imfw::CandidatePager;
using imfw::ImeSessionState;

// ---- diagnostic trace (ROSPINYIN_TRACE) ------------------------------
// Why a file instead of DbgPrint: ReactOS only forwards DbgPrint to the
// serial port while the kernel debugger is enabled (a FreeLoader boot option
// the default entry does not pass), and ntdll!DbgPrint would add an import
// the shipping .ime does not otherwise need. Appending a line to
// C:\rospinyin-trace.log needs nothing but kernel32, is readable from inside
// the guest, and survives a crash because each line is opened, written and
// closed. The whole block (and the writes) compile out unless
// ROSPINYIN_TRACE is defined — see the option of the same name in
// ime/CMakeLists.txt.
#if defined(ROSPINYIN_TRACE)
// The path constant lives in trace_stamp.h so DllMain writes the same file.
using imfw::trace::kTracePath;

class TraceLine {
public:
    TraceLine() : len_(0) { line_[0] = '\0'; }
    ~TraceLine() { commit(); }

    TraceLine& s(const char* text) {
        while (text != nullptr && *text != '\0' && len_ + 1 < sizeof(line_)) {
            line_[len_++] = *text++;
        }
        line_[len_] = '\0';
        return *this;
    }
    TraceLine& d(long long value) { return number(value, false); }
    TraceLine& h(unsigned long value) {
        return number(static_cast<long long>(value), true);
    }

private:
    TraceLine& number(long long value, bool hex);
    void commit();

    char line_[320];
    std::size_t len_;
};

TraceLine& TraceLine::number(long long value, bool hex) {
    const unsigned base = hex ? 16u : 10u;
    unsigned long long magnitude = value < 0
        ? 0ULL - static_cast<unsigned long long>(value)
        : static_cast<unsigned long long>(value);
    char tmp[24];
    int count = 0;

    if (hex) s("0x");
    do {
        const unsigned digit = static_cast<unsigned>(magnitude % base);
        tmp[count++] = static_cast<char>(digit < 10 ? '0' + digit
                                                    : 'a' + digit - 10);
        magnitude /= base;
    } while (magnitude != 0 && count < 23);
    if (value < 0 && !hex) tmp[count++] = '-';
    while (count > 0 && len_ + 1 < sizeof(line_)) line_[len_++] = tmp[--count];
    line_[len_] = '\0';
    return *this;
}

void TraceLine::commit() {
    if (len_ == 0) return;

    // Prefix the line with wall clock + tick count (see trace_stamp.h for why
    // both) so interleaved lines keep their order and lines from different
    // boots stay distinguishable.
    char out[400];
    unsigned n = imfw::trace::stamp(out, 0, sizeof(out));

    // pid/tid: several processes probe the .ime during bring-up (our imhost,
    // the shell, the app under test). Without them "ImeInquire ran" cannot be
    // attributed to the instance imm32 was supposed to load.
    out[n++] = 'p';
    out[n++] = '=';
    {
        char num[12];
        int c = 0;
        unsigned long v = static_cast<unsigned long>(GetCurrentProcessId());
        do {
            num[c++] = static_cast<char>('0' + v % 10);
            v /= 10;
        } while (v != 0 && c < 11);
        while (c > 0 && n + 2 < sizeof(out)) out[n++] = num[--c];
    }
    out[n++] = ' ';
    out[n++] = 't';
    out[n++] = '=';
    {
        char num[12];
        int c = 0;
        unsigned long v = static_cast<unsigned long>(GetCurrentThreadId());
        do {
            num[c++] = static_cast<char>('0' + v % 10);
            v /= 10;
        } while (v != 0 && c < 11);
        while (c > 0 && n + 2 < sizeof(out)) out[n++] = num[--c];
    }
    out[n++] = ' ';

    for (std::size_t i = 0; i < len_ && n + 2 < sizeof(out); ++i) {
        out[n++] = line_[i];
    }
    out[n++] = '\n';

    const HANDLE file = CreateFileA(kTracePath, FILE_APPEND_DATA,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    // FILE_APPEND_DATA alone was not enough: a run that should have produced
    // three lines (attach, attach, detach) left only the last one, which
    // silently drops evidence. Seek explicitly before writing.
    SetFilePointer(file, 0, nullptr, FILE_END);
    DWORD written = 0;
    WriteFile(file, out, static_cast<DWORD>(n), &written, nullptr);
    CloseHandle(file);
}
#define IMFW_TRACE(tag) TraceLine().s(tag)
#else
// Same call syntax so instrumented code compiles unchanged when the channel
// is off; the optimiser drops every call.
class TraceNoop {
public:
    TraceNoop& s(const char*) { return *this; }
    TraceNoop& d(long long) { return *this; }
    TraceNoop& h(unsigned long) { return *this; }
};
#define IMFW_TRACE(tag) TraceNoop()
#endif

constexpr BYTE kCtrlBit = 0x80;
constexpr BYTE kAltBit = 0x80;

// lParam bit 31 of WM_KEYDOWN/WM_KEYUP: 1 = the key is being released. The
// same lParam is handed to ImeProcessKey, and it is the only transition
// information available anywhere in the IME path (ImeToAsciiEx gets none).
constexpr unsigned long kKeyUpBit = 0x80000000UL;

enum class KeyIndex { Shift = 0x10, Control = 0x11, Alt = 0x12 };

bool modifier_down(const BYTE* lpbKeyState, KeyIndex index, BYTE bit) {
    return lpbKeyState != nullptr &&
           (lpbKeyState[static_cast<int>(index)] & bit) != 0;
}

// Translate the framework-managed virtual keys to their character. The
// policy deciding which of these are selection / paging keys lives in the
// Scheme (G4); the input alphabet is also translated by the Scheme.
char32_t framework_key_char(UINT vk) {
    if (vk >= '0' && vk <= '9') return static_cast<char32_t>(vk);
    switch (vk) {
    case VK_SPACE:      return U' ';
    case VK_OEM_MINUS:  return U'-';
    case VK_OEM_PLUS:   return U'=';
    case VK_OEM_COMMA:  return U',';
    case VK_OEM_PERIOD: return U'.';
    case VK_SUBTRACT:   return U'-';   // numpad minus
    case VK_ADD:        return U'=';   // numpad plus
    default:            return 0;
    }
}

// Commit text as WM_IME_CHAR TRANSMESSAGEs (G1: one message per UTF-16 code
// unit, surrogate pairs for non-BMP). The UTF-16 logic lives in
// imfw-engine (imfw/emit_text.hpp) so it is unit-tested on the host; the
// slot struct is layout-compatible with TRANSMESSAGE, guarded below.
//
// Audit 2026-11-17 §2.1 (P0): the buffer ImeToAsciiEx receives is a
// TRANSMSGLIST whose uMsgCount header is pre-filled by IMM32 with the
// buffer capacity. Messages must be written at list->TransMsg[0] (offset
// sizeof(UINT)), never at offset 0 — that would clobber the header and
// shift every message by one slot, so IMM32 would read garbage and drop
// the last message. The capacity we honor is min(uMsgCount, kMaxEmit).
UINT emit_text(rpimm::TransMsgList* lpTransMsgList, const std::u32string& text) {
    static_assert(sizeof(imfw::TransMsgSlot) == sizeof(rpimm::TransMsg),
                  "TRANSMESSAGE must stay layout-compatible with "
                  "imfw::TransMsgSlot");
    using rpimm_list = rpimm::TransMsgList;
#if defined(_WIN32) || defined(__i686__) || defined(__i386__)
    // Win32 (ILP32) ABI: TRANSMESSAGE is 4-byte packed (UINT/WPARAM/LPARAM
    // all 4 bytes), so the array follows the header with no padding. On
    // LP64 hosts the pointer-sized fields pad the slot and the offset grows
    // accordingly — the runtime buffer is only ever seen on Win32.
    static_assert(offsetof(rpimm_list, TransMsg) == sizeof(UINT),
                  "TRANSMSGLIST.TransMsg must follow uMsgCount directly "
                  "(no padding) on the Win32 ABI");
#endif
    if (lpTransMsgList == nullptr || lpTransMsgList->uMsgCount == 0) return 0;
    const UINT capacity = lpTransMsgList->uMsgCount < imfw::kMaxEmitMessages
                              ? lpTransMsgList->uMsgCount
                              : imfw::kMaxEmitMessages;
    return imfw::emit_text(
        reinterpret_cast<imfw::TransMsgSlot*>(lpTransMsgList->TransMsg),
        capacity, text);
}

// ---- host adapter (X2-a) ------------------------------------------------
// The whole platform vocabulary of the key policy lives in the two pieces
// below: key_event_from() turns one native message into an imfw::KeyEvent, and
// Win32Host supplies the four things the policy cannot do by itself. The
// decisions themselves are in imfw-key (framework/src/host_key.cpp), which is
// compiled and tested on the build host — this file no longer contains a single
// `if` that decides what a key means.
class Win32Host final : public imfw::Host {
public:
    explicit Win32Host(rpimm::InputContext hIMC) : hIMC_(hIMC) {}

    // Chinese/English state, owned by the IME (see AlignOpenStatus).
    bool is_open() override {
        return ImmGetOpenStatus(reinterpret_cast<HIMC>(hIMC_)) != FALSE;
    }
    void set_open(bool open) override {
        ImmSetOpenStatus(reinterpret_cast<HIMC>(hIMC_), open ? TRUE : FALSE);
        IMFW_TRACE("opentoggle -> ").d(open ? 1 : 0);
    }

    void show(const std::string& composition,
              const std::vector<std::string>& segments,
              const imfw::CandidatePager& pager) override {
        const std::wstring text = imfw::utf8_to_wide(composition);
        imfw::UiShow(hIMC_, text.c_str(), segments, pager);
#if defined(ROSPINYIN_TRACE)
        // Where the two popups ended up, and what the anchor was. The key
        // counters cannot answer "the box moved while I was paging": they only
        // say that a SetWindowPos happened, not which window or from/to where.
        // Printed only when the geometry changed, so a page-flip that moves
        // nothing is silent and a page-flip that moves something is one
        // obvious line.
        const imfw::UiGeometry g = imfw::UiLastGeometry();
        static imfw::UiGeometry last = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
        const bool changed = g.comp_x != last.comp_x || g.comp_y != last.comp_y ||
                             g.comp_w != last.comp_w || g.comp_h != last.comp_h ||
                             g.cand_x != last.cand_x || g.cand_y != last.cand_y ||
                             g.cand_w != last.cand_w || g.cand_h != last.cand_h;
        if (changed) {
            last = g;
            IMFW_TRACE("uigeom comp=").d(g.comp_x).s(",").d(g.comp_y).s(",")
                .d(g.comp_w).s(",").d(g.comp_h)
                .s(" cand=").d(g.cand_x).s(",").d(g.cand_y).s(",")
                .d(g.cand_w).s(",").d(g.cand_h)
                .s(" anchor=").d(g.anchor_x).s(",").d(g.anchor_y)
                .s(" caret=").d(g.anchor_from_caret);
        }
#endif
    }
    void hide() override { imfw::UiHide(hIMC_); }
    bool can_render(char32_t codepoint) override {
        return imfw::UiCanRender(codepoint);
    }

    void on_feed(char32_t input, const imfw::Result& result) override {
        // A local buffer, not std::string(1, c).c_str(): the temporary would be
        // destroyed before TraceLine's destructor writes the line.
        char shown[2] = {static_cast<char>(input < 0x80 ? input : '?'), '\0'};
        const imfw::UiCounters ui = imfw::UiCountersSnapshot();
        IMFW_TRACE("feed ch=").s(shown)
            .s(" act=").d(static_cast<long long>(result.action))
            .s(" comp=[").s(result.composition.c_str()).s("] cand=")
            .d(static_cast<long long>(result.candidates.size()))
            // Window work done for this key: moves+resizes that actually
            // changed something, and repaint requests. P0-3 measured the
            // "composition window jumps" report against these two numbers.
            .s(" wmove=").d(static_cast<long long>(ui.place_changes))
            .s(" wrep=").d(static_cast<long long>(ui.invalidates));
    }
    void on_candidates_filtered(std::size_t dropped, std::size_t total,
                                bool kept_all) override {
        IMFW_TRACE("cand-filter dropped ").d(static_cast<long long>(dropped))
            .s(" of ").d(static_cast<long long>(total))
            .s(kept_all ? " -> font cannot draw CJK, keeping all"
                        : " (no glyph)");
    }

private:
    rpimm::InputContext hIMC_;
};

// One native message -> one KeyEvent. ImeToAsciiEx has no lParam (that is the
// point of the key-up rule in wants_key), so it passes 0.
imfw::KeyEvent key_event_from(UINT vk, unsigned long lParam,
                              const BYTE* lpbKeyState) {
    imfw::KeyEvent event;
    event.vk = vk;
    event.ch = framework_key_char(vk);
    event.kind = vk == VK_BACK     ? imfw::KeyKind::Backspace
                 : vk == VK_ESCAPE ? imfw::KeyKind::Escape
                 : vk == VK_SHIFT  ? imfw::KeyKind::Shift
                                   : imfw::KeyKind::Other;
    event.key_up = (lParam & kKeyUpBit) != 0;
    event.ctrl = modifier_down(lpbKeyState, KeyIndex::Control, kCtrlBit);
    event.alt = modifier_down(lpbKeyState, KeyIndex::Alt, kAltBit);
    return event;
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

namespace {

// Defined further down together with its P0-2 rationale. Declared here because
// ImeSelect's first-activation path has to align the open flag too: ReactOS can
// deliver ImeSelect without a preceding ImeSetActiveContext, and that is
// exactly how the very first switch to this layout ended up "closed".
void AlignOpenStatus(rpimm::InputContext hIMC, ImeSessionState& state);

}  // namespace

BOOL WINAPI ImeSelect(rpimm::InputContext hIMC, BOOL fSelect) {
    IMFW_TRACE("select imc=").h(static_cast<unsigned long>(
        reinterpret_cast<std::uintptr_t>(hIMC)))
        .s(" f=").d(fSelect);
    if (fSelect) {
        // The engine contract requires on_select(true) before feed() does
        // anything; without this the IME would silently swallow every key.
        ImeSessionState* state = imfw::acquire_session(hIMC);
        IMFW_TRACE("select state=").d(state != nullptr)
            .s(" engine=").d(state != nullptr && state->engine != nullptr);
        if (state != nullptr && state->engine != nullptr) {
            state->engine->on_select(true);
        }
        // U1 (2026-11-18 真机): align here as well, still once per session.
        // Without this, a first activation that skips ImeSetActiveContext
        // leaves ImmGetOpenStatus() at the FALSE that IntCreateInputContext
        // set: the tray shows EN and the user's first Shift *opens* the IME
        // instead of switching language. The once-per-session guard inside
        // AlignOpenStatus keeps this from undoing a user toggle later on.
        if (state != nullptr) AlignOpenStatus(hIMC, *state);
        imfw::UiToolbarShow(hIMC);
    } else if (hIMC != nullptr) {
        imfw::UiToolbarHide(hIMC);
        imfw::UiDestroy(hIMC);
        imfw::release_session(hIMC);
    }
    return TRUE;  // select/deselect must never fail during a layout switch
}

namespace {

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

}  // namespace

BOOL WINAPI ImeSetActiveContext(rpimm::InputContext hIMC, BOOL fFlag) {
    IMFW_TRACE("active f=").d(fFlag)
        .s(" imc=").h(static_cast<unsigned long>(
            reinterpret_cast<std::uintptr_t>(hIMC)));
    if (hIMC == nullptr) return TRUE;
    if (fFlag) {
        // Some IMM32 implementations only deliver ImeSetActiveContext for a
        // newly focused HIMC; activating here as well is idempotent.
        ImeSessionState* state = imfw::acquire_session(hIMC);
        if (state != nullptr) AlignOpenStatus(hIMC, *state);
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
    // Ownership question, not work: the policy is wants_key() (imfw-key).
    // Everything platform-shaped is in the two lines that build the event —
    // the decision itself is compiled and tested on the build host.
    ImeSessionState* state = imfw::acquire_session(hIMC);
    Win32Host host(hIMC);
    const imfw::KeyEvent event =
        key_event_from(uVirtKey, static_cast<unsigned long>(lParam), lpbKeyState);

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
#if defined(ROSPINYIN_TRACE)
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
    Win32Host host(hIMC);
    // No lParam in this ABI: key_up is always false here, which is correct
    // because ImeProcessKey never claims a key-up (see wants_key).
    const imfw::KeyEvent event = key_event_from(uVirtKey, 0, lpbKeyState);
    const imfw::KeyResult result = imfw::handle_key(*state, event, host);
    return emit_text(lpTransMsgList, result.commit);
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
