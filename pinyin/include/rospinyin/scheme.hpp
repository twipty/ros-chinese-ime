// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// PinyinScheme: the RosPinyin implementation of the reusable framework's
// plugin contract (imfw/scheme.hpp). Everything pinyin-specific lives here
// or in the session/normalization code — the framework itself stays free of
// pinyin knowledge so a shape-based IME can reuse it unchanged.

#pragma once

#include <imfw/code_table.hpp>
#include <imfw/scheme.hpp>

#include <cstdint>
#include <memory>

namespace rospinyin {

class PinyinScheme final : public imfw::Scheme {
public:
    PinyinScheme();

    // ---- identity and resources ----
    const wchar_t* ui_server_class() const override;
    const wchar_t* composition_class() const override;
    const wchar_t* candidate_class() const override;
    const wchar_t* toolbar_class() const override;
    const wchar_t* toolbar_label() const override;
    // "开" / "关": the toolbar's Chinese/English indicator. The shell tray's
    // red 中 belongs to the keyboard layout and must not move on Shift; this
    // string is the state, and it is the only thing that does move.
    const wchar_t* toolbar_state_label(bool open) const override;

    // ---- key policy ----
    char32_t input_char(std::uint32_t vk) const override;
    bool is_input_char(char32_t cp) const override;
    // M3: Chinese punctuation is a language fact shared by every Chinese
    // scheme -> forward to the ONE framework table, never build a private one.
    std::u32string punctuation(char32_t ch) const override;

    // ---- engine ----
    std::unique_ptr<imfw::Engine> create_engine() const override;

    // ---- data ----
    // Data is scheme-owned: the system-directory .dat when present, the
    // built-in fallback table otherwise (see dat_source.hpp). The framework
    // never sees the table. reload_data() may run on a const scheme from a
    // foreign thread while engines still hold the previous generation; the
    // swap is a single pointer write under the framework's session lock.
    void reload_data() const override;

private:
    // Mutable: reload_data() is logically const for the framework (the
    // scheme's observable behaviour — what it feeds engines — stays the
    // same kind of table); the pointer swap replaces the generation.
    // Shared so in-flight sessions keep a valid table across reloads.
    mutable std::shared_ptr<const imfw::CodeTable> table_;
};

}  // namespace rospinyin
