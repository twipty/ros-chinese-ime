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
using imfw::dat::SylEntry;

constexpr std::size_t kHeaderSize = sizeof(Header);          // 52
constexpr std::size_t kCharEntrySize = sizeof(CharEntry);    // 20
constexpr std::size_t kSylEntrySize = sizeof(SylEntry);      // 16
constexpr std::size_t kCharTableOff = kHeaderSize;           // 52
constexpr std::size_t kSylTableOff = kCharTableOff + 4 * kCharEntrySize;  // 132
constexpr std::size_t kPoolOff = kSylTableOff + 3 * kSylEntrySize;        // 180

// Field offsets inside Header (all little-endian uint32 after magic[8]).
constexpr std::size_t kOffVersion = 8;
constexpr std::size_t kOffFlags = 12;
constexpr std::size_t kOffCharCount = 16;
constexpr std::size_t kOffSylCount = 20;
constexpr std::size_t kOffCharTableOff = 24;
constexpr std::size_t kOffSylTableOff = 28;
constexpr std::size_t kOffPoolOff = 32;
constexpr std::size_t kOffPoolSize = 36;

void put_u32(std::string& blob, std::size_t offset, std::uint32_t value) {
    std::memcpy(blob.data() + offset, &value, sizeof(value));
}

// Build a valid dat blob with three keys:
//   zhong -> [中(rank 7), 钟(rank 500)]  readings "zho1ng"
//   guo   -> [国(rank 8)]                 reading  "guo2"
//   de    -> [的(rank 1)]                 reading  "de"
std::string make_dat() {
    std::string pool;
    auto intern = [&](const char* s) -> std::uint32_t {
        const std::uint32_t offset = static_cast<std::uint32_t>(pool.size());
        pool.append(s);
        pool.push_back('\0');
        return offset;
    };
    const std::uint32_t zhong_name = intern("zhong");   // 0
    const std::uint32_t zho1ng = intern("zho1ng");      // 6
    const std::uint32_t guo_name = intern("guo");       // 13
    const std::uint32_t guo2 = intern("guo2");          // 17
    const std::uint32_t de_name = intern("de");         // 22
    const std::uint32_t de = intern("de");              // 25
    (void)de_name; (void)de;

    std::vector<CharEntry> chars(4);
    chars[0] = CharEntry{0x4E2D, 7,   0, zho1ng, 0};  // 中
    chars[1] = CharEntry{0x949F, 500, 0, zho1ng, 0};  // 钟
    chars[2] = CharEntry{0x56FD, 8,   1, guo2, 0};    // 国
    chars[3] = CharEntry{0x7684, 1,   2, de, 0};      // 的

    std::vector<SylEntry> syls(3);
    syls[0] = SylEntry{zhong_name, 0, 2, 0};
    syls[1] = SylEntry{guo_name, 2, 1, 0};
    syls[2] = SylEntry{de_name, 3, 1, 0};

    Header header{};
    std::memcpy(header.magic, imfw::dat::kMagic, sizeof(imfw::dat::kMagic));
    header.version = imfw::dat::kVersion;
    header.flags = 0;
    header.char_count = static_cast<std::uint32_t>(chars.size());
    header.syl_count = static_cast<std::uint32_t>(syls.size());
    header.char_table_off = static_cast<std::uint32_t>(kCharTableOff);
    header.syl_table_off = static_cast<std::uint32_t>(kSylTableOff);
    header.pool_off = static_cast<std::uint32_t>(kPoolOff);
    header.pool_size = static_cast<std::uint32_t>(pool.size());

    std::string blob(kPoolOff + pool.size(), '\0');
    std::memcpy(blob.data(), &header, sizeof(header));
    for (std::size_t i = 0; i < chars.size(); ++i) {
        std::memcpy(blob.data() + kCharTableOff + i * kCharEntrySize,
                    &chars[i], sizeof(chars[i]));
    }
    for (std::size_t i = 0; i < syls.size(); ++i) {
        std::memcpy(blob.data() + kSylTableOff + i * kSylEntrySize,
                    &syls[i], sizeof(syls[i]));
    }
    std::memcpy(blob.data() + kPoolOff, pool.data(), pool.size());
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

// Two keys, one polyphonic character whose second reading is only in alt_off:
//   shei -> [谁(rank 648)]   reading "she2i", alt "shui2"
//   shui -> [水(rank 202)]   reading "shui3"
std::string make_poly_dat() {
    std::string pool;
    auto intern = [&](const char* s) -> std::uint32_t {
        const std::uint32_t offset = static_cast<std::uint32_t>(pool.size());
        pool.append(s);
        pool.push_back('\0');
        return offset;
    };
    const std::uint32_t shei_name = intern("shei");
    const std::uint32_t shui_name = intern("shui");
    const std::uint32_t she2i = intern("she2i");
    const std::uint32_t shui2 = intern("shui2");
    const std::uint32_t shui3 = intern("shui3");

    std::vector<CharEntry> chars(2);
    chars[0] = CharEntry{0x8C01, 648, 0, she2i, shui2};  // 谁, alt reading shui
    chars[1] = CharEntry{0x6C34, 202, 1, shui3, 0};      // 水

    std::vector<SylEntry> syls(2);
    syls[0] = SylEntry{shei_name, 0, 1, 0};
    syls[1] = SylEntry{shui_name, 1, 1, 0};

    constexpr std::size_t char_off = kHeaderSize;
    constexpr std::size_t syl_off = char_off + 2 * kCharEntrySize;
    constexpr std::size_t pool_off = syl_off + 2 * kSylEntrySize;

    Header header{};
    std::memcpy(header.magic, imfw::dat::kMagic, sizeof(imfw::dat::kMagic));
    header.version = imfw::dat::kVersion;
    header.flags = 0;
    header.char_count = static_cast<std::uint32_t>(chars.size());
    header.syl_count = static_cast<std::uint32_t>(syls.size());
    header.char_table_off = static_cast<std::uint32_t>(char_off);
    header.syl_table_off = static_cast<std::uint32_t>(syl_off);
    header.pool_off = static_cast<std::uint32_t>(pool_off);
    header.pool_size = static_cast<std::uint32_t>(pool.size());

    std::string blob(pool_off + pool.size(), '\0');
    std::memcpy(blob.data(), &header, sizeof(header));
    for (std::size_t i = 0; i < chars.size(); ++i) {
        std::memcpy(blob.data() + char_off + i * kCharEntrySize, &chars[i],
                    sizeof(chars[i]));
    }
    for (std::size_t i = 0; i < syls.size(); ++i) {
        std::memcpy(blob.data() + syl_off + i * kSylEntrySize, &syls[i],
                    sizeof(syls[i]));
    }
    std::memcpy(blob.data() + pool_off, pool.data(), pool.size());
    return blob;
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

    // ---- corruption: reject, never half-load ---------------------------
    {
        imfw::CodeTable table;
        const std::string valid = make_dat();

        check(table.load_dat({}) == 0, "empty blob is rejected");
        check(table.load_dat(std::string(20, '\0')) == 0,
              "blob shorter than the header is rejected");
        {
            std::string b = valid;
            b[0] = 'X';
            check(table.load_dat(b) == 0, "bad magic is rejected");
        }
        {
            // Red line 1 of docs/plan/DAT-FORMAT-V2.md: shape.dat shares the
            // skeleton, so only the magic keeps a shape table from being read
            // as pinyin (slot 0 would become a pool offset posing as a code
            // point). Pin the refusal by name, not just "some bad byte".
            std::string b = valid;
            std::memcpy(&b[0], imfw::dat::kMagicShape,
                        sizeof(imfw::dat::kMagicShape));
            check(table.load_dat(b) == 0,
                  "shape.dat magic is rejected by the pinyin loader");
        }
        {
            std::string b = valid;
            // Pin an unknown version, not "the next one": version 2 is the
            // unified format and becomes readable with F-T1, at which point
            // this case would silently turn into a v2-parse test.
            put_u32(b, kOffVersion, imfw::dat::kVersionV2 + 1);
            check(table.load_dat(b) == 0, "unknown version is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kOffFlags, 1);
            check(table.load_dat(b) == 0, "nonzero flags are rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kOffCharTableOff, 53);
            check(table.load_dat(b) == 0,
                  "char table not at the header boundary is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kOffSylTableOff,
                    static_cast<std::uint32_t>(kSylTableOff + 1));
            check(table.load_dat(b) == 0,
                  "syl table not following char table is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kOffPoolOff, static_cast<std::uint32_t>(kPoolOff + 1));
            check(table.load_dat(b) == 0,
                  "pool not following syl table is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kOffPoolSize, 0xFFFFFFFFu);
            check(table.load_dat(b) == 0,
                  "pool extending past the blob is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kCharTableOff + 0 * kCharEntrySize + 8, 9999);
            check(table.load_dat(b) == 0,
                  "character with out-of-range syl_id is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kCharTableOff + 0 * kCharEntrySize + 12, 9999);
            check(table.load_dat(b) == 0,
                  "character with out-of-pool reading offset is rejected");
        }
        {
            std::string b = valid;
            put_u32(b, kSylTableOff + 0 * kSylEntrySize + 0, 9999);
            check(table.load_dat(b) == 0,
                  "syllable with out-of-pool name offset is rejected");
        }
        {
            std::string b = valid;
            // "zho1ng" starts at pool offset 6; upper-case is not canonical.
            b[kPoolOff + 6] = 'Z';
            check(table.load_dat(b) == 0,
                  "non-canonical reading byte is rejected");
        }
        {
            std::string b = valid;
            // The last pool string is "de\0"; removing its terminator makes
            // pool_string run past the pool limit.
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
