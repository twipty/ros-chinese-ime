// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Framework utility test: imfw::CodeTable is the generic, pinyin-agnostic
// reader/index over an imfw .dat blob (dat_format.hpp). It is the one piece
// of framework/src without a dedicated host test even though it carries the
// bulk of the binary-parsing validation and the candidate ordering contract
// (rank ascending, codepoint tie-break). This test builds a byte-accurate
// blob exactly as mkdat does (no file I/O, no Windows headers) and pins both
// the happy path and the "reject, never half-load" corruption paths.
//
// The format has exactly one version (2). Every field the loader is supposed
// to check has a case here, because a validation branch nobody tests is a
// branch that will be dropped in the next refactor.

#include <imfw/code_table.hpp>
#include <imfw/dat_format.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "code table test failed: %s\n", what);
        ++failures;
    }
}

using imfw::dat::CharEntry;
using imfw::dat::Header;
using imfw::dat::LexCode;
using imfw::dat::LexEntry;
using imfw::dat::SylEntry;

constexpr std::size_t kHeaderSize = sizeof(Header);          // 80
constexpr std::size_t kCharEntrySize = sizeof(CharEntry);    // 24
constexpr std::size_t kSylEntrySize = sizeof(SylEntry);      // 16

// Field offsets inside Header (all little-endian uint32 after magic[8]).
constexpr std::size_t kOffVersion = 8;
constexpr std::size_t kOffFlags = 12;
constexpr std::size_t kOffEntryCount = 16;
constexpr std::size_t kOffCodeCount = 20;
constexpr std::size_t kOffLexEntryCount = 24;
constexpr std::size_t kOffLexCodeCount = 28;
constexpr std::size_t kOffEntryTableOff = 32;
constexpr std::size_t kOffCodeTableOff = 36;
constexpr std::size_t kOffLexEntryTableOff = 40;
constexpr std::size_t kOffLexCodeTableOff = 44;
constexpr std::size_t kOffPoolOff = 48;
constexpr std::size_t kOffPoolSize = 52;
constexpr std::size_t kOffEntrySize = 68;
constexpr std::size_t kOffCodeSize = 72;

// A version that no build will ever claim. Not kVersion + 1: the "next"
// version is exactly the value that becomes legal at the next format bump,
// and the case would then silently stop testing rejection.
constexpr std::uint32_t kNoSuchVersion = 0x7F7Fu;

void put_u32(std::string& blob, std::size_t offset, std::uint32_t value) {
    std::memcpy(blob.data() + offset, &value, sizeof(value));
}

std::uint32_t read_u32(const std::string& blob, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, blob.data() + offset, sizeof(value));
    return value;
}

// Fill a header whose offset chain is self-consistent for the given record
// sizes. A test that changes entry_size gets a file that is coherent *as* a
// 20-byte-entry file, so the only thing left to reject it is the size check
// itself -- which is what we want to pin.
Header build_header(std::uint32_t chars, std::uint32_t codes,
                    std::uint32_t words, std::uint32_t keys,
                    std::uint32_t pool_size,
                    std::uint32_t entry_size = kCharEntrySize,
                    std::uint32_t code_size = kSylEntrySize) {
    Header header{};
    std::memcpy(header.magic, imfw::dat::kMagic, sizeof(imfw::dat::kMagic));
    header.version = imfw::dat::kVersion;
    header.flags = 0;
    header.entry_count = chars;
    header.code_count = codes;
    header.lex_entry_count = words;
    header.lex_code_count = keys;
    header.entry_table_off = static_cast<std::uint32_t>(kHeaderSize);
    header.code_table_off = header.entry_table_off + chars * entry_size;
    header.lex_entry_table_off = header.code_table_off + codes * code_size;
    header.lex_code_table_off = header.lex_entry_table_off + words * entry_size;
    header.pool_off = header.lex_code_table_off + keys * code_size;
    header.pool_size = pool_size;
    header.entry_size = entry_size;
    header.code_size = code_size;
    return header;
}

struct Pool {
    std::string bytes;
    std::uint32_t intern(const char* s) {
        const std::uint32_t offset = static_cast<std::uint32_t>(bytes.size());
        bytes.append(s);
        bytes.push_back('\0');
        return offset;
    }
};

