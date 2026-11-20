// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT

#include <rospinyin/pinyin_session.hpp>

#include <algorithm>
#include <map>

namespace rospinyin {

using imfw::Action;
using imfw::Candidate;
using imfw::Result;
using imfw::dat::kUnknownRank;

namespace {

// Mandarin syllables are at most six letters long (zhuang, chuang, shuang).
constexpr std::size_t kMaxSyllableLength = 6;

bool segment_part(const std::string& text, std::size_t position,
                  const imfw::CodeTable& table, std::vector<std::string>& out,
                  std::map<std::size_t, bool>& memo) {
    if (position == text.size()) return true;
    const auto cached = memo.find(position);
    if (cached != memo.end()) return cached->second;

    const std::size_t longest = std::min(kMaxSyllableLength,
                                         text.size() - position);
    for (std::size_t length = longest; length >= 1; --length) {
        const std::string part = text.substr(position, length);
        if (!table.is_code(part)) continue;

        out.push_back(part);
        if (segment_part(text, position + length, table, out, memo)) {
            memo[position] = true;
            return true;
        }
        out.pop_back();
    }

    memo[position] = false;
    return false;
}

bool is_typed_letter(char32_t cp) {
    return (cp >= U'a' && cp <= U'z') || (cp >= U'A' && cp <= U'Z');
}

// Continuous pinyin input must be accepted: "zhongguo" is [zhong][guo],
// and even the mid-word state "zhongg" is legal ([zhong] + prefix "g").
// Accept when the whole buffer is a prefix, or when some split leaves a
// valid prefix after one or more complete syllables.
//
// Apostrophes are hard syllable boundaries (see segment_syllables()). The
// CodeTable prefix set contains only letters, so a text with a quote can
// never be a prefix there; handle the boundary explicitly instead. The head
// before the quote must form complete syllables and the tail after it must
// remain extendable (empty is legal mid-typing, e.g. right after "xi'").
// Without this branch feed() rejects and drops the separator typed in
// "xi'an", silently degrading it to "xian".
bool can_extend(const std::string& text, const imfw::CodeTable& table) {
    // Empty table (release build with a missing/unreadable .dat): accept
    // every letter so the composition string stays alive and the user can
    // backspace out of it (decision 2026-11-19: show something honest at the
    // caret instead of silently swallowing keys; there are no candidates to
    // choose from, and Esc/Backspace are the exits). One table.is_code()
    // probe, no recursion — the table has no prefixes, so every branch below
    // would return false anyway.
    if (table.char_count() == 0) return true;

    if (table.is_prefix(text)) return true;

    const std::size_t apostrophe = text.find('\'');
    if (apostrophe != std::string::npos) {
        const std::string head = text.substr(0, apostrophe);
        const std::string tail = text.substr(apostrophe + 1);
        if (!head.empty() && !segment_syllables(head, table).empty() &&
            (tail.empty() || can_extend(tail, table))) {
            return true;
        }
    }

    for (std::size_t split = 1; split < text.size(); ++split) {
        if (table.is_prefix(text.substr(split)) &&
            !segment_syllables(text.substr(0, split), table).empty()) {
            return true;
        }
    }
    return false;
}

// The syllables that are *finished* in a buffer that may not be.
//
// One keystroke into a new syllable the buffer cannot be segmented as a whole:
// "nih" is [ni] plus the leading "h" of the next syllable, "zhongg" is [zhong]
// plus "g". can_extend() accepts those states (that is what makes continuous
// typing work), but segment_syllables() rejects them, so recompute() used to
// empty the candidate list and the candidate window blinked off for exactly one
// keystroke at every syllable boundary.
//
// The unfinished tail is deliberately not returned. There is nothing to offer
// for it yet, and leaving it out is what makes CompositionWindow draw it grey
// as a pending syllable (its segment loop treats the text past the last end as
// one more, non-active chunk).
std::vector<std::string> completed_syllables(const std::string& text,
                                             const imfw::CodeTable& table) {
    if (text.empty()) return {};

    // A whole-buffer segmentation is the historical behaviour and stays exact.
    std::vector<std::string> whole = segment_syllables(text, table);
    if (!whole.empty()) return whole;

    // A trailing apostrophe is a boundary the user just typed: everything
    // before it is complete by definition ("xi'" is [xi] plus a syllable that
    // has no letters yet, which is a legal mid-typing state).
    if (text.back() == '\'') {
        return segment_syllables(text.substr(0, text.size() - 1), table);
    }

    // Otherwise: the longest complete head whose tail is still a viable prefix.
    // Longest keeps the most already-typed syllables on screen and matches the
    // split can_extend() finds for the same buffer.
    for (std::size_t split = text.size() - 1; split > 0; --split) {
        if (!table.is_prefix(text.substr(split))) continue;
        std::vector<std::string> head =
            segment_syllables(text.substr(0, split), table);
        if (!head.empty()) return head;
    }
    return {};
}

}  // namespace

std::vector<std::string> segment_syllables(const std::string& letters,
                                           const imfw::CodeTable& table) {
    if (letters.empty()) return {};

    // Apostrophes are hard syllable boundaries.
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t apostrophe = letters.find('\'', start);
        if (apostrophe == std::string::npos) {
            parts.push_back(letters.substr(start));
            break;
        }
        parts.push_back(letters.substr(start, apostrophe - start));
        start = apostrophe + 1;
    }

