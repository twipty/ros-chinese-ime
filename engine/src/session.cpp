// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// The engine library is intentionally tiny at M1: its only compiled symbol
// is the build identifier. M2 adds the syllable table, segmentation and
// candidate generation here without changing engine.hpp's contract.

#include <rospinyin/engine.hpp>
#include <rospinyin/version.hpp>

namespace rospinyin {

const char* engine_build_id() noexcept {
    return ROSPINYIN_VERSION_STRING;
}

}  // namespace rospinyin
