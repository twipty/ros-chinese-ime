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

// Window-operation counters for the diagnostic trace. Kept here (instead of
// exposing framework/ui/ui_window.h) so the IMM32 layer never depends on UI
// headers. Absolute values, monotonically increasing over the process.
struct UiCounters {
    unsigned place_calls;    // geometry attempts
    unsigned place_changes;  // attempts that really moved/resized a window
    unsigned shows;          // ShowWindow calls that were not a no-op
    unsigned invalidates;    // InvalidateRect calls
};
UiCounters UiCountersSnapshot();

// Last geometry handed to the two popup windows, for the diagnostic trace.
// "The popup moved while I was paging" cannot be answered from key counters:
// it needs the rectangle the windows ended up with and the anchor it was
// computed from. anchor_from_caret is 0 when GetCaretPos() failed and the
// mouse position was used as a fallback -- the one case where a stray mouse
// position can drag the popup somewhere else.
struct UiGeometry {
    int comp_x, comp_y, comp_w, comp_h;
    int cand_x, cand_y, cand_w, cand_h;
    int anchor_x, anchor_y;
    int anchor_from_caret;
};
UiGeometry UiLastGeometry();

// P1-2: can the candidate font actually draw this code point? Uses
// GetGlyphIndicesW against the same font the candidate window paints with,
// cached per process. Fails OPEN (returns true) when GDI cannot answer, so a
// missing implementation can never hide candidates.
bool UiCanRender(char32_t codepoint);

}  // namespace imfw
