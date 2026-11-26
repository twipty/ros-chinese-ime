// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Windows-free orchestration for the GUI installer's step1..4 sequence.
// The Win32/registry calls stay in the adapter; this header owns only the
// order, the error vocabulary and the user-facing Chinese messages so those
// can be tested on Linux. No Preload write, no default-layout switch.

#ifndef ROSPINYIN_INSTALLER_STEPS_HPP
#define ROSPINYIN_INSTALLER_STEPS_HPP

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace rospinyin {
namespace installer {

enum class InstallStep : int {
    CopyPayload = 1,       // step1: copy rospinyin.ime + rospinyin.dat
    RegisterLayout = 2,    // step2: ImmInstallIMEW -> dynamic HKL
    WriteLayoutText = 3,   // step3: Layout Text
    WriteDisplayName = 4,  // step4: Layout Display Name
};

inline constexpr std::array<InstallStep, 4> kInstallSteps = {
    InstallStep::CopyPayload,
    InstallStep::RegisterLayout,
    InstallStep::WriteLayoutText,
    InstallStep::WriteDisplayName,
};

// Numeric values match the existing CLI exit codes (install.cpp:56-63) so
// the GUI can show a familiar code without a second translation table.
enum class InstallErrorCode : int {
    Ok = 0,
    IoError = 1,
    NeedsReboot = 2,
    RegError = 3,
    VerifyFailed = 4,
    PlatformUnsupported = 5,
    PayloadCorrupt = 6,
};

inline constexpr std::wstring_view kInstallRegistryKey =
    L"SOFTWARE\\RosPinyin\\Install";

inline std::wstring_view step_name(InstallStep step) {
    switch (step) {
        case InstallStep::CopyPayload:
            return L"[1/4] 拷贝 rospinyin.ime + rospinyin.dat";
        case InstallStep::RegisterLayout:
            return L"[2/4] 注册输入法布局 (ImmInstallIMEW)";
        case InstallStep::WriteLayoutText:
            return L"[3/4] 写入 Layout Text";
        case InstallStep::WriteDisplayName:
            return L"[4/4] 写入 Layout Display Name";
    }
    return L"";
}

inline std::wstring_view error_message(InstallErrorCode code) {
    switch (code) {
        case InstallErrorCode::Ok:
            return L"安装完成，请重启后使用。";
        case InstallErrorCode::IoError:
            return L"文件读写失败，请检查临时目录与系统目录权限。";
        case InstallErrorCode::NeedsReboot:
            return L"文件被占用，需要重启后重试。";
        case InstallErrorCode::RegError:
            return L"输入法注册失败，请确认以管理员权限运行。";
        case InstallErrorCode::VerifyFailed:
            return L"文件校验失败，安装未生效。";
        case InstallErrorCode::PlatformUnsupported:
            return L"当前系统未列入已验证平台白名单，安装已取消。";
        case InstallErrorCode::PayloadCorrupt:
            return L"内嵌文件已损坏，请重新下载安装器。";
    }
    return L"未知错误。";
}

inline std::wstring_view next_action(InstallErrorCode code) {
    switch (code) {
        case InstallErrorCode::Ok:
            return L"请重启系统，然后用 Alt+Shift 切换到「中文(简体) - RosPinyin」。";
        case InstallErrorCode::IoError:
            return L"检查磁盘空间与权限后重试。";
        case InstallErrorCode::NeedsReboot:
            return L"请重启后重新运行安装程序。";
        case InstallErrorCode::RegError:
            return L"重启并以管理员权限重试。";
        case InstallErrorCode::VerifyFailed:
            return L"删除残留文件后重试。";
        case InstallErrorCode::PlatformUnsupported:
            return L"Windows 真机请使用命令行 /step1../step4。";
        case InstallErrorCode::PayloadCorrupt:
            return L"请重新下载安装器并核对 SHA256。";
    }
    return L"请记录错误码并反馈。";
}

// [全] mode shows tier-1 codepoints that ReactOS' built-in Droid font cannot
// draw. The 57 MB WenJin font pack is deliberately NOT part of the GUI payload
// (fonts stay on HTTP only), so the installer's job is to point at it. Keeping
// both user-facing surfaces here makes the URL and file list host-testable.
inline constexpr std::wstring_view kFontHintUrl =
    L"http://10.0.2.2:8000/fonts/wenjin-v2.100/";

// Permanent one-line note under the two buttons on the main window.
inline constexpr std::wstring_view kFontHintBanner =
    L"[全] 模式需另装文津宋体三片（HTTP 分发）；\n"
    L"必须三字体齐备才能正常显示，缺少任意一个的后果不可预知；\n"
    L"[快] 模式用系统 Droid 字体，无需安装。";

// Appended to the post-install message box. The three TTFs are plane-split
// members of one family: installing only one of them still leaves tofu in the
// other planes, so all three are named along with the checksum file.
inline std::wstring font_hint_message() {
    return std::wstring(L"字体提示：[全] 模式的生僻字需另装文津宋体。\n")
         + L"本安装器不附带字体，请从 HTTP 下载：\n"
         + L"  " + std::wstring(kFontHintUrl) + L"\n"
         + L"下载 WenJinMinchoP0-Regular.ttf、WenJinMinchoP2-Regular.ttf、\n"
         + L"WenJinMinchoP3-Regular.ttf（同一字体族，三者缺一不可），\n"
         + L"必须三字体齐备才能正常显示，缺少任意一个的后果不可预知。\n"
         + L"用同目录 SHA256SUMS.txt 校验后安装，并重启系统。\n"
         + L"[快] 模式使用系统 Droid 字体，无需安装额外字体。";
}

// `run(step, index)` returns Ok to continue or the first failing code to stop.
// `report(index, step, label)` is called before each step so the GUI can show
// progress even when that step fails.
template <typename RunStep, typename ReportProgress>
inline InstallErrorCode run_install_steps(RunStep run, ReportProgress report) {
    for (std::size_t i = 0; i < kInstallSteps.size(); ++i) {
        const InstallStep step = kInstallSteps[i];
        report(i + 1, step, step_name(step));
        const InstallErrorCode rc = run(step, i + 1);
        if (rc != InstallErrorCode::Ok) return rc;
    }
    return InstallErrorCode::Ok;
}

}  // namespace installer
}  // namespace rospinyin

#endif  // ROSPINYIN_INSTALLER_STEPS_HPP
