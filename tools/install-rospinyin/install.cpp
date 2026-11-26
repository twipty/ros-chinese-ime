// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// install-rospinyin.exe — WBS 2.3.2, rewritten from scratch against the M0
// red lines. The reference installer wrote every registry key in one shot
// and preloaded the IME; on ReactOS 0.4.16 that produced STOP 0x000000F4.
//
// RED LINE (M0): this tool NEVER writes HKCU\Keyboard Layout\<preload list>
// and NEVER makes RosPinyin the default layout. It only appends a selectable
// keyboard layout. tools/check_m1.py enforces this by scanning this file
// with comments stripped.
//
// RED LINE (M0): registration is split into steps so a single key at a time
// can be added and reboot-verified on ReactOS:
//   /step1  copy rospinyin.ime + rospinyin.dat into system32   (no registration)
//   /step2  ImmInstallIMEW -> dynamic HKL, save it    (system writes its key)
//   /step3  write "Layout Text"                      (one value)
//   /step4  write "Layout Display Name"              (one value)
//   /i      run step1..step4 in sequence (Windows reference machine ONLY)
//   /status print the recorded HKL, payload state and optional Droid font state
//   /u      uninstall (removes our own keys and files; leaves other layouts)
//   /fontinstall <DROID.TTF>  optional: install the nightly fallback font pack
//
// WBS 2.3.1 / D3: the HKL is whatever ImmInstallIMEW returns; 0xE0100804 is
// only a troubleshooting hint, never a hardcoded value.
//
// M2 update (P1-1): the payload is TWO files. rospinyin.dat is the code
// table the .ime maps at first key; an .ime without it shows an empty
// candidate window, and the old installer silently produced exactly that.
// Both files are copied, size-verified, and — because a running IME (or the
// shell's preload) keeps the image section mapped, which makes an in-place
// overwrite fail with ERROR_SHARING_VIOLATION — renamed aside and replaced.
//
// M4 update (2026-11-26): the payload is THREE files. rospinyin-lexicon.dat
// carries the word list; without it the IME loads but produces no phrases.
// All three are copied, size- and SHA-256-verified, and removed on /u.

#include <windows.h>
#include <imm.h>
#include "droid_font.hpp"
// M4: SHA-256 for the install-time word-list check. payload_blob.hpp holds
// only the algorithm (no embedded payload bytes) so including it cannot bloat
// this CLI with the 27 MB generated macro. lexicon_guard.hpp is the single
// source of the pinned size/hash shared with the IME loader and CI.
#include "payload_blob.hpp"
#include <rospinyin/lexicon_guard.hpp>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <initializer_list>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kImeFileName[]     = L"rospinyin.ime";
constexpr wchar_t kDatFileName[]     = L"rospinyin.dat";
constexpr wchar_t kLexiconFileName[] = L"rospinyin-lexicon.dat";
constexpr wchar_t kLayoutText[]      = L"中文(简体) - RosPinyin";
constexpr wchar_t kKeyboardLayouts[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts";
constexpr wchar_t kRegInstall[]      = L"SOFTWARE\\RosPinyin\\Install";
constexpr wchar_t kRegHklValue[]     = L"HKL";
constexpr wchar_t kLayoutDisplayName[] =
    L"@%SystemRoot%\\system32\\rospinyin.ime,-1024";

// Exit codes are part of the contract with m0/vm/*.sh: 0 = done, 2 = the
// payload could not be placed (reboot required), 3 = registry, 4 = the
// installed copy does not match the source (partial write).
enum : int {
    kOk = 0,
    kBadArgs = 64,
    kIoError = 1,
    kNeedsReboot = 2,
    kRegError = 3,
    kVerifyFailed = 4,
};

void log_error(const wchar_t* what, DWORD error) {
    std::fwprintf(stderr, L"[error] %ls (GetLastError=%lu)\n", what,
                  static_cast<unsigned long>(error));
}

bool system_path(const wchar_t* name, wchar_t* out, std::size_t cch) {
    UINT n = GetSystemDirectoryW(out, static_cast<UINT>(cch));
    if (n == 0 || n >= cch) {
        return false;
    }
    if (std::wcslen(out) + 1 + std::wcslen(name) >= cch) {
        return false;
    }
    std::wcscat(out, L"\\");
    std::wcscat(out, name);
    return true;
}

bool source_path(const wchar_t* name, wchar_t* out, std::size_t cch) {
    wchar_t exe[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0) {
        return false;
    }
    wchar_t* slash = std::wcsrchr(exe, L'\\');
    if (!slash) {
        return false;
    }
    slash[1] = L'\0';
    if (std::wcslen(exe) + std::wcslen(name) >= cch) {
        return false;
    }
    std::wcscpy(out, exe);
    std::wcscat(out, name);
    return true;
}

bool file_size(const wchar_t* path, unsigned long long* out) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data)) {
        return false;
    }
    *out = (static_cast<unsigned long long>(data.nFileSizeHigh) << 32) |
           data.nFileSizeLow;
    return true;
}