// Build a valid dat blob with three keys:
//   zhong -> [中(rank 7), 钟(rank 500)]  readings "zho1ng"
//   guo   -> [国(rank 8)]                 reading  "guo2"
//   de    -> [的(rank 1)]                 reading  "de"
std::string make_dat() {
    Pool pool;
    const std::uint32_t zhong_name = pool.intern("zhong");   // 0
    const std::uint32_t zho1ng = pool.intern("zho1ng");      // 6
    const std::uint32_t guo_name = pool.intern("guo");       // 13
    const std::uint32_t guo2 = pool.intern("guo2");          // 17
    const std::uint32_t de_name = pool.intern("de");         // 22
    const std::uint32_t de = pool.intern("de");              // 25

    std::vector<CharEntry> chars(4);
    chars[0] = CharEntry{0x4E2D, 7, 0, zho1ng, 0, 0};  // 中
    chars[1] = CharEntry{0x949F, 500, 0, zho1ng, 0, 0};  // 钟
    chars[2] = CharEntry{0x56FD, 8, 1, guo2, 0, 0};    // 国
    chars[3] = CharEntry{0x7684, 1, 2, de, 0, 0};      // 的

    std::vector<SylEntry> syls(3);
    syls[0] = SylEntry{zhong_name, 0, 2, 0};
    syls[1] = SylEntry{guo_name, 2, 1, 0};
    syls[2] = SylEntry{de_name, 3, 1, 0};

    const Header header = build_header(4, 3, 0, 0,
                                       static_cast<std::uint32_t>(pool.bytes.size()));
    const std::size_t char_off = header.entry_table_off;
    const std::size_t code_off = header.code_table_off;

    std::string blob(header.pool_off + pool.bytes.size(), '\0');
    std::memcpy(blob.data(), &header, sizeof(header));
    for (std::size_t i = 0; i < chars.size(); ++i) {
        std::memcpy(blob.data() + char_off + i * kCharEntrySize, &chars[i],
                    sizeof(chars[i]));
    }
    for (std::size_t i = 0; i < syls.size(); ++i) {
        std::memcpy(blob.data() + code_off + i * kSylEntrySize, &syls[i],
                    sizeof(syls[i]));
    }
    std::memcpy(blob.data() + header.pool_off, pool.bytes.data(),
                pool.bytes.size());
    return blob;
}

// Two keys, one polyphonic character whose second reading is only in alt_off:
//   shei -> [谁(rank 648)]   reading "she2i", alt "shui2"
//   shui -> [水(rank 202)]   reading "shui3"
std::string make_poly_dat() {
    Pool pool;
    const std::uint32_t shei_name = pool.intern("shei");
    const std::uint32_t shui_name = pool.intern("shui");
    const std::uint32_t she2i = pool.intern("she2i");
    const std::uint32_t shui2 = pool.intern("shui2");
    const std::uint32_t shui3 = pool.intern("shui3");

    std::vector<CharEntry> chars(2);
    chars[0] = CharEntry{0x8C01, 648, 0, she2i, shui2, 0};  // 谁, alt "shui2"
    chars[1] = CharEntry{0x6C34, 202, 1, shui3, 0, 0};      // 水

    std::vector<SylEntry> syls(2);
    syls[0] = SylEntry{shei_name, 0, 1, 0};
    syls[1] = SylEntry{shui_name, 1, 1, 0};

    const Header header = build_header(2, 2, 0, 0,
                                       static_cast<std::uint32_t>(pool.bytes.size()));

    std::string blob(header.pool_off + pool.bytes.size(), '\0');
    std::memcpy(blob.data(), &header, sizeof(header));
    for (std::size_t i = 0; i < chars.size(); ++i) {
        std::memcpy(blob.data() + header.entry_table_off + i * kCharEntrySize,
                    &chars[i], sizeof(chars[i]));
    }
    for (std::size_t i = 0; i < syls.size(); ++i) {
        std::memcpy(blob.data() + header.code_table_off + i * kSylEntrySize,
                    &syls[i], sizeof(syls[i]));
    }
    std::memcpy(blob.data() + header.pool_off, pool.bytes.data(),
                pool.bytes.size());
    return blob;
}

