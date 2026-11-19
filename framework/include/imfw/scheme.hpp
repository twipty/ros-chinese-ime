// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Per-input-method plugin contract. The framework owns everything an IME
// has in common — the IMM32 entry points, key routing, candidate paging,
// composition/candidate/toolbar windows. A concrete input method (pinyin
// today, a shape IME next) supplies this Scheme and owns its own data: the
// framework never loads, holds or passes a code table (dat_filename /
// load_fallback were removed when data ownership moved into the schemes;
// the generic CodeTable utility class stays in imfw for schemes that want
// it).
//
// The interface is deliberately free of Windows headers: `vk` is a plain
// virtual-key number, so pinyin/scheme code builds and is unit-tested on
// Linux exactly like the rest of the engine.
//
// Wiring: scheme_instance() (bottom of this file) is the framework's query
// entry point; the assembling module (e.g. ime/scheme_reg.cpp) supplies
// imfw::scheme_factory(), which the registry calls lazily on first use — never
// from DllMain beyond constructing a trivial static object. See
// imfw/scheme_registry.hpp for the seam and how a test injects a fake.

#pragma once

#include <imfw/engine.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace imfw {

class Scheme {
public:
    virtual ~Scheme() = default;

    // ---- identity and resources ------------------------------------------

    // Data policy: every scheme owns its data entirely. The framework never
    // loads, holds or passes a code table; the scheme decides where its data
    // lives (system directory .dat, multi-pack store, embedded resource) and
    // hands each engine its own view through create_engine(). A configuration
    // tool signals data changes through reload_data() (see below).
    // No dat_filename / load_fallback on purpose — those were the old
    // framework-managed-table contract (xingma gap G2 sentinel), removed when
    // data ownership moved fully into the schemes.

    // UI server window class returned by ImeInquire (IMM32 registers it).
    // Hard limit: at most 15 characters. imm32 copies what ImeInquire returns
    // into IMEINFOEX.wszUIClass[16] / IMEDPI.szUIClass[16] with StringCchCopyW
    // and then looks the class up by the *truncated* name; a longer name makes
    // GetClassInfoW fail and imm32 rejects the IME with fLoadFlag=1, which
    // shows up only as "the IME never loads" (see PinyinScheme for the assert).
    virtual const wchar_t* ui_server_class() const = 0;
    // Window class names for the composition / candidate / toolbar windows.
    // Per-IME names let two IME DLLs coexist in one process.
    virtual const wchar_t* composition_class() const = 0;
    virtual const wchar_t* candidate_class() const = 0;
    virtual const wchar_t* toolbar_class() const = 0;

    // Short label shown in the toolbar while this IME is active ("拼"). This
    // names the *input method*, not its state: the state is the next virtual.
    virtual const wchar_t* toolbar_label() const = 0;

    // Toolbar state text for an open/closed IME ("开" / "关"), painted after
    // the label as "拼:开". Empty (the default) means "this scheme has no
    // state to show" and the toolbar keeps the bare label.
    //
    // This split is the fix for a wrong mental model: Shift switches the IME's
    // Chinese/English state, not the keyboard layout. The shell's own tray
    // indicator follows the *layout* (a red 中 that must NOT change when Shift
    // is pressed); the one thing that can show the state is our own toolbar,
    // and it can only do so if the state text comes from the scheme that owns
    // the language.
    virtual const wchar_t* toolbar_state_label(bool open) const {
        (void)open;
        return nullptr;
    }

    // ---- key policy --------------------------------------------------------
    // Framework gap G4: the framework used to hardcode selection and paging
    // keys, so an engine could neither see those keys nor redefine them.
    // The policy now lives here with the classic defaults; a scheme may
    // override either half. The framework still routes backspace and Esc
    // itself and still only consumes selection/page keys mid-composition
    // with candidates on screen (otherwise they reach the application).
    //
    // The scheme additionally owns its input alphabet:
    //   input_char(vk) translates a virtual key to a code point (pinyin:
    //   a-z and the apostrophe; a shape IME: its letter range). Uppercase
    //   is normalized by the scheme.

    // Canonical code point for a virtual key, or 0 when the key is not part
    // of this IME's input alphabet.
    virtual char32_t input_char(std::uint32_t vk) const = 0;

    // True when feed() accepts this code point.
    virtual bool is_input_char(char32_t cp) const = 0;

    // Keys that pick a candidate from the pager: space selects the first,
    // digits '1'..'9' select by page-relative index. Default matches the
    // classic behaviour.
    virtual bool is_selection_key(char32_t ch) const {
        return ch == U' ' || (ch >= U'1' && ch <= U'9');
    }

    // Paging direction for a key: +1 next page, -1 previous page, 0 when
    // the key does not page. Default: '-' and ',' previous; '=' and '.'
    // next.
    virtual int page_key(char32_t ch) const {
        if (ch == U'-' || ch == U',') return -1;
        if (ch == U'=' || ch == U'.') return 1;
        return 0;
    }

    // ---- engine ----------------------------------------------------------

    // One engine instance per input context. The scheme builds it against
    // its own data (a pinyin CodeTable, a shape ShapeStore, ...); the
    // framework never touches that data.
    virtual std::unique_ptr<Engine> create_engine() const = 0;

    // ---- configuration and data reload --------------------------------------
    // Framework gap G3: ImeConfigure used to always decline (FALSE), forcing
    // users into the start menu / command line. When the scheme can show its
    // own configuration UI (in-process dialog or launching an external tool),
    // implement this hook: `parent` is the caller's window handle (opaque
    // here so the interface stays free of Windows headers). Return true when
    // the request was handled — the framework then reloads sessions, which
    // calls reload_data() first (below), and reports success to IMM32. The
    // default keeps declining.
    virtual bool configure(void* parent) const {
        (void)parent;
        return false;
    }

    // Called by the framework before sessions are rebuilt (ImeConfigure
    // success path and reload_sessions). Re-read data files, swap packs or
    // re-derive whatever the scheme's engines need; create_engine() picks
    // the new state up. The default is a no-op for schemes whose data cannot
    // change at runtime.
    virtual void reload_data() const {}
};

// The scheme this binary was assembled with, or the one a test installed
// (imfw::SchemeRegistry::install). Defined by the framework over the assembly's
// scheme_factory(), so swapping or faking a scheme no longer means touching
// this layer or redefining this symbol.
Scheme& scheme_instance();

}  // namespace imfw