// A loaded .ime / mapped .dat cannot be overwritten in place: the section is
// still mapped by every process that has the IME selected, so CopyFileW
// returns ERROR_SHARING_VIOLATION (32) or ERROR_ACCESS_DENIED (5).
// What Windows does allow is moving the *name*: the mapped image keeps the
// old file alive, and the new copy takes the freed name. The stale file is
// left as <name>.old and removed on the next install or by /u.
bool move_aside(const wchar_t* dst) {
    wchar_t old[MAX_PATH]{};
    if (std::wcslen(dst) + 5 >= MAX_PATH) return false;
    std::wcscpy(old, dst);
    std::wcscat(old, L".old");
    DeleteFileW(old);  // best effort: an older .old may still be present
    return MoveFileExW(dst, old, MOVEFILE_REPLACE_EXISTING) != 0;
}

int copy_one(const wchar_t* name) {
    wchar_t src[MAX_PATH]{}, dst[MAX_PATH]{};
    if (!source_path(name, src, MAX_PATH)) {
        log_error(L"cannot resolve the source path", GetLastError());
        return kIoError;
    }
    if (!system_path(name, dst, MAX_PATH)) {
        log_error(L"cannot resolve the system path", GetLastError());
        return kIoError;
    }
    if (GetFileAttributesW(src) == INVALID_FILE_ATTRIBUTES) {
        std::fwprintf(stderr,
                      L"[error] %ls is missing next to this exe.\n"
                      L"        Build it first: cmake --build build-win-trace\n",
                      name);
        return kIoError;
    }

    if (CopyFileW(src, dst, FALSE)) {
        std::fwprintf(stdout, L"[step1] copied %ls\n", dst);
        return kOk;
    }

    const DWORD error = GetLastError();
    if (error != ERROR_SHARING_VIOLATION && error != ERROR_ACCESS_DENIED) {
        log_error(L"CopyFileW failed", error);
        return kIoError;
    }

    // In use: rename aside and retry once.
    if (!move_aside(dst)) {
        std::fwprintf(stderr,
                      L"[error] %ls is in use and cannot be renamed aside "
                      L"(err=%lu).\n"
                      L"        Reboot ReactOS, then run /step1 again.\n",
                      dst, static_cast<unsigned long>(GetLastError()));
        return kNeedsReboot;
    }
    if (!CopyFileW(src, dst, FALSE)) {
        log_error(L"CopyFileW after renaming the old file aside",
                  GetLastError());
        return kNeedsReboot;
    }
    std::fwprintf(stdout,
                  L"[step1] copied %ls (old file kept as %ls.old; the running "
                  L"IME still uses it until reboot)\n",
                  dst, dst);
    return kOk;
}

// M4: read a file into memory and return its SHA-256 as lowercase hex.
// Used for the install-time double check (source hash == installed hash ==
// pinned constant). A short read is a hard error, never a partial digest.
bool file_sha256(const wchar_t* path, std::string* out) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    const DWORD size = GetFileSize(file, nullptr);
    if (size == 0 || size == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return false;
    }
    std::vector<std::uint8_t> bytes(size);
    DWORD read = 0;
    const BOOL ok = ReadFile(file, bytes.data(), size, &read, nullptr);
    CloseHandle(file);
    if (!ok || read != size) return false;
    *out = rospinyin::installer::sha256_hex(bytes.data(), bytes.size());
    return true;
}