    std::vector<std::string> result;
    for (const auto& part : parts) {
        if (part.empty()) return {};
        std::vector<std::string> one;
        std::map<std::size_t, bool> memo;
        if (!segment_part(part, 0, table, one, memo)) return {};
        result.insert(result.end(), one.begin(), one.end());
    }
    return result;
}

void PinyinSession::recompute() {
    // Only finished syllables: "nih" keeps [ni] alive instead of clearing the
    // window until the next letter completes a syllable (see
    // completed_syllables()). Commit still erases exactly the first syllable,
    // so the unfinished tail survives a selection.
    syllables_ = completed_syllables(composition_, *table_);
    candidates_.clear();
    if (syllables_.empty()) return;

    // Left to right: the candidate window offers the *first* syllable that has
    // not been converted yet. Typing "zhongguo" therefore shows 中 first; after
    // it is committed the composition shrinks to "guo" and 国 is offered. The
    // alternative (converting the last syllable first) reads right to left and
    // silently throws away everything typed before it.
    const auto entries = table_->lookup(syllables_.front());
    candidates_.reserve(entries.size());
    // Two passes: ranked characters first, then (only if that left nothing to
    // pick) the Unihan-only ones. Rare characters are not dropped from the
    // table -- they are just not offered while a common reading exists.
    std::vector<Candidate> rare;
    for (const auto& entry : entries) {
        Candidate candidate;
        candidate.text.assign(1, static_cast<char32_t>(entry.codepoint));
        // Higher is better. Rank 1 maps to 999999; unknown rank maps to 0.
        candidate.score = entry.rank == kUnknownRank
                              ? 0
                              : static_cast<int>(1000000u - entry.rank);
        // The dat's tier bit is a build-time fact; the engine only translates
        // its polarity (0 = bundled font covers it) into the host's predicate.
        // Rank and tier are independent: a common character may still need an
        // extra font, and a rare one may be perfectly drawable.
        candidate.glyph_visible = (entry.tier == 0);
        if (entry.rank == kUnknownRank && !show_rare_) {
            rare.push_back(std::move(candidate));
        } else {
            candidates_.push_back(std::move(candidate));
        }
    }
    if (candidates_.empty()) candidates_ = std::move(rare);
}

Result PinyinSession::commit(std::size_t index) {
    if (index >= candidates_.size()) return Result{};
    return commit_text(candidates_[index].text);
}

// Commit one syllable's character and keep whatever is left of the composition
// alive. zhongguo -> 中 + "guo" -> 国 gives 中国 in typed order; clearing the
// whole buffer after the first pick would have dropped "zhong" instead.
Result PinyinSession::commit_text(const std::u32string& text) {
    Result result;
    if (text.empty()) return result;
    result.action = Action::Commit;
    result.commit_text = text;

    if (syllables_.empty()) {
        composition_.clear();
    } else {
        const std::size_t taken = syllables_.front().size();
        if (taken >= composition_.size()) {
            composition_.clear();
        } else {
            composition_.erase(0, taken);
            // "xi'an" -> after committing 西 the buffer must not start with the
            // separator, or segmentation rejects the leading quote.
            if (!composition_.empty() && composition_[0] == '\'') {
                composition_.erase(0, 1);
            }
        }
    }

    if (composition_.empty()) {
        candidates_.clear();
        syllables_.clear();
        return result;
    }

    recompute();
    result.composition = composition_;
    result.candidates = candidates_;
    result.segments = syllables_;
    return result;
}

Result PinyinSession::feed(char32_t codepoint) {
    if (!active_) return Result{};

    if (is_typed_letter(codepoint) || codepoint == U'\'') {
        const char typed = static_cast<char>(
            codepoint >= U'A' && codepoint <= U'Z'
                ? codepoint - U'A' + U'a'
                : codepoint);
        composition_.push_back(typed);
        // A partial prefix or a completed-syllable-plus-prefix is legal
        // input; only strings that can never become pinyin are rejected.
        if (!can_extend(composition_, *table_)) {
            composition_.pop_back();
            return Result{Action::Absorb, composition_, candidates_, syllables_, {}};
        }
        recompute();
        return Result{Action::Absorb, composition_, candidates_, syllables_, {}};
    }

    if (codepoint == U' ') {
        return candidates_.empty() ? Result{} : commit(0);
    }

    if (codepoint >= U'1' && codepoint <= U'9') {
        const std::size_t index = static_cast<std::size_t>(codepoint - U'1');
        return index < candidates_.size() ? commit(index) : Result{};
    }

    if (codepoint == 0x1B) {  // Escape
        return cancel();
    }

    return Result{};
}

Result PinyinSession::backspace() {
    if (composition_.empty()) return Result{};
    composition_.pop_back();
    recompute();
    return Result{Action::Absorb, composition_, candidates_, syllables_, {}};
}

Result PinyinSession::cancel() {
    composition_.clear();
    candidates_.clear();
    syllables_.clear();
    return Result{Action::Absorb, {}, {}, {}, {}};
}

void PinyinSession::on_select(bool selected) {
    active_ = selected;
    if (!selected) {
        composition_.clear();
        candidates_.clear();
        syllables_.clear();
    }
}

}  // namespace rospinyin
