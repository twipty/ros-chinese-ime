// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Self-contained GUI installer shell. It deliberately owns no registry-write
// logic: payloads are inflated into a private %TEMP% directory, the existing
// CLI installer is copied there, and /step1../step4 are run from that copy.
// That keeps the M0 red-line code in exactly one implementation (install.cpp)
// while giving first-time users a double-click flow.

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

// Only the zlib-compatible inflate path is used; keep the shipped shell small.
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_IMPLEMENTATION
#include "miniz.h"

#include "version.h"
#include "gui_logic.hpp"
#include "install_steps.hpp"
#include "payload_blob.hpp"
#include "payload_base64.hpp"
#include "platform_guard.hpp"

namespace installer = rospinyin::installer;

namespace {

constexpr int kIdInstall = 1001;
constexpr int kIdUninstall = 1002;
constexpr int kIdStatus = 1003;
constexpr int kIdFontHint = 1004;
constexpr int kIdAgreement = 1005;   // AC2 consent checkbox
constexpr int kIdViewLicense = 1006; // AC3 open the license viewer
constexpr int kIdWhatsNew = 1007;    // AC4 update-points STATIC
constexpr int kIdVersion = 1008;     // AC1 version STATIC
constexpr int kIdLicenseText = 1009; // AC3 read-only EDIT in the license window
constexpr int kIdCountdown = 1010;   // countdown label in the reboot window
constexpr int kIdCancelReboot = 1011;// cancel button in the reboot window

namespace gui = rospinyin::installer::gui;

HINSTANCE g_instance = nullptr;

// AC2: one agreement state per process, always re-checked on each launch.
gui::AgreementState g_agreement;
HWND g_install_button = nullptr;
HWND g_countdown_label = nullptr;
HWND g_reboot_window = nullptr;
UINT_PTR g_countdown_timer = 0;
int g_countdown_remaining = 0;
gui::RebootController g_reboot;

constexpr wchar_t kCliFileName[] = L"install-rospinyin.exe";
constexpr wchar_t kImeFileName[] = L"rospinyin.ime";
constexpr wchar_t kDatFileName[] = L"rospinyin.dat";
// M4: the word list is the 4th payload. This name must match the CLI's
// kLexiconFileName exactly — both read it from the same %TEMP% directory.
constexpr wchar_t kLexiconFileName[] = L"rospinyin-lexicon.dat";

HWND g_status = nullptr;

void set_status(const wchar_t* text) {
    if (g_status) SetWindowTextW(g_status, text);
}

const wchar_t* payload_error_text(installer::PayloadError code) {
    switch (code) {
        case installer::PayloadError::InflateFailed:
            return L"解压失败";
        case installer::PayloadError::SizeMismatch:
            return L"解压后长度不符";
        case installer::PayloadError::CrcMismatch:
            return L"CRC 不符";
        case installer::PayloadError::TrailingData:
            return L"压缩流有尾随字节";
        case installer::PayloadError::Ok:
            break;
    }
    return L"未知 payload 错误";
}

bool path_join(const std::wstring& dir, const wchar_t* name,
               std::wstring& out) {
    if (dir.empty()) return false;
    out = dir;
    if (out.back() != L'\\') out.push_back(L'\\');
    out += name;
    return true;
}

bool write_file(const std::wstring& path, const std::vector<std::uint8_t>& data) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    std::size_t written = 0;
    while (written < data.size()) {
        const DWORD chunk = static_cast<DWORD>(
            data.size() - written > 0x10000000u ? 0x10000000u
                                                 : data.size() - written);
        DWORD done = 0;
        if (!WriteFile(h, data.data() + written, chunk, &done, nullptr) ||
            done == 0) {
            ok = false;
            break;
        }
        written += done;
    }
    CloseHandle(h);
    return ok;
}