int step1_copy_payload() {
    const int ime = copy_one(kImeFileName);
    if (ime != kOk) return ime;
    const int dat = copy_one(kDatFileName);
    if (dat != kOk) return dat;
    // M4: the word list is mandatory; a missing or unreadable source is a
    // hard install failure, never a "single-character-only" half install.
    const int lex = copy_one(kLexiconFileName);
    if (lex != kOk) return lex;

    // Size check: a truncated copy (disk full, killed process) is worse than
    // a failed one because the IME then loads and misparses the table.
    for (const wchar_t* name :
         {kImeFileName, kDatFileName, kLexiconFileName}) {
        wchar_t src[MAX_PATH]{}, dst[MAX_PATH]{};
        unsigned long long a = 0, b = 0;
        if (!source_path(name, src, MAX_PATH) ||
            !system_path(name, dst, MAX_PATH) ||
            !file_size(src, &a) || !file_size(dst, &b)) {
            log_error(L"cannot stat the copied file", GetLastError());
            return kVerifyFailed;
        }
        if (a != b) {
            std::fwprintf(stderr,
                          L"[error] %ls: source %lu bytes, installed %lu "
                          L"bytes\n",
                          name, static_cast<unsigned long>(a),
                          static_cast<unsigned long>(b));
            return kVerifyFailed;
        }
        // M4: the word list must additionally match the pinned SHA-256 that
        // lexicon_guard uses at runtime. ime/dat are versioned independently
        // and are only checked source==installed.
        std::string src_sha, dst_sha;
        if (!file_sha256(src, &src_sha) || !file_sha256(dst, &dst_sha)) {
            log_error(L"cannot hash the copied file", GetLastError());
            return kVerifyFailed;
        }
        if (src_sha != dst_sha) {
            std::fwprintf(stderr,
                          L"[error] %ls: source/installed SHA-256 differ\n"
                          L"        src=%hs\n        dst=%hs\n",
                          name, src_sha.c_str(), dst_sha.c_str());
            return kVerifyFailed;
        }
        if (name == kLexiconFileName) {
            const char* pinned =
                rospinyin::lexicon::kPolicy.expected_sha256;
            if (a != rospinyin::lexicon::kPolicy.expected_size ||
                src_sha != pinned) {
                std::fwprintf(stderr,
                              L"[error] word list hash mismatch: got %hs, "
                              L"expected %hs (size %lu)\n",
                              src_sha.c_str(), pinned,
                              static_cast<unsigned long>(a));
                return kVerifyFailed;
            }
            std::fwprintf(stdout,
                          L"[step1] %ls verified (%lu bytes, SHA-256 OK)\n",
                          name, static_cast<unsigned long>(a));
            continue;
        }
        std::fwprintf(stdout, L"[step1] %ls verified (%lu bytes)\n", name,
                      static_cast<unsigned long>(a));
    }
    return kOk;
}

unsigned hkl_to_uint(HKL hkl) {
    return static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(hkl));
}

HKL uint_to_hkl(unsigned value) {
    return reinterpret_cast<HKL>(static_cast<std::uintptr_t>(value));
}

bool save_hkl(HKL hkl) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegInstall, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t text[16]{};
    std::swprintf(text, 16, L"%08X", hkl_to_uint(hkl));
    const DWORD cb = static_cast<DWORD>(
        (std::wcslen(text) + 1) * sizeof(wchar_t));
    const LSTATUS st = RegSetValueExW(key, kRegHklValue, 0, REG_SZ,
                                      reinterpret_cast<const BYTE*>(text), cb);
    RegCloseKey(key);
    return st == ERROR_SUCCESS;
}

bool load_hkl(HKL* out) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegInstall, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return false;
    }
    wchar_t text[16]{};
    DWORD cb = sizeof(text);
    DWORD type = 0;
    const LSTATUS st = RegQueryValueExW(key, kRegHklValue, nullptr, &type,
                                        reinterpret_cast<BYTE*>(text), &cb);
    RegCloseKey(key);
    if (st != ERROR_SUCCESS || type != REG_SZ) {
        return false;
    }
    text[15] = L'\0';
    *out = uint_to_hkl(static_cast<unsigned>(std::wcstoul(text, nullptr, 16)));
    return true;
}

