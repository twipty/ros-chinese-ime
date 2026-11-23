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

    // Continuous typing across a syllable boundary: zhongguo -> 中国, converted
    // left to right. The candidate list belongs to the syllable at the front of
    // the buffer; committing it leaves the rest to convert next.
    session.cancel();
    feed_string(session, "zhongguo");
    check_equal(session.composition(), "zhongguo", "continuous input kept");
    check(session.syllables().size() == 2,
          "continuous input splits into two syllables");
    check(!session.candidates().empty() && session.candidates()[0].text == U"中",
          "first syllable zhong yields 中 first");
    const Result first_char = session.feed(U' ');
    check(first_char.commit_text == U"中", "space commits 中, the first syllable");
    check_equal(session.composition(), "guo",
                "the unconverted tail stays in the composition");
    check(!session.candidates().empty() && session.candidates()[0].text == U"国",
          "the remaining syllable guo yields 国 next");
    const Result second_char = session.feed(U' ');
    check(second_char.commit_text == U"国", "space commits 国 next");
    check(session.composition().empty(),
          "composition is empty once every syllable is committed");

    // Deselect resets state (IMM32 can select repeatedly).
    session.cancel();
    feed_string(session, "guo");
    session.on_select(false);
    check(session.composition().empty(), "deselect must clear composition");
    check(session.feed(U'g').action == Action::Passthrough,
          "deselected session must passthrough again");
    session.on_select(true);

    // Uppercase virtual keys are normalized by feed() itself, not only by
    // the Scheme's input_char(): ZHONG composes exactly like lowercase.
    session.cancel();
    session.feed(U'Z'); session.feed(U'H'); session.feed(U'O');
    session.feed(U'N'); session.feed(U'G');
    check_equal(session.composition(), "zhong", "uppercase input normalized");
    check(!session.candidates().empty() && session.candidates()[0].text == U"中",
          "uppercase input still yields 中");

    // Apostrophe is a hard boundary through the interactive feed() path too,
    // not only segment_syllables(): the separator must be kept and the
    // candidates must come from the syllable after it.
    session.cancel();
    feed_string(session, "xi'an");
    check_equal(session.composition(), "xi'an", "typed apostrophe is kept");
    check(session.syllables().size() == 2,
          "xi'an splits into two typed syllables");
    if (session.syllables().size() == 2) {
        check_equal(session.syllables()[0], "xi", "xi'an first syllable");
        check_equal(session.syllables()[1], "an", "xi'an second syllable");
    }
    check(!session.candidates().empty() && session.candidates()[0].text == U"西",
          "xi'an candidates come from the first syllable xi");
    const Result xi_commit = session.commit_text(U"西");
    check_equal(session.composition(), "an",
                "committing 西 drops the separator with the syllable");

    // A separator may end the buffer mid-typing ("xi'" waits for the next
    // syllable), but the tail after it must still be a legal pinyin prefix.
    //
    // Typing the separator must not empty the list: everything before it is
    // finished, so 西 stays on offer while the next syllable has no letters yet.
    // Before the incomplete-tail fix the whole buffer failed to segment here and
    // the candidate window blinked off for one keystroke.
    session.cancel();
    feed_string(session, "xi'");
    check_equal(session.composition(), "xi'", "trailing separator is accepted");
    check(session.syllables().size() == 1 && session.syllables()[0] == "xi",
          "trailing separator keeps the finished syllable");
    check(!session.candidates().empty() && session.candidates()[0].text == U"西",
          "trailing separator keeps xi's candidates (no blink)");
    session.cancel();
    feed_string(session, "xi'b");
    check_equal(session.composition(), "xi'",
                "illegal syllable after separator is rejected, separator kept");

    // The candidate list must never go empty once a syllable is complete: a
    // half-typed *next* syllable is a normal state ("zhongg" = [zhong] + "g"),
    // not a reason to withdraw the offer for the finished one. This is the
    // "候选窗一闪一闪" report expressed as a predicate: after the first
    // candidate appears, no further letter may empty the list.
    {
        rospinyin::PinyinSession typing(make_shared_table());
        typing.on_select(true);
        bool seen = false;
        bool blinked = false;
        const char* sequence = "zhongguo";
        for (const char* p = sequence; *p != '\0'; ++p) {
            typing.feed(static_cast<char32_t>(static_cast<unsigned char>(*p)));
            if (!typing.candidates().empty()) {
                seen = true;
            } else if (seen) {
                blinked = true;
            }
        }
        check(seen, "zhongguo reaches a state with candidates");
        check(!blinked, "no candidate gap while typing zhongguo");
        check(typing.syllables().size() == 2,
              "a finished buffer still reports both syllables");
    }

    // ... and the unfinished tail is not reported as a segment, so the
    // composition window can draw it grey instead of underlining a syllable
    // that has no characters to choose from yet.
    {
        rospinyin::PinyinSession typing(make_shared_table());
        typing.on_select(true);
        feed_string(typing, "zhongg");
        check(typing.syllables().size() == 1,
              "an unfinished syllable is not a segment");
        check(!typing.candidates().empty() &&
                  typing.candidates()[0].text == U"中",
              "zhong's candidates survive the half-typed g");
        // Committing the finished syllable erases only it; "g" is still there.
        const Result picked = typing.commit_text(U"中");
        check_equal(picked.composition, "g",
                    "the uncommitted tail survives the selection");
        check(picked.candidates.empty(),
              "a lone g has nothing to offer yet");
    }

    // Invalid input is rejected without corrupting a valid in-flight prefix.
    session.cancel();
    feed_string(session, "zhon");
    const Result rejected = session.feed(U'b');
    check(rejected.action == Action::Absorb &&
              rejected.commit_text.empty(),
          "non-pinyin letter is absorbed, never committed");
    check_equal(session.composition(), "zhon",
                "rejected letter leaves the valid prefix intact");

    // No-op paths on an empty composition must pass through, not absorb.
    session.cancel();
    check(session.backspace().action == Action::Passthrough,
          "backspace on empty composition passes through");
    check(session.feed(U' ').action == Action::Passthrough,
          "space with no candidates passes through");
    check(session.feed(U'1').action == Action::Passthrough,
          "digit with no candidates passes through");
    check(session.feed(0x1B).action == Action::Absorb,
          "Esc still absorbs even when composition is empty");

    // ---- unranked characters stay in the table, in table order ------------
    {
        imfw::CodeTable table;
        rospinyin::add_reading(table, U'水', 202, "shui3");
        rospinyin::add_reading(table, U'谁', 648, "shui2");
        // Unihan-only character: present in the table, no frequency rank.
        // There is no rare filter any more: it is offered in table order,
        // after the ranked ones. In the shipped .dat mkdat gives tier-0
        // unranked characters an explicit low weight instead.
        rospinyin::add_reading(table, U'\u3968', imfw::dat::kUnknownRank, "shui4");
        table.build_index();
        auto shared = std::make_shared<const imfw::CodeTable>(std::move(table));

        rospinyin::PinyinSession session(shared);
        session.on_select(true);
        feed_string(session, "shui");
        check(session.candidates().size() == 3,
              "unranked characters are offered, not silently dropped");
        if (session.candidates().size() == 3) {
            check(session.candidates()[0].text == std::u32string(1, U'水'),
                  "ranked candidates keep frequency order");
            check(session.candidates()[1].text == std::u32string(1, U'谁'),
                  "谁 is offered for shui");
            check(session.candidates()[2].text == std::u32string(1, U'\u3968'),
                  "the unranked character comes last, still selectable");
        }
    }

    // Tier -> glyph_visible: the engine only translates the dat bit's
    // polarity. Rank is an ordering axis, never an admission gate: every
    // entry is returned and [快]/[全] filtering is the host's job alone.
    {
        imfw::CodeTable table;
        table.add(U'中', 7, "zho1ng", "zhong", 0);
        table.add(U'钟', 500, "zho1ng", "zhong", 1);
        table.add(U'\u3968', imfw::dat::kUnknownRank, "zho1ng", "zhong", 1);
        table.build_index();
        auto shared = std::make_shared<const imfw::CodeTable>(std::move(table));

        rospinyin::PinyinSession session(shared);
        session.on_select(true);
        feed_string(session, "zhong");
        check(session.candidates().size() == 3,
              "the engine returns every entry regardless of rank or tier");
        if (session.candidates().size() == 3) {
            check(session.candidates()[0].text == U"中" &&
                      session.candidates()[0].glyph_visible,
                  "tier 0 becomes glyph_visible=true");
            check(session.candidates()[1].text == U"钟" &&
                      !session.candidates()[1].glyph_visible,
                  "tier 1 becomes glyph_visible=false");
            check(session.candidates()[2].text ==
                          std::u32string(1, U'\u3968') &&
                      !session.candidates()[2].glyph_visible,
                  "an unranked tier-1 entry keeps its visibility bit");
        }
    }

    // A syllable with only unranked characters must still be typeable.
    {
        imfw::CodeTable table;
        rospinyin::add_reading(table, U'\u3968', imfw::dat::kUnknownRank, "guo2");
        table.build_index();
        auto shared = std::make_shared<const imfw::CodeTable>(std::move(table));

        rospinyin::PinyinSession session(shared);
        session.on_select(true);
        feed_string(session, "guo");
        check(session.candidates().size() == 1,
              "an unranked-only syllable still yields its candidate");
    }

    if (failures != 0) {
        std::fprintf(stderr, "%d pinyin session test(s) failed\n", failures);
        return 1;
    }
    std::printf("RosPinyin pinyin session OK (segmentation + v-key + commit)\n");
    return 0;
}
