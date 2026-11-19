// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// PinyinScheme: RosPinyin's implementation of imfw::Scheme. Windows-free so
// it builds and is unit-tested on Linux together with the session code.
// The built-in fallback table moved here from the old ime/dat_loader.cpp —
// it is pinyin data and therefore belongs to the plugin, not the framework.
//
// ROSPINYIN_BUILTIN_TABLE (decision 2026-11-19): the 109-reading embedded
// table is test/diagnostic infrastructure, not a release safety net — the
// supported install path already guarantees the .dat. Release builds
// compile with the switch OFF, so the symbols below stay out of the binary;
// load_builtin_table() still exists (emptying the table) so host tests that
// reference it compile in both configurations. What a release build does
// when the .dat is missing is pinned by tests/builtin_gate_test.cpp: the
// empty-table contract — letters accumulate, candidates stay empty,
// Backspace/Esc are the exits, nothing crashes.

#include <rospinyin/scheme.hpp>
#include <rospinyin/dat_source.hpp>
#include <rospinyin/pinyin.hpp>
#include <rospinyin/pinyin_session.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace rospinyin {
namespace {

#if defined(ROSPINYIN_BUILTIN_TABLE)
// Canonical readings use the digit tone immediately after the vowel and 'v'
// for the u-umlaut (see docs/milestones/m2/M2-DATA-PLAN.md section 7).
struct BuiltinReading {
    const char* reading;
    const wchar_t* chars;  // BMP Hanzi: wchar_t value == code point
};

constexpr BuiltinReading kBuiltin[] = {
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
#endif  // ROSPINYIN_BUILTIN_TABLE

}  // namespace

namespace {

// The IME class name handed to IMM32 through ImeInquire must fit in
// IMEINFOEX.wszUIClass[16] (15 chars + NUL). imm32!Imm32InquireIme copies it
// with StringCchCopyW into IMEDPI.szUIClass[16] and then calls
// GetClassInfoW() **on the truncated name**: the 17-character
// "RosPinyinUIServer" is cut to "RosPinyinUIServe", the lookup misses,
// Imm32InquireIme fails, imm32 FreeLibrary()s us and stamps fLoadFlag=1 on
// the layout. From outside that looks exactly like "the IME never loads" —
// no error, no trace line, and ImmGetProperty() returns 0. Hence the assert.
constexpr wchar_t kUiServerClass[] = L"RosPinyinUISrv";
static_assert(sizeof(kUiServerClass) / sizeof(wchar_t) - 1 <= 15,
              "UI server class name must fit IMEINFOEX.wszUIClass[16]");

}  // namespace

// ---- identity and resources -------------------------------------------

PinyinScheme::PinyinScheme() : table_(load_pinyin_table()) {}

const wchar_t* PinyinScheme::ui_server_class() const {
    return kUiServerClass;
}

const wchar_t* PinyinScheme::composition_class() const {
    return L"RosPinyinComposition";
}

const wchar_t* PinyinScheme::candidate_class() const {
    return L"RosPinyinCandidate";
}

const wchar_t* PinyinScheme::toolbar_class() const {
    return L"RosPinyinToolbar";
}

const wchar_t* PinyinScheme::toolbar_label() const { return L"拼"; }

const wchar_t* PinyinScheme::toolbar_state_label(bool open) const {
    return open ? L"开" : L"关";
}

// ---- key policy ---------------------------------------------------------

// VK_OEM_7 (0xDE) is the apostrophe key on US layouts: a hard syllable
// separator in pinyin (xi'an).
constexpr std::uint32_t kVkOem7 = 0xDE;

char32_t PinyinScheme::input_char(std::uint32_t vk) const {
    if (vk >= 'A' && vk <= 'Z') return static_cast<char32_t>(vk - 'A' + 'a');
    if (vk >= 'a' && vk <= 'z') return static_cast<char32_t>(vk);
    if (vk == kVkOem7) return U'\'';
    return 0;
}

bool PinyinScheme::is_input_char(char32_t cp) const {
    return (cp >= U'a' && cp <= U'z') || cp == U'\'';
}

// ---- engine -------------------------------------------------------------

std::unique_ptr<imfw::Engine> PinyinScheme::create_engine() const {
    return std::make_unique<PinyinSession>(table_);
}

// ---- data ----------------------------------------------------------------

void add_reading(imfw::CodeTable& table, char32_t codepoint,
                 std::uint32_t rank, const std::string& reading) {
    table.add(codepoint, rank, reading, pinyin::syllable_of(reading));
}

#if defined(ROSPINYIN_BUILTIN_TABLE)
// Real loader: the 109 embedded readings, indexed. Only this configuration
// can answer "what does the engine do with no .dat and no fallback" with
// data; release builds never carry it.
void load_builtin_table(imfw::CodeTable& out) {
    for (const auto& item : kBuiltin) {
        if (item.chars == nullptr || item.chars[0] == L'\0') continue;
        std::uint32_t rank = 1000;
        for (const wchar_t* p = item.chars; *p != L'\0'; ++p) {
            // All characters here are BMP Hanzi: the wchar_t value is the
            // Unicode code point.
            add_reading(out, static_cast<char32_t>(*p), rank++, item.reading);
        }
    }
    out.build_index();
}
#else
// Symbol must survive in both configurations so host tests referencing it
// link either way. With the table compiled out, "loading the built-in
// table" is exactly "an empty table" — which is also what a release build
// without a .dat runs on (empty-table contract,
// tests/builtin_gate_test.cpp).
void load_builtin_table(imfw::CodeTable& out) {
    out.clear();
    out.build_index();
}
#endif  // ROSPINYIN_BUILTIN_TABLE

// The framework calls this before rebuilding sessions (ImeConfigure success
// path): re-read the .dat (a config tool may have replaced it), keeping the
// old table alive for sessions from the previous generation.
void PinyinScheme::reload_data() const {
    table_ = load_pinyin_table();
}

}  // namespace rospinyin