bool open_layout_key(HKEY* out) {
    HKL hkl = nullptr;
    if (!load_hkl(&hkl) || !hkl) {
        return false;
    }
    wchar_t sub[MAX_PATH]{};
    std::swprintf(sub, MAX_PATH, L"%ls\\%08X", kKeyboardLayouts,
                  hkl_to_uint(hkl));
    return RegOpenKeyExW(HKEY_LOCAL_MACHINE, sub, 0, KEY_WRITE, out) ==
           ERROR_SUCCESS;
}

bool write_value(HKEY key, const wchar_t* name, const wchar_t* value,
                 DWORD type) {
    const DWORD cb = static_cast<DWORD>(
        (std::wcslen(value) + 1) * sizeof(wchar_t));
    return RegSetValueExW(key, name, 0, type,
                          reinterpret_cast<const BYTE*>(value), cb) ==
           ERROR_SUCCESS;
}

int step1_copy_file() { return step1_copy_payload(); }

int step2_install_layout() {
    wchar_t dst[MAX_PATH]{};
    if (!system_path(kImeFileName, dst, MAX_PATH)) {
        return kIoError;
    }
    const HKL hkl = ImmInstallIMEW(dst, kLayoutText);
    if (!hkl) {
        log_error(L"ImmInstallIMEW failed", GetLastError());
        return kRegError;
    }
    if (!save_hkl(hkl)) {
        log_error(L"cannot record the returned HKL", GetLastError());
        return kRegError;
    }
    std::fwprintf(stdout, L"[step2] ImmInstallIMEW returned HKL=%08X\n",
                  hkl_to_uint(hkl));
    return kOk;
}

int step3_layout_text() {
    HKEY key = nullptr;
    if (!open_layout_key(&key)) {
        log_error(L"cannot open our Keyboard Layouts key (run /step2 first)",
                  GetLastError());
        return kRegError;
    }
    const bool ok = write_value(key, L"Layout Text", kLayoutText, REG_SZ);
    RegCloseKey(key);
    if (!ok) {
        log_error(L"cannot write Layout Text", GetLastError());
        return kRegError;
    }
    std::fwprintf(stdout, L"[step3] wrote Layout Text\n");
    return kOk;
}

int step4_display_name() {
    HKEY key = nullptr;
    if (!open_layout_key(&key)) {
        log_error(L"cannot open our Keyboard Layouts key (run /step2 first)",
                  GetLastError());
        return kRegError;
    }
    const bool ok = write_value(key, L"Layout Display Name",
                                kLayoutDisplayName, REG_EXPAND_SZ);
    RegCloseKey(key);
    if (!ok) {
        log_error(L"cannot write Layout Display Name", GetLastError());
        return kRegError;
    }
    std::fwprintf(stdout, L"[step4] wrote Layout Display Name\n");
    return kOk;
}

void report_payload(const wchar_t* name) {
    wchar_t dst[MAX_PATH]{}, old[MAX_PATH]{};
    if (!system_path(name, dst, MAX_PATH)) {
        std::fwprintf(stdout, L"%-14ls : <path error>\n", name);
        return;
    }
    unsigned long long size = 0;
    if (file_size(dst, &size)) {
        // %lu, not %llu: this exe links msvcrt.dll (not MinGW's ANSI stdio),
        // and the "ll" length modifier is not portable across msvcrt builds.
        // A payload is never anywhere near 4 GiB, so the cast is lossless.
        std::fwprintf(stdout, L"%-14ls : present (%lu bytes)\n", name,
                      static_cast<unsigned long>(size));
    } else {
        std::fwprintf(stdout, L"%-14ls : MISSING\n", name);
    }
    std::wcscpy(old, dst);
    std::wcscat(old, L".old");
    if (GetFileAttributesW(old) != INVALID_FILE_ATTRIBUTES) {
        std::fwprintf(stdout,
                      L"stale payload  : %ls exists -> the running IME still "
                      L"uses it; reboot, then rerun /step1 to clean up\n",
                      old);
    }
}

