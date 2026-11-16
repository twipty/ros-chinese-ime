// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Host-side counterpart of WBS 2.4: the M1 session must pass every key
// through, keep no composition state, and survive repeated layout switches.
// The in-VM 50-switch stress test is still required before closing M1.

#include <imfw/engine.hpp>
#include <imfw/session.hpp>
#include <rospinyin/version.hpp>
#include <cstdio>
#include <cstring>

namespace {

bool is_empty_result(const imfw::Result& r) {
    return r.action == imfw::Action::Passthrough &&
           r.composition.empty() && r.candidates.empty() &&
           r.commit_text.empty();
}

int fail(const char* what) {
    std::fprintf(stderr, "session test failed: %s\n", what);
    return 1;
}

}  // namespace

int main() {
    imfw::PassthroughSession session;

    if (session.active()) {
        return fail("session must start deselected");
    }
    if (!is_empty_result(session.feed(U'a'))) {
        return fail("feed before select must be a no-op");
    }

    session.on_select(true);
    for (int i = 0; i < 1000; ++i) {
        const imfw::Result r = session.feed(static_cast<char32_t>(U'a' + i % 26));
        if (!is_empty_result(r)) {
            return fail("feed must always passthrough at M1");
        }
        if (!is_empty_result(session.backspace())) {
            return fail("backspace must be a no-op at M1");
        }
        if (!is_empty_result(session.cancel())) {
            return fail("cancel must be a no-op at M1");
        }
    }
    if (session.passthrough_keys() != 1000) {
        return fail("passthrough counter mismatch");
    }
    if (!session.composition().empty()) {
        return fail("composition must stay empty at M1");
    }

    // WBS 2.4.2: 50 select/deselect cycles, no state leakage or crashes.
    for (int i = 0; i < 50; ++i) {
        session.on_select(false);
        if (session.active()) return fail("deselect did not stick");
        session.on_select(true);
        if (!session.active()) return fail("select did not stick");
        if (!is_empty_result(session.feed(U'z'))) {
            return fail("switch cycle changed engine behaviour");
        }
    }
    if (session.passthrough_keys() != 1050) {
        return fail("state leaked across select cycles");
    }

    // engine_build_id() must be a real symbol from the linked engine library.
    if (std::strlen(imfw::engine_build_id()) == 0) {
        return fail("engine static library not linked");
    }

    std::printf("RosPinyin %s session OK (1000 keys, 50 switches)\n",
                ROSPINYIN_VERSION_STRING);
    return 0;
}
