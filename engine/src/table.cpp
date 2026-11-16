// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT

#include <rospinyin/table.hpp>
#include <rospinyin/pinyin.hpp>

#include <algorithm>
#include <cstring>

namespace rospinyin {
namespace {

// Read a NUL-terminated string from the dat string pool with bounds checks.
bool pool_string(std::string_view blob, std::size_t pool_off,
                 std::size_t pool_size, std::uint32_t offset,
                 std::string& out) {
    if (offset >= pool_size) return false;
    const std::size_t begin = pool_off + offset;
    if (begin >= blob.size()) return false;
    const std::size_t limit = pool_off + pool_size;
    const std::size_t end = blob.find('\0', begin);
    if (end == std::string_view::npos || end >= limit) return false;
    out.assign(blob.data() + begin, end - begin);
    return true;
}

template <typename T>
bool read_record(std::string_view blob, std::size_t offset, T& out) {
    if (offset > blob.size() || sizeof(T) > blob.size() - offset) return false;
    std::memcpy(&out, blob.data() + offset, sizeof(T));
    return true;
}

}  // namespace

std::size_t PinyinTable::load_dat(std::string_view bytes) {
    dat::Header header{};
    if (!read_record(bytes, 0, header)) return 0;
    if (std::memcmp(header.magic, dat::kMagic, sizeof(dat::kMagic)) != 0) {
        return 0;
    }
    if (header.version != dat::kVersion || header.flags != 0) return 0;

    const std::uint64_t char_end = static_cast<std::uint64_t>(header.char_table_off) +
        static_cast<std::uint64_t>(header.char_count) * sizeof(dat::CharEntry);
    const std::uint64_t syl_end = static_cast<std::uint64_t>(header.syl_table_off) +
        static_cast<std::uint64_t>(header.syl_count) * sizeof(dat::SylEntry);
    const std::uint64_t pool_end = static_cast<std::uint64_t>(header.pool_off) +
        header.pool_size;

    if (header.char_table_off != sizeof(dat::Header)) return 0;
    if (header.syl_table_off != char_end) return 0;
    if (header.pool_off != syl_end) return 0;
    if (pool_end > bytes.size()) return 0;

    clear();
    for (std::uint32_t i = 0; i < header.char_count; ++i) {
        dat::CharEntry record{};
        const std::size_t offset = header.char_table_off +
            static_cast<std::size_t>(i) * sizeof(dat::CharEntry);
        if (!read_record(bytes, offset, record)) {
            clear();
            return 0;
        }
        std::string reading;
        if (!pool_string(bytes, header.pool_off, header.pool_size,
                         record.reading_off, reading)) {
            clear();
            return 0;
        }
        if (!pinyin::is_canonical(reading)) {
            clear();
            return 0;
        }
        entries_.push_back(TableEntry{
            static_cast<char32_t>(record.codepoint),
            record.rank,
            reading,
            pinyin::tone_of(reading),
        });
    }

    build_index();
    return entries_.size();
}

void PinyinTable::add(char32_t codepoint, std::uint32_t rank,
                      std::string reading) {
    TableEntry entry;
    entry.codepoint = codepoint;
    entry.rank = rank;
    entry.reading = std::move(reading);
    entry.tone = pinyin::tone_of(entry.reading);
    entries_.push_back(std::move(entry));
}

void PinyinTable::clear() {
    entries_.clear();
    by_syllable_.clear();
    syllables_.clear();
    prefixes_.clear();
}

void PinyinTable::build_index() {
    by_syllable_.clear();
    syllables_.clear();
    prefixes_.clear();

    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const std::string syllable =
            pinyin::syllable_of(entries_[i].reading);
        by_syllable_[syllable].push_back(i);
        syllables_.insert(syllable);
        for (std::size_t length = 1; length <= syllable.size(); ++length) {
            prefixes_.insert(syllable.substr(0, length));
        }
    }

    for (auto& item : by_syllable_) {
        auto& indices = item.second;
        std::sort(indices.begin(), indices.end(),
                  [this](std::size_t a, std::size_t b) {
                      const std::uint32_t rank_a = entries_[a].rank;
                      const std::uint32_t rank_b = entries_[b].rank;
                      if (rank_a != rank_b) return rank_a < rank_b;
                      return entries_[a].codepoint < entries_[b].codepoint;
                  });
    }
}

std::vector<TableEntry> PinyinTable::lookup(std::string_view syllable) const {
    std::vector<TableEntry> result;
    const auto it = by_syllable_.find(std::string(syllable));
    if (it == by_syllable_.end()) return result;
    result.reserve(it->second.size());
    for (const std::size_t index : it->second) {
        result.push_back(entries_[index]);
    }
    return result;
}

bool PinyinTable::is_syllable(std::string_view syllable) const {
    return syllables_.count(std::string(syllable)) != 0;
}

bool PinyinTable::is_prefix(std::string_view prefix) const {
    return prefixes_.count(std::string(prefix)) != 0;
}

}  // namespace rospinyin
