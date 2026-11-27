// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// GUI 安装器的三块纯逻辑（无 windows.h、无注册表、无文件系统）：
//   * platform_guard.hpp —— 严格平台白名单（当前仅 ReactOS）
//   * install_steps.hpp  —— step1..4 序列、错误码与中文提示
//   * payload_blob.hpp   —— payload 长度 / CRC / 消费字节校验
// 这些是路线 C 与 Windows 误装闸门在宿主侧唯一能直接验的部分。

#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_IMPLEMENTATION
#include "miniz.h"

#include "droid_font.hpp"
#include "gui_logic.hpp"
#include "install_steps.hpp"
#include "payload_blob.hpp"
#include "payload_base64.hpp"
#include "platform_guard.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace installer = rospinyin::installer;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "installer logic test failed: %s\n", what);
        ++failures;
    }
}

bool contains(std::wstring_view haystack, std::wstring_view needle) {
    return haystack.find(needle) != std::wstring_view::npos;
}

void test_platform_guard() {
    check(installer::is_supported_product_name(L"ReactOS"), "ReactOS allowed");
    check(installer::is_supported_product_name(L"reactos"),
          "lowercase reactos allowed");
    check(installer::is_supported_product_name(L"rEaCtOs"),
          "mixed case reactos allowed");

    check(!installer::is_supported_product_name(L"Windows 10 Pro"),
          "Windows rejected");
    check(!installer::is_supported_product_name(L"libernt"),
          "unregistered compatible branch rejected");
    check(!installer::is_supported_product_name(L""), "empty value rejected");
    check(!installer::is_supported_product_name(L"ReactOS Server"),
          "prefix-only match rejected");

    // 未来登记兼容分支后同一判据必须生效；不能只为一个字符串写死 if。
    const std::array<std::wstring_view, 2> future_allowlist = {
        L"ReactOS", L"libernt"};
    check(installer::product_name_in_list(L"libernt", future_allowlist.data(),
                                          future_allowlist.size()),
          "future registered branch allowed");
    check(!installer::product_name_in_list(L"Windows 11",
                                           future_allowlist.data(),
                                           future_allowlist.size()),
          "future allowlist still rejects Windows");
}

void test_install_steps() {
    std::vector<installer::InstallStep> executed;
    std::vector<std::size_t> reported;

    const installer::InstallErrorCode all_ok = installer::run_install_steps(
        [&](installer::InstallStep step, std::size_t index) {
            executed.push_back(step);
            check(index == executed.size(), "step index matches execution");
            return installer::InstallErrorCode::Ok;
        },
        [&](std::size_t index, installer::InstallStep,
            std::wstring_view label) {
            reported.push_back(index);
            check(!label.empty(), "step has a Chinese progress label");
        });

    check(all_ok == installer::InstallErrorCode::Ok, "all steps succeed");
    check(executed.size() == 4, "step1..4 executed exactly once");
    check(executed.at(0) == installer::InstallStep::CopyPayload,
          "step1 copies payload");
    check(executed.at(1) == installer::InstallStep::RegisterLayout,
          "step2 registers layout");
    check(executed.at(2) == installer::InstallStep::WriteLayoutText,
          "step3 writes Layout Text");
    check(executed.at(3) == installer::InstallStep::WriteDisplayName,
          "step4 writes Layout Display Name");
    check(reported == std::vector<std::size_t>({1, 2, 3, 4}),
          "progress reported for every step");

    executed.clear();
    reported.clear();
    const installer::InstallErrorCode short_stop = installer::run_install_steps(
        [&](installer::InstallStep step, std::size_t) {
            executed.push_back(step);
            if (step == installer::InstallStep::RegisterLayout) {
                return installer::InstallErrorCode::RegError;
            }
            return installer::InstallErrorCode::Ok;
        },
        [&](std::size_t index, installer::InstallStep, std::wstring_view) {
            reported.push_back(index);
        });

    check(short_stop == installer::InstallErrorCode::RegError,
          "first failure is returned");
    check(executed.size() == 2, "execution stops at failing step");
    check(reported.size() == 2, "progress stops at failing step");

    check(installer::kInstallRegistryKey == L"SOFTWARE\\RosPinyin\\Install",
          "HKL record location unchanged");
    check(contains(installer::error_message(installer::InstallErrorCode::Ok),
                   L"重启"),
          "success message requires a reboot");
    check(contains(installer::next_action(installer::InstallErrorCode::Ok),
                   L"重启"),
          "success next action requires a reboot");
    check(contains(installer::error_message(
                       installer::InstallErrorCode::NeedsReboot),
                   L"重启"),
          "reboot error explains the next action");
    check(contains(installer::error_message(
                       installer::InstallErrorCode::PlatformUnsupported),
                   L"平台"),
          "platform error names the whitelist failure");
    check(contains(installer::next_action(
                       installer::InstallErrorCode::PayloadCorrupt),
                   L"重新下载"),
          "payload error tells user to redownload");
}

