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

#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef SM_CXSCREEN
#define SM_CXSCREEN 0
#endif
#ifndef SM_CYSCREEN
#define SM_CYSCREEN 1
#endif

namespace imfw {
namespace {

struct UiContext {
    ui::CompositionWindow composition;
    ui::CandidateWindow candidates;
    ui::Toolbar toolbar;
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

// Every window is placed AND sized here, in one SetWindowPos per window per
// call, and only when the geometry actually changed (see
// UiWindow::apply_geometry). The composition window measures its own text in
// update() but no longer calls SetWindowPos itself: two owners of the same
// window geometry is how "zhongguo" ended up clipped to the width of "zhong".
void place_windows(UiContext& ctx, const POINT& anchor) {
    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);

    int x = anchor.x + 12;
    int y = anchor.y + 20;
    if (x + ctx.composition.width() > screen_w && screen_w > 0) {
        x = screen_w - ctx.composition.width() - 8;
    }
    if (x < 0) x = 0;
    ctx.composition.apply_geometry(x, y, ctx.composition.width(),
                                   ctx.composition.height());

    y += ctx.composition.height();
    if (y + ctx.candidates.height() > screen_h && screen_h > 0) {
        y = anchor.y - ctx.candidates.height() - 4;
        if (y < 0) y = 0;
    }
    ctx.candidates.apply_geometry(x, y, ctx.candidates.width(),
                                  ctx.candidates.height());

    g_last_geometry.comp_x = x;
    g_last_geometry.comp_y = anchor.y + 20;
    g_last_geometry.comp_w = ctx.composition.width();
    g_last_geometry.comp_h = ctx.composition.height();
    g_last_geometry.cand_x = x;
    g_last_geometry.cand_y = y;
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
    place_windows(*ctx, caret_anchor());
}

void UiHide(rpimm::InputContext hIMC) {
    UiContext* ctx = store().find(hIMC);
    if (ctx == nullptr) return;
    ctx->composition.hide();
    ctx->candidates.hide();
}

void UiDestroy(rpimm::InputContext hIMC) {
    store().erase(hIMC);
}

void UiToolbarShow(rpimm::InputContext hIMC) {
    UiContext* ctx = store().get(hIMC);
    if (ctx == nullptr) return;
    const Scheme& scheme = scheme_instance();
    ctx->toolbar.update(g_hInst, scheme.toolbar_class(), scheme.toolbar_label());
}

void UiToolbarHide(rpimm::InputContext hIMC) {
    UiContext* ctx = store().find(hIMC);
    if (ctx == nullptr) return;
    ctx->toolbar.hide();
}

UiCounters UiCountersSnapshot() {
    const ui::UiStats s = ui::ui_stats();
    return UiCounters{s.place_calls, s.place_changes, s.show_real,
                      s.invalidates};
}

UiGeometry UiLastGeometry() { return g_last_geometry; }

namespace {

// Glyph probe for the candidate font. The shipped subset is U+4E00-U+9FA5, so
// a ranked character from another block (extension A, compatibility) used to
// reach the candidate list and paint as a tofu box; picking it inserted a box
// into the document. GetGlyphIndicesW answers the question with the *same*
// font the window paints with, and the answer is cached: `shi` alone has 95
// candidates.
class GlyphProbe {
public:
    GlyphProbe() {
        InitializeCriticalSection(&cs_);
        dc_ = CreateCompatibleDC(nullptr);
        font_ = ui::CreateUiFont(16);
        if (dc_ != nullptr && font_ != nullptr) {
            previous_ = SelectObject(dc_, font_);
        }
    }

    ~GlyphProbe() {
        if (dc_ != nullptr) {
            if (previous_ != nullptr) SelectObject(dc_, previous_);
            DeleteDC(dc_);
        }
        if (font_ != nullptr) DeleteObject(font_);
        DeleteCriticalSection(&cs_);
    }

    bool has(char32_t codepoint) {
        // Anything outside the BMP is not in our data set; and a probe that
        // cannot run at all must not hide text.
        if (dc_ == nullptr || font_ == nullptr || codepoint > 0xFFFF) return true;

        EnterCriticalSection(&cs_);
        const auto cached = cache_.find(codepoint);
        if (cached != cache_.end()) {
            const bool value = cached->second;
            LeaveCriticalSection(&cs_);
            return value;
        }
        LeaveCriticalSection(&cs_);

        const wchar_t wide = static_cast<wchar_t>(codepoint);
        WORD index = 0;
        const DWORD result =
            GetGlyphIndicesW(dc_, &wide, 1, &index, GGI_MARK_NONEXISTING_GLYPHS);
        const bool present = result != GDI_ERROR && index != 0xFFFF;

        EnterCriticalSection(&cs_);
        cache_[codepoint] = present;
        LeaveCriticalSection(&cs_);
        return present;
    }

private:
    CRITICAL_SECTION cs_;
    HDC dc_ = nullptr;
    HFONT font_ = nullptr;
    HGDIOBJ previous_ = nullptr;
    std::map<char32_t, bool> cache_;
};

GlyphProbe& glyph_probe() {
    static GlyphProbe instance;  // first use, never in DllMain
    return instance;
}

}  // namespace

bool UiCanRender(char32_t codepoint) { return glyph_probe().has(codepoint); }

}  // namespace imfw
