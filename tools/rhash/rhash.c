/* RosPinyin - Simplified Chinese pinyin IME for ReactOS
 * SPDX-License-Identifier: MIT
 * See LICENSE and NOTICE.txt at the repository root.
 *
 * rhash - print the SHA-256 of a file, inside the guest.
 *
 * Why this exists: ReactOS 0.4.16 ships no sha256sum, no certutil and no
 * PowerShell, so the only check available in the guest used to be the byte
 * count from `dir`.  That check is not sound here: this repository has two
 * builds of rospinyin.ime that are both 940,888 bytes with different content
 * (and two more sharing 929,030).  Size cannot tell them apart, and the guest
 * clock drifts, so timestamps cannot either.  A hash is the only answer that
 * survives the trip.
 *
 * Deliberate constraints, same as the installer:
 *   - kernel32 + msvcrt only.  No CryptoAPI (its ReactOS implementation is
 *     incomplete), no third-party code, no C++ runtime.
 *   - SHA-256 implemented here, so the guest needs nothing installed.
 *   - No %llu: msvcrt does not guarantee it (see HANDOVER 11.4); sizes are
 *     printed as unsigned long, which covers any file ReactOS can hold.
 *
 * Usage:
 *   rhash.exe                          hash %SystemRoot%\system32\rospinyin.ime
 *                                      and rospinyin.dat
 *   rhash.exe <file> [file...]         hash the given files
 *
 * Output, one line per file:
 *   <64 hex chars>  <bytes>  <path>
 *
 * Exit code 0 = every file hashed, 1 = at least one failed, 2 = no system dir.
 */

#include <stdio.h>
#include <string.h>

typedef unsigned int u32;
typedef unsigned long long u64;

/* FIPS 180-4 constants. */
static const u32 K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

typedef struct {
    u32 h[8];
    u64 bits;
    unsigned char buf[64];
    unsigned n;
} sha256;

static u32 ror(u32 x, int n) { return (x >> n) | (x << (32 - n)); }

static void compress(sha256 *s, const unsigned char *p) {
    u32 w[64];
    u32 a, b, c, d, e, f, g, h;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((u32)p[i * 4] << 24) | ((u32)p[i * 4 + 1] << 16) |
               ((u32)p[i * 4 + 2] << 8) | (u32)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        u32 s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        u32 s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];

    for (i = 0; i < 64; i++) {
        u32 t1 = h + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) +
                 ((e & f) ^ (~e & g)) + K[i] + w[i];
        u32 t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) +
                 ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void sha_init(sha256 *s) {
    s->h[0] = 0x6a09e667u; s->h[1] = 0xbb67ae85u;
    s->h[2] = 0x3c6ef372u; s->h[3] = 0xa54ff53au;
    s->h[4] = 0x510e527fu; s->h[5] = 0x9b05688cu;
    s->h[6] = 0x1f83d9abu; s->h[7] = 0x5be0cd19u;
    s->bits = 0;
    s->n = 0;
}

static void sha_update(sha256 *s, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    s->bits += (u64)len * 8;
    while (len > 0) {
        unsigned take = 64 - s->n;
        if (take > len) take = (unsigned)len;
        memcpy(s->buf + s->n, p, take);
        s->n += take;
        p += take;
        len -= take;
        if (s->n == 64) {
            compress(s, s->buf);
            s->n = 0;
        }
    }
}

static void sha_final(sha256 *s, unsigned char out[32]) {
    unsigned char pad[72];
    unsigned padlen;
    u64 bits = s->bits;
    int i;

    padlen = (s->n < 56) ? (56 - s->n) : (120 - s->n);
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    for (i = 0; i < 8; i++) {
        pad[padlen + i] = (unsigned char)(bits >> (56 - i * 8));
    }
    sha_update(s, pad, padlen + 8);
    /* sha_update already counted the padding into s->bits; that is harmless
     * because nothing reads s->bits afterwards. */

    for (i = 0; i < 8; i++) {
        out[i * 4 + 0] = (unsigned char)(s->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(s->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(s->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(s->h[i]);
    }
}

#ifdef _WIN32

#include <windows.h>
#include <wchar.h>

static int hash_one(const wchar_t *path) {
    static unsigned char buf[65536];
    HANDLE h;
    sha256 s;
    unsigned char md[32];
    DWORD got = 0;
    unsigned long total = 0;
    char hex[65];
    char narrow[1024];
    int i;

    /* Share everything: the .ime may be mapped by the very process we are
     * trying to verify, and refusing to read it is the failure mode that made
     * people distrust this check in the first place. */
    h = CreateFileW(path, GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"[error] cannot open %ls (err=%lu)\n",
                 path, (unsigned long)GetLastError());
        return 1;
    }

    sha_init(&s);
    for (;;) {
        if (!ReadFile(h, buf, (DWORD)sizeof buf, &got, NULL)) {
            fwprintf(stderr, L"[error] read failed on %ls (err=%lu)\n",
                     path, (unsigned long)GetLastError());
            CloseHandle(h);
            return 1;
        }
        if (got == 0) break;
        sha_update(&s, buf, got);
        total += got;
    }
    CloseHandle(h);
    sha_final(&s, md);

    for (i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", md[i]);
    hex[64] = 0;

    if (!WideCharToMultiByte(CP_ACP, 0, path, -1, narrow, (int)sizeof narrow,
                             NULL, NULL)) {
        narrow[0] = 0;
    }
    printf("%s  %lu  %s\n", hex, total, narrow);
    return 0;
}

int wmain(int argc, wchar_t **argv) {
    int rc = 0;
    int i;

    if (argc > 1) {
        for (i = 1; i < argc; i++) rc |= hash_one(argv[i]);
        return rc ? 1 : 0;
    }

    {
        static const wchar_t *names[2] = {L"rospinyin.ime", L"rospinyin.dat"};
        wchar_t dir[MAX_PATH];
        wchar_t path[MAX_PATH];
        UINT n = GetSystemDirectoryW(dir, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) {
            fwprintf(stderr, L"[error] cannot resolve the system directory\n");
            return 2;
        }
        for (i = 0; i < 2; i++) {
            _snwprintf(path, MAX_PATH, L"%ls\\%ls", dir, names[i]);
            path[MAX_PATH - 1] = 0;
            rc |= hash_one(path);
        }
    }
    return rc ? 1 : 0;
}

#endif /* _WIN32 */

#if defined(RHASH_HOST_TEST) && !defined(_WIN32)
/* Host-side self test: same code path, POSIX I/O, compared against sha256sum.
 * Built only by the developer, never shipped. */
#include <stdlib.h>

int main(int argc, char **argv) {
    int rc = 0;
    int i;
    for (i = 1; i < argc; i++) {
        FILE *fp = fopen(argv[i], "rb");
        static unsigned char buf[65536];
        sha256 s;
        unsigned char md[32];
        size_t got;
        unsigned long total = 0;
        int k;
        if (!fp) { perror(argv[i]); rc = 1; continue; }
        sha_init(&s);
        while ((got = fread(buf, 1, sizeof buf, fp)) > 0) {
            sha_update(&s, buf, got);
            total += (unsigned long)got;
        }
        fclose(fp);
        sha_final(&s, md);
        for (k = 0; k < 32; k++) printf("%02x", md[k]);
        printf("  %lu  %s\n", total, argv[i]);
    }
    return rc;
}
#endif