bool make_temp_dir(std::wstring& out) {
    wchar_t tmp[MAX_PATH]{};
    const UINT n = GetTempPathW(MAX_PATH, tmp);
    if (n == 0 || n >= MAX_PATH) return false;
    for (int attempt = 0; attempt < 100; ++attempt) {
        wchar_t candidate[MAX_PATH]{};
        std::swprintf(candidate, MAX_PATH, L"%lsrospinyin-%lu-%d", tmp,
                      static_cast<unsigned long>(GetCurrentProcessId()),
                      attempt);
        if (CreateDirectoryW(candidate, nullptr)) {
            out = candidate;
            return true;
        }
        if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
    }
    return false;
}

void cleanup_temp_dir(const std::wstring& dir) {
    if (dir.empty()) return;
    for (const wchar_t* name :
         {kImeFileName, kDatFileName, kCliFileName, kLexiconFileName}) {
        std::wstring path;
        if (path_join(dir, name, path)) DeleteFileW(path.c_str());
    }
    RemoveDirectoryW(dir.c_str());
}

bool load_resource_bytes(WORD id, std::vector<std::uint8_t>& out) {
    // RT_RCDATA is the ANSI MAKEINTRESOURCE(10); the W API needs the wide form.
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id),
                              MAKEINTRESOURCEW(10));
    if (!res) return false;
    const DWORD size = SizeofResource(nullptr, res);
    HGLOBAL mem = LoadResource(nullptr, res);
    if (!size || !mem) return false;
    const void* ptr = LockResource(mem);
    if (!ptr) return false;
    const auto* bytes = static_cast<const std::uint8_t*>(ptr);
    out.assign(bytes, bytes + size);
    return true;
}

// M4: the word list is split into ROSPINYIN_LEX_RCDATA_SEGMENTS consecutive
// RCDATA blobs (104..111) because one giant RCDATA makes windres hang. Load
// them in ID order and concatenate back into the original compressed stream.
bool load_lexicon_resource_bytes(std::vector<std::uint8_t>& out) {
    out.clear();
    for (int seg = 0; seg < ROSPINYIN_LEX_RCDATA_SEGMENTS; ++seg) {
        std::vector<std::uint8_t> piece;
        if (!load_resource_bytes(
                static_cast<WORD>(ROSPINYIN_IDR_LEX_PAYLOAD + seg), piece)) {
            return false;
        }
        out.insert(out.end(), piece.begin(), piece.end());
    }
    return true;
}

bool extract_payload(WORD id, std::size_t compressed_size,
                     std::size_t uncompressed_size,
                     std::uint32_t expected_crc, const char* expected_sha,
                     const std::wstring& dir, const wchar_t* name,
                     std::wstring* detail = nullptr,
                     bool segmented = false) {
    std::wstring hash_note;
    auto fail = [detail, &hash_note](const wchar_t* reason) {
        if (detail) {
            *detail = reason;
            if (!hash_note.empty()) *detail += L"; " + hash_note;
        }
        return false;
    };

    std::vector<std::uint8_t> compressed;
    const bool loaded = segmented ? load_lexicon_resource_bytes(compressed)
                                  : load_resource_bytes(id, compressed);
    if (!loaded) {
        return fail(L"资源读取失败");
    }
    if (compressed.size() != compressed_size) {
        return fail(L"资源长度不符");
    }

    const std::string actual_sha =
        installer::sha256_hex(compressed.data(), compressed.size());
    const std::string expected = expected_sha;
    hash_note = L"rcdata_sha256=" +
                std::wstring(actual_sha.begin(), actual_sha.end()) +
                L" expected=" +
                std::wstring(expected.begin(), expected.end()) + L" match=" +
                (actual_sha == expected ? L"yes" : L"no");

    int inflate_status = MZ_OK;
    mz_ulong inflate_consumed = 0;
    mz_ulong inflate_produced = 0;
    auto inflate = [&](const std::vector<std::uint8_t>& src,
                       std::size_t expected, std::vector<std::uint8_t>& dst) {
        dst.assign(expected, 0);
        mz_ulong dest_len = static_cast<mz_ulong>(expected);
        mz_ulong src_len = static_cast<mz_ulong>(src.size());
        inflate_status =
            mz_uncompress2(dst.data(), &dest_len, src.data(), &src_len);
        inflate_consumed = src_len;
        inflate_produced = dest_len;
        if (inflate_status != MZ_OK) {
            return installer::InflateResult{
                installer::PayloadError::InflateFailed, 0};
        }
        dst.resize(static_cast<std::size_t>(dest_len));
        return installer::InflateResult{
            installer::PayloadError::Ok, static_cast<std::size_t>(src_len)};
    };
    std::vector<std::uint8_t> output;
    const installer::PayloadError error = installer::decode_payload(
        compressed, uncompressed_size, expected_crc, output, inflate);
    if (error != installer::PayloadError::Ok) {
        if (error == installer::PayloadError::InflateFailed) {
            std::wstring d = L"解压失败(status=" +
                             std::to_wstring(inflate_status) + L",in=" +
                             std::to_wstring(inflate_consumed) + L",buf=" +
                             std::to_wstring(inflate_produced) + L")";
            if (detail) *detail = d + L"; " + hash_note;
            return false;
        }
        return fail(payload_error_text(error));
    }
    std::wstring path;
    if (!path_join(dir, name, path) || !write_file(path, output)) {
        return fail(L"临时文件写入失败");
    }
    return true;
}

