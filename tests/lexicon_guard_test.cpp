// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// M4 S2 / AC2: the four rejection states of the bundled word list.
// `rospinyin::lexicon::classify()` is a Windows-free pure function so all four
// states are decidable on the host instead of only on a guest (the Windows
// file IO in dat_source_win.cpp cannot be compiled here). A tiny synthetic
// policy keeps the test independent of the 6.5 MB real asset.

#include <rospinyin/lexicon_guard.hpp>

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace guard = rospinyin::lexicon;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "lexicon guard test failed: %s\n", what);
        ++failures;
    }
}

std::vector<std::uint8_t> sample(std::size_t size,
                                 const char* magic = "ROSPYDAT",
                                 std::uint32_t version = 2) {
    std::vector<std::uint8_t> bytes(size, 0);
    if (size >= 8) std::memcpy(bytes.data(), magic, 8);
    if (size >= 12) std::memcpy(bytes.data() + 8, &version, 4);
    return bytes;
}

}  // namespace

int main() {
    // Pin against the same SHA-256 vector installer_logic_test uses, so the
    // two independent implementations cannot drift into self-certification.
    const char abc[] = "abc";
    check(guard::sha256_hex(reinterpret_cast<const std::uint8_t*>(abc), 3) ==
              "ba7816bf8f01cfea414140de5dae2223"
              "b00361a396177a9cb410ff61f20015ad",
          "SHA-256 matches the standard vector");

    // The production policy is pinned: a silent retarget of the shipped word
    // list would make the runtime guard verify the wrong bytes.
    check(guard::kPolicy.expected_size == 6530601u,
          "production policy pins the word-list size");
    check(std::string_view(guard::kPolicy.expected_sha256) ==
              "a89ec5e500c47c121687e9789fade9b8187c1ec2b69d25d5e82f1a680cfc1c6a",
          "production policy pins the word-list SHA-256");
    check(std::string_view(guard::kPolicy.magic, 8) == "ROSPYDAT",
          "production policy pins the pinyin magic");
    check(guard::kPolicy.version == 2u,
          "production policy pins the dat format version");

    // Four rejection states on a small synthetic policy.
    const auto good = sample(64);
    const std::string good_sha =
        guard::sha256_hex(good.data(), good.size());
    const guard::LexiconPolicy small{64, good_sha.c_str(), "ROSPYDAT", 2};

    check(guard::classify(nullptr, 0, small) == guard::LexiconStatus::Missing,
          "null bytes are Missing");
    check(guard::classify(good.data(), 0, small) ==
              guard::LexiconStatus::Missing,
          "zero-length bytes are Missing");
    check(guard::classify(good.data(), 63, small) ==
              guard::LexiconStatus::Truncated,
          "a short file is Truncated");
    check(guard::classify(good.data(), 64, small) ==
              guard::LexiconStatus::Ok,
          "a matching file is Ok");

    const auto shape = sample(64, "ROSSHDAT", 2);
    check(guard::classify(shape.data(), 64, small) ==
              guard::LexiconStatus::Corrupt,
          "the shape-table magic is Corrupt here");
    const auto future = sample(64, "ROSPYDAT", 3);
    check(guard::classify(future.data(), 64, small) ==
              guard::LexiconStatus::Corrupt,
          "an unknown dat version is Corrupt");
    const auto oversized = sample(65);
    check(guard::classify(oversized.data(), 65, small) ==
              guard::LexiconStatus::Corrupt,
          "a file larger than the pinned size is Corrupt");

    auto tampered = good;
    tampered[20] ^= 0xFFu;
    check(guard::classify(tampered.data(), 64, small) ==
              guard::LexiconStatus::HashMismatch,
          "a byte flip with intact magic is HashMismatch");

    if (failures != 0) {
        std::fprintf(stderr, "%d lexicon guard check(s) failed\n", failures);
        return 1;
    }
    std::printf("lexicon guard: all checks passed\n");
    return 0;
}
