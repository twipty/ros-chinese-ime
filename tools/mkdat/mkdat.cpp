// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// mkdat: compile Unihan kMandarin / kHanyuPinlu + hanziDB.csv into the
// compact, mmap-friendly rospinyin.dat described in dat_format.hpp.
//
// Usage:
//   mkdat --unihan <Unihan_Readings.txt> --frequency <hanziDB.csv>
//         --out <rospinyin.dat> [--stats] [--quiet]
//
// Both input files are fetched reproducibly by fetch_data.py (fixed URLs +
// sha256). mkdat itself never touches the network.

#include <rospinyin/pinyin.hpp>
#include <imfw/dat_format.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using imfw::dat::CharEntry;
using imfw::dat::Header;
using imfw::dat::SylEntry;
using imfw::dat::kMagic;
using imfw::dat::kUnknownRank;
using imfw::dat::kVersion;

struct UniReading {
    std::vector<std::string> readings;  // canonicalized kMandarin readings
    std::string pinlu_primary;          // highest-frequency kHanyuPinlu reading
    std::uint32_t pinlu_count = 0;
};

struct FreqEntry {
    std::uint32_t rank = kUnknownRank;
    std::string pinyin;  // canonicalized hanziDB pinyin
};

struct Options {
    std::string unihan_path;
    std::string frequency_path;
    std::string out_path;
    bool stats = false;
    bool quiet = false;
};

int usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s --unihan <Unihan_Readings.txt> "
                 "--frequency <hanziDB.csv> --out <rospinyin.dat> "
                 "[--stats] [--quiet]\n",
                 argv0);
    return 2;
}

bool parse_args(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&](std::string& target) -> bool {
            if (i + 1 >= argc) return false;
            target = argv[++i];
            return true;
        };
        if (arg == "--unihan") {
            if (!value(out.unihan_path)) return false;
        } else if (arg == "--frequency") {
            if (!value(out.frequency_path)) return false;
        } else if (arg == "--out") {
            if (!value(out.out_path)) return false;
        } else if (arg == "--stats") {
            out.stats = true;
        } else if (arg == "--quiet") {
            out.quiet = true;
        } else {
            return false;
        }
    }
    return !out.unihan_path.empty() && !out.frequency_path.empty() &&
           !out.out_path.empty();
}

std::string read_whole_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::fprintf(stderr, "mkdat: cannot open %s\n", path.c_str());
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

std::uint32_t codepoint_from_uplus(const std::string& token) {
    if (token.size() < 3 || token[0] != 'U' || token[1] != '+') {
        return 0;
    }
    try {
        const unsigned long value = std::stoul(token.substr(2), nullptr, 16);
        return value <= 0x10FFFF ? static_cast<std::uint32_t>(value) : 0;
    } catch (...) {
        return 0;
    }
}

std::uint32_t first_codepoint(const std::string& utf8) {
    if (utf8.empty()) return 0;
    const auto b0 = static_cast<unsigned char>(utf8[0]);
    if (b0 < 0x80) return b0;

    std::size_t length = 0;
    std::uint32_t value = 0;
    if ((b0 & 0xE0) == 0xC0) {
        length = 2;
        value = b0 & 0x1Fu;
    } else if ((b0 & 0xF0) == 0xE0) {
        length = 3;
        value = b0 & 0x0Fu;
    } else if ((b0 & 0xF8) == 0xF0) {
        length = 4;
        value = b0 & 0x07u;
    } else {
        return 0;
    }
    if (utf8.size() < length) return 0;
    for (std::size_t i = 1; i < length; ++i) {
        const auto b = static_cast<unsigned char>(utf8[i]);
        if ((b & 0xC0) != 0x80) return 0;
        value = (value << 6) | (b & 0x3Fu);
    }
    return value;
}

std::vector<std::string> split_whitespace(const std::string& s) {
    std::istringstream stream(s);
    std::vector<std::string> out;
    std::string token;
    while (stream >> token) out.push_back(token);
    return out;
}

std::vector<std::string> parse_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                field.push_back(c);
            }
        } else if (c == '"') {
            in_quotes = true;
        } else if (c == ',') {
            fields.push_back(field);
            field.clear();
        } else {
            field.push_back(c);
        }
    }
    fields.push_back(field);
    return fields;
}

// Look up a column index by its exact header name, with the historical
// "charcter" typo accepted as an alias for "character".
std::size_t column_index(const std::vector<std::string>& header,
                         const std::string& primary,
                         const std::string& alias) {
    for (std::size_t i = 0; i < header.size(); ++i) {
        if (header[i] == primary) return i;
    }
    if (!alias.empty()) {
        for (std::size_t i = 0; i < header.size(); ++i) {
            if (header[i] == alias) return i;
        }
    }
    return std::numeric_limits<std::size_t>::max();
}

