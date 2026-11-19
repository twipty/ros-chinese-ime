// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Win32Host: the platform half of the key policy (see win32_host.h). Every
// Win32 call the policy can trigger is in this file -- which is what makes the
// policy itself linkable and testable on the build host.

#include "win32_host.h"

#include "ime_ui.h"
#include "trace_line.h"

// IMM32 client API (ImmGetOpenStatus / ImmSetOpenStatus). The IME-side ABI in
// imm32_abi.h deliberately does not pull this header, but the *client* half is
// needed here for the open-status alignment described at AlignOpenStatus().
#include <imm.h>

#include <imfw/utf8.hpp>

namespace imfw {
namespace win32 {

bool Win32Host::is_open() {
    return ImmGetOpenStatus(reinterpret_cast<HIMC>(hIMC_)) != FALSE;
}

void Win32Host::set_open(bool open) {
    ImmSetOpenStatus(reinterpret_cast<HIMC>(hIMC_), open ? TRUE : FALSE);
    IMFW_TRACE("opentoggle -> ").d(open ? 1 : 0);
}

void Win32Host::show(const std::string& composition,
                     const std::vector<std::string>& segments,
                     const imfw::CandidatePager& pager) {
    const std::wstring text = imfw::utf8_to_wide(composition);
    imfw::UiShow(hIMC_, text.c_str(), segments, pager);
#if IMFW_TRACE_ON
    // Where the two popups ended up, and what the anchor was. The key counters
    // cannot answer "the box moved while I was paging": they only say that a
    // SetWindowPos happened, not which window or from/to where. Printed only
    // when the geometry changed, so a page-flip that moves nothing is silent
    // and a page-flip that moves something is one obvious line.
    const imfw::UiGeometry g = imfw::UiLastGeometry();
    static imfw::UiGeometry last = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    const bool changed = g.comp_x != last.comp_x || g.comp_y != last.comp_y ||
                         g.comp_w != last.comp_w || g.comp_h != last.comp_h ||
                         g.cand_x != last.cand_x || g.cand_y != last.cand_y ||
                         g.cand_w != last.cand_w || g.cand_h != last.cand_h;
    if (changed) {
        last = g;
        IMFW_TRACE("uigeom comp=").d(g.comp_x).s(",").d(g.comp_y).s(",")
            .d(g.comp_w).s(",").d(g.comp_h)
            .s(" cand=").d(g.cand_x).s(",").d(g.cand_y).s(",")
            .d(g.cand_w).s(",").d(g.cand_h)
            .s(" anchor=").d(g.anchor_x).s(",").d(g.anchor_y)
            .s(" caret=").d(g.anchor_from_caret);
    }
#endif
}

void Win32Host::hide() { imfw::UiHide(hIMC_); }

bool Win32Host::can_render(char32_t codepoint) {
    return imfw::UiCanRender(codepoint);
}

void Win32Host::on_feed(char32_t input, const imfw::Result& result) {
    // A local buffer, not std::string(1, c).c_str(): the temporary would be
    // destroyed before Line's destructor writes the line.
    char shown[2] = {static_cast<char>(input < 0x80 ? input : '?'), '\0'};
    const imfw::UiCounters ui = imfw::UiCountersSnapshot();
    IMFW_TRACE("feed ch=").s(shown)
        .s(" act=").d(static_cast<long long>(result.action))
        .s(" comp=[").s(result.composition.c_str()).s("] cand=")
        .d(static_cast<long long>(result.candidates.size()))
        // Window work done for this key: moves+resizes that actually changed
        // something, and repaint requests. P0-3 measured the "composition
        // window jumps" report against these two numbers.
        .s(" wmove=").d(static_cast<long long>(ui.place_changes))
        .s(" wrep=").d(static_cast<long long>(ui.invalidates));
}

void Win32Host::on_candidates_filtered(std::size_t dropped, std::size_t total,
                                       bool kept_all) {
    IMFW_TRACE("cand-filter dropped ").d(static_cast<long long>(dropped))
        .s(" of ").d(static_cast<long long>(total))
        .s(kept_all ? " -> font cannot draw CJK, keeping all"
                    : " (no glyph)");
}

}  // namespace win32
}  // namespace imfw