// The GUI is a single self-contained exe: the CLI it drives is the third
// RCDATA payload, never a loose file expected beside the installer.
bool extract_cli(const std::wstring& dir, std::wstring& out,
                 std::wstring* detail = nullptr) {
    const bool ok = extract_payload(
        ROSPINYIN_IDR_CLI_PAYLOAD, installer::kCliCompressedSize,
        installer::kCliUncompressedSize, installer::kCliCrc32,
        installer::kCliCompressedSha256, dir, kCliFileName, detail);
    if (!ok) return false;
    return path_join(dir, kCliFileName, out);
}

installer::InstallErrorCode cli_error(DWORD exit_code) {
    if (exit_code <= static_cast<DWORD>(
                         installer::InstallErrorCode::PayloadCorrupt)) {
        return static_cast<installer::InstallErrorCode>(exit_code);
    }
    return installer::InstallErrorCode::IoError;
}

installer::InstallErrorCode run_cli(const std::wstring& exe,
                                    const wchar_t* argument) {
    std::wstring cmd = L"\"" + exe + L"\" " + argument;
    std::vector<wchar_t> mutable_cmd(cmd.begin(), cmd.end());
    mutable_cmd.push_back(L'\0');

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    if (!CreateProcessW(exe.c_str(), mutable_cmd.data(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return installer::InstallErrorCode::IoError;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    if (!GetExitCodeProcess(pi.hProcess, &code)) code = 1;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return cli_error(code);
}

bool payload_installed() {
    wchar_t system[MAX_PATH]{};
    if (GetSystemDirectoryW(system, MAX_PATH) == 0) return false;
    std::wstring dir(system);
    std::wstring ime, dat, lex;
    if (!path_join(dir, kImeFileName, ime) ||
        !path_join(dir, kDatFileName, dat) ||
        !path_join(dir, kLexiconFileName, lex)) {
        return false;
    }
    // M4: a v0.1 install has ime+dat but no word list. It must be treated as
    // "not fully installed" so the overlay prompt offers the destructive
    // upgrade that actually delivers phrases.
    return GetFileAttributesW(ime.c_str()) != INVALID_FILE_ATTRIBUTES &&
           GetFileAttributesW(dat.c_str()) != INVALID_FILE_ATTRIBUTES &&
           GetFileAttributesW(lex.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool check_platform() {
    wchar_t product[256]{};
    DWORD type = 0;
    DWORD size = sizeof(product);
    const LSTATUS st = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName",
        RRF_RT_REG_SZ, &type, product, &size);
    const std::wstring name =
        (st == ERROR_SUCCESS && type == REG_SZ) ? std::wstring(product) : L"";
    if (installer::is_supported_product_name(name)) return true;

    std::wstring message =
        L"当前系统未列入本安装器已验证平台白名单（ProductName=" + name +
        L"）。为保护您的系统，已取消安装。\n\n"
        L"Windows 真机请使用命令行 /step1../step4 方式。";
    MessageBoxW(nullptr, message.c_str(), L"RosPinyin 安装器",
                MB_OK | MB_ICONERROR);
    return false;
}

// --- AC3: license viewer (self-contained: text comes from RCDATA) ---------

std::wstring utf8_to_wide(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty()) return L"(协议文本缺失)";
    const int needed = MultiByteToWideChar(
        CP_UTF8, 0, reinterpret_cast<const char*>(bytes.data()),
        static_cast<int>(bytes.size()), nullptr, 0);
    if (needed <= 0) return L"(协议文本解码失败)";
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(bytes.data()),
                        static_cast<int>(bytes.size()), out.data(), needed);
    return out;
}

std::wstring load_license_text() {
    std::vector<std::uint8_t> license_bytes, notice_bytes;
    std::wstring out;
    if (load_resource_bytes(ROSPINYIN_IDR_LICENSE, license_bytes)) {
        out += utf8_to_wide(license_bytes);
    }
    out += L"\r\n\r\n================ NOTICE ================\r\n\r\n";
    if (load_resource_bytes(ROSPINYIN_IDR_NOTICE, notice_bytes)) {
        out += utf8_to_wide(notice_bytes);
    }
    return out;
}

LRESULT CALLBACK license_window_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                     LPARAM lparam) {
    switch (msg) {
        case WM_CREATE: {
            const std::wstring text = load_license_text();
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text.c_str(),
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                ES_READONLY | ES_AUTOVSCROLL,
                            10, 10, 560, 360, hwnd,
                            reinterpret_cast<HMENU>(kIdLicenseText), nullptr,
                            nullptr);
            CreateWindowExW(0, L"BUTTON", L"关闭",
                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 240, 380,
                            100, 30, hwnd, reinterpret_cast<HMENU>(IDCANCEL),
                            nullptr, nullptr);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void open_license_window(HINSTANCE instance) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = license_window_proc;
        wc.hInstance = instance;
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"RosPinyinLicenseViewer";
        if (!RegisterClassW(&wc)) return;
        registered = true;
    }
    HWND hwnd = CreateWindowExW(
        0, L"RosPinyinLicenseViewer", L"RosPinyin 许可协议",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 450, nullptr, nullptr, instance,
        nullptr);
    if (hwnd) ShowWindow(hwnd, SW_SHOW);
}

// --- AC5-8: reboot countdown window --------------------------------------

bool try_reboot() {
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(),
                         TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        TOKEN_PRIVILEGES tp{};
        if (LookupPrivilegeValueW(nullptr, L"SeShutdownPrivilege",
                                  &tp.Privileges[0].Luid)) {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr,
                                  nullptr);
        }
        CloseHandle(token);
    }
    return ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0) != 0;
}

