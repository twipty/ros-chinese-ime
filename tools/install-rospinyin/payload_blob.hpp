// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Verification layer for the compressed payloads embedded in the GUI
// installer. The actual inflate implementation (miniz in the Win32 adapter)
// is injected, so length / CRC / consumed-byte checks are host-testable
// without a Windows toolchain or a new build dependency.

#ifndef ROSPINYIN_INSTALLER_PAYLOAD_BLOB_HPP
#define ROSPINYIN_INSTALLER_PAYLOAD_BLOB_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rospinyin {
namespace installer {

enum class PayloadError : int {
    Ok = 0,
    InflateFailed = 1,
    SizeMismatch = 2,
    CrcMismatch = 3,
    TrailingData = 4,
};

struct InflateResult {
    PayloadError status;
    std::size_t bytes_consumed;
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

inline std::array<std::uint8_t, 32> sha256(const std::uint8_t* data,
                                           std::size_t size) noexcept {
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

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t i = 0; i < 8; ++i) {
        digest[4 * i] = static_cast<std::uint8_t>(h[i] >> 24);
        digest[4 * i + 1] = static_cast<std::uint8_t>(h[i] >> 16);
        digest[4 * i + 2] = static_cast<std::uint8_t>(h[i] >> 8);
        digest[4 * i + 3] = static_cast<std::uint8_t>(h[i]);
    }
    return digest;
}

inline std::string sha256_hex(const std::uint8_t* data,
                              std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    const std::array<std::uint8_t, 32> digest = sha256(data, size);
    std::string text;
    text.resize(64);
    for (std::size_t i = 0; i < digest.size(); ++i) {
        text[2 * i] = digits[digest[i] >> 4];
        text[2 * i + 1] = digits[digest[i] & 0x0Fu];
    }
    return text;
}

inline std::uint32_t crc32(const std::uint8_t* data, std::size_t size) noexcept {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= static_cast<std::uint32_t>(data[i]);
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask =
                0u - static_cast<std::uint32_t>(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

// `inflate(compressed, expected_size, output)` must return its status and the
// number of compressed bytes it actually consumed. Trailing bytes are a hard
// error: a tampered resource must not be silently accepted.
template <typename Inflate>
inline PayloadError decode_payload(const std::vector<std::uint8_t>& compressed,
                                   std::size_t expected_size,
                                   std::uint32_t expected_crc,
                                   std::vector<std::uint8_t>& output,
                                   Inflate inflate) {
    output.clear();
    const InflateResult result =
        inflate(compressed, expected_size, output);
    if (result.status != PayloadError::Ok) return result.status;
    if (result.bytes_consumed != compressed.size()) {
        return PayloadError::TrailingData;
    }
    if (output.size() != expected_size) return PayloadError::SizeMismatch;
    if (crc32(output.data(), output.size()) != expected_crc) {
        return PayloadError::CrcMismatch;
    }
    return PayloadError::Ok;
}

}  // namespace installer
}  // namespace rospinyin

#endif  // ROSPINYIN_INSTALLER_PAYLOAD_BLOB_HPP
