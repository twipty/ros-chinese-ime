// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Windows half of the scheme-owned data source: read
// <SystemDirectory>\rospinyin.dat and parse it through the generic
// imfw::CodeTable. A member of rospinyin-engine on Windows (pinyin/CMakeLists.txt,
// X2-e2), which is also where ROSPINYIN_REQUIRE_SYSTEM_TABLE compiles out the
// weak stub that host tests link instead.

#include <rospinyin/dat_source.hpp>

#include <rospinyin/pinyin.hpp>

#include <windows.h>

#include <string>
#include <string_view>
#include <vector>

#include <imfw/trace_stamp.hpp>

namespace rospinyin {
namespace {

constexpr wchar_t kDatFilename[] = L"rospinyin.dat";

#ifdef ROSPINYIN_TRACE
// 数据指纹：让读表的人自己说读了哪张表。
//
// "改了 .dat 却没生效"已经反复出现三轮，而宿主侧看到的只有「候选没变化」——
// 那既可能是文件没换、也可能是换了但被别的进程缓存、还可能是换错了目录。
// 从 .ime 内部把路径/大小/字数/音节数，以及一个语义探针（shui 里有没有简体
// 谁 U+8C01）写进 trace，任何一轮都能在三秒内判定用的是哪张表。
void AppendUint(std::string& out, unsigned long value) {
    char tmp[12];
    unsigned count = 0;
    do {
        tmp[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value != 0 && count < 11);
    while (count > 0) out.push_back(tmp[--count]);
}

void TraceDat(const std::string& body) {
    char line[512];
    unsigned len = imfw::trace::stamp(line, 0, sizeof(line));
    for (std::size_t i = 0; i < body.size() && len + 1 < sizeof(line); ++i) {
        line[len++] = body[i];
    }
    line[len++] = '\n';

    const HANDLE file = CreateFileA(
        imfw::trace::kTracePath, GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    // FILE_APPEND_DATA 与 GetFileSize 在本平台都会丢行，只用显式定位。
    SetFilePointer(file, 0, nullptr, FILE_END);
    DWORD written = 0;
    WriteFile(file, line, len, &written, nullptr);
    CloseHandle(file);
}

std::string NarrowPath(const std::wstring& wide) {
    char buf[MAX_PATH * 3] = {};
    const int n = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, buf,
                                      sizeof(buf), nullptr, nullptr);
    return n > 0 ? std::string(buf) : std::string("?");
}

bool HasCodepoint(const imfw::CodeTable& table, std::string_view key,
                  char32_t codepoint) {
    for (const auto& entry : table.lookup(key)) {
        if (entry.codepoint == codepoint) return true;
    }
    return false;
}
#endif

}  // namespace

TablePtr load_system_table() {
    wchar_t system_dir[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(system_dir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return nullptr;

    const std::wstring path = std::wstring(system_dir) + L"\\" + kDatFilename;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
#ifdef ROSPINYIN_TRACE
        std::string body = "dat MISSING path=" + NarrowPath(path) + " err=";
        AppendUint(body, GetLastError());
        body += " -> 回退内置表（候选会明显偏少，且没有多音字）";
        TraceDat(body);
#endif
        return nullptr;
    }

    const DWORD size = GetFileSize(file, nullptr);
    if (size == 0 || size == INVALID_FILE_SIZE) {
        CloseHandle(file);
#ifdef ROSPINYIN_TRACE
        std::string body = "dat TRUNCATED path=" + NarrowPath(path);
        body += " size=";
        AppendUint(body, static_cast<unsigned long>(size));
        TraceDat(body);
#endif
        return nullptr;
    }

    std::vector<char> bytes(size);
    DWORD read = 0;
    const BOOL ok = ReadFile(file, bytes.data(), size, &read, nullptr);
    CloseHandle(file);
    if (!ok || read != size) return nullptr;

    auto table = std::make_shared<imfw::CodeTable>();
    // syllable_of turns an alternative reading ("she2i"/"shui2") into the
    // toneless lookup key the syllable table is keyed by. Without it the alt
    // readings in the dat would be inert and polyphonic characters would only
    // be reachable through the single kMandarin reading.
    if (table->load_dat(std::string_view(bytes.data(), bytes.size()),
                        &rospinyin::pinyin::syllable_of) == 0) {
#ifdef ROSPINYIN_TRACE
        std::string body = "dat PARSE_FAIL path=" + NarrowPath(path) + " size=";
        AppendUint(body, static_cast<unsigned long>(size));
        TraceDat(body);
#endif
        return nullptr;
    }

#ifdef ROSPINYIN_TRACE
    {
        // 判据一：表规模（宿主端构建 = 44355 字 / 418 音节）。
        // 判据二：shui 下有没有简体「谁」U+8C01 —— 只有补了 alt 索引的新表
        //         才有；老表这里是 0，繁体 U+8AB0 独占。
        std::string body = "dat OK path=" + NarrowPath(path) + " size=";
        AppendUint(body, static_cast<unsigned long>(size));
        body += " chars=";
        AppendUint(body, static_cast<unsigned long>(table->char_count()));
        // 注意口径：syls 是表头声明的音节数（418），而这里打的是
        // by_code_ 的键数，含 alt 建出来的别名键（419），比表头多 1。
        // 两个数都对，混在一起会让人以为数据坏了，所以标签用 codes。
        body += " codes=";
        AppendUint(body, static_cast<unsigned long>(table->code_count()));
        body += " shui=";
        AppendUint(body, static_cast<unsigned long>(table->lookup("shui").size()));
        body += " shui_simp=";
        AppendUint(body, HasCodepoint(*table, "shui", 0x8C01) ? 1u : 0u);
        body += " shui_trad=";
        AppendUint(body, HasCodepoint(*table, "shui", 0x8AB0) ? 1u : 0u);
        TraceDat(body);
    }
#endif
    return table;
}

}  // namespace rospinyin