bool parse_unihan(const std::string& text,
                  std::map<std::uint32_t, UniReading>& out,
                  std::size_t& warnings) {
    std::size_t line_no = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        std::string line = text.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (end == std::string::npos) start = text.size() + 1;
        else start = end + 1;
        ++line_no;

        if (line.empty() || line[0] == '#') continue;

        // Unihan files are tab-separated: U+XXXX <field> <value>.
        const std::size_t tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;
        const std::size_t tab2 = line.find('\t', tab1 + 1);
        if (tab2 == std::string::npos) continue;

        const std::uint32_t cp =
            codepoint_from_uplus(line.substr(0, tab1));
        if (cp == 0) continue;
        const std::string field = line.substr(tab1 + 1, tab2 - tab1 - 1);
        const std::string value = line.substr(tab2 + 1);

        if (field == "kMandarin") {
            UniReading& entry = out[cp];
            for (const auto& token : split_whitespace(value)) {
                const std::string canonical =
                    rospinyin::pinyin::canonical(token);
                if (canonical.empty() ||
                    !rospinyin::pinyin::is_canonical(canonical)) {
                    ++warnings;
                    continue;
                }
                entry.readings.push_back(canonical);
            }
        } else if (field == "kHanyuPinlu") {
            UniReading& entry = out[cp];
            for (const auto& token : split_whitespace(value)) {
                const std::size_t open = token.find('(');
                if (open == std::string::npos || token.back() != ')') continue;
                const std::string reading = token.substr(0, open);
                const std::string count_text =
                    token.substr(open + 1, token.size() - open - 2);
                std::uint32_t count = 0;
                try {
                    count = static_cast<std::uint32_t>(std::stoul(count_text));
                } catch (...) {
                    continue;
                }
                const std::string canonical =
                    rospinyin::pinyin::canonical(reading);
                if (canonical.empty() ||
                    !rospinyin::pinyin::is_canonical(canonical)) {
                    ++warnings;
                    continue;
                }
                if (count > entry.pinlu_count) {
                    entry.pinlu_count = count;
                    entry.pinlu_primary = canonical;
                }
            }
        }
        (void)line_no;
    }
    return true;
}

bool parse_frequency(const std::string& text,
                     std::map<std::uint32_t, FreqEntry>& out,
                     std::size_t& warnings) {
    std::size_t start = 0;
    bool first_line = true;
    std::size_t char_col = std::numeric_limits<std::size_t>::max();
    std::size_t pinyin_col = std::numeric_limits<std::size_t>::max();
    std::size_t rank_col = std::numeric_limits<std::size_t>::max();

    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        std::string line = text.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (end == std::string::npos) start = text.size() + 1;
        else start = end + 1;

        if (first_line) {
            first_line = false;
            // Strip UTF-8 BOM if present.
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }
            const auto header = parse_csv_line(line);
            char_col = column_index(header, "charcter", "character");
            if (char_col == std::numeric_limits<std::size_t>::max()) {
                char_col = column_index(header, "char", "");
            }
            pinyin_col = column_index(header, "pinyin", "");
            rank_col = column_index(header, "frequency_rank", "rank");
            if (char_col == std::numeric_limits<std::size_t>::max() ||
                pinyin_col == std::numeric_limits<std::size_t>::max() ||
                rank_col == std::numeric_limits<std::size_t>::max()) {
                std::fprintf(stderr,
                             "mkdat: hanziDB.csv is missing a required column "
                             "(charcter/character, pinyin, frequency_rank)\n");
                return false;
            }
            continue;
        }

        if (line.empty()) continue;
        const auto fields = parse_csv_line(line);
        if (fields.size() <= std::max({char_col, pinyin_col, rank_col})) {
            ++warnings;
            continue;
        }

        const std::uint32_t cp = first_codepoint(fields[char_col]);
        if (cp == 0) {
            ++warnings;
            continue;
        }
        const std::string canonical = rospinyin::pinyin::canonical(
            fields[pinyin_col]);
        if (canonical.empty() ||
            !rospinyin::pinyin::is_canonical(canonical)) {
            ++warnings;
            continue;
        }

        FreqEntry entry;
        entry.pinyin = canonical;
        try {
            const unsigned long rank = std::stoul(fields[rank_col]);
            entry.rank = rank == 0 ? kUnknownRank
                                   : static_cast<std::uint32_t>(rank);
        } catch (...) {
            entry.rank = kUnknownRank;
        }
        out[cp] = entry;
    }
    return true;
}

struct BuiltData {
    std::vector<CharEntry> chars;
    std::vector<SylEntry> syllables;
    std::string pool;
};

