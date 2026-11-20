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

// The word list is not indexed yet (that is M4), but a file that declares one
// must still hold a coherent one: the key runs tile the word table exactly,
// every key is a legal reading string, and every word has text, at least two
// syllables and no reserved bits. Checking here means a corrupt lexicon fails
// at load time instead of at the first lookup once M4 lands -- and it keeps
// "declared but empty" from passing as "no word list".
bool validate_lexicon(std::string_view bytes, const dat::Header& header) {
    if (header.lex_entry_count == 0) return header.lex_code_count == 0;
    // Words with no key can never be reached: that is a builder bug, not data.
    if (header.lex_code_count == 0) return false;

    std::uint32_t index = 0;
    for (std::uint32_t i = 0; i < header.lex_code_count; ++i) {
        dat::LexCode code{};
        const std::size_t code_offset = header.lex_code_table_off +
            static_cast<std::size_t>(i) * sizeof(dat::LexCode);
        if (!read_record(bytes, code_offset, code)) return false;
        if (code.flags != 0) return false;
        if (code.first_entry != index) return false;

        std::string key;
        if (!pool_string(bytes, header.pool_off, header.pool_size,
                         code.key_off, key)) {
            return false;
        }
        if (!is_dat_reading(key)) return false;

        if (code.entry_count == 0) return false;
        if (static_cast<std::uint64_t>(index) + code.entry_count >
            header.lex_entry_count) {
            return false;
        }
        for (std::uint32_t j = 0; j < code.entry_count; ++j, ++index) {
            dat::LexEntry word{};
            const std::size_t word_offset = header.lex_entry_table_off +
                static_cast<std::size_t>(index) * sizeof(dat::LexEntry);
            if (!read_record(bytes, word_offset, word)) return false;
            // Base magic: bit0 (composition-only, a shape concept) is not legal.
            if ((word.flags & ~dat::kBaseFlagsMask) != 0) return false;
            if (word.alt_off != 0) return false;
            if (word.rank == 0) return false;
            if (word.syl_count < 2) return false;

            std::string text;
            if (!pool_string(bytes, header.pool_off, header.pool_size,
                             word.text_off, text)) {
                return false;
            }
            if (text.empty()) return false;

            if (word.reading_off != 0) {
                std::string reading;
                if (!pool_string(bytes, header.pool_off, header.pool_size,
                                 word.reading_off, reading)) {
                    return false;
                }
                std::uint32_t parts = 1;
                std::size_t begin = 0;
                bool ok = true;
                while (begin <= reading.size()) {
                    std::size_t end = reading.find(' ', begin);
                    if (end == std::string::npos) end = reading.size();
                    if (!is_dat_reading(
                            std::string_view(reading.data() + begin, end - begin))) {
                        ok = false;
                        break;
                    }
                    if (end == reading.size()) break;
                    ++parts;
                    begin = end + 1;
                }
                if (!ok || parts != word.syl_count) return false;
            }
        }
    }
    // Every declared word must be reachable through some key.
    return index == header.lex_entry_count;
}

}  // namespace