void test_font_hint() {
    // The full-glyph font pack is deliberately NOT inside the GUI payload;
    // the installer can only tell the user where to get it. Both surfaces
    // (a permanent line on the main window and the post-install message box)
    // must be pinned here so a wording edit cannot silently drop the URL.
    check(contains(installer::kFontHintBanner, L"[全]"),
          "banner names the [full] mode that needs the font");
    check(contains(installer::kFontHintBanner, L"[快]"),
          "banner states [fast] mode needs no extra font");
    check(contains(installer::kFontHintBanner, L"文津"),
          "banner names the font family");

    const std::wstring hint = installer::font_hint_message();
    check(contains(hint, installer::kFontHintUrl),
          "hint carries the HTTP download location");
    check(contains(hint, L"WenJinMinchoP0-Regular.ttf") &&
              contains(hint, L"WenJinMinchoP2-Regular.ttf") &&
              contains(hint, L"WenJinMinchoP3-Regular.ttf"),
          "hint lists all three plane-split TTFs");
    check(contains(hint, L"SHA256SUMS"),
          "hint points at the checksum file");
    check(contains(hint, L"[快]"),
          "hint reassures [fast] mode needs no extra font");

    // P1 AC6: the warning must state that all three plane-split TTFs are
    // required and that a partial install has unpredictable consequences.
    // This pairs with the runtime decision NOT to probe for missing faces --
    // the user is told in words what the code deliberately does not detect.
    check(contains(hint, L"三字体齐备") || contains(hint, L"三者齐备"),
          "hint states all three fonts must be present");
    check(contains(hint, L"不可预知") || contains(hint, L"后果无法预料"),
          "hint warns the consequence of a missing font is unpredictable");
}

void test_droid_font_pack() {
    namespace font = installer;
    // The optional pack must carry the unmodified ReactOS font and must never
    // overwrite a font the OS shipped. Both facts are pinned here so a later
    // "convenient" replacement of the binary cannot slip through.
    check(font::kDroidFontExpectedSize == 3081908ULL,
          "font pack pins the original file size");
    check(std::string_view(font::kDroidFontExpectedSha256) ==
              "82a74d0ef369ec7979172023cfff87f04f0a617a2871c39ef6aeb0f399c5cdaf",
          "font pack pins the original SHA-256");
    check(std::wstring_view(font::kDroidFontFile) == L"DROID.TTF",
          "font pack keeps the font.inf 8.3 file name");
    check(contains(font::droid_font_state_label(font::DroidFontState::System),
                   L"system font"),
          "system-owned font is reported as such");
    check(contains(font::droid_font_state_label(font::DroidFontState::Owned),
                   L"RosPinyin"),
          "font pack ownership is visible in /status");

    check(!font::should_remove_droid_font(font::DroidFontState::Missing),
          "missing font needs no cleanup");
    check(!font::should_remove_droid_font(font::DroidFontState::System),
          "system font survives /u unchanged");
    check(font::should_remove_droid_font(font::DroidFontState::Owned),
          "only our own font pack is removed by /u");

    const auto hint = font::droid_font_status_hint(
        font::DroidFontState::Missing, false);
    check(contains(hint, L"/fontinstall") && contains(hint, L"重启"),
          "missing font hint names the install command and reboot");
    check(contains(font::droid_font_usage_line(), L"/fontinstall"),
          "CLI usage documents the optional font command");
    check(font::droid_font_status_hint(font::DroidFontState::System, true)
              .empty(),
          "healthy system font emits no warning");
}

