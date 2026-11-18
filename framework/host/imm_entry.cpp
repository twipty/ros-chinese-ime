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
#include "trace_stamp.h"

// IMM32 client API (ImmGetOpenStatus / ImmSetOpenStatus). The IME-side ABI in
// imm32_abi.h deliberately does not pull this header, but the *client* half is
// needed here for the open-status alignment described at AlignOpenStatus().
#include <imm.h>

#include <imfw/candidate_pager.hpp>
#include <imfw/emit_text.hpp>
#include <imfw/scheme.hpp>
#include <imfw/utf8.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <string>

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

void refresh_ui(rpimm::InputContext hIMC, ImeSessionState& state) {
    // Composition is UTF-8 (G4 connected conclusion: engines may show CJK
    // in the composition window, not just ASCII codes).
    const std::wstring text = imfw::utf8_to_wide(state.composition);
    imfw::UiShow(hIMC, text.c_str(), state.segments, state.pager);
#if defined(ROSPINYIN_TRACE)
    // Where the two popups ended up, and what the anchor was. The key counters
    // cannot answer "the box moved while I was paging": they only say that a
    // SetWindowPos happened, not which window or from/to where. Printed only
    // when the geometry changed, so a page-flip that moves nothing is silent
    // and a page-flip that moves something is one obvious line.
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

// Chinese/English state, owned by the IME.
//
// History matters here because P0-2 concluded the opposite: at the time,
// `open` read 0 while the IME worked, so gating on it would have disabled the
// IME outright. The missing half was that nothing ever *set* it — imm32 creates
// the IMC with fOpen = FALSE and the activation path does not touch it. Now
// AlignOpenStatus() sets it at every activation and VK_SHIFT toggles it, so the
// flag is a state this IME owns and can trust; IMM32 only stores it.
bool ime_is_open(rpimm::InputContext hIMC) {
    return ImmGetOpenStatus(reinterpret_cast<HIMC>(hIMC)) != FALSE;
}

// Decide whether the IME should consume this key. Called by IMM32 before
// ImeToAsciiEx; keeping it side-effect free is important.
bool should_consume_key(rpimm::InputContext hIMC, ImeSessionState* state,
                        UINT vk, const BYTE* lpbKeyState) {
    if (state == nullptr) return false;
    // Closed = English: every key belongs to the application. This is the whole
    // point of the Shift toggle; without it "switching to English" would leave
    // the IME swallowing letters and inserting Chinese.
    if (!ime_is_open(hIMC)) return false;
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
    // Selection and paging keys (policy owned by the Scheme, G4) are only
    // meaningful mid-composition with candidates on screen; otherwise they
    // pass through to the application (e.g. space in an empty editor).
    if (scheme.is_selection_key(ch) || scheme.page_key(ch) != 0) {
        return state->composing && !state->pager.empty();
    }
    return false;
}

void finish_composition(rpimm::InputContext hIMC, ImeSessionState& state) {
    state.engine->cancel();
    state.pager.reset({});
    state.composing = false;
    state.composition.clear();
    state.segments.clear();
    imfw::UiHide(hIMC);
}

// Chinese <-> English. Called only from the Shift gesture in ImeProcessKey.
//
// Closing must drop an in-flight composition: leaving the letters on screen
// while every following key goes to the application produces a popup that
// never updates (the exact "frozen composition window" symptom).
void set_ime_open(rpimm::InputContext hIMC, ImeSessionState& state,
                  bool open) {
    if (open == ime_is_open(hIMC)) return;
    ImmSetOpenStatus(reinterpret_cast<HIMC>(hIMC), open ? TRUE : FALSE);
    if (!open && state.composing) finish_composition(hIMC, state);
    IMFW_TRACE("opentoggle -> ").d(open ? 1 : 0);
}

// Track the last engine output so paging/refresh need no engine call.
void adopt_result(ImeSessionState& state, const imfw::Result& result) {
    state.composition = result.composition;
    state.segments = result.segments;
}

// P1-2: keep only candidates the candidate font can draw. Without this,
// `shi` offered a ranked extension-A character that painted as a tofu box and
// inserted a box into the document when picked.
//
// Two guards make a wrong font harmless:
//   * everything dropped (or more than half) means the font has no CJK at all
//     — that is a font problem, not a candidate problem, so keep the list;
//   * the probe fails open (see UiCanRender), so a GDI without
//     GetGlyphIndicesW behaves exactly as before.
std::vector<imfw::Candidate> renderable_candidates(
    const std::vector<imfw::Candidate>& input) {
    if (input.empty()) return input;

    std::vector<imfw::Candidate> kept;
    kept.reserve(input.size());
    for (const auto& candidate : input) {
        bool drawable = true;
        for (const char32_t codepoint : candidate.text) {
            if (codepoint != 0 && !imfw::UiCanRender(codepoint)) {
                drawable = false;
                break;
            }
        }
        if (drawable) kept.push_back(candidate);
    }

    const std::size_t dropped = input.size() - kept.size();
    if (dropped == 0) return input;
    if (kept.empty() || kept.size() * 2 < input.size()) {
        IMFW_TRACE("cand-filter dropped ").d(static_cast<long long>(dropped))
            .s(" of ").d(static_cast<long long>(input.size()))
            .s(" -> font cannot draw CJK, keeping all");
        return input;
    }
    IMFW_TRACE("cand-filter dropped ").d(static_cast<long long>(dropped))
        .s(" of ").d(static_cast<long long>(input.size()))
        .s(" (no glyph)");
    return kept;
}

// Emit one picked character and either keep composing or close the session.
//
// The pinyin engine converts left to right, so committing 中 out of
// "zhongguo" leaves "guo" in the composition (Result::composition, candidates
// and segments are all filled in). Dropping that remainder — what an
// unconditional finish_composition() used to do — threw away everything the
// user had typed after the first syllable. A stateless engine returns an empty
// composition here, so this branch is the plain "commit and close" path.
UINT apply_commit(rpimm::InputContext hIMC, ImeSessionState& state,
                  const imfw::Result& result,
                  rpimm::TransMsgList* lpTransMsgList) {
    if (result.commit_text.empty()) return 0;
    const std::u32string text = result.commit_text;

    if (result.composition.empty()) {
        finish_composition(hIMC, state);
    } else {
        adopt_result(state, result);
        state.pager.reset(renderable_candidates(result.candidates));
        state.composing = true;
        refresh_ui(hIMC, state);
    }
    return emit_text(lpTransMsgList, text);
}

UINT process_key(rpimm::InputContext hIMC, UINT vk, const BYTE* lpbKeyState,
                 rpimm::TransMsgList* lpTransMsgList) {
    ImeSessionState* state = imfw::acquire_session(hIMC);
    if (state == nullptr || state->engine == nullptr) return 0;

    const imfw::Scheme& scheme = imfw::scheme_instance();
    const char32_t ch = framework_key_char(vk);

    // Digit / space selection is resolved by the pager so page 2 digit 1
    // selects candidate 10, not candidate 1. Which keys select is the
    // Scheme's policy (G4); digits map to page-relative indices, any other
    // selection key picks the first candidate.
    if (ch != 0 && scheme.is_selection_key(ch)) {
        std::size_t digit = 0;
        if (ch >= U'1' && ch <= U'9') {
            digit = static_cast<std::size_t>(ch - U'1');
        }
        const imfw::Candidate* chosen = state->pager.select(digit);
        if (chosen == nullptr || chosen->text.empty()) return 0;
        // The pager holds a *filtered* copy of the candidate list (P1-2 drops
        // characters the font cannot draw), so its index means nothing to the
        // engine — hand the text back instead and let the engine decide what
        // is left to convert.
        return apply_commit(hIMC, *state,
                            state->engine->commit_text(chosen->text),
                            lpTransMsgList);
    }

    // Paging direction is likewise the Scheme's policy (G4).
    if (ch != 0) {
        const int page = scheme.page_key(ch);
        if (page != 0) {
            const bool moved = page < 0 ? state->pager.prev_page()
                                        : state->pager.next_page();
            if (moved) refresh_ui(hIMC, *state);
            return 0;
        }
    }

    if (vk == VK_BACK) {
        const imfw::Result result = state->engine->backspace();
        adopt_result(*state, result);
        state->pager.reset(renderable_candidates(result.candidates));
        state->composing = !result.composition.empty();
        if (state->composing) refresh_ui(hIMC, *state);
        else imfw::UiHide(hIMC);
        return 0;
    }

    if (vk == VK_ESCAPE) {
        finish_composition(hIMC, *state);
        return 0;
    }

    const char32_t input = scheme.input_char(vk);
    if (input == 0 || !scheme.is_input_char(input)) return 0;

    const imfw::Result result = state->engine->feed(input);
    // One line per fed character. The composition string after each feed is
    // the fastest way to separate "the key arrived twice" (letters doubled:
    // `nn`, `aa`) from "the key arrived once and was rejected by can_extend"
    // (composition unchanged). Without it both look like a wrong result.
    {
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
    switch (result.action) {
    case imfw::Action::Commit:
        return apply_commit(hIMC, *state, result, lpTransMsgList);
    case imfw::Action::Absorb:
        adopt_result(*state, result);
        state->pager.reset(renderable_candidates(result.candidates));
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
    // Shift owns Chinese/English. ReactOS gives us every key message, including
    // VK_SHIFT (verified in the trace: `processkey vk=0x10 -> 0`), so the IME
    // has to implement the toggle itself -- imm32 only stores fOpen and no
    // hot key is registered for Shift.
    //
    // Lone-Shift semantics: a latch cleared by any other key means Shift+A is
    // still a capital A rather than "switch to English, then type a". The
    // switch happens on key-UP, which is where a user expects it.
    if (uVirtKey == VK_SHIFT) {
        const bool up = (static_cast<unsigned long>(lParam) & kKeyUpBit) != 0;
        ImeSessionState* state = imfw::acquire_session(hIMC);
        if (state != nullptr) {
            if (!up) {
                state->shift_latched = true;
            } else {
                const bool lone = state->shift_latched;
                state->shift_latched = false;
                if (lone) set_ime_open(hIMC, *state, !ime_is_open(hIMC));
            }
        }
        IMFW_TRACE("processkey vk=").h(uVirtKey)
            .s(up ? " shift-up" : " shift-down")
            .s(" lone=").d(state != nullptr && state->shift_latched ? 1 : 0)
            .s(" open=").d(ime_is_open(hIMC) ? 1 : 0)
            .s(" -> 0");
        return FALSE;
    }

    // A key-UP must never be claimed. This is the only place the decision can
    // be made: ReactOS routes every key message through IntImmProcessKey
    // (co_IntProcessKeyboardMessage sets bKeyUpDown for WM_KEYUP as well) and
    // imm32!ImmProcessKey ignores the transition bit. Claiming a key-up sets
    // pIC->bNeedsTrans, and the following ImmGenerateMessage calls
    // ImeToAsciiEx — which receives no lParam, so it cannot filter either.
    //
    // Symptom when it is wrong: every keystroke reaches the engine twice.
    // "n" becomes "nn", "a" becomes "aa". Doubled letters that still form a
    // legal pinyin prefix are kept (extra characters); the rest are rejected
    // by can_extend() and vanish (missing characters). Both halves of the
    // "extra / lost characters" report come from this one line.
    if ((static_cast<unsigned long>(lParam) & kKeyUpBit) != 0) {
        IMFW_TRACE("processkey vk=").h(uVirtKey).s(" up -> 0");
        return FALSE;
    }

    ImeSessionState* state = imfw::acquire_session(hIMC);
    if (state != nullptr) state->shift_latched = false;  // Shift was a modifier
    const bool consume = should_consume_key(hIMC, state, uVirtKey, lpbKeyState);
#if defined(ROSPINYIN_TRACE)
    // open= is now the gate itself (Chinese/English), so a persistent 0 here
    // means "closed", not "IMM32 is broken". The earlier P0-2 reading -- open
    // is an observation, never a gate -- was only valid while nothing set the
    // flag; see AlignOpenStatus() and set_ime_open().
    const int open = ime_is_open(hIMC) ? 1 : 0;
    IMFW_TRACE("processkey vk=").h(uVirtKey)
        .s(" state=").d(state != nullptr)
        .s(" open=").d(open)
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
    return process_key(hIMC, uVirtKey, lpbKeyState, lpTransMsgList);
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