std::size_t CodeTable::load_dat(std::string_view bytes,
                                KeyOfReading key_of_reading) {
    dat::Header header{};
    if (!read_record(bytes, 0, header)) return 0;
    if (std::memcmp(header.magic, dat::kMagic, sizeof(dat::kMagic)) != 0) {
        return 0;
    }
    if (header.version != dat::kVersion || header.flags != 0) return 0;

    // A version 1 table (52-byte header, no tier bit, no word list) is not
    // readable here on purpose: see the version note in dat_format.hpp.
    if (header.entry_size != sizeof(dat::CharEntry) ||
        header.code_size != sizeof(dat::SylEntry)) {
        return 0;
    }

    // Shape-only header fields: pinyin writes zero and a non-zero value means
    // this is somebody else's table (or a mispacked one), not a pinyin one.
    if (header.name_off != 0 || header.alphabet_off != 0 ||
        header.source_off != 0 || header.max_code_len != 0 ||
        header.auto_commit != 0 || header.reserved[0] != 0 ||
        header.reserved[1] != 0) {
        return 0;
    }

    // The four tables butt against each other with no gaps and no overlap.
    const std::uint64_t entry_end =
        static_cast<std::uint64_t>(header.entry_table_off) +
        static_cast<std::uint64_t>(header.entry_count) * header.entry_size;
    const std::uint64_t code_end =
        static_cast<std::uint64_t>(header.code_table_off) +
        static_cast<std::uint64_t>(header.code_count) * header.code_size;
    const std::uint64_t lex_entry_end =
        static_cast<std::uint64_t>(header.lex_entry_table_off) +
        static_cast<std::uint64_t>(header.lex_entry_count) * header.entry_size;
    const std::uint64_t lex_code_end =
        static_cast<std::uint64_t>(header.lex_code_table_off) +
        static_cast<std::uint64_t>(header.lex_code_count) * header.code_size;
    const std::uint64_t pool_end =
        static_cast<std::uint64_t>(header.pool_off) + header.pool_size;

    if (header.entry_table_off != sizeof(dat::Header)) return 0;
    if (header.code_table_off != entry_end) return 0;
    if (header.lex_entry_table_off != code_end) return 0;
    if (header.lex_code_table_off != lex_entry_end) return 0;
    if (header.pool_off != lex_code_end) return 0;
    if (pool_end > bytes.size()) return 0;

    // Key names come from the dat's own key table; no pinyin knowledge here.
    // The character segments must tile [0, entry_count) in syllable order:
    // mkdat writes them as a prefix sum, so any other arrangement means the
    // file was not produced by this toolchain.
    std::vector<std::string> key_names(header.code_count);
    std::uint32_t next_entry = 0;
    for (std::uint32_t i = 0; i < header.code_count; ++i) {
        dat::SylEntry record{};
        const std::size_t offset = header.code_table_off +
            static_cast<std::size_t>(i) * sizeof(dat::SylEntry);
        if (!read_record(bytes, offset, record)) return 0;
        if (record.flags != 0) return 0;
        if (record.first_entry != next_entry) return 0;
        next_entry += record.entry_count;
        if (next_entry > header.entry_count) return 0;
        if (!pool_string(bytes, header.pool_off, header.pool_size,
                         record.name_off, key_names[i])) {
            return 0;
        }
        if (!is_dat_reading(key_names[i])) return 0;
    }
    if (next_entry != header.entry_count) return 0;

    if (!validate_lexicon(bytes, header)) return 0;

    clear();
    for (std::uint32_t i = 0; i < header.entry_count; ++i) {
        dat::CharEntry record{};
        const std::size_t offset = header.entry_table_off +
            static_cast<std::size_t>(i) * sizeof(dat::CharEntry);
        if (!read_record(bytes, offset, record)) {
            clear();
            return 0;
        }
        if (record.syl_id >= key_names.size()) {
            clear();
            return 0;
        }
        // rank is either a 1-based order or the "no order" sentinel; zero is
        // neither, and a reserved flag bit is a format we do not know.
        // The base loader rejects bit0 (composition-only is a shape concept).
        if (record.rank == 0) {
            clear();
            return 0;
        }
        if ((record.flags & ~dat::kBaseFlagsMask) != 0) {
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
        // Preserve the tier bit (0 = bundled font, 1 = extra font needed).
        // Filtering is the engine's job, not the parser's; dropping the bit
        // here would leave even a graded dat indistinguishable from an
        // ungraded one and make "visible candidate" unenforceable.
        entry.tier = (record.flags & dat::kFlagTier) ? 1u : 0u;

        if (key_of_reading != nullptr && record.alt_off != 0) {
            std::string alts;
            if (!pool_string(bytes, header.pool_off, header.pool_size,
                             record.alt_off, alts)) {
                clear();
                return 0;
            }
            std::size_t begin = 0;
            while (begin < alts.size()) {
                std::size_t end = alts.find(' ', begin);
                if (end == std::string::npos) end = alts.size();
                const std::string_view piece(alts.data() + begin, end - begin);
                begin = end + 1;
                if (!is_dat_reading(piece)) {
                    clear();
                    return 0;
                }
                std::string alt_key = key_of_reading(piece);
                if (alt_key.empty() || alt_key == entry.key) continue;
                bool seen = false;
                for (const auto& known : entry.alt_keys) {
                    if (known == alt_key) {
                        seen = true;
                        break;
                    }
                }
                if (!seen) entry.alt_keys.push_back(std::move(alt_key));
            }
        }

        entries_.push_back(std::move(entry));
    }

    build_index();
    return entries_.size();
}

void CodeTable::add(char32_t codepoint, std::uint32_t rank,
                    std::string reading, std::string key, std::uint8_t tier) {
    Entry entry;
    entry.codepoint = codepoint;
    entry.rank = rank;
    entry.reading = std::move(reading);
    entry.key = std::move(key);
    entry.tier = tier ? 1u : 0u;
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
        // Polyphonic readings answer to their own key as well. The character
        // still appears once per lookup, because each bucket holds its index.
        for (const auto& alt : entries_[i].alt_keys) {
            by_code_[alt].push_back(i);
            codes_.insert(alt);
            for (std::size_t length = 1; length <= alt.size(); ++length) {
                prefixes_.insert(alt.substr(0, length));
            }
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
        result.push_back(TableEntry{entry.codepoint, entry.rank, entry.reading,
                                    entry.tier});
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
