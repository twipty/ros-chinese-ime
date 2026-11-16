// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Implementation of the ime_ui.h bridge. Per-HIMC windows are created lazily
// on the first UiShow call (i.e. from an IMM32 key entry point), never from
// DllMain. Window state is destroyed when the IME is deselected.

#include "ime_ui.h"
#include "rospinyin.h"

#include "candidate_window.h"
#include "composition_window.h"
#include "ui_window.h"

#include <windows.h>

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

namespace rospinyin {
namespace {

struct UiContext {
    ui::CompositionWindow composition;
    ui::CandidateWindow candidates;
};

// Same locking rationale as ime/session_store.cpp: CRITICAL_SECTION keeps
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

POINT caret_anchor() {
    POINT pt{};
    const HWND focus = GetFocus();
    if (focus != nullptr && GetCaretPos(&pt)) {
        ClientToScreen(focus, &pt);
        return pt;
    }
    pt.x = 0;
    pt.y = 0;
    GetCursorPos(&pt);
    return pt;
}

void place_windows(UiContext& ctx, const POINT& anchor) {
    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);

    int x = anchor.x + 12;
    int y = anchor.y + 20;
    if (x + ctx.composition.width() > screen_w && screen_w > 0) {
        x = screen_w - ctx.composition.width() - 8;
    }
    if (x < 0) x = 0;
    ctx.composition.move_to(x, y);

    y += ctx.composition.height();
    if (y + ctx.candidates.height() > screen_h && screen_h > 0) {
        y = anchor.y - ctx.candidates.height() - 4;
        if (y < 0) y = 0;
    }
    ctx.candidates.move_to(x, y);
}

}  // namespace

void UiShow(rpimm::InputContext hIMC, const wchar_t* composition_text,
            const std::vector<std::string>& syllables,
            const CandidatePager& pager) {
    UiContext* ctx = store().get(hIMC);
    if (ctx == nullptr) return;

    ctx->composition.update(g_hInst, composition_text, syllables);
    ctx->candidates.update(g_hInst, pager);

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

}  // namespace rospinyin
