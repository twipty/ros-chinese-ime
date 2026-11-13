// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// install-rospinyin.exe — WBS 2.3.2, rewritten from scratch against the M0
// red lines. The reference installer wrote every registry key in one shot
// and preloaded the IME; on ReactOS 0.4.16 that produced STOP 0x000000F4.
//
// RED LINE (M0): this tool NEVER writes HKCU\Keyboard Layout\Preload and
// NEVER makes RosPinyin the default layout. It only appends a selectable
// keyboard layout. tools/check_m1.py enforces this by scanning this file
// with comments stripped.
//
// RED LINE (M0): registration is split into steps so a single key at a time
// can be added and reboot-verified on ReactOS:
//   /step1  copy rospinyin.ime into system32          (no registration)
//   /step2  ImmInstallIMEW -> dynamic HKL, save it    (system writes its key)
//   /step3  write "Layout Text"                      (one value)
//   /step4  write "Layout Display Name"              (one value)
//   /i      run step1..step4 in sequence (Windows reference machine ONLY)
//   /status print the recorded HKL and file state
//   /u      uninstall (removes our own keys and file; leaves other layouts)
//
// WBS 2.3.1 / D3: the HKL is whatever ImmInstallIMEW returns; 0xE0100804 is
// only a troubleshooting hint, never a hardcoded value.

#include <windows.h>
#include <imm.h>
#include <cstdint>
#include <cstdio>
#include <cwchar>

