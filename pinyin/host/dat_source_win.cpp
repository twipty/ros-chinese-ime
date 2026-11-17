// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Windows half of the scheme-owned data source: read
// <SystemDirectory>\rospinyin.dat and parse it through the generic
// imfw::CodeTable. Compiled into the assembled rospinyin.ime only (see
// ime/CMakeLists.txt); host tests link the weak fallback in
// pinyin/src/dat_source.cpp instead.

#include <rospinyin/dat_source.hpp>

#include <windows.h>

#include <string>
#include <vector>

namespace rospinyin {
namespace {

constexpr wchar_t kDatFilename[] = L"rospinyin.dat";

}  // namespace

TablePtr load_system_table() {
    wchar_t system_dir[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(system_dir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return nullptr;

    const std::wstring path = std::wstring(system_dir) + L"\\" + kDatFilename;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) return nullptr;

    const DWORD size = GetFileSize(file, nullptr);
    if (size == 0 || size == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return nullptr;
    }

    std::vector<char> bytes(size);
    DWORD read = 0;
    const BOOL ok = ReadFile(file, bytes.data(), size, &read, nullptr);
    CloseHandle(file);
    if (!ok || read != size) return nullptr;

    auto table = std::make_shared<imfw::CodeTable>();
    if (table->load_dat(std::string_view(bytes.data(), bytes.size())) == 0) {
        return nullptr;
    }
    return table;
}

}  // namespace rospinyin