// --- Optional Droid font pack (nightly 0.4.17-dev only) -----------------
// The font lives in %SystemRoot%\Fonts, not system32. A stable 0.4.16 guest
// already has it and must keep it: we only claim ownership after the user
// installs the separate package through /fontinstall.

bool droid_font_path(wchar_t* out, std::size_t cch) {
    UINT n = GetWindowsDirectoryW(out, static_cast<UINT>(cch));
    if (n == 0 || n >= cch) return false;
    static constexpr wchar_t suffix[] = L"\\Fonts\\";
    if (std::wcslen(out) + std::wcslen(suffix) +
            std::wcslen(rospinyin::installer::kDroidFontFile) >= cch) {
        return false;
    }
    std::wcscat(out, suffix);
    std::wcscat(out, rospinyin::installer::kDroidFontFile);
    return true;
}

bool droid_owned() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegInstall, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return false;
    }
    DWORD value = 0, type = 0, size = sizeof(value);
    const bool owned =
        RegQueryValueExW(key, rospinyin::installer::kDroidOwnedValue, nullptr,
                         &type, reinterpret_cast<BYTE*>(&value), &size) ==
            ERROR_SUCCESS &&
        type == REG_DWORD && value == 1;
    RegCloseKey(key);
    return owned;
}

bool droid_registered() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, rospinyin::installer::kFontsKey, 0,
                      KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t value[MAX_PATH]{};
    DWORD size = sizeof(value), type = 0;
    const bool registered =
        RegQueryValueExW(key, rospinyin::installer::kDroidFontValue, nullptr,
                         &type, reinterpret_cast<BYTE*>(value), &size) ==
        ERROR_SUCCESS;
    RegCloseKey(key);
    return registered;
}

rospinyin::installer::DroidFontState droid_state() {
    wchar_t path[MAX_PATH]{};
    if (!droid_font_path(path, MAX_PATH)) {
        return rospinyin::installer::DroidFontState::Missing;
    }
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
        return rospinyin::installer::DroidFontState::Missing;
    }
    return droid_owned() ? rospinyin::installer::DroidFontState::Owned
                         : rospinyin::installer::DroidFontState::System;
}

int font_install(const wchar_t* source) {
    namespace font = rospinyin::installer;
    unsigned long long size = 0;
    if (!file_size(source, &size)) {
        log_error(L"cannot read the font package file", GetLastError());
        return kIoError;
    }
    if (size != font::kDroidFontExpectedSize) {
        std::fwprintf(stderr,
                      L"[error] DROID.TTF size mismatch: got %llu, "
                      L"expected %llu; refusing to install\n",
                      size, font::kDroidFontExpectedSize);
        return kVerifyFailed;
    }
    wchar_t dst[MAX_PATH]{};
    if (!droid_font_path(dst, MAX_PATH)) {
        log_error(L"cannot resolve the Fonts directory", GetLastError());
        return kIoError;
    }
    // Rule: never replace a font the OS already shipped. Only a previously
    // owned file (from our own optional pack) may be refreshed.
    if (GetFileAttributesW(dst) != INVALID_FILE_ATTRIBUTES && !droid_owned()) {
        std::fwprintf(stdout,
                      L"[fontinstall] system Droid font already present; "
                      L"not replacing it\n");
        return kVerifyFailed;
    }
    if (!CopyFileW(source, dst, FALSE)) {
        log_error(L"cannot copy DROID.TTF into the Fonts directory",
                  GetLastError());
        return kIoError;
    }
    HKEY fonts = nullptr;
    LSTATUS st = RegOpenKeyExW(HKEY_LOCAL_MACHINE, font::kFontsKey, 0,
                               KEY_WRITE, &fonts);
    if (st != ERROR_SUCCESS) {
        st = RegCreateKeyExW(HKEY_LOCAL_MACHINE, font::kFontsKey, 0, nullptr,
                             REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                             &fonts, nullptr);
    }
    if (st != ERROR_SUCCESS) {
        log_error(L"cannot open the Fonts registry key", st);
        return kRegError;
    }
    st = RegSetValueExW(fonts, font::kDroidFontValue, 0, REG_SZ,
                        reinterpret_cast<const BYTE*>(font::kDroidFontFile),
                        sizeof(font::kDroidFontFile));
    RegCloseKey(fonts);
    if (st != ERROR_SUCCESS) {
        log_error(L"cannot register DROID.TTF", st);
        return kRegError;
    }
    HKEY own = nullptr;
    st = RegCreateKeyExW(HKEY_CURRENT_USER, kRegInstall, 0, nullptr,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &own,
                         nullptr);
    if (st == ERROR_SUCCESS) {
        const DWORD one = 1;
        st = RegSetValueExW(own, font::kDroidOwnedValue, 0, REG_DWORD,
                            reinterpret_cast<const BYTE*>(&one), sizeof(one));
        RegCloseKey(own);
    }
    if (st != ERROR_SUCCESS) {
        log_error(L"cannot record font-pack ownership", st);
        return kRegError;
    }
    std::fwprintf(stdout,
                  L"[fontinstall] installed optional Droid font pack; "
                  L"reboot to render Chinese on nightly\n");
    return kOk;
}

