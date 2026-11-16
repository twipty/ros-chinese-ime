// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Assembly glue: hands the framework its Scheme. Everything else in
// rospinyin.ime is reusable framework code (framework/) plus the pinyin
// plugin (pinyin/). A second input method would define its own Scheme here
// and link the same framework.

#include <imfw/scheme.hpp>
#include <rospinyin/scheme.hpp>

namespace imfw {

Scheme& scheme_instance() {
    static rospinyin::PinyinScheme scheme;
    return scheme;
}

}  // namespace imfw