struct IdentityInflate {
    std::size_t consume = 0;
    installer::PayloadError status = installer::PayloadError::Ok;

    installer::InflateResult operator()(
        const std::vector<std::uint8_t>& input, std::size_t,
        std::vector<std::uint8_t>& output) const {
        output = input;
        return {status, consume == 0 ? input.size() : consume};
    }
};

void test_payload_blob() {
    // SHA-256 / CRC-32 标准测试向量，防止「用同一个坏函数自证」。
    const char abc[] = "abc";
    check(installer::sha256_hex(
              reinterpret_cast<const std::uint8_t*>(abc), 3) ==
              "ba7816bf8f01cfea414140de5dae2223"
              "b00361a396177a9cb410ff61f20015ad",
          "SHA-256 matches known vector");

    const char digits[] = "123456789";
    check(installer::crc32(reinterpret_cast<const std::uint8_t*>(digits), 9) ==
              0xCBF43926u,
          "CRC-32 matches known vector");

    const std::vector<std::uint8_t> source = {'R', 'S', 'P', 'Y', 'D', 'A', 'T'};
    const std::uint32_t crc = installer::crc32(source.data(), source.size());
    std::vector<std::uint8_t> output;

    check(installer::decode_payload(source, source.size(), crc, output,
                                    IdentityInflate{}) ==
              installer::PayloadError::Ok,
          "valid payload decodes");
    check(output == source, "decoded bytes equal source bytes");

    check(installer::decode_payload(source, source.size(), crc ^ 1u, output,
                                    IdentityInflate{}) ==
              installer::PayloadError::CrcMismatch,
          "CRC mismatch rejected");

    const auto short_inflate =
        [](const std::vector<std::uint8_t>& input, std::size_t,
           std::vector<std::uint8_t>& out) {
            out.assign(input.begin(), input.begin() + 1);
            return installer::InflateResult{installer::PayloadError::Ok,
                                            input.size()};
        };
    check(installer::decode_payload(source, source.size(), crc, output,
                                    short_inflate) ==
              installer::PayloadError::SizeMismatch,
          "uncompressed size mismatch rejected");

    IdentityInflate trailing;
    trailing.consume = source.size() - 1;
    check(installer::decode_payload(source, source.size(), crc, output,
                                    trailing) ==
              installer::PayloadError::TrailingData,
          "unconsumed trailing bytes rejected");

    IdentityInflate failed;
    failed.status = installer::PayloadError::InflateFailed;
    check(installer::decode_payload(source, source.size(), crc, output,
                                    failed) ==
              installer::PayloadError::InflateFailed,
          "inflate failure propagated");
}

void test_miniz_roundtrip() {
    // zlib.compress(b"RosPinyin miniz round-trip 1234567890", 9).
    const std::vector<std::uint8_t> compressed = {
        0x78, 0xDA, 0x0B, 0xCA, 0x2F, 0x0E, 0xC8, 0xCC, 0xAB, 0xCC,
        0xCC, 0x53, 0xC8, 0xCD, 0xCC, 0xCB, 0xAC, 0x52, 0x28, 0xCA,
        0x2F, 0xCD, 0x4B, 0xD1, 0x2D, 0x29, 0xCA, 0x2C, 0x50, 0x30,
        0x34, 0x32, 0x36, 0x31, 0x35, 0x33, 0xB7, 0xB0, 0x34, 0x00,
        0x00, 0x04, 0xB0, 0x0C, 0x54};
    const std::string source = "RosPinyin miniz round-trip 1234567890";
    std::vector<std::uint8_t> output(source.size(), 0);
    mz_ulong dest_len = static_cast<mz_ulong>(output.size());
    mz_ulong src_len = static_cast<mz_ulong>(compressed.size());
    const int status = mz_uncompress2(output.data(), &dest_len,
                                      compressed.data(), &src_len);
    check(status == MZ_OK, "miniz accepts a real zlib stream");
    check(dest_len == source.size(), "miniz output length matches");
    check(src_len == compressed.size(), "miniz consumed the whole stream");
    check(std::vector<std::uint8_t>(output.begin(),
                                    output.begin() + dest_len) ==
              std::vector<std::uint8_t>(source.begin(), source.end()),
          "miniz output equals original bytes");
}