int show_status() {
    report_payload(kImeFileName);
    report_payload(kDatFileName);
    report_payload(kLexiconFileName);
    // M4 / AC4: show an explicit hash-verdict line for the word list, not just
    // its presence. "present" alone was the old silent-single-char symptom.
    {
        wchar_t dst[MAX_PATH]{};
        if (system_path(kLexiconFileName, dst, MAX_PATH)) {
            std::string sha;
            if (file_sha256(dst, &sha)) {
                const bool ok =
                    sha == rospinyin::lexicon::kPolicy.expected_sha256;
                std::fwprintf(stdout,
                              L"lexicon sha   : %hs (%ls)\n",
                              sha.c_str(), ok ? L"OK" : L"MISMATCH");
            } else {
                std::fwprintf(stdout,
                              L"lexicon sha   : <unreadable>\n");
            }
        }
    }

    HKL hkl = nullptr;
    if (load_hkl(&hkl) && hkl) {
        std::fwprintf(stdout, L"recorded HKL  : %08X\n", hkl_to_uint(hkl));
    } else {
        std::fwprintf(stdout, L"recorded HKL  : (none)\n");
    }
    HKEY key = nullptr;
    std::fwprintf(stdout, L"layout key    : %ls\n",
                  open_layout_key(&key) ? L"present" : L"missing");
    if (key) {
        RegCloseKey(key);
    }
    const auto state = droid_state();
    const bool registered = droid_registered();
    std::fwprintf(stdout, L"droid font    : %ls\n",
                  rospinyin::installer::droid_font_state_label(state));
    std::fwprintf(stdout, L"droid font reg: %ls\n",
                  registered ? L"present" : L"missing");
    const auto hint = rospinyin::installer::droid_font_status_hint(
        state, registered);
    if (!hint.empty()) {
        std::fwprintf(stdout, L"%ls\n", hint.data());
    }
    return kOk;
}