bool choose_primary(const UniReading* uni, const FreqEntry* freq,
                    std::string& primary, std::vector<std::string>& alts) {
    // Unihan lists polyphonic readings in primary-first order. Preserve that
    // order; do NOT sort, or 万 (wàn mò) would wrongly pick "mo4".
    std::vector<std::string> candidates;
    if (uni != nullptr) {
        std::set<std::string> seen;
        for (const auto& reading : uni->readings) {
            if (seen.insert(reading).second) candidates.push_back(reading);
        }
    }

    if (!candidates.empty()) {
        primary.clear();
        // kHanyuPinlu breaks ties among polyphonic kMandarin readings by
        // matching on the toneless syllable (it may omit the tone mark).
        if (uni != nullptr && !uni->pinlu_primary.empty()) {
            const std::string wanted =
                rospinyin::pinyin::syllable_of(uni->pinlu_primary);
            for (const auto& reading : candidates) {
                if (rospinyin::pinyin::syllable_of(reading) == wanted) {
                    primary = reading;
                    break;
                }
            }
        }
        if (primary.empty()) primary = candidates.front();
        for (const auto& reading : candidates) {
            if (reading != primary) alts.push_back(reading);
        }
        return true;
    }

    if (freq != nullptr && !freq->pinyin.empty()) {
        primary = freq->pinyin;
        return true;
    }
    return false;
}

BuiltData build(const std::map<std::uint32_t, UniReading>& unihan,
                const std::map<std::uint32_t, FreqEntry>& frequency,
                std::size_t& skipped) {
    struct Working {
        std::uint32_t codepoint = 0;
        std::uint32_t rank = kUnknownRank;
        std::uint32_t syl_id = 0;
        std::string reading;
        std::string alts;
    };

    std::vector<Working> working;
    std::set<std::uint32_t> all_codepoints;
    for (const auto& item : unihan) all_codepoints.insert(item.first);
    for (const auto& item : frequency) all_codepoints.insert(item.first);

    for (const std::uint32_t cp : all_codepoints) {
        const auto uni_it = unihan.find(cp);
        const auto freq_it = frequency.find(cp);
        const UniReading* uni =
            uni_it == unihan.end() ? nullptr : &uni_it->second;
        const FreqEntry* freq =
            freq_it == frequency.end() ? nullptr : &freq_it->second;

        std::string primary;
        std::vector<std::string> alts;
        if (!choose_primary(uni, freq, primary, alts)) {
            ++skipped;
            continue;
        }
        if (primary.empty() ||
            !rospinyin::pinyin::is_canonical(primary)) {
            ++skipped;
            continue;
        }

        Working item;
        item.codepoint = cp;
        item.rank = freq != nullptr ? freq->rank : kUnknownRank;
        item.reading = primary;
        for (std::size_t i = 0; i < alts.size(); ++i) {
            if (i != 0) item.alts.push_back(' ');
            item.alts.append(alts[i]);
        }
        working.push_back(std::move(item));
    }

    // Assign a stable id to every distinct toneless syllable, sorted by name.
    std::set<std::string> syllable_names;
    for (const auto& item : working) {
        const std::string syl = rospinyin::pinyin::syllable_of(item.reading);
        syllable_names.insert(syl);
    }
    std::vector<std::string> syllable_vector(syllable_names.begin(),
                                             syllable_names.end());
    std::unordered_map<std::string, std::uint32_t> syllable_ids;
    for (std::uint32_t i = 0; i < syllable_vector.size(); ++i) {
        syllable_ids[syllable_vector[i]] = i;
    }
    for (auto& item : working) {
        item.syl_id =
            syllable_ids[rospinyin::pinyin::syllable_of(item.reading)];
    }

    // Sort by syllable, then frequency rank (unknown last), then codepoint.
    const auto unknown_sort_key =
        static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    std::sort(working.begin(), working.end(),
              [unknown_sort_key](const Working& a, const Working& b) {
                  if (a.syl_id != b.syl_id) return a.syl_id < b.syl_id;
                  const std::uint32_t ar =
                      a.rank == kUnknownRank ? unknown_sort_key : a.rank;
                  const std::uint32_t br =
                      b.rank == kUnknownRank ? unknown_sort_key : b.rank;
                  if (ar != br) return ar < br;
                  return a.codepoint < b.codepoint;
              });

    // Build syllable ranges from the sorted char array.
    std::vector<SylEntry> syllables(syllable_vector.size());
    std::vector<std::uint32_t> counts(syllable_vector.size(), 0);
    for (std::uint32_t i = 0; i < working.size(); ++i) {
        const std::uint32_t id = working[i].syl_id;
        if (counts[id] == 0) syllables[id].first_char = i;
        ++counts[id];
    }
    for (std::uint32_t id = 0; id < syllables.size(); ++id) {
        syllables[id].char_count = counts[id];
    }

    // Build the deduplicated string pool in a deterministic order.
    std::string pool;
    std::unordered_map<std::string, std::uint32_t> pool_offsets;
    auto intern = [&](const std::string& value) -> std::uint32_t {
        const auto it = pool_offsets.find(value);
        if (it != pool_offsets.end()) return it->second;
        const auto offset = static_cast<std::uint32_t>(pool.size());
        pool.append(value);
        pool.push_back('\0');
        pool_offsets[value] = offset;
        return offset;
    };

    for (const auto& name : syllable_vector) intern(name);
    for (const auto& item : working) intern(item.reading);
    for (const auto& item : working) {
        if (!item.alts.empty()) intern(item.alts);
    }

    BuiltData result;
    result.chars.reserve(working.size());
    for (const auto& item : working) {
        CharEntry entry{};
        entry.codepoint = item.codepoint;
        entry.rank = item.rank;
        entry.syl_id = item.syl_id;
        entry.reading_off = intern(item.reading);
        entry.alt_off = item.alts.empty() ? 0 : intern(item.alts);
        result.chars.push_back(entry);
    }
    for (std::uint32_t id = 0; id < syllables.size(); ++id) {
        syllables[id].name_off = intern(syllable_vector[id]);
    }
    result.syllables = std::move(syllables);
    result.pool = std::move(pool);
    return result;
}