// A dat that carries a word list: two characters (你 ni3, 好 hao3) and two
// words keyed by their syllable sequences.
//   ni3hao3 -> 你好
//   ni3men5 -> 你们
std::string make_lex_dat() {
    Pool pool;
    const std::uint32_t ni_name = pool.intern("ni");
    const std::uint32_t ni3 = pool.intern("ni3");
    const std::uint32_t hao_name = pool.intern("hao");
    const std::uint32_t hao3 = pool.intern("hao3");
    const std::uint32_t ni_hao = pool.intern("你好");
    const std::uint32_t ni_men = pool.intern("你们");
    const std::uint32_t key_ni_hao = pool.intern("ni3hao3");
    const std::uint32_t key_ni_men = pool.intern("ni3men5");

    std::vector<CharEntry> chars(2);
    chars[0] = CharEntry{0x4F60, 5, 0, ni3, 0, 0};    // 你
    chars[1] = CharEntry{0x597D, 9, 1, hao3, 0, 0};   // 好

    std::vector<SylEntry> syls(2);
    syls[0] = SylEntry{ni_name, 0, 1, 0};
    syls[1] = SylEntry{hao_name, 1, 1, 0};

    std::vector<LexEntry> words(2);
    // reading_off = 0: the reading is the key with the spaces taken out, so
    // the pool does not carry a second copy of every word's pinyin.
    words[0] = LexEntry{ni_hao, 1, 2, 0, 0, 0};
    words[1] = LexEntry{ni_men, 2, 2, 0, 0, 0};

    std::vector<LexCode> keys(2);
    keys[0] = LexCode{key_ni_hao, 0, 1, 0};
    keys[1] = LexCode{key_ni_men, 1, 1, 0};

    const Header header = build_header(2, 2, 2, 2,
                                       static_cast<std::uint32_t>(pool.bytes.size()));

    std::string blob(header.pool_off + pool.bytes.size(), '\0');
    std::memcpy(blob.data(), &header, sizeof(header));
    for (std::size_t i = 0; i < chars.size(); ++i) {
        std::memcpy(blob.data() + header.entry_table_off + i * kCharEntrySize,
                    &chars[i], sizeof(chars[i]));
    }
    for (std::size_t i = 0; i < syls.size(); ++i) {
        std::memcpy(blob.data() + header.code_table_off + i * kSylEntrySize,
                    &syls[i], sizeof(syls[i]));
    }
    for (std::size_t i = 0; i < words.size(); ++i) {
        std::memcpy(blob.data() + header.lex_entry_table_off + i * sizeof(LexEntry),
                    &words[i], sizeof(words[i]));
    }
    for (std::size_t i = 0; i < keys.size(); ++i) {
        std::memcpy(blob.data() + header.lex_code_table_off + i * sizeof(LexCode),
                    &keys[i], sizeof(keys[i]));
    }
    std::memcpy(blob.data() + header.pool_off, pool.bytes.data(),
                pool.bytes.size());
    return blob;
}

// Strips tone digits: "she2i" -> "shei". Mirrors pinyin::syllable_of for the
// purposes of this test without dragging the pinyin engine in.
std::string toneless(std::string_view reading) {
    std::string out;
    for (const char c : reading) {
        if (c < '0' || c > '9') out.push_back(c);
    }
    return out;
}

}  // namespace

