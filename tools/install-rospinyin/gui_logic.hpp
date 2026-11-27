// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Windows-free logic for the GUI installer shell. The Win32 adapter (gui.cpp)
// owns the controls, the message loop and the actual ExitWindowsEx call; this
// header owns only the decisions and text assembly so they can be asserted on
// the Linux host, exactly like install_steps.hpp / platform_guard.hpp.
//
// AC map (S0): AC1 version strings, AC2 agreement gate, AC4 whats-new text,
// AC5-8 reboot choice/countdown/failure. AC3 (scrollable window) and the
// visual parts are guest-only and deliberately not represented here.

#ifndef ROSPINYIN_GUI_LOGIC_HPP
#define ROSPINYIN_GUI_LOGIC_HPP

#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

// NOTE: there is intentionally no version macro here. The single source of
// truth is ime/version.h (ROSPINYIN_VERSION_STRING); gui.cpp includes it and
// passes the value into make_window_texts(). This header only tests the
// assembly logic with caller-supplied strings, so no second literal can drift.

namespace rospinyin {
namespace installer {
namespace gui {

struct WindowTexts {
    std::wstring title;          // caption bar
    std::wstring version_label;  // main-surface STATIC
};

// AC1: both surfaces derive from one string; callers must pass the value read
// from version.h, never a second literal.
inline WindowTexts make_window_texts(std::wstring_view version) {
    WindowTexts out;
    out.title = L"RosPinyin " + std::wstring(version) + L" 安装器";
    out.version_label = L"版本：" + std::wstring(version);
    return out;
}

// version.h is a narrow literal (RC-style); convert it once at the call site.
inline WindowTexts make_window_texts(const char* version) {
    if (!version) return make_window_texts(L"");
    return make_window_texts(
        std::wstring(version, version + std::strlen(version)));
}

// AC2: the install button is disabled until the user checks the agreement box.
// The state is deliberately not persisted (S0 boundary 2: re-check every run).
class AgreementState {
public:
    void set_checked(bool checked) noexcept { checked_ = checked; }
    bool install_enabled() const noexcept { return checked_; }
    bool checked() const noexcept { return checked_; }
private:
    bool checked_ = false;
};

// DEF-001: the agreement checkbox used BS_CHECKBOX (manual toggle) while the
// Win32 adapter never wrote BM_SETCHECK back, so clicks could not be seen.
// Style bits themselves are Win32-only, so keep the *decision* here: gui.cpp
// derives BS_AUTOCHECKBOX / WS_TABSTOP from it and the Linux host test can
// assert that neither property is silently dropped in a later edit.
struct AgreementControlSpec {
    bool auto_toggle;         // BS_AUTOCHECKBOX, not BS_CHECKBOX
    bool keyboard_focusable;  // WS_TABSTOP + dialog navigation in message loop
};

inline constexpr AgreementControlSpec agreement_control_spec() noexcept {
    return {true, true};
}

// AC4: whats-new text is carried verbatim under a fixed heading. The text
// itself comes from the whatsnew RCDATA, so a release only edits that file.
inline std::wstring format_whatsnew(std::wstring_view body) {
    return L"本版更新：\n" + std::wstring(body);
}

// AC5-8: reboot decision as a small state machine. No Win32 call happens in
// this header; the adapter supplies a callback that performs ExitWindowsEx.
enum class RebootChoice {
    No,   // completion dialog "No" -> keep the manual-reboot hint only
    Yes,  // completion dialog "Yes" -> enter the 10-second countdown
};

class RebootController {
public:
    // Remember the action; choosing Yes only enters the countdown, it does
    // not fire the action immediately (AC6).
    void run_choice(RebootChoice choice, std::function<void()> on_reboot) {
        choice_ = choice;
        on_reboot_ = std::move(on_reboot);
        armed_ = (choice == RebootChoice::Yes);
        elapsed_ = false;
    }

    void cancel_countdown() noexcept { armed_ = false; }

    // AC7: reaching zero fires the registered action exactly once; repeated
    // ticks (or a second call) never fire it again.
    void countdown_elapsed() {
        if (!armed_ || elapsed_) return;
        elapsed_ = true;
        if (on_reboot_) on_reboot_();
    }

    void note_reboot_failed() noexcept { reboot_failed_ = true; }
    bool reboot_failed() const noexcept { return reboot_failed_; }
    // AC8: a reboot failure must not downgrade the completed installation.
    bool install_failed() const noexcept { return false; }

private:
    RebootChoice choice_ = RebootChoice::No;
    std::function<void()> on_reboot_;
    bool armed_ = false;
    bool elapsed_ = false;
    bool reboot_failed_ = false;
};

// AC8: shown when the reboot API fails; installation remains a success.
inline std::wstring reboot_failure_message() {
    return L"安装已完成，但自动重启失败。\n请手动执行 shutdown /r 后生效。";
}

}  // namespace gui
}  // namespace installer
}  // namespace rospinyin

#endif  // ROSPINYIN_GUI_LOGIC_HPP