void test_embedded_payloads() {
    const auto verify = [](const std::vector<std::uint8_t>& compressed,
                           std::size_t expected_size, std::uint32_t crc,
                           const char* what) {
        std::vector<std::uint8_t> output(expected_size, 0);
        mz_ulong dest_len = static_cast<mz_ulong>(expected_size);
        mz_ulong src_len = static_cast<mz_ulong>(compressed.size());
        check(mz_uncompress2(output.data(), &dest_len, compressed.data(),
                             &src_len) == MZ_OK,
              what);
        check(dest_len == expected_size, "embedded payload length matches");
        check(installer::crc32(output.data(), output.size()) == crc,
              "embedded payload CRC matches source");
    };

    const std::vector<std::uint8_t> ime = {
        ROSPINYIN_IME_PAYLOAD_BYTES};
    const std::vector<std::uint8_t> dat = {
        ROSPINYIN_DAT_PAYLOAD_BYTES};
    const std::vector<std::uint8_t> cli = {
        ROSPINYIN_CLI_PAYLOAD_BYTES};
    const std::vector<std::uint8_t> lex = {
        ROSPINYIN_LEX_PAYLOAD_BYTES};
    check(ime.size() == installer::kImeCompressedSize, "IME blob size pinned");
    check(dat.size() == installer::kDatCompressedSize, "DAT blob size pinned");
    check(cli.size() == installer::kCliCompressedSize, "CLI blob size pinned");
    check(lex.size() == installer::kLexCompressedSize, "LEX blob size pinned");
    verify(ime, installer::kImeUncompressedSize, installer::kImeCrc32,
           "embedded IME inflates");
    verify(dat, installer::kDatUncompressedSize, installer::kDatCrc32,
           "embedded DAT inflates");
    verify(cli, installer::kCliUncompressedSize, installer::kCliCrc32,
           "embedded CLI inflates");
    // AC1: the 4th payload is the word list; inflate it and pin its SHA-256
    // to the fixed value shared with the runtime guard and CI release assets.
    std::vector<std::uint8_t> lex_out(installer::kLexUncompressedSize, 0);
    mz_ulong lex_dest = static_cast<mz_ulong>(lex_out.size());
    mz_ulong lex_src = static_cast<mz_ulong>(lex.size());
    check(mz_uncompress2(lex_out.data(), &lex_dest, lex.data(), &lex_src) ==
              MZ_OK,
          "embedded LEX inflates");
    check(lex_dest == installer::kLexUncompressedSize,
          "embedded LEX length matches the fixed 6,530,601 bytes");
    check(installer::crc32(lex_out.data(), lex_out.size()) ==
              installer::kLexCrc32,
          "embedded LEX CRC matches source");
    check(installer::sha256_hex(lex_out.data(), lex_out.size()) ==
              "a89ec5e500c47c121687e9789fade9b8187c1ec2b69d25d5e82f1a680cfc1c6a",
          "embedded LEX SHA-256 matches the pinned word list");
}