void perform_reboot() {
    g_reboot.countdown_elapsed();
    // countdown_elapsed() fires the registered callback (try_reboot wrapper).
}

LRESULT CALLBACK reboot_window_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam) {
    switch (msg) {
        case WM_CREATE: {
            g_countdown_label = CreateWindowExW(
                0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 20, 340, 40, hwnd,
                reinterpret_cast<HMENU>(kIdCountdown), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"取消重启",
                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 130, 70,
                            120, 32, hwnd,
                            reinterpret_cast<HMENU>(kIdCancelReboot), nullptr,
                            nullptr);
            return 0;
        }
        case WM_TIMER:
            if (wparam == 1) {
                --g_countdown_remaining;
                if (g_countdown_remaining <= 0) {
                    KillTimer(hwnd, 1);
                    g_countdown_timer = 0;
                    perform_reboot();
                    DestroyWindow(hwnd);
                } else if (g_countdown_label) {
                    std::wstring label = L"系统将在 " +
                                         std::to_wstring(g_countdown_remaining) +
                                         L" 秒后重启…";
                    SetWindowTextW(g_countdown_label, label.c_str());
                }
                return 0;
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wparam) == kIdCancelReboot) {
                g_reboot.cancel_countdown();
                if (g_countdown_timer) KillTimer(hwnd, 1);
                g_countdown_timer = 0;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_DESTROY:
            g_reboot_window = nullptr;
            g_countdown_label = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void open_reboot_countdown(HINSTANCE instance) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = reboot_window_proc;
        wc.hInstance = instance;
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"RosPinyinRebootCountdown";
        if (!RegisterClassW(&wc)) return;
        registered = true;
    }
    g_reboot_window = CreateWindowExW(
        0, L"RosPinyinRebootCountdown", L"即将重启",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 380, 150, nullptr, nullptr, instance,
        nullptr);
    if (!g_reboot_window) return;
    g_countdown_remaining = 10;
    ShowWindow(g_reboot_window, SW_SHOW);
    g_countdown_timer = SetTimer(g_reboot_window, 1, 1000, nullptr);
}