int main() {
    // ---- happy path: parse a valid blob ---------------------------------
    {
        const std::string blob = make_dat();
        imfw::CodeTable table;
        const std::size_t loaded = table.load_dat(blob);
        check(loaded == 4, "valid dat loads all four characters");
        check(table.char_count() == 4, "char_count reflects the dat");
        check(table.code_count() == 3, "code_count is the number of keys");

        const auto zhong = table.lookup("zhong");
        check(zhong.size() == 2, "zhong has two candidates");
        if (zhong.size() == 2) {
            check(zhong[0].codepoint == 0x4E2D && zhong[0].rank == 7,
                  "rank 7 (中) comes first");
            check(zhong[1].codepoint == 0x949F && zhong[1].rank == 500,
                  "rank 500 (钟) comes second");
            check(zhong[0].reading == "zho1ng" && zhong[1].reading == "zho1ng",
                  "readings are copied from the string pool");
        }
        const auto guo = table.lookup("guo");
        check(guo.size() == 1 && guo[0].codepoint == 0x56FD &&
                  guo[0].reading == "guo2",
              "guo resolves to 国 with its full reading");
        const auto de = table.lookup("de");
        check(de.size() == 1 && de[0].codepoint == 0x7684,
              "de resolves to 的");
        check(table.lookup("xx").empty(), "unknown key has no candidates");

        check(table.is_code("zhong") && table.is_code("guo") &&
                  table.is_code("de"),
              "is_code recognises every key");
        check(!table.is_code("zho") && !table.is_code("zhongg"),
              "is_code rejects non-keys");
        check(table.is_prefix("z") && table.is_prefix("zho") &&
                  table.is_prefix("zhong"),
              "is_prefix recognises every prefix of a key");
        check(!table.is_prefix("x") && !table.is_prefix("zhongg") &&
                  !table.is_prefix(""),
              "is_prefix rejects non-prefixes");
    }

    // ---- tier bit is data, not corruption -------------------------------
    {
        // bit1 marks "needs a font beyond the bundled one". The loader must
        // accept it: filtering on it is the engine's job (F-T2), not the
        // parser's, and a parser that rejected it would reject half the table.
        std::string b = make_dat();
        put_u32(b, read_u32(b, kOffEntryTableOff) + 20,
                imfw::dat::kFlagTier);
        imfw::CodeTable table;
        check(table.load_dat(b) == 4, "tier bit is accepted by the loader");
    }

    // ---- builder API: rank order, codepoint tie-break, clear ------------
    {
        imfw::CodeTable table;
        table.add(U'钟', 10, "zhong", "zhong");   // higher codepoint
        table.add(U'中', 10, "zhong", "zhong");   // same rank, lower codepoint
        table.add(U'种', 20, "zhong", "zhong");
        table.add(U'人', imfw::dat::kUnknownRank, "ren", "ren");
        table.add(U'仁', 30, "ren", "ren");
        table.build_index();

        check(table.char_count() == 5 && table.code_count() == 2,
              "builder counts entries and keys");
        const auto zhong = table.lookup("zhong");
        check(zhong.size() == 3, "builder keeps all entries for a key");
        if (zhong.size() == 3) {
            check(zhong[0].codepoint == U'中' &&
                      zhong[1].codepoint == U'钟',
                  "equal ranks tie-break by ascending codepoint");
            check(zhong[2].codepoint == U'种',
                  "higher rank sorts after lower rank");
        }
        const auto ren = table.lookup("ren");
        check(ren.size() == 2 && ren[0].codepoint == U'仁' &&
                  ren[1].rank == imfw::dat::kUnknownRank,
              "unknown rank sorts after ranked entries");

        table.clear();
        check(table.char_count() == 0 && table.code_count() == 0,
              "clear resets counts");
        check(table.lookup("zhong").empty() && !table.is_code("zhong") &&
                  !table.is_prefix("z"),
              "clear resets the indexes");
    }

    // ---- word list: present, well formed, not indexed yet ---------------
    {
        // M4 indexes the words; until then the parser's job is to prove the
        // declared word list is coherent, not to silently ignore it.
        const std::string blob = make_lex_dat();
        imfw::CodeTable table;
        check(table.load_dat(blob) == 2,
              "dat with a word list still loads its characters");
        check(table.char_count() == 2, "word list does not inflate char_count");
        check(table.lookup("ni").size() == 1, "characters stay reachable");
    }
    {
        // Words nothing can reach is a builder bug: reject, do not guess.
        std::string b = make_lex_dat();
        const Header header = build_header(2, 2, 2, 0, 32);
        std::memcpy(b.data(), &header, sizeof(header));
        imfw::CodeTable table;
        check(table.load_dat(b) == 0, "word list without keys is rejected");
    }
    {
        // A key that does not start where the previous run ended means the
        // word table cannot be walked; the loader checks the chain.
        std::string b = make_lex_dat();
        const std::uint32_t keys_off = read_u32(b, kOffLexCodeTableOff);
        put_u32(b, keys_off + 4, read_u32(b, keys_off + 4) + 1);
        imfw::CodeTable table;
        check(table.load_dat(b) == 0,
              "word key run that skips a word is rejected");
    }
    {
        // A single-syllable "word" is a character in disguise.
        std::string b = make_lex_dat();
        const std::uint32_t words_off = read_u32(b, kOffLexEntryTableOff);
        put_u32(b, words_off + 8, 1);
        imfw::CodeTable table;
        check(table.load_dat(b) == 0, "one-syllable word entry is rejected");
    }

    // ---- corruption: reject, never half-load ---------------------------
    {
        imfw::CodeTable table;
        const std::string valid = make_dat();

        check(table.load_dat({}) == 0, "empty blob is rejected");
        check(table.load_dat(std::string(40, '\0')) == 0,
              "blob shorter than the header is rejected");
        {
            std::string b = valid;
            b[0] = 'X';
            check(table.load_dat(b) == 0, "bad magic is rejected");
        }
        {
            // Shape.dat shares the skeleton, so only the magic keeps a shape
            // table from being read as pinyin (slot 0 would become a pool
            // offset posing as a code point). Pin the refusal by name.
            std::string b = valid;
            std::memcpy(&b[0], imfw::dat::kMagicShape,
                        sizeof(imfw::dat::kMagicShape));
            check(table.load_dat(b) == 0,
                  "shape.dat magic is rejected by the pinyin loader");
        }
        {
            // Version 1 is the old 52-byte-header layout: it has no tier bit
            // and no word table, so reading it would filter on a bit that is
            // not there. It must fail loudly, not be upgraded silently.
            std::string b = valid;
            put_u32(b, kOffVersion, 1);
            check(table.load_dat(b) == 0, "version 1 (legacy) is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kOffVersion, kNoSuchVersion);
            check(table.load_dat(b) == 0, "unknown version is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kOffFlags, 1);
            check(table.load_dat(b) == 0, "nonzero header flags are rejected");
        }
        {
            // A half-upgraded file: right version, old record width. The
            // loader must notice the size before it walks the records.
            std::string b = valid;
            put_u32(b, kOffEntrySize, 20);
            check(table.load_dat(b) == 0,
                  "entry_size that disagrees with the records is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kOffCodeSize, 12);
            check(table.load_dat(b) == 0,
                  "code_size that disagrees with the records is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kOffEntryTableOff, static_cast<std::uint32_t>(kHeaderSize + 1));
            check(table.load_dat(b) == 0,
                  "entry table not at the header boundary is rejected");
        }
        {
            std::string b = valid;
            std::uint32_t code_off = 0;
            std::memcpy(&code_off, b.data() + kOffCodeTableOff, sizeof(code_off));
            put_u32(b, kOffCodeTableOff, code_off + 1);
            check(table.load_dat(b) == 0,
                  "code table not following the entry table is rejected");
        }
        {
            std::string b = valid;
            std::uint32_t pool_off = 0;
            std::memcpy(&pool_off, b.data() + kOffPoolOff, sizeof(pool_off));
            put_u32(b, kOffPoolOff, pool_off + 1);
            check(table.load_dat(b) == 0,
                  "pool not following the word tables is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kOffPoolSize, 0xFFFFFFFFu);
            check(table.load_dat(b) == 0,
                  "pool extending past the blob is rejected");
        }
        {
            // Pinyin never writes the shape-only header fields; a non-zero
            // one means the file is somebody else's or was written wrong.
            std::string b = valid;
            put_u32(b, 56, 1);  // name_off
            check(table.load_dat(b) == 0,
                  "shape-only header field set is rejected");
        }
        {
            std::string b = valid;
            std::uint32_t entry_off = 0;
            std::memcpy(&entry_off, b.data() + kOffEntryTableOff, sizeof(entry_off));
            put_u32(b, entry_off + 0 * kCharEntrySize + 8, 9999);
            check(table.load_dat(b) == 0,
                  "character with out-of-range syl_id is rejected");
        }
        {
            std::string b = valid;
            std::uint32_t entry_off = 0;
            std::memcpy(&entry_off, b.data() + kOffEntryTableOff, sizeof(entry_off));
            put_u32(b, entry_off + 0 * kCharEntrySize + 12, 9999);
            check(table.load_dat(b) == 0,
                  "character with out-of-pool reading offset is rejected");
        }
        {
            // rank 0 is neither a 1-based order nor the sentinel.
            std::string b = valid;
            std::uint32_t entry_off = 0;
            std::memcpy(&entry_off, b.data() + kOffEntryTableOff, sizeof(entry_off));
            put_u32(b, entry_off + 0 * kCharEntrySize + 4, 0);
            check(table.load_dat(b) == 0, "rank zero is rejected");
        }
        {
            // bit2 is reserved: a reader that guessed would mis-filter.
            std::string b = valid;
            std::uint32_t entry_off = 0;
            std::memcpy(&entry_off, b.data() + kOffEntryTableOff, sizeof(entry_off));
            put_u32(b, entry_off + 0 * kCharEntrySize + 20, 0x4);
            check(table.load_dat(b) == 0, "reserved flag bit is rejected");
        }
        {
            std::string b = valid;
            std::uint32_t code_off = 0;
            std::memcpy(&code_off, b.data() + kOffCodeTableOff, sizeof(code_off));
            put_u32(b, code_off + 0 * kSylEntrySize + 0, 9999);
            check(table.load_dat(b) == 0,
                  "syllable with out-of-pool name offset is rejected");
        }
        {
            // Slot 3 of a syllable is reserved for pinyin.
            std::string b = valid;
            std::uint32_t code_off = 0;
            std::memcpy(&code_off, b.data() + kOffCodeTableOff, sizeof(code_off));
            put_u32(b, code_off + 0 * kSylEntrySize + 12, 1);
            check(table.load_dat(b) == 0,
                  "syllable flags must be zero");
        }
        {
            // Segments must tile the entry table: moving the first range
            // leaves the first character unreachable.
            std::string b = valid;
            std::uint32_t code_off = 0;
            std::memcpy(&code_off, b.data() + kOffCodeTableOff, sizeof(code_off));
            put_u32(b, code_off + 0 * kSylEntrySize + 4, 1);
            check(table.load_dat(b) == 0,
                  "syllable range that does not start at zero is rejected");
        }
        {
            std::string b = valid;
            std::uint32_t pool_off = 0;
            std::memcpy(&pool_off, b.data() + kOffPoolOff, sizeof(pool_off));
            // "zho1ng" starts 6 bytes into the pool; upper case is not canonical.
            b[pool_off + 6] = 'Z';
            check(table.load_dat(b) == 0,
                  "non-canonical reading byte is rejected");
        }
        {
            // The last pool string is "de\0"; removing its terminator makes
            // pool_string run past the pool limit.
            std::string b = valid;
            b[b.size() - 1] = 'X';
            check(table.load_dat(b) == 0,
                  "unterminated pool string is rejected");
        }

        // A rejected load must leave the table empty (no partial data).
        check(table.char_count() == 0 && table.lookup("zhong").empty(),
              "rejected loads leave the table empty");
    }

    // ---- alt readings: inert without a deriver, live with one ------------
    {
        const std::string blob = make_poly_dat();

        // No deriver: the table still knows the character under its own key,
        // but the alt reading is not indexed (this is the historical
        // behaviour, kept for callers that do not want it).
        imfw::CodeTable plain;
        check(plain.load_dat(blob) == 2, "poly dat loads two characters");
        check(plain.lookup("shei").size() == 1, "shei finds 谁");
        check(plain.lookup("shui").size() == 1,
              "without a deriver shui finds only 水");

        imfw::CodeTable poly;
        check(poly.load_dat(blob, &toneless) == 2, "poly dat loads with deriver");
        check(poly.lookup("shei").size() == 1, "shei still finds 谁");
        const auto shui = poly.lookup("shui");
        check(shui.size() == 2, "shui finds 水 and 谁 through the alt reading");
        if (shui.size() == 2) {
            check(shui[0].codepoint == 0x6C34 && shui[0].rank == 202,
                  "alt-indexed candidates keep rank order (水 first)");
            check(shui[1].codepoint == 0x8C01 && shui[1].rank == 648,
                  "谁 is reachable by its alternative reading");
            check(shui[1].reading == "she2i",
                  "the reported reading stays the canonical one");
        }
        // A character is offered once per lookup, not once per key.
        std::size_t occurrences = 0;
        for (const auto& entry : poly.lookup("shei")) {
            if (entry.codepoint == 0x8C01) ++occurrences;
        }
        check(occurrences == 1, "alt indexing does not duplicate a character");
    }

    if (failures != 0) {
        std::fprintf(stderr, "%d code table test(s) failed\n", failures);
        return 1;
    }
    std::printf("code table contract OK (dat parse + ordering + reject)\n");
    return 0;
}
