// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Implementation of the ime_ui.h bridge. Per-HIMC windows are created lazily
// on the first UiShow call (i.e. from an IMM32 key entry point), never from
// DllMain. Window state is destroyed when the IME is deselected. All window
// classes and the toolbar label come from the Scheme, so this file serves
// every assembled IME unchanged.

#include "ime_ui.h"
#include "ime_host.h"

#include "candidate_window.h"
#include "composition_window.h"
#include "toolbar.h"
#include "ui_window.h"

#include <windows.h>

#include <imfw/scheme.hpp>
#include <imfw/toolbar_text.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace imfw {
namespace {

struct UiContext {
    ui::CompositionWindow composition;
    ui::CandidateWindow candidates;
    ui::Toolbar toolbar;
    // Anchor captured once when a composition starts. Moving the popups on
    // every keystroke (caret + measured width) is what made them crawl across
    // the screen while typing and jump while paging.
    POINT anchor = {0, 0};
    bool anchored = false;
    bool flip = false;  // caret too close to the bottom: draw above it
    // What the toolbar is currently showing. The label carries fOpen and
    // fOpen moves with no window message to tell us, so the painted value is
    // remembered here: that is what makes the per-key sync a bool compare
    // instead of a repaint.
    bool toolbar_open = true;
    bool toolbar_full = false;
    bool toolbar_shown = false;
};

// Same locking rationale as host/session_store.cpp: CRITICAL_SECTION keeps
// libwinpthread out of the IME PE on MinGW.
class UiStore {
public:
    UiStore() { InitializeCriticalSection(&cs_); }
    ~UiStore() { DeleteCriticalSection(&cs_); }

    UiContext* find(rpimm::InputContext hIMC) {
        if (hIMC == nullptr) return nullptr;
        EnterCriticalSection(&cs_);
        UiContext* result = nullptr;
        const auto it = contexts_.find(hIMC);
        if (it != contexts_.end()) result = it->second.get();
        LeaveCriticalSection(&cs_);
        return result;
    }

    UiContext* get(rpimm::InputContext hIMC) {
        if (hIMC == nullptr) return nullptr;
        EnterCriticalSection(&cs_);
        auto& slot = contexts_[hIMC];
        if (!slot) slot = std::make_unique<UiContext>();
        UiContext* result = slot.get();
        LeaveCriticalSection(&cs_);
        return result;
    }

    void erase(rpimm::InputContext hIMC) {
        if (hIMC == nullptr) return;
        EnterCriticalSection(&cs_);
        contexts_.erase(hIMC);
        LeaveCriticalSection(&cs_);
    }

private:
    CRITICAL_SECTION cs_;
    std::map<rpimm::InputContext, std::unique_ptr<UiContext>> contexts_;
};

UiStore& store() {
    static UiStore instance;  // constructed on first use, never in DllMain
    return instance;
}

// Written by place_windows(), read by the host trace (UiLastGeometry).
UiGeometry g_last_geometry = {};
int g_anchor_from_caret = 0;

POINT caret_anchor() {
    POINT pt{};
    const HWND focus = GetFocus();
    if (focus != nullptr && GetCaretPos(&pt)) {
        ClientToScreen(focus, &pt);
        g_anchor_from_caret = 1;
        return pt;
    }
    // No focus or no caret: fall back to the mouse. Recorded because this is
    // the only way the popup can end up somewhere unrelated to the input
    // field, and a trace that cannot tell the two apart is useless when the
    // user reports "the box jumped".
    g_anchor_from_caret = 0;
    pt.x = 0;
    pt.y = 0;
    GetCursorPos(&pt);
    return pt;
}

// How much room the popup group needs at most. Used to clamp the anchor so it
// never has to be recomputed later: a clamp that reads the window's *measured*
// width moves the box every time the text grows a letter.
constexpr int kPopupRightMargin = 300;
constexpr int kPopupBottomMargin = 260;

// Every window is placed AND sized here, in one SetWindowPos per window per
// call, and only when the geometry actually changed (see
// UiWindow::apply_geometry). The composition window measures its own text in
// update() but no longer calls SetWindowPos itself: two owners of the same
// window geometry is how "zhongguo" ended up clipped to the width of "zhong".
//
// The anchor comes from the context, not from the live caret, and x is
// computed from the anchor alone -- deliberately no measured width in the
// formula. Typing another letter or flipping a page must not move a window the
// user is currently reading.
void place_windows(UiContext& ctx, const POINT& anchor) {
    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);

    int x = anchor.x + 12;
    if (screen_w > 0 && x > screen_w - kPopupRightMargin) {
        x = screen_w - kPopupRightMargin;
    }
    if (x < 0) x = 0;

