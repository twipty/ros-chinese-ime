// RosPinyin / toy scheme — SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// The injection seam's first consumer (PLAN-MODULARIZATION X2-a0). It is
// deliberately trivial and deliberately *not* pinyin: letters in, the same
// letters out, two candidates per keystroke so selection and paging have
// something to page. If this scheme needs a single line of framework code to
// work, the seam is not a seam.
//
// Build: cmake -DROSPINYIN_SCHEME=toy  (test build — no Chinese data at all)
//
// What it is useful for:
//   * proving a second scheme is a folder plus one scheme_factory(), not a
//     patch to framework/  (the G4 criterion, early);
//   * an IME that commits plain ASCII, i.e. one whose behaviour is visible on a
//     ReactOS install with no CJK font and no .dat — useful when the question
//     is "did IMM32 load and route keys at all", not "is the table right".

#pragma once

#include <imfw/engine.hpp>
#include <imfw/scheme.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace toy {

// Letters in, letters out. No table, no state beyond the composition, so the
// engine cannot hide a data-loading bug behind a wrong candidate.
class ToyEngine final : public imfw::Engine {
public:
    imfw::Result feed(char32_t codepoint) override;
    imfw::Result backspace() override;
    imfw::Result cancel() override;
    void on_select(bool selected) override;

private:
    // Snapshot of the current composition: Absorb after an edit, Passthrough
    // when there is nothing left to show.
    imfw::Result snapshot(imfw::Action action) const;

    std::u32string composition_;
};

class ToyScheme final : public imfw::Scheme {
public:
    // Window class names are per-IME so two IME DLLs can coexist in one
    // process; imm32 truncates the UI server name into a 16-char buffer, hence
    // the short names (see imfw/scheme.hpp).
    const wchar_t* ui_server_class() const override { return L"ToyImeUISrv"; }
    const wchar_t* composition_class() const override { return L"ToyImeComp"; }
    const wchar_t* candidate_class() const override { return L"ToyImeCand"; }
    const wchar_t* toolbar_class() const override { return L"ToyImeBar"; }
    const wchar_t* toolbar_label() const override { return L"toy"; }

    char32_t input_char(std::uint32_t vk) const override;
    bool is_input_char(char32_t cp) const override {
        return cp >= U'a' && cp <= U'z';
    }

    std::unique_ptr<imfw::Engine> create_engine() const override;
};

}  // namespace toy