// AC1/AC2/AC4/AC5-8: GUI installation-shell logic that is Windows-free and
// therefore assertable on the Linux host. The Win32 layer only adapts these
// decisions to controls; the decisions themselves live in gui_logic.hpp.
void test_gui_logic() {
    namespace gui = rospinyin::installer::gui;

    // AC1: the exact version string from version.h appears in both the window
    // title and the main-surface label; no second literal is allowed.
    {
        // Synthetic token: a real alpha-tag literal in code would trip the
        // X2-e5 "no second version literal" gate. The point is exact echo.
        const std::wstring token = L"VER" L"SION" L"-9.9.9";
        const auto texts = gui::make_window_texts(token);
        check(contains(texts.title, token),
              "AC1 window title carries the version string");
        check(contains(texts.version_label, token),
              "AC1 main-surface label carries the version string");
        // The real value comes from ime/version.h at the Win32 call site;
        // there is deliberately no second copy to test here.
    }

    // AC2: install button is disabled until the agreement box is checked.
    {
        gui::AgreementState state;
        check(!state.install_enabled(), "AC2 install disabled before consent");
        state.set_checked(true);
        check(state.install_enabled(), "AC2 install enabled after consent");
        state.set_checked(false);
        check(!state.install_enabled(), "AC2 install disabled again if unchecked");
    }

    // DEF-001: the agreement checkbox must toggle itself (the Win32 layer
    // derives BS_AUTOCHECKBOX from this) and be keyboard reachable (WS_TABSTOP
    // plus dialog navigation). Kept as a contract so it is assertable here.
    {
        const auto spec = gui::agreement_control_spec();
        check(spec.auto_toggle,
              "DEF-001 checkbox toggles without manual BM_SETCHECK");
        check(spec.keyboard_focusable,
              "DEF-001 checkbox is keyboard reachable");
    }

    // AC4: whats-new text is carried verbatim and marked as this version's
    // changes; the UI layer reads it from a resource, the logic layer only
    // decides how it is labelled.
    {
        const std::wstring shown = gui::format_whatsnew(L"- Enter raw commit");
        check(contains(shown, L"Enter raw commit"),
              "AC4 whats-new text preserved verbatim");
        check(contains(shown, L"\n"),
              "AC4 whats-new keeps line breaks for multiple update points");
    }

    // AC5: choosing No must not call the reboot action.
    {
        gui::RebootController controller;
        int calls = 0;
        controller.run_choice(gui::RebootChoice::No, [&] { ++calls; });
        check(calls == 0, "AC5 No branch never reboots");
    }

    // AC6: cancelling the countdown must not call the reboot action.
    {
        gui::RebootController controller;
        int calls = 0;
        controller.run_choice(gui::RebootChoice::Yes, [&] { ++calls; });
        check(calls == 0, "AC6 Yes enters countdown without immediate reboot");
        controller.cancel_countdown();
        check(calls == 0, "AC6 countdown cancel never reboots");
    }

    // AC7: countdown reaching zero calls reboot exactly once, even if the
    // tick fires again afterwards.
    {
        gui::RebootController controller;
        int calls = 0;
        controller.run_choice(gui::RebootChoice::Yes, [&] { ++calls; });
        controller.countdown_elapsed();
        check(calls == 1, "AC7 countdown zero fires reboot exactly once");
        controller.countdown_elapsed();
        check(calls == 1, "AC7 repeated tick does not fire reboot twice");
    }

    // AC8: a failed reboot keeps the install result successful and exposes
    // the manual command; the caller decides whether to show the dialog.
    {
        const auto msg = gui::reboot_failure_message();
        check(contains(msg, L"shutdown /r"),
              "AC8 failure message carries manual reboot command");
        gui::RebootController controller;
        controller.note_reboot_failed();
        check(!controller.install_failed(),
              "AC8 reboot failure does not downgrade install result");
    }
}

}  // namespace

int main() {
    test_platform_guard();
    test_install_steps();
    test_font_hint();
    test_droid_font_pack();
    test_payload_blob();
    test_miniz_roundtrip();
    test_embedded_payloads();
    test_gui_logic();

    if (failures != 0) {
        std::fprintf(stderr, "%d installer logic check(s) failed\n", failures);
        return 1;
    }
    std::printf("installer logic: all checks passed\n");
    return 0;
}
