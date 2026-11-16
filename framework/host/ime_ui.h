// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Bridge between the IMM32 layer (host) and the UI windows (framework/ui).
// All window classes come from the Scheme, so every assembled IME renders
// its own set of windows.

#pragma once

#include "imm32_abi.h"

#include <imfw/candidate_pager.hpp>

#include <string>
#include <vector>

namespace imfw {

// Show or refresh the composition and candidate windows for hIMC.
// composition_text is the raw input string; segments is the engine's
// segmentation (used to underline boundaries); pager holds the current
// candidate page. Windows are created here, never in DllMain.
void UiShow(rpimm::InputContext hIMC, const wchar_t* composition_text,
           const std::vector<std::string>& segments,
           const CandidatePager& pager);

// Hide both windows without destroying them (composition committed/cancelled,
// input context deactivated). Safe to call repeatedly.
void UiHide(rpimm::InputContext hIMC);

// Destroy the per-HIMC windows and state (called when the IME is deselected).
void UiDestroy(rpimm::InputContext hIMC);

// Show the floating toolbar (bottom-right of the screen) with the scheme's
// label while this IME is selected.
void UiToolbarShow(rpimm::InputContext hIMC);

// Hide the toolbar (IME deselected / context deactivated).
void UiToolbarHide(rpimm::InputContext hIMC);

}  // namespace imfw
