// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Bridge between the IMM32 layer (host) and the UI windows (framework/platform/win32/ui).
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
// full_glyph is ImeSessionState::full_glyph; when true the two popups split
// their text into WenJin plane fonts. The UI layer never queries state itself.
void UiShow(rpimm::InputContext hIMC, const wchar_t* composition_text,
           const std::vector<std::string>& segments,
           const CandidatePager& pager, bool full_glyph);

// Hide both windows without destroying them (composition committed/cancelled,
// input context deactivated). Safe to call repeatedly.
void UiHide(rpimm::InputContext hIMC);

// Destroy the per-HIMC windows and state (called when the IME is deselected).
void UiDestroy(rpimm::InputContext hIMC);

// Show the floating toolbar (bottom-right of the screen) with the scheme's
// label, the IME state ("拼:开" / "拼:关") and the session's glyph mode
// ("[快]" / "[全]"). `ime_open` is fOpen as the IMM32 layer reads it and
// `full_glyph` is ImeSessionState::full_glyph -- the UI layer never queries
// either itself.
void UiToolbarShow(rpimm::InputContext hIMC, bool ime_open, bool full_glyph);

// Repaint the toolbar if `ime_open` differs from what it currently shows.
// Cheap enough for a key path (one bool compare) and necessary there: fOpen
// changes without any window message reaching us -- our own Shift gesture, and
// imm32's Ctrl+Space which never enters this DLL.
void UiToolbarSetOpen(rpimm::InputContext hIMC, bool ime_open);

// Hide the toolbar (IME deselected / context deactivated).
void UiToolbarHide(rpimm::InputContext hIMC);

// Repaint after a [快]/[全] flip (Ctrl+Shift+G). The caller owns the mode
// state; this function only renders it and updates the remembered caption.
void UiToolbarRefresh(rpimm::InputContext hIMC, bool full_glyph);

// Window-operation counters for the diagnostic trace. Kept here (instead of
// exposing framework/platform/win32/ui/ui_window.h) so the IMM32 layer never depends on UI
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

}  // namespace imfw