void do_install() {
    if (payload_installed() &&
        MessageBoxW(nullptr, L"检测到已安装 RosPinyin，是否覆盖安装？",
                    L"RosPinyin 安装器", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }
    if (!check_platform()) return;

    std::wstring temp;
    if (!make_temp_dir(temp)) {
        set_status(L"无法创建临时目录。");
        MessageBoxW(nullptr, installer::error_message(
                                 installer::InstallErrorCode::IoError)
                                 .data(),
                    L"RosPinyin 安装器", MB_OK | MB_ICONERROR);
        return;
    }

    installer::InstallErrorCode result = installer::InstallErrorCode::Ok;
    std::wstring payload_detail;
    do {
        if (!extract_payload(ROSPINYIN_IDR_IME_PAYLOAD,
                             installer::kImeCompressedSize,
                             installer::kImeUncompressedSize,
                             installer::kImeCrc32,
                             installer::kImeCompressedSha256, temp,
                             kImeFileName, &payload_detail) ||
            !extract_payload(ROSPINYIN_IDR_DAT_PAYLOAD,
                             installer::kDatCompressedSize,
                             installer::kDatUncompressedSize,
                             installer::kDatCrc32,
                             installer::kDatCompressedSha256, temp,
                             kDatFileName, &payload_detail) ||
            // M4: extract the word list into the same %TEMP% dir before the
            // CLI runs — /step1 now reads all three files from its own dir.
            // segmented=true: the compressed bytes live in 8 RCDATA blobs
            // (windres cannot handle one 116k-literal blob).
            !extract_payload(ROSPINYIN_IDR_LEX_PAYLOAD,
                             installer::kLexCompressedSize,
                             installer::kLexUncompressedSize,
                             installer::kLexCrc32,
                             installer::kLexCompressedSha256, temp,
                             kLexiconFileName, &payload_detail, true)) {
            result = installer::InstallErrorCode::PayloadCorrupt;
            break;
        }

        std::wstring temp_cli;
        if (!extract_cli(temp, temp_cli, &payload_detail)) {
            result = installer::InstallErrorCode::PayloadCorrupt;
            break;
        }

        const wchar_t* args[] = {L"/step1", L"/step2", L"/step3", L"/step4"};
        result = installer::run_install_steps(
            [&](installer::InstallStep, std::size_t index) {
                return run_cli(temp_cli, args[index - 1]);
            },
            [](std::size_t, installer::InstallStep,
               std::wstring_view label) { set_status(label.data()); });
    } while (false);

    cleanup_temp_dir(temp);
    if (result == installer::InstallErrorCode::Ok) {
        set_status(L"安装完成，请重启系统。");
        const std::wstring message =
            std::wstring(installer::error_message(result)) + L"\n\n" +
            std::wstring(installer::next_action(result)) + L"\n\n" +
            installer::font_hint_message();
        const int choice = MessageBoxW(
            nullptr, message.c_str(), L"RosPinyin 安装器",
            MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON2);
        // AC5: No keeps the manual hint and never touches the reboot API.
        g_reboot.run_choice(
            choice == IDYES ? gui::RebootChoice::Yes : gui::RebootChoice::No,
            [] {
                // AC8: failure is surfaced but never downgrades the install.
                if (!try_reboot()) {
                    g_reboot.note_reboot_failed();
                    MessageBoxW(nullptr, gui::reboot_failure_message().c_str(),
                                L"RosPinyin 安装器", MB_OK | MB_ICONWARNING);
                }
            });
        if (choice == IDYES) {
            // AC6/AC7: Yes enters a cancellable 10-second countdown.
            open_reboot_countdown(g_instance);
        }
    } else {
        std::wstring message =
            std::wstring(installer::error_message(result)) + L"\n\n" +
            std::wstring(installer::next_action(result)) + L"\n错误码：" +
            std::to_wstring(static_cast<int>(result));
        if (!payload_detail.empty()) {
            message += L"\n诊断：" + payload_detail;
        }
        set_status(L"安装失败。");
        MessageBoxW(nullptr, message.c_str(), L"RosPinyin 安装器",
                    MB_OK | MB_ICONERROR);
    }
}

void do_uninstall() {
    std::wstring temp;
    if (!make_temp_dir(temp)) {
        set_status(L"无法创建临时目录。");
        MessageBoxW(nullptr,
                    installer::error_message(
                        installer::InstallErrorCode::IoError)
                        .data(),
                    L"RosPinyin 卸载器", MB_OK | MB_ICONERROR);
        return;
    }

    installer::InstallErrorCode result = installer::InstallErrorCode::Ok;
    std::wstring payload_detail;
    do {
        std::wstring temp_cli;
        if (!extract_cli(temp, temp_cli, &payload_detail)) {
            result = installer::InstallErrorCode::PayloadCorrupt;
            break;
        }
        result = run_cli(temp_cli, L"/u");
    } while (false);

    cleanup_temp_dir(temp);
    if (result == installer::InstallErrorCode::Ok) {
        set_status(L"卸载完成，请重启系统。");
        MessageBoxW(nullptr, L"RosPinyin 已卸载。\n\n请重启系统后生效。",
                    L"RosPinyin 卸载器", MB_OK | MB_ICONINFORMATION);
    } else {
        set_status(L"卸载失败。");
        std::wstring message = std::wstring(installer::error_message(result));
        if (!payload_detail.empty()) {
            message += L"\n诊断：" + payload_detail;
        }
        MessageBoxW(nullptr, message.c_str(), L"RosPinyin 卸载器",
                    MB_OK | MB_ICONERROR);
    }
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam,
                             LPARAM lparam) {
    switch (msg) {
        case WM_CREATE: {
            // AC1: version label from the single version.h source.
            const auto texts = gui::make_window_texts(
                ROSPINYIN_VERSION_STRING);
            CreateWindowExW(0, L"STATIC", texts.version_label.c_str(),
                            WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 12, 430, 22,
                            hwnd, reinterpret_cast<HMENU>(kIdVersion), nullptr,
                            nullptr);
            CreateWindowExW(0, L"BUTTON", L"安装 RosPinyin",
                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON |
                                WS_TABSTOP | WS_DISABLED,
                            20, 44, 180, 36, hwnd,
                            reinterpret_cast<HMENU>(kIdInstall), nullptr,
                            nullptr);
            g_install_button = GetDlgItem(hwnd, kIdInstall);
            CreateWindowExW(0, L"BUTTON", L"卸载 RosPinyin",
                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                            220, 44, 180, 36, hwnd,
                            reinterpret_cast<HMENU>(kIdUninstall), nullptr,
                            nullptr);
            // AC2/AC3: consent gate + license viewer. DEF-001: the checkbox
            // toggles itself; the message handler only reads the resulting
            // state. WS_TABSTOP plus IsDialogMessage in the message loop makes
            // it reachable by Tab, and a focused auto-checkbox toggles on Space.
            const auto agreement_spec = gui::agreement_control_spec();
            UINT agreement_style = WS_CHILD | WS_VISIBLE;
            agreement_style |=
                agreement_spec.auto_toggle ? BS_AUTOCHECKBOX : BS_CHECKBOX;
            if (agreement_spec.keyboard_focusable) {
                agreement_style |= WS_TABSTOP;
            }
            CreateWindowExW(0, L"BUTTON", L"我已阅读并同意许可协议",
                            agreement_style, 20, 92, 240, 22, hwnd,
                            reinterpret_cast<HMENU>(kIdAgreement), nullptr,
                            nullptr);
            CreateWindowExW(0, L"BUTTON", L"查看协议",
                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                            270, 92, 130, 24, hwnd,
                            reinterpret_cast<HMENU>(kIdViewLicense), nullptr,
                            nullptr);
            // AC4: whats-new text from RCDATA (UTF-8 -> wide).
            std::vector<std::uint8_t> whatsnew_bytes;
            std::wstring whatsnew_text = L"(无更新说明)";
            if (load_resource_bytes(ROSPINYIN_IDR_WHATSNEW, whatsnew_bytes)) {
                whatsnew_text = gui::format_whatsnew(
                    utf8_to_wide(whatsnew_bytes));
            }
            CreateWindowExW(0, L"STATIC", whatsnew_text.c_str(),
                            WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 126, 440, 80,
                            hwnd, reinterpret_cast<HMENU>(kIdWhatsNew), nullptr,
                            nullptr);
            g_status = CreateWindowExW(
                0, L"STATIC", L"请先勾选同意许可协议后再安装。",
                WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 214, 440, 44, hwnd,
                reinterpret_cast<HMENU>(kIdStatus), nullptr, nullptr);
            CreateWindowExW(
                0, L"STATIC", installer::kFontHintBanner.data(),
                WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 264, 440, 66, hwnd,
                reinterpret_cast<HMENU>(kIdFontHint), nullptr, nullptr);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == kIdAgreement) {
                const bool checked =
                    SendMessageW(reinterpret_cast<HWND>(lparam), BM_GETCHECK,
                                 0, 0) == BST_CHECKED;
                g_agreement.set_checked(checked);
                if (g_install_button) {
                    EnableWindow(g_install_button,
                                 g_agreement.install_enabled() ? TRUE
                                                               : FALSE);
                }
                set_status(g_agreement.install_enabled()
                               ? L"已同意协议，可以安装。"
                               : L"请先勾选同意许可协议后再安装。");
                return 0;
            }
            if (LOWORD(wparam) == kIdViewLicense) {
                open_license_window(g_instance);
                return 0;
            }
            if (LOWORD(wparam) == kIdInstall && g_agreement.install_enabled()) {
                do_install();
            }
            if (LOWORD(wparam) == kIdUninstall) do_uninstall();
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_instance = instance;
    WNDCLASSW wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    // Without a class background brush, DefWindowProc has nothing to erase the
    // client area with and the caller's window (desktop/Explorer) shows through.
    // Use the native dialog face colour so the shell matches standard Win32 UI.
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"RosPinyinInstallerGUI";
    if (!RegisterClassW(&wc)) return 1;

    const auto texts = gui::make_window_texts(ROSPINYIN_VERSION_STRING);
    HWND hwnd = CreateWindowExW(
        0, wc.lpszClassName, texts.title.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
        CW_USEDEFAULT, 480, 400, nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    // DEF-001: WS_TABSTOP alone does not move focus in a plain (non-dialog)
    // window. IsDialogMessage walks the tab chain; unrelated top-level windows
    // (license viewer / countdown) are passed through to DispatchMessage.
    const bool dialog_navigation =
        gui::agreement_control_spec().keyboard_focusable;
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (dialog_navigation && IsDialogMessageW(hwnd, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