namespace {

constexpr wchar_t kImeFileName[]     = L"rospinyin.ime";
constexpr wchar_t kLayoutText[]      = L"中文(简体) - RosPinyin";
constexpr wchar_t kKeyboardLayouts[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts";
constexpr wchar_t kRegInstall[]      = L"SOFTWARE\\RosPinyin\\Install";
constexpr wchar_t kRegHklValue[]     = L"HKL";
constexpr wchar_t kLayoutDisplayName[] =
    L"@%SystemRoot%\\system32\\rospinyin.ime,-1024";

void log_error(const wchar_t* what, DWORD error) {
    std::fwprintf(stderr, L"[error] %ls (GetLastError=%lu)\n", what,
                  static_cast<unsigned long>(error));
}

bool system_ime_path(wchar_t* out, std::size_t cch) {
    UINT n = GetSystemDirectoryW(out, static_cast<UINT>(cch));
    if (n == 0 || n >= cch) {
        return false;
    }
    if (std::wcslen(out) + 1 + std::wcslen(kImeFileName) >= cch) {
        return false;
    }
    std::wcscat(out, L"\\");
    std::wcscat(out, kImeFileName);
    return true;
}

bool source_ime_path(wchar_t* out, std::size_t cch) {
    wchar_t exe[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0) {
        return false;
    }
    wchar_t* slash = std::wcsrchr(exe, L'\\');
    if (!slash) {
        return false;
    }
    slash[1] = L'\0';
    if (std::wcslen(exe) + std::wcslen(kImeFileName) >= cch) {
        return false;
    }
    std::wcscpy(out, exe);
    std::wcscat(out, kImeFileName);
    return true;
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

int step1_copy_file() {
    wchar_t src[MAX_PATH]{}, dst[MAX_PATH]{};
    if (!source_ime_path(src, MAX_PATH) || !system_ime_path(dst, MAX_PATH)) {
        log_error(L"cannot resolve IME paths", GetLastError());
        return 1;
    }
    if (!CopyFileW(src, dst, FALSE)) {
        log_error(L"CopyFileW failed (is rospinyin.ime next to this exe?)",
                  GetLastError());
        return 2;
    }
    std::fwprintf(stdout, L"[step1] copied to %ls\n", dst);
    return 0;
}

int step2_install_layout() {
    wchar_t dst[MAX_PATH]{};
    if (!system_ime_path(dst, MAX_PATH)) {
        return 1;
    }
    const HKL hkl = ImmInstallIMEW(dst, kLayoutText);
    if (!hkl) {
        log_error(L"ImmInstallIMEW failed", GetLastError());
        return 2;
    }
    if (!save_hkl(hkl)) {
        log_error(L"cannot record the returned HKL", GetLastError());
        return 3;
    }
    std::fwprintf(stdout, L"[step2] ImmInstallIMEW returned HKL=%08X\n",
                  hkl_to_uint(hkl));
    return 0;
}

int step3_layout_text() {
    HKEY key = nullptr;
    if (!open_layout_key(&key)) {
        log_error(L"cannot open our Keyboard Layouts key (run /step2 first)",
                  GetLastError());
        return 1;
    }
    const bool ok = write_value(key, L"Layout Text", kLayoutText, REG_SZ);
    RegCloseKey(key);
    if (!ok) {
        log_error(L"cannot write Layout Text", GetLastError());
        return 2;
    }
    std::fwprintf(stdout, L"[step3] wrote Layout Text\n");
    return 0;
}

int step4_display_name() {
    HKEY key = nullptr;
    if (!open_layout_key(&key)) {
        log_error(L"cannot open our Keyboard Layouts key (run /step2 first)",
                  GetLastError());
        return 1;
    }
    const bool ok = write_value(key, L"Layout Display Name",
                                kLayoutDisplayName, REG_EXPAND_SZ);
    RegCloseKey(key);
    if (!ok) {
        log_error(L"cannot write Layout Display Name", GetLastError());
        return 2;
    }
    std::fwprintf(stdout, L"[step4] wrote Layout Display Name\n");
    return 0;
}

int show_status() {
    wchar_t dst[MAX_PATH]{};
    if (!system_ime_path(dst, MAX_PATH)) {
        return 1;
    }
    const bool file_ok = GetFileAttributesW(dst) != INVALID_FILE_ATTRIBUTES;
    std::fwprintf(stdout, L"IME file : %ls (%ls)\n", dst,
                  file_ok ? L"present" : L"missing");
    HKL hkl = nullptr;
    if (load_hkl(&hkl) && hkl) {
        std::fwprintf(stdout, L"recorded HKL : %08X\n", hkl_to_uint(hkl));
    } else {
        std::fwprintf(stdout, L"recorded HKL : (none)\n");
    }
    HKEY key = nullptr;
    std::fwprintf(stdout, L"layout key   : %ls\n",
                  open_layout_key(&key) ? L"present" : L"missing");
    if (key) {
        RegCloseKey(key);
    }
    return 0;
}

int uninstall() {
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
    wchar_t dst[MAX_PATH]{};
    if (system_ime_path(dst, MAX_PATH)) {
        DeleteFileW(dst);
    }
    // Deliberately no Preload cleanup: we never wrote one.
    std::fwprintf(stdout, L"[uninstall] done\n");
    return 0;
}

int install_all() {
    const int rc1 = step1_copy_file();
    if (rc1) return rc1;
    const int rc2 = step2_install_layout();
    if (rc2) return rc2;
    const int rc3 = step3_layout_text();
    if (rc3) return rc3;
    return step4_display_name();
}

void usage() {
    std::fwprintf(stdout,
        L"RosPinyin installer (M1)\n"
        L"  /step1   copy rospinyin.ime into system32 only\n"
        L"  /step2   ImmInstallIMEW, record the dynamic HKL\n"
        L"  /step3   write Layout Text\n"
        L"  /step4   write Layout Display Name\n"
        L"  /i       all steps at once (Windows reference machine ONLY)\n"
        L"  /status  show file / HKL / layout key state\n"
        L"  /u       uninstall\n"
        L"On ReactOS run /step1../step4 separately and reboot after each.\n"
        L"This tool never adds a default keyboard layout (M0 0xF4 red line).\n");
}

}  // namespace

// wmain is the Unicode entry point: MinGW needs -municode (set in
// CMakeLists.txt); MSVC supports it natively. On Linux this is a plain
// function used only by the host-side syntax check.
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        usage();
        return 64;
    }
    const wchar_t* cmd = argv[1];
    if (std::wcscmp(cmd, L"/step1") == 0) return step1_copy_file();
    if (std::wcscmp(cmd, L"/step2") == 0) return step2_install_layout();
    if (std::wcscmp(cmd, L"/step3") == 0) return step3_layout_text();
    if (std::wcscmp(cmd, L"/step4") == 0) return step4_display_name();
    if (std::wcscmp(cmd, L"/i") == 0) return install_all();
    if (std::wcscmp(cmd, L"/status") == 0) return show_status();
    if (std::wcscmp(cmd, L"/u") == 0) return uninstall();
    usage();
    return 64;
}
