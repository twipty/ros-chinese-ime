// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// See dat_loader.hpp. This file is Windows-only (compiled into the assembled
// .ime) and is never called from DllMain.

#include "dat_loader.hpp"

#include <windows.h>

#include <string>
#include <vector>

namespace imfw {

bool load_system_dat(CodeTable& out, const wchar_t* dat_filename) {
    if (dat_filename == nullptr || dat_filename[0] == L'\0') return false;

    wchar_t system_dir[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(system_dir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return false;

    const std::wstring path = std::wstring(system_dir) + L"\\" + dat_filename;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    const DWORD size = GetFileSize(file, nullptr);
    if (size == 0 || size == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return false;
    }

    std::vector<char> bytes(size);
    DWORD read = 0;
    const BOOL ok = ReadFile(file, bytes.data(), size, &read, nullptr);
    CloseHandle(file);
    if (!ok || read != size) return false;

    CodeTable candidate;
    if (candidate.load_dat(std::string_view(bytes.data(), bytes.size())) == 0) {
        return false;
    }
    out = std::move(candidate);
    return true;
}

bool load_ime_table(CodeTable& out, const Scheme& scheme) {
    if (load_system_dat(out, scheme.dat_filename())) return true;
    return scheme.load_fallback(out);
}

}  // namespace imfw