    int comp_y = anchor.y + 20;
    int cand_y = comp_y + ctx.composition.height();
    if (ctx.flip) {
        const int bottom = anchor.y - 4;
        cand_y = bottom - ctx.candidates.height();
        comp_y = cand_y - ctx.composition.height();
        if (comp_y < 0) comp_y = 0;
    }
    ctx.composition.apply_geometry(x, comp_y, ctx.composition.width(),
                                   ctx.composition.height());
    ctx.candidates.apply_geometry(x, cand_y, ctx.candidates.width(),
                                  ctx.candidates.height());

    g_last_geometry.comp_x = x;
    g_last_geometry.comp_y = comp_y;
    g_last_geometry.comp_w = ctx.composition.width();
    g_last_geometry.comp_h = ctx.composition.height();
    g_last_geometry.cand_x = x;
    g_last_geometry.cand_y = cand_y;
    g_last_geometry.cand_w = ctx.candidates.width();
    g_last_geometry.cand_h = ctx.candidates.height();
    g_last_geometry.anchor_x = anchor.x;
    g_last_geometry.anchor_y = anchor.y;
    g_last_geometry.anchor_from_caret = g_anchor_from_caret;
}

}  // namespace

void UiShow(rpimm::InputContext hIMC, const wchar_t* composition_text,
            const std::vector<std::string>& segments,
            const CandidatePager& pager) {
    UiContext* ctx = store().get(hIMC);
    if (ctx == nullptr) return;

    const Scheme& scheme = scheme_instance();
    ctx->composition.update(g_hInst, scheme.composition_class(),
                            composition_text, segments);
    ctx->candidates.update(g_hInst, scheme.candidate_class(), pager);

    if (!ctx->composition.visible() && !ctx->candidates.visible()) return;
    // Anchor once per composition. Re-reading the caret on every key was the
    // other half of "the box drifts while I type": the caret moves with every
    // committed character, and the IME's own popup followed it.
    if (!ctx->anchored) {
        const POINT anchor = caret_anchor();
        const int screen_h = GetSystemMetrics(SM_CYSCREEN);
        ctx->anchor = anchor;
        ctx->flip = screen_h > 0 && anchor.y + kPopupBottomMargin > screen_h;
        ctx->anchored = true;
    }
    place_windows(*ctx, ctx->anchor);
}

void UiHide(rpimm::InputContext hIMC) {
    UiContext* ctx = store().find(hIMC);
    if (ctx == nullptr) return;
    ctx->composition.hide();
    ctx->candidates.hide();
    // Next composition starts wherever the caret is *then*.
    ctx->anchored = false;
}

void UiDestroy(rpimm::InputContext hIMC) {
    store().erase(hIMC);
}

void UiToolbarShow(rpimm::InputContext hIMC, bool ime_open, bool full_glyph) {
    UiContext* ctx = store().get(hIMC);
    if (ctx == nullptr) return;
    const Scheme& scheme = scheme_instance();
    const std::wstring text =
        imfw::toolbar_text_with_mode(scheme, ime_open, full_glyph);
    ctx->toolbar_open = ime_open;
    ctx->toolbar_full = full_glyph;
    ctx->toolbar_shown = true;
    ctx->toolbar.update(g_hInst, scheme.toolbar_class(), text.c_str());
}

void UiToolbarRefresh(rpimm::InputContext hIMC, bool full_glyph) {
    UiContext* ctx = store().find(hIMC);
    if (ctx == nullptr || !ctx->toolbar_shown) return;
    if (ctx->toolbar_full == full_glyph) return;
    UiToolbarShow(hIMC, ctx->toolbar_open, full_glyph);
}

void UiToolbarSetOpen(rpimm::InputContext hIMC, bool ime_open) {
    UiContext* ctx = store().find(hIMC);
    if (ctx == nullptr || !ctx->toolbar_shown) return;
    if (ctx->toolbar_open == ime_open) return;
    UiToolbarShow(hIMC, ime_open, ctx->toolbar_full);
}

void UiToolbarHide(rpimm::InputContext hIMC) {
    UiContext* ctx = store().find(hIMC);
    if (ctx == nullptr) return;
    ctx->toolbar.hide();
    ctx->toolbar_shown = false;
}

UiCounters UiCountersSnapshot() {
    const ui::UiStats s = ui::ui_stats();
    return UiCounters{s.place_calls, s.place_changes, s.show_real,
                      s.invalidates};
}

UiGeometry UiLastGeometry() { return g_last_geometry; }

}  // namespace imfw
