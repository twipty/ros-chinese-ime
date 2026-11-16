// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// See dat_loader.hpp. This file is Windows-only (compiled into rospinyin.ime)
// and is never called from DllMain.

#include "dat_loader.hpp"

#include <windows.h>

#include <string>
#include <vector>

namespace rospinyin {
namespace {

// Canonical readings use the digit tone immediately after the vowel and 'v'
// for the u-umlaut (see m2/M2-DATA-PLAN.md section 7).
struct BuiltinSyllable {
    const char* reading;
    const wchar_t* chars;  // BMP Hanzi: wchar_t value == code point
};

constexpr BuiltinSyllable kBuiltin[] = {
    {"de",    L"的得地"},
    {"yi1",   L"一衣医依"},
    {"yi4",   L"意义易亿"},
    {"shi4",  L"是事时世市试"},
    {"shi1",  L"师诗失"},
    {"shi2",  L"十实石时"},
    {"bu4",   L"不步部"},
    {"le",    L"了乐"},
    {"zai4",  L"在再"},
    {"ren2",  L"人仁"},
    {"you3",  L"有友"},
    {"you4",  L"又右幼"},
    {"wo3",   L"我"},
    {"ta1",   L"他她它"},
    {"na4",   L"那"},
    {"na3",   L"哪"},
    {"zhe4",  L"这著"},
    {"zhong1",L"中终钟"},
    {"zhong4",L"重种"},
    {"guo2",  L"国"},
    {"guo4",  L"过"},
    {"ni3",   L"你"},
    {"ni2",   L"尼泥"},
    {"hao3",  L"好"},
    {"hao4",  L"号"},
    {"pin1",  L"拼"},
    {"yin1",  L"音因"},
    {"nv3",   L"女"},
    {"lv4",   L"律绿"},
    {"lv3",   L"旅吕"},
    {"lve4",  L"略"},
    {"nve4",  L"虐"},
    {"kou3",  L"口"},
    {"shou3", L"手首"},
    {"xin1",  L"心新"},
    {"mu4",   L"目木"},
    {"ri4",   L"日"},
    {"yue4",  L"月越"},
    {"shui3", L"水"},
    {"huo3",  L"火"},
    {"shan1", L"山"},
    {"tian1", L"天添"},
    {"da4",   L"大"},
    {"xiao3", L"小晓"},
    {"duo1",  L"多"},
    {"shao3", L"少"},
    {"shang4",L"上"},
    {"xia4",  L"下夏"},
    {"zuo3",  L"左"},
    {"qian2", L"前钱"},
    {"hou4",  L"后厚"},
    {"dong1", L"东冬"},
    {"xi1",   L"西吸希"},
    {"nan2",  L"南男难"},
    {"bei3",  L"北"},
    {"feng1", L"风丰"},
    {"yu3",   L"雨语"},
    {"xue3",  L"雪"},
    {"xue2",  L"学"},
    {"hua1",  L"花"},
    {"cao3",  L"草"},
    {"shu4",  L"树书数"},
    {"lin2",  L"林临"},
    {"he2",   L"和河合"},
    {"hai3",  L"海"},
    {"di4",   L"地第"},
    {"cheng2",L"城成诚"},
    {"gong1", L"工公功"},
    {"zuo4",  L"作坐"},
    {"sheng1",L"生声升"},
    {"lao3",  L"老"},
    {"jiao4", L"教叫"},
    {"wen2",  L"文闻"},
    {"zi4",   L"字自"},
    {"dian4", L"电店"},
    {"nao3",  L"脑"},
    {"kai1",  L"开"},
    {"guan1", L"关"},
    {"kan4",  L"看"},
    {"ting1", L"听厅"},
    {"shuo1", L"说"},
    {"du2",   L"读独"},
    {"xie3",  L"写"},
    {"ji4",   L"记计季"},
    {"si1",   L"思私"},
    {"xiang3",L"想享"},
    {"zhi1",  L"知之支"},
    {"dao4",  L"道到"},
    {"ming2", L"名明"},
    {"xing2", L"行形"},
    {"cai2",  L"才材"},
    {"neng2", L"能"},
    {"hui4",  L"会汇"},
    {"yao4",  L"要药"},
    {"ke3",   L"可渴"},
    {"yi3",   L"以已"},
    {"ji2",   L"及集"},
    {"wei4",  L"为位未"},
    {"chu1",  L"出初"},
    {"ru4",   L"入"},
    {"hui2",  L"回"},
    {"lai2",  L"来"},
    {"qu4",   L"去趣"},
    {"ma1",   L"妈吗"},
    {"ma3",   L"马码"},
    {"ge4",   L"个各"},
    {"ge2",   L"格"},
    {"jin1",  L"今金"},
    {"nian2", L"年"},
};

}  // namespace

void load_builtin_table(PinyinTable& out) {
    for (const auto& item : kBuiltin) {
        if (item.chars == nullptr || item.chars[0] == L'\0') continue;
        std::uint32_t rank = 1000;
        for (const wchar_t* p = item.chars; *p != L'\0'; ++p) {
            // All characters here are BMP Hanzi: the wchar_t value is the
            // Unicode code point.
            out.add(static_cast<char32_t>(*p), rank++, item.reading);
        }
    }
    out.build_index();
}

bool load_system_dat(PinyinTable& out) {
    wchar_t system_dir[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(system_dir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return false;

    const std::wstring path = std::wstring(system_dir) + L"\\rospinyin.dat";
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

    PinyinTable candidate;
    if (candidate.load_dat(std::string_view(bytes.data(), bytes.size())) == 0) {
        return false;
    }
    out = std::move(candidate);
    return true;
}

bool load_ime_table(PinyinTable& out) {
    if (load_system_dat(out)) return true;
    load_builtin_table(out);
    return false;
}

}  // namespace rospinyin
