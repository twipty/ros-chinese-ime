/* dat_probe.c - read-only inspector for the guest-side rospinyin.dat.
 * Prints the v2 header and the zhong syllable segment's tier breakdown so we
 * can tell a graded 44k table from a tiny fallback table without trusting the
 * file name or size. Static i686 build; no writes to the system.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static unsigned int rd32(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

int main(void) {
    char path[MAX_PATH];
    unsigned char* b;
    long sz;
    FILE* f;
    unsigned int ver, flags, nch, nsyl, char_off, syl_off, pool_off;
    unsigned int i;

    GetSystemDirectoryA(path, MAX_PATH);
    strcat(path, "\\rospinyin.dat");
    f = fopen(path, "rb");
    if (!f) { printf("dat OPEN FAIL %s\n", path); return 1; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    b = (unsigned char*)malloc((size_t)sz);
    if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) {
        printf("dat READ FAIL size=%ld\n", sz); return 1;
    }
    fclose(f);

    ver = rd32(b + 8); flags = rd32(b + 12);
    nch = rd32(b + 16); nsyl = rd32(b + 20);
    char_off = rd32(b + 32); syl_off = rd32(b + 36);
    pool_off = rd32(b + 48);
    printf("dat size=%ld magic=%.8s ver=%u flags=%u nch=%u nsyl=%u\n",
           sz, (const char*)b, ver, flags, nch, nsyl);
    printf("offs char=%u syl=%u pool=%u\n", char_off, syl_off, pool_off);
    if (ver != 2 || nch == 0 || nsyl == 0 || pool_off >= (unsigned int)sz) {
        printf("dat HEADER BAD -> loader would PARSE_FAIL (empty table)\n");
        return 2;
    }

    for (i = 0; i < nsyl; ++i) {
        const unsigned char* sr = b + syl_off + i * 16;
        unsigned int name = rd32(sr), first = rd32(sr + 4), cnt = rd32(sr + 8);
        const char* s;
        unsigned int j, t0 = 0, t1 = 0;
        if (name >= (unsigned int)sz || name >= pool_off) continue;
        s = (const char*)(b + pool_off + name);
        if (strcmp(s, "zhong") != 0) continue;
        for (j = 0; j < cnt; ++j) {
            const unsigned char* e = b + char_off + (first + j) * 24;
            unsigned int cp = rd32(e), rank = rd32(e + 4), fl = rd32(e + 20);
            if ((fl >> 1) & 1) t1++;
            else {
                t0++;
                if (t0 <= 10) printf("  tier0 cp=U+%04X rank=%u\n", cp, rank);
            }
        }
        printf("zhong first=%u cnt=%u tier0=%u tier1=%u\n", first, cnt, t0, t1);
        if (t0 == 0) printf("NO VISIBLE CANDIDATE -> empty candidate window\n");
        return 0;
    }
    printf("zhong segment NOT FOUND -> wrong table\n");
    return 3;
}
