// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
#include <rospinyin/version.hpp>
#include <cstdio>
#include <cstring>

int main() {
    if (ROSPINYIN_VERSION_MAJOR != 0 || ROSPINYIN_VERSION_MINOR != 0) {
        std::fprintf(stderr, "unexpected version macros\n");
        return 1;
    }
    if (std::strlen(ROSPINYIN_VERSION_STRING) == 0) {
        std::fprintf(stderr, "empty version string\n");
        return 2;
    }
    std::printf("RosPinyin %s smoke OK\n", ROSPINYIN_VERSION_STRING);
    return 0;
}
