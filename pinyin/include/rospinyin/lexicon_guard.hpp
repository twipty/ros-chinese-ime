// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// M4 S2 / AC2 — the Windows-free half of "the word list is mandatory".
//
// dat_source_win.cpp owns the CreateFileW/ReadFile part, which only compiles
// on Windows and therefore cannot be host-tested. The *decision* — does this
// byte buffer constitute the pinned word list? — is a pure function here so
// all four rejection states are decidable in ctest on Linux. The Windows side
// reads the file and hands the bytes to classify(); only Ok continues into
// CodeTable::load_lexicon_dat().
//
// SHA-256 is copied from tools/install-rospinyin/payload_blob.hpp (the same
// algorithm, not a new dependency). Both implementations are pinned to the
// standard "abc" vector in their respective tests, so they cannot drift into
// certifying each other. Kept header-local because the pinyin public include
// tree must stay independent of the installer tool directory.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace rospinyin {
namespace lexicon {

enum class LexiconStatus : int {
    Ok = 0,
    Missing = 1,
    Truncated = 2,
    Corrupt = 3,
    HashMismatch = 4,
};

struct LexiconPolicy {
    std::size_t expected_size;      // pinned byte length
    const char* expected_sha256;    // 64 lowercase hex chars
    const char* magic;              // 8 bytes, e.g. "ROSPYDAT"
    std::uint32_t version;          // dat format version, little-endian
};

// The one production policy. Defined exactly once; check_m1.py and the tests
// pin every field so a silent retarget of the shipped word list is a failure.
inline constexpr LexiconPolicy kPolicy{
    6530601u,
    "a89ec5e500c47c121687e9789fade9b8187c1ec2b69d25d5e82f1a680cfc1c6a",
    "ROSPYDAT",
    2u,
};

namespace detail {

inline std::uint32_t rotr32(std::uint32_t value, unsigned bits) noexcept {
    return (value >> bits) | (value << (32u - bits));
}

inline void sha256_transform(std::uint32_t h[8],
                             const std::uint8_t block[64]) noexcept {
    static constexpr std::uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcFu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
    std::uint32_t w[64] = {};
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = (static_cast<std::uint32_t>(block[4 * i]) << 24) |
               (static_cast<std::uint32_t>(block[4 * i + 1]) << 16) |
               (static_cast<std::uint32_t>(block[4 * i + 2]) << 8) |
               static_cast<std::uint32_t>(block[4 * i + 3]);
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr32(w[i - 15], 7) ^
                                 rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr32(w[i - 2], 17) ^
                                 rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = hh + s1 + ch + k[i] + w[i];
        const std::uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = s0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

}  // namespace detail

inline std::string sha256_hex(const std::uint8_t* data,
                              std::size_t size) {
    std::uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                          0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    const std::uint64_t bit_length = static_cast<std::uint64_t>(size) * 8u;
    std::size_t pos = 0;
    while (size - pos >= 64u) {
        detail::sha256_transform(h, data + pos);
        pos += 64u;
    }
    std::uint8_t block[64] = {};
    const std::size_t remaining = size - pos;
    for (std::size_t i = 0; i < remaining; ++i) block[i] = data[pos + i];
    block[remaining] = 0x80u;
    if (remaining >= 56u) {
        detail::sha256_transform(h, block);
        for (std::size_t i = 0; i < 64; ++i) block[i] = 0;
    }
    for (std::size_t i = 0; i < 8; ++i) {
        block[56 + i] = static_cast<std::uint8_t>(
            (bit_length >> (56u - 8u * i)) & 0xFFu);
    }
    detail::sha256_transform(h, block);
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.resize(64);
    for (std::size_t i = 0; i < 8; ++i) {
        const std::uint32_t word = h[i];
        text[8 * i]     = digits[(word >> 28) & 0x0Fu];
        text[8 * i + 1] = digits[(word >> 24) & 0x0Fu];
        text[8 * i + 2] = digits[(word >> 20) & 0x0Fu];
        text[8 * i + 3] = digits[(word >> 16) & 0x0Fu];
        text[8 * i + 4] = digits[(word >> 12) & 0x0Fu];
        text[8 * i + 5] = digits[(word >> 8) & 0x0Fu];
        text[8 * i + 6] = digits[(word >> 4) & 0x0Fu];
        text[8 * i + 7] = digits[word & 0x0Fu];
    }
    return text;
}

// Classify a word-list byte buffer against a policy. Decision order is stable
// and testable: null/empty -> Missing; short -> Truncated; over-long or a bad
// magic/version -> Corrupt; intact header but wrong digest -> HashMismatch;
// otherwise Ok. Callers must treat only Ok as loadable.
inline LexiconStatus classify(const std::uint8_t* data, std::size_t size,
                              const LexiconPolicy& policy = kPolicy) {
    if (data == nullptr || size == 0) return LexiconStatus::Missing;
    // The dat header is 80 bytes; anything shorter cannot even declare a v2
    // table. Keep an explicit floor so a synthetic policy cannot read past a
    // tiny buffer while probing magic/version.
    if (size < 12) return LexiconStatus::Truncated;
    if (size < policy.expected_size) return LexiconStatus::Truncated;
    if (size > policy.expected_size) return LexiconStatus::Corrupt;
    if (std::memcmp(data, policy.magic, 8) != 0) return LexiconStatus::Corrupt;
    const std::uint32_t version =
        static_cast<std::uint32_t>(data[8]) |
        (static_cast<std::uint32_t>(data[9]) << 8) |
        (static_cast<std::uint32_t>(data[10]) << 16) |
        (static_cast<std::uint32_t>(data[11]) << 24);
    if (version != policy.version) return LexiconStatus::Corrupt;
    if (sha256_hex(data, size) != policy.expected_sha256) {
        return LexiconStatus::HashMismatch;
    }
    return LexiconStatus::Ok;
}

}  // namespace lexicon
}  // namespace rospinyin
