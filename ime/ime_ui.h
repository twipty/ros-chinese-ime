// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Bridge between the IMM32 layer and the UI windows (WBS 3.3.1/3.3.2).
// Kept in ime/ so both sides include the same declarations.

#pragma once

#include "imm32_abi.h"
#include <rospinyin/candidate_pager.hpp>

#include <string>
#include <vector>

namespace rospinyin {

// Show or refresh the composition and candidate windows for hIMC.
// composition_text is the raw pinyin string; syllables are the engine's
// segmentation (used to underline syllable boundaries); pager holds the
// current candidate page. Windows are created here, never in DllMain.
void UiShow(rpimm::InputContext hIMC, const wchar_t* composition_text,
            const std::vector<std::string>& syllables,
            const CandidatePager& pager);

// Hide both windows without destroying them (composition committed/cancelled,
// input context deactivated). Safe to call repeatedly.
void UiHide(rpimm::InputContext hIMC);

// Destroy the per-HIMC windows and state (called when the IME is deselected).
void UiDestroy(rpimm::InputContext hIMC);

}  // namespace rospinyin
