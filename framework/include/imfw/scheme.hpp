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
    // The framework itself owns paging (- = , .), selection (digits/space),
    // backspace and Esc. The scheme only translates its own input alphabet.

    // Canonical code point for a virtual key, or 0 when the key is not part
    // of this IME's input alphabet (pinyin: a-z and the apostrophe; a shape
    // IME: its letter range). Uppercase is normalized by the scheme.
    virtual char32_t input_char(std::uint32_t vk) const = 0;

    // True when feed() accepts this code point.
    virtual bool is_input_char(char32_t cp) const = 0;

    // ---- engine ----------------------------------------------------------

    // One engine instance per input context, sharing the loaded table.
    virtual std::unique_ptr<Engine> create_engine(const CodeTable& table) const = 0;

    // ---- data --------------------------------------------------------------

    // Fill a fallback table when the .dat file is missing or corrupt so the
    // IME stays usable on a bare installation. Return false when the IME
    // has no fallback (the IME then simply does not compose).
    virtual bool load_fallback(CodeTable& out) const = 0;
};

// Defined by the assembling IME module (see ime/scheme_reg.cpp).
Scheme& scheme_instance();

}  // namespace imfw
