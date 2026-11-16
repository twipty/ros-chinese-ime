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
bool can_extend(const std::string& text, const imfw::CodeTable& table) {
    if (table.is_prefix(text)) return true;
    for (std::size_t split = 1; split < text.size(); ++split) {
        if (table.is_prefix(text.substr(split)) &&
            !segment_syllables(text.substr(0, split), table).empty()) {
            return true;
        }
    }
    return false;
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
    syllables_ = segment_syllables(composition_, table_);
    candidates_.clear();
    if (syllables_.empty()) return;

    const auto entries = table_.lookup(syllables_.back());
    candidates_.reserve(entries.size());
    for (const auto& entry : entries) {
        Candidate candidate;
        candidate.text.assign(1, static_cast<char32_t>(entry.codepoint));
        // Higher is better. Rank 1 maps to 999999; unknown rank maps to 0.
        candidate.score = entry.rank == kUnknownRank
                              ? 0
                              : static_cast<int>(1000000u - entry.rank);
        candidates_.push_back(std::move(candidate));
    }
}

Result PinyinSession::commit(std::size_t index) {
    Result result;
    if (index >= candidates_.size()) return result;
    result.action = Action::Commit;
    result.commit_text = candidates_[index].text;
    composition_.clear();
    candidates_.clear();
    syllables_.clear();
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
        if (!can_extend(composition_, table_)) {
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