int uninstall() {
    // Capture ownership before the HKCU install key is deleted below.
    const bool remove_font =
        rospinyin::installer::should_remove_droid_font(droid_state());
    HKL hkl = nullptr;
    if (load_hkl(&hkl) && hkl) {
        wchar_t sub[MAX_PATH]{};
        std::swprintf(sub, MAX_PATH, L"%ls\\%08X", kKeyboardLayouts,
                      hkl_to_uint(hkl));
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, sub);
    }
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegInstall, 0, KEY_WRITE, &key) ==
        ERROR_SUCCESS) {
        RegDeleteValueW(key, kRegHklValue);
        RegCloseKey(key);
    }
    RegDeleteKeyW(HKEY_CURRENT_USER, kRegInstall);

    // All three payload files, plus any stale .old left by a live replacement.
    // M4: the word list is removed symmetrically with ime/dat (owner decision
    // 2026-11-26) so a v0.2 uninstall leaves no orphaned dictionary.
    for (const wchar_t* name :
         {kImeFileName, kDatFileName, kLexiconFileName}) {
        wchar_t dst[MAX_PATH]{};
        if (!system_path(name, dst, MAX_PATH)) continue;
        if (!DeleteFileW(dst)) {
            const DWORD error = GetLastError();
            if (error == ERROR_SHARING_VIOLATION ||
                error == ERROR_ACCESS_DENIED) {
                std::fwprintf(stdout,
                              L"[uninstall] %ls is in use; it will disappear "
                              L"after a reboot.\n",
                              dst);
            }
        }
        std::wcscat(dst, L".old");
        DeleteFileW(dst);
    }
    // Optional nightly font pack: symmetric cleanup only when we installed it.
    if (remove_font) {
        HKEY fonts = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                          rospinyin::installer::kFontsKey, 0, KEY_WRITE,
                          &fonts) == ERROR_SUCCESS) {
            RegDeleteValueW(fonts, rospinyin::installer::kDroidFontValue);
            RegCloseKey(fonts);
        }
        wchar_t font[MAX_PATH]{};
        if (droid_font_path(font, MAX_PATH)) {
            DeleteFileW(font);
        }
        HKEY own = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegInstall, 0, KEY_WRITE,
                          &own) == ERROR_SUCCESS) {
            RegDeleteValueW(own, rospinyin::installer::kDroidOwnedValue);
            RegCloseKey(own);
        }
        std::fwprintf(stdout,
                      L"[uninstall] optional Droid font pack removed\n");
    } else {
        std::fwprintf(stdout,
                      L"[uninstall] Droid font left untouched (not installed "
                      L"by this package)\n");
    }
    // Deliberately no preload-list cleanup: we never wrote one.
    std::fwprintf(stdout, L"[uninstall] done\n");
    return kOk;
}

int install_all() {
    const int rc1 = step1_copy_payload();
    if (rc1) return rc1;
    const int rc2 = step2_install_layout();
    if (rc2) return rc2;
    const int rc3 = step3_layout_text();
    if (rc3) return rc3;
    return step4_display_name();
}

void usage() {
    std::fwprintf(stdout,
        L"RosPinyin installer (M4)\n"
        L"  /step1   copy ime + dat + lexicon into system32 (size + SHA-256)\n"
        L"  /step2   ImmInstallIMEW, record the dynamic HKL\n"
        L"  /step3   write Layout Text\n"
        L"  /step4   write Layout Display Name\n"
        L"  /i       all steps at once (Windows reference machine ONLY)\n"
        L"  /status  show payload / lexicon hash / HKL / layout / font state\n"
        L"  /u       uninstall\n"
        L"  /fontinstall <DROID.TTF>  optional nightly font pack\n"
        L"On ReactOS run /step1../step4 separately and reboot after each.\n"
        L"This tool never adds a default keyboard layout (M0 0xF4 red line).\n");
}

}  // namespace

// wmain is the Unicode entry point: MinGW needs -municode (set in
// CMakeLists.txt); MSVC supports it natively. On Linux this is a plain
// function used only by the host-side syntax check.
int wmain(int argc, wchar_t** argv) {
    const bool font_install_cmd =
        argc == 3 && std::wcscmp(argv[1], L"/fontinstall") == 0;
    if (argc != 2 && !font_install_cmd) {
        usage();
        return kBadArgs;
    }
    const wchar_t* cmd = argv[1];
    if (std::wcscmp(cmd, L"/step1") == 0) return step1_copy_file();
    if (std::wcscmp(cmd, L"/step2") == 0) return step2_install_layout();
    if (std::wcscmp(cmd, L"/step3") == 0) return step3_layout_text();
    if (std::wcscmp(cmd, L"/step4") == 0) return step4_display_name();
    if (std::wcscmp(cmd, L"/i") == 0) return install_all();
    if (std::wcscmp(cmd, L"/status") == 0) return show_status();
    if (std::wcscmp(cmd, L"/u") == 0) return uninstall();
    if (font_install_cmd) return font_install(argv[2]);
    usage();
    return kBadArgs;
}