bool write_dat(const std::string& path, const BuiltData& data) {
    Header header{};
    std::copy(std::begin(kMagic), std::end(kMagic), header.magic);
    header.version = kVersion;
    header.flags = 0;
    header.char_count = static_cast<std::uint32_t>(data.chars.size());
    header.syl_count = static_cast<std::uint32_t>(data.syllables.size());
    header.char_table_off = imfw::dat::kHeaderSize;
    header.syl_table_off =
        static_cast<std::uint32_t>(header.char_table_off +
                                   data.chars.size() * sizeof(CharEntry));
    header.pool_off =
        static_cast<std::uint32_t>(header.syl_table_off +
                                   data.syllables.size() * sizeof(SylEntry));
    header.pool_size = static_cast<std::uint32_t>(data.pool.size());

    std::vector<unsigned char> buffer(header.pool_off + data.pool.size(), 0);
    std::copy(reinterpret_cast<const unsigned char*>(&header),
              reinterpret_cast<const unsigned char*>(&header) + sizeof(header),
              buffer.begin());
    std::copy(reinterpret_cast<const unsigned char*>(data.chars.data()),
              reinterpret_cast<const unsigned char*>(data.chars.data()) +
                  data.chars.size() * sizeof(CharEntry),
              buffer.begin() + header.char_table_off);
    if (!data.syllables.empty()) {
        std::copy(reinterpret_cast<const unsigned char*>(data.syllables.data()),
                  reinterpret_cast<const unsigned char*>(data.syllables.data()) +
                      data.syllables.size() * sizeof(SylEntry),
                  buffer.begin() + header.syl_table_off);
    }
    std::copy(data.pool.begin(), data.pool.end(),
              buffer.begin() + header.pool_off);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::fprintf(stderr, "mkdat: cannot write %s\n", path.c_str());
        return false;
    }
    output.write(reinterpret_cast<const char*>(buffer.data()),
                 static_cast<std::streamsize>(buffer.size()));
    return output.good();
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_args(argc, argv, options)) return usage(argv[0]);

    std::size_t warnings = 0;
    std::map<std::uint32_t, UniReading> unihan;
    std::map<std::uint32_t, FreqEntry> frequency;

    const std::string unihan_text = read_whole_file(options.unihan_path);
    if (unihan_text.empty()) return 1;
    if (!parse_unihan(unihan_text, unihan, warnings)) return 1;

    const std::string frequency_text = read_whole_file(options.frequency_path);
    if (frequency_text.empty()) return 1;
    if (!parse_frequency(frequency_text, frequency, warnings)) return 1;

    std::size_t skipped = 0;
    const BuiltData data = build(unihan, frequency, skipped);
    if (!write_dat(options.out_path, data)) return 1;

    if (!options.quiet) {
        std::printf(
            "mkdat: %zu chars, %zu syllables, %zu-byte string pool -> %s\n",
            data.chars.size(), data.syllables.size(), data.pool.size(),
            options.out_path.c_str());
        if (warnings != 0 || skipped != 0) {
            std::printf("mkdat: warnings=%zu skipped=%zu\n", warnings, skipped);
        }
    }
    if (options.stats) {
        std::size_t ranked = 0;
        std::size_t with_alt = 0;
        for (const auto& c : data.chars) {
            if (c.rank != kUnknownRank) ++ranked;
            if (c.alt_off != 0) ++with_alt;
        }
        std::printf("mkdat stats: ranked=%zu unknown=%zu polyphonic=%zu\n",
                    ranked, data.chars.size() - ranked, with_alt);
    }
    return 0;
}
