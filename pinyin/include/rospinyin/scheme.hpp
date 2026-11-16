// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// PinyinScheme: the RosPinyin implementation of the reusable framework's
// plugin contract (imfw/scheme.hpp). Everything pinyin-specific lives here
// or in the session/normalization code — the framework itself stays free of
// pinyin knowledge so a shape-based IME can reuse it unchanged.

#pragma once

#include <imfw/scheme.hpp>

namespace rospinyin {

class PinyinScheme final : public imfw::Scheme {
public:
    // ---- identity and resources ----
    const wchar_t* dat_filename() const override;
    const wchar_t* ui_server_class() const override;
    const wchar_t* composition_class() const override;
    const wchar_t* candidate_class() const override;
    const wchar_t* toolbar_class() const override;
    const wchar_t* toolbar_label() const override;

    // ---- key policy ----
    char32_t input_char(std::uint32_t vk) const override;
    bool is_input_char(char32_t cp) const override;

    // ---- engine ----
    std::unique_ptr<imfw::Engine> create_engine(
        const imfw::CodeTable& table) const override;

    // ---- data ----
    bool load_fallback(imfw::CodeTable& out) const override;
};

}  // namespace rospinyin
