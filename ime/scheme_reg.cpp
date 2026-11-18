// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Assembly glue: tells the framework which scheme this .ime is. Everything
// else in rospinyin.ime is reusable framework code (framework/) plus the
// pinyin plugin (pinyin/). A second input method supplies its own
// scheme_factory() here and links the same framework — see
// ime/scheme_reg_toy.cpp and samples/toy-scheme/ for the worked example.
//
// Note what this file no longer does: it does not define the framework's
// query entry point (imfw::scheme_instance()). That symbol is framework-owned
// since X2-a0; the assembly only answers "which scheme", which is the half that
// differs per input method.

#include <imfw/scheme_registry.hpp>
#include <rospinyin/scheme.hpp>

namespace imfw {

Scheme* scheme_factory() {
    // Function-local static: built on the first query (IMM32's first key at
    // the earliest), never while the loader holds its lock.
    static rospinyin::PinyinScheme scheme;
    return &scheme;
}

}  // namespace imfw
