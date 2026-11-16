// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT

#include <imfw/code_table.hpp>

#include <algorithm>
#include <cstring>

namespace imfw {
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

// Generic reading sanity check for every input method: readings/keys are
// lowercase ASCII letters, digits and the apostrophe. Pinyin readings carry
// tone digits ("zho1ng"), shape codes are plain letters ("wqvb"); anything
// else means the dat was compiled by a different tool and must be rejected
// rather than half-loaded.
bool is_dat_reading(std::string_view s) {
    if (s.empty()) return false;
    for (const char c : s) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                         c == '\'';
        if (!ok) return false;
    }
    return true;
}

}  // namespace

std::size_t CodeTable::load_dat(std::string_view bytes) {
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

    // Key names come from the dat's own key table; no pinyin knowledge here.
    std::vector<std::string> key_names(header.syl_count);
    for (std::uint32_t i = 0; i < header.syl_count; ++i) {
        dat::SylEntry record{};
        const std::size_t offset = header.syl_table_off +
            static_cast<std::size_t>(i) * sizeof(dat::SylEntry);
        if (!read_record(bytes, offset, record)) return 0;
        if (!pool_string(bytes, header.pool_off, header.pool_size,
                         record.name_off, key_names[i])) {
            return 0;
        }
        if (!is_dat_reading(key_names[i])) return 0;
    }

    clear();
    for (std::uint32_t i = 0; i < header.char_count; ++i) {
        dat::CharEntry record{};
        const std::size_t offset = header.char_table_off +
            static_cast<std::size_t>(i) * sizeof(dat::CharEntry);
        if (!read_record(bytes, offset, record)) {
            clear();
            return 0;
        }
        if (record.syl_id >= key_names.size()) {
            clear();
            return 0;
        }
        std::string reading;
        if (!pool_string(bytes, header.pool_off, header.pool_size,
                         record.reading_off, reading)) {
            clear();
            return 0;
        }
        if (!is_dat_reading(reading)) {
            clear();
            return 0;
        }
        Entry entry;
        entry.codepoint = static_cast<char32_t>(record.codepoint);
        entry.rank = record.rank;
        entry.reading = std::move(reading);
        entry.key = key_names[record.syl_id];
        entries_.push_back(std::move(entry));
    }

    build_index();
    return entries_.size();
}

void CodeTable::add(char32_t codepoint, std::uint32_t rank,
                    std::string reading, std::string key) {
    Entry entry;
    entry.codepoint = codepoint;
    entry.rank = rank;
    entry.reading = std::move(reading);
    entry.key = std::move(key);
    entries_.push_back(std::move(entry));
}

void CodeTable::clear() {
    entries_.clear();
    by_code_.clear();
    codes_.clear();
    prefixes_.clear();
}

void CodeTable::build_index() {
    by_code_.clear();
    codes_.clear();
    prefixes_.clear();

    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const std::string& code = entries_[i].key;
        by_code_[code].push_back(i);
        codes_.insert(code);
        for (std::size_t length = 1; length <= code.size(); ++length) {
            prefixes_.insert(code.substr(0, length));
        }
    }

    for (auto& item : by_code_) {
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

std::vector<TableEntry> CodeTable::lookup(std::string_view key) const {
    std::vector<TableEntry> result;
    const auto it = by_code_.find(std::string(key));
    if (it == by_code_.end()) return result;
    result.reserve(it->second.size());
    for (const std::size_t index : it->second) {
        const Entry& entry = entries_[index];
        result.push_back(TableEntry{entry.codepoint, entry.rank, entry.reading});
    }
    return result;
}

bool CodeTable::is_code(std::string_view key) const {
    return codes_.count(std::string(key)) != 0;
}

bool CodeTable::is_prefix(std::string_view prefix) const {
    return prefixes_.count(std::string(prefix)) != 0;
}

}  // namespace imfw
