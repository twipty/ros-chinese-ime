// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Per-input-method plugin contract. The framework owns everything an IME
// has in common — the IMM32 entry points, key routing, candidate paging,
// composition/candidate/toolbar windows and .dat loading. A concrete input
// method (pinyin today, a shape IME next) only supplies this Scheme.
//
// The interface is deliberately free of Windows headers: `vk` is a plain
// virtual-key number, so pinyin/scheme code builds and is unit-tested on
// Linux exactly like the rest of the engine.
//
// Wiring: the assembling module (e.g. ime/scheme_reg.cpp) defines
// scheme_instance(); the framework calls it lazily — never from DllMain
// beyond constructing a trivial static object.

#pragma once

#include <imfw/code_table.hpp>
#include <imfw/engine.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace imfw {

class Scheme {
public:
    virtual ~Scheme() = default;

    // ---- identity and resources ------------------------------------------

    // Data file searched in the system directory, e.g. L"rospinyin.dat".
    // Return nullptr (or an empty string) when the scheme owns its data
    // entirely (e.g. a shape IME with its own multi-pack store): the
    // framework then never touches the system directory, and the empty
    // framework table passed to create_engine() is simply ignored.
    virtual const wchar_t* dat_filename() const = 0;

    // UI server window class returned by ImeInquire (IMM32 registers it).
    virtual const wchar_t* ui_server_class() const = 0;
    // Window class names for the composition / candidate / toolbar windows.
    // Per-IME names let two IME DLLs coexist in one process.
    virtual const wchar_t* composition_class() const = 0;
    virtual const wchar_t* candidate_class() const = 0;
    virtual const wchar_t* toolbar_class() const = 0;

    // Short label shown in the toolbar while this IME is active ("拼").
    virtual const wchar_t* toolbar_label() const = 0;

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

    // One engine instance per input context, sharing the loaded table.
    virtual std::unique_ptr<Engine> create_engine(const CodeTable& table) const = 0;

    // ---- data --------------------------------------------------------------

    // Fill a fallback table when the .dat file is missing or corrupt so the
    // IME stays usable on a bare installation. Return false when the IME
    // has no fallback (the IME then simply does not compose).
    virtual bool load_fallback(CodeTable& out) const = 0;

    // ---- configuration -------------------------------------------------------
    // Framework gap G3: ImeConfigure used to always decline (FALSE), forcing
    // users into the start menu / command line. When the scheme can show its
    // own configuration UI (in-process dialog or launching an external tool),
    // implement this hook: `parent` is the caller's window handle (opaque
    // here so the interface stays free of Windows headers). Return true when
    // the request was handled — the framework then reloads data and sessions
    // (so a config tool may swap packs or rewrite the .dat) and reports
    // success to IMM32. The default keeps declining.
    virtual bool configure(void* parent) const {
        (void)parent;
        return false;
    }
};

// Defined by the assembling IME module (see ime/scheme_reg.cpp).
Scheme& scheme_instance();

}  // namespace imfw
