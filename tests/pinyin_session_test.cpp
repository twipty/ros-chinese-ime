// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// M2 engine skeleton tests: segmentation (longest-match + backtracking),
// v-key input, frequency ordering and commit paths. The table is built in
// memory from a handful of entries; no dat file or network is required.

#include <rospinyin/pinyin_session.hpp>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "pinyin session test failed: %s\n", what);
        ++failures;
    }
}

void check_equal(const std::string& actual, const char* expected,
                 const char* what) {
    if (actual != expected) {
        std::fprintf(stderr,
                     "pinyin session test failed: %s (expected '%s', got '%s')\n",
                     what, expected, actual.c_str());
        ++failures;
    }
}

imfw::CodeTable make_table() {
    imfw::CodeTable table;
    // reading, codepoint, rank (lower = more frequent)
    rospinyin::add_reading(table, U'中', 7, "zho1ng");
    rospinyin::add_reading(table, U'钟', 500, "zho1ng");
    rospinyin::add_reading(table, U'种', 501, "zho1ng");
    rospinyin::add_reading(table, U'国', 8, "guo2");
    rospinyin::add_reading(table, U'的', 1, "de");
    rospinyin::add_reading(table, U'女', 4, "nv3");
    rospinyin::add_reading(table, U'律', 5, "lv4");
    rospinyin::add_reading(table, U'略', 6, "lve4");
    rospinyin::add_reading(table, U'西', 100, "xi1");
    rospinyin::add_reading(table, U'安', 200, "an1");
    rospinyin::add_reading(table, U'先', 50, "xia1n");
    table.build_index();
    return table;
}

std::shared_ptr<const imfw::CodeTable> make_shared_table() {
    return std::make_shared<const imfw::CodeTable>(make_table());
}

void feed_string(rospinyin::PinyinSession& session, const char* text) {
    for (const char* p = text; *p != '\0'; ++p) {
        session.feed(static_cast<char32_t>(static_cast<unsigned char>(*p)));
    }
}

}  // namespace

int main() {
    using namespace rospinyin;
    using namespace imfw;
    const auto table = make_shared_table();
    const imfw::CodeTable& tbl = *table;

    // Segmentation: longest match, backtracking, apostrophe boundary.
    const auto zhongguo = segment_syllables("zhongguo", tbl);
    check(zhongguo.size() == 2, "zhongguo must split into two syllables");
    if (zhongguo.size() == 2) {
        check_equal(zhongguo[0], "zhong", "first syllable of zhongguo");
        check_equal(zhongguo[1], "guo", "second syllable of zhongguo");
    }
    const auto xian = segment_syllables("xian", tbl);
    check(xian.size() == 1 && xian[0] == "xian",
          "xian must prefer the longer syllable");
    const auto xian_split = segment_syllables("xi'an", tbl);
    check(xian_split.size() == 2, "xi'an must split into two syllables");
    check(segment_syllables("zh", tbl).empty(),
          "incomplete pinyin must not segment");
    check(segment_syllables("", tbl).empty(),
          "empty composition must not segment");

    PinyinSession session(table);
    check(session.feed(U'a').action == Action::Passthrough,
          "inactive session must passthrough");

    session.on_select(true);
    check(session.feed(U'a').action == Action::Absorb,
          "active session must absorb pinyin letters");

    // 'v' key maps to the u-umlaut data: nv -> nǚ (女).
    session.cancel();
    feed_string(session, "nv");
    check_equal(session.composition(), "nv", "v-key composition");
    check(!session.candidates().empty(), "nv must produce candidates");
    if (!session.candidates().empty()) {
        check(session.candidates()[0].text == U"女",
              "nv first candidate must be 女");
    }
    const Result committed = session.feed(U' ');
    check(committed.action == Action::Commit, "space must commit");
    check(committed.commit_text == U"女", "space must commit 女");
    check(session.composition().empty(), "composition cleared after commit");

    // lv / lve also work through the v-key convention.
    session.cancel();
    feed_string(session, "lv");
    check(!session.candidates().empty() && session.candidates()[0].text == U"律",
          "lv first candidate must be 律");
    session.cancel();
    feed_string(session, "lve");
    check(!session.candidates().empty() && session.candidates()[0].text == U"略",
          "lve first candidate must be 略");

    // Digit selection and frequency ordering (中 < 钟 < 种 by rank).
    session.cancel();
    feed_string(session, "zhong");
    check(session.candidates().size() == 3, "zhong has three candidates");
    if (session.candidates().size() == 3) {
        check(session.candidates()[0].text == U"中", "rank 1 = 中");
        check(session.candidates()[1].text == U"钟", "rank 2 = 钟");
        check(session.candidates()[2].text == U"种", "rank 3 = 种");
    }
    const Result second = session.feed(U'2');
    check(second.action == Action::Commit && second.commit_text == U"钟",
          "digit 2 commits the second candidate");

    // Backspace and Esc.
    session.cancel();
    feed_string(session, "zhong");
    const Result erased = session.backspace();
    check(erased.action == Action::Absorb, "backspace must absorb");
    check_equal(session.composition(), "zhon", "backspace removes one letter");
    const Result escaped = session.feed(0x1B);
    check(escaped.action == Action::Absorb && session.composition().empty(),
          "Esc must cancel the composition");

    // Continuous typing across a syllable boundary: zhongguo -> 中国.
    session.cancel();
    feed_string(session, "zhongguo");
    check_equal(session.composition(), "zhongguo", "continuous input kept");
    check(session.syllables().size() == 2,
          "continuous input splits into two syllables");
    check(!session.candidates().empty() && session.candidates()[0].text == U"国",
          "last syllable guo yields 国 first");

    // Deselect resets state (IMM32 can select repeatedly).
    session.cancel();
    feed_string(session, "guo");
    session.on_select(false);
    check(session.composition().empty(), "deselect must clear composition");
    check(session.feed(U'g').action == Action::Passthrough,
          "deselected session must passthrough again");

    if (failures != 0) {
        std::fprintf(stderr, "%d pinyin session test(s) failed\n", failures);
        return 1;
    }
    std::printf("RosPinyin pinyin session OK (segmentation + v-key + commit)\n");
    return 0;
}
