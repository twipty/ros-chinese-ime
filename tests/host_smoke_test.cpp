// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// X2-b: the host smoke test.
//
// host_key_test.cpp pins decisions one at a time. This file pins the
// *sequence*: one fixed run of keys, one exact list of calls the host
// receives. That is the regression net the rest of X2 leans on —
//
//   * X2-c moves the decision into route_key/Intent and promises the call
//     sequence is byte-for-byte unchanged;
//   * X2-d splits the god object into translation units and promises the same.
//
// Without this file both promises are checked by reading a diff. With it, a
// reordering of show/hide, a missing refresh, or a commit that goes out twice
// fails a test instead of a user's typing session.
//
// The engine is *scripted* on purpose: the assertions must be about the host,
// so the engine's answers are constants. It also records what it was asked to
// do, which is how "the commit reached the engine exactly once" is checked.
//
// No <windows.h>, no IMM32, no GUI.

#include <imfw/host.hpp>
#include <imfw/scheme.hpp>

#include <cstdio>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "host smoke test failed: %s\n", what);
        ++failures;
    }
}

// UTF-8 so the call log is readable ("commit:钟" rather than "commit:1").
std::string utf8_of(const std::u32string& text) {
    std::string out;
    for (const char32_t cp : text) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

// ---- scripted engine ----------------------------------------------------
// Every operation answers from a queue. An empty queue is a *test failure*,
// not a default: it means the host asked the engine for something the script
// did not expect (a doubled feed, a stray backspace).
class ScriptedEngine final : public imfw::Engine {
public:
    std::deque<imfw::Result> feed_script;
    std::deque<imfw::Result> backspace_script;
    std::deque<imfw::Result> commit_script;
    std::vector<std::string> ops;
    bool underrun = false;

    imfw::Result feed(char32_t codepoint) override {
        ops.push_back(std::string("feed:") + static_cast<char>(codepoint));
        return take(feed_script);
    }

    imfw::Result backspace() override {
        ops.push_back("backspace");
        return take(backspace_script);
    }

    imfw::Result cancel() override {
        ops.push_back("cancel");
        return imfw::Result{};
    }

    imfw::Result commit_text(const std::u32string& text) override {
        ops.push_back("commit:" + utf8_of(text));
        return take(commit_script);
    }

    void on_select(bool) override {}

private:
    imfw::Result take(std::deque<imfw::Result>& queue) {
        if (queue.empty()) {
            underrun = true;
            return imfw::Result{};
        }
        imfw::Result result = queue.front();
        queue.pop_front();
        return result;
    }
};

// ---- fake scheme --------------------------------------------------------
// Letters in, digits/space select, '-'/'=' page: the imfw::Scheme defaults,
// which is what the framework actually relies on (G4 says a scheme may
// override them, not that it must).
class SmokeScheme final : public imfw::Scheme {
public:
    const wchar_t* ui_server_class() const override { return L"SmokeUISrv"; }
    const wchar_t* composition_class() const override { return L"SmokeComp"; }
    const wchar_t* candidate_class() const override { return L"SmokeCand"; }
    const wchar_t* toolbar_class() const override { return L"SmokeBar"; }
    const wchar_t* toolbar_label() const override { return L"smoke"; }

    char32_t input_char(std::uint32_t vk) const override {
        if (vk >= 'A' && vk <= 'Z') return static_cast<char32_t>('a' + (vk - 'A'));
        return 0;
    }
    bool is_input_char(char32_t ch) const override {
        return ch >= U'a' && ch <= U'z';
    }

    std::unique_ptr<imfw::Engine> create_engine() const override {
        return std::make_unique<ScriptedEngine>();
    }
};

SmokeScheme g_scheme;

// ---- recording host -----------------------------------------------------
// The log *is* the deliverable of this test: one line per observable call, in
// order, with the arguments that decide what the user sees.
class RecordingHost final : public imfw::Host {
public:
    bool open = true;
    std::vector<std::string> log;
    std::vector<char32_t> unrenderable;
    int filter_calls = 0;
    std::size_t filtered_dropped = 0;
    std::size_t filtered_total = 0;
    bool filtered_kept_all = true;

    bool is_open() override { return open; }

    void set_open(bool value) override {
        open = value;
        log.push_back(value ? "set_open(1)" : "set_open(0)");
    }

    void show(const std::string& composition,
              const std::vector<std::string>& segments,
              const imfw::CandidatePager& pager) override {
        std::string line = "show comp=" + composition + " segs=";
        for (const auto& segment : segments) line += "[" + segment + "]";
        line += " cands=" + std::to_string(pager.total());
        line += " page=" + std::to_string(pager.page() + 1) + "/" +
                std::to_string(pager.page_count());
        log.push_back(std::move(line));
    }

    void hide() override { log.push_back("hide"); }

    bool can_render(char32_t codepoint) override {
        for (const char32_t blocked : unrenderable) {
            if (blocked == codepoint) return false;
        }
        return true;
    }

    void on_candidates_filtered(std::size_t dropped, std::size_t total,
                                bool kept_all) override {
        ++filter_calls;
        filtered_dropped = dropped;
        filtered_total = total;
        filtered_kept_all = kept_all;
    }
};

// ---- event builders (exactly what the Win32 adapter produces) -----------
imfw::KeyEvent letter_event(unsigned vk) {
    imfw::KeyEvent event;
    event.vk = vk;
    return event;
}

imfw::KeyEvent char_event(char32_t ch) {
    imfw::KeyEvent event;
    event.vk = static_cast<std::uint32_t>(ch);
    event.ch = ch;
    return event;
}

imfw::KeyEvent kind_event(imfw::KeyKind kind) {
    imfw::KeyEvent event;
    event.kind = kind;
    return event;
}

imfw::KeyEvent shift_event(bool key_up) {
    imfw::KeyEvent event = kind_event(imfw::KeyKind::Shift);
    event.vk = 0x10;
    event.key_up = key_up;
    return event;
}

// ---- script helpers -----------------------------------------------------
imfw::Candidate candidate(const char32_t* text, int score) {
    imfw::Candidate out;
    out.text = text;
    out.score = score;
    return out;
}

imfw::Result absorb(std::string composition, std::vector<std::string> segments,
                    std::vector<imfw::Candidate> candidates = {}) {
    imfw::Result result;
    result.action = imfw::Action::Absorb;
    result.composition = std::move(composition);
    result.segments = std::move(segments);
    result.candidates = std::move(candidates);
    return result;
}

imfw::Result committed(std::u32string text, std::string composition,
                       std::vector<std::string> segments,
                       std::vector<imfw::Candidate> candidates) {
    imfw::Result result;
    result.action = imfw::Action::Commit;
    result.commit_text = std::move(text);
    result.composition = std::move(composition);
    result.segments = std::move(segments);
    result.candidates = std::move(candidates);
    return result;
}

const std::vector<imfw::Candidate>& zhong_candidates() {
    static const std::vector<imfw::Candidate> list = {
        candidate(U"中", 999993), candidate(U"钟", 999500),
        candidate(U"种", 999499)};
    return list;
}

std::vector<imfw::Candidate> many_candidates(int count) {
    std::vector<imfw::Candidate> list;
    for (int i = 0; i < count; ++i) list.push_back(candidate(U"候", 1000 - i));
    return list;
}

// Compare two logs and print the first difference: "the sequence changed" is
// useless without knowing where.
void check_log(const std::vector<std::string>& actual,
               const std::vector<std::string>& expected, const char* what) {
    const std::size_t common = actual.size() < expected.size() ? actual.size()
                                                              : expected.size();
    for (std::size_t i = 0; i < common; ++i) {
        if (actual[i] == expected[i]) continue;
        std::fprintf(stderr,
                     "host smoke test failed: %s\n  first difference at call %zu\n"
                     "  expected: %s\n  actual:   %s\n",
                     what, i, expected[i].c_str(), actual[i].c_str());
        ++failures;
        return;
    }
    if (actual.size() == expected.size()) return;
    std::fprintf(stderr,
                 "host smoke test failed: %s\n  call count %zu, expected %zu\n",
                 what, actual.size(), expected.size());
    if (actual.size() > common) {
        std::fprintf(stderr, "  first extra: %s\n", actual[common].c_str());
    } else {
        std::fprintf(stderr, "  first missing: %s\n", expected[common].c_str());
    }
    ++failures;
}

void check_ops(const std::vector<std::string>& actual,
               const std::vector<std::string>& expected) {
    std::string actual_joined;
    std::string expected_joined;
    for (const auto& op : actual) actual_joined += op + " ";
    for (const auto& op : expected) expected_joined += op + " ";
    if (actual_joined == expected_joined) return;
    std::fprintf(stderr,
                 "host smoke test failed: engine call log\n  expected: %s\n  actual:   %s\n",
                 expected_joined.c_str(), actual_joined.c_str());
    ++failures;
}

}  // namespace

// The binary plays the assembling module: the registry needs a factory, and
// nothing here links a real IME.
namespace imfw {

Scheme* scheme_factory() { return &g_scheme; }

}  // namespace imfw

int main() {
    // ------------------------------------------------------------------
    // The fixed sequence: zhongguo -> 2 -> Backspace, Backspace -> Esc ->
    // lone Shift. Five segments, one keyboard session.
    // ------------------------------------------------------------------
    {
        imfw::ImeSessionState session;
        auto engine = std::make_unique<ScriptedEngine>();
        ScriptedEngine* script = engine.get();
        session.engine = std::move(engine);

        // One result per keystroke, mirroring the pinyin engine's shape:
        // the candidate list belongs to the first syllable and survives a
        // half-typed next one.
        script->feed_script = {
            absorb("z", {"z"}),
            absorb("zh", {"zh"}),
            absorb("zho", {"zho"}),
            absorb("zhon", {"zhon"}),
            absorb("zhong", {"zhong"}, zhong_candidates()),
            absorb("zhongg", {"zhong"}, zhong_candidates()),
            absorb("zhonggu", {"zhong"}, zhong_candidates()),
            absorb("zhongguo", {"zhong", "guo"}, zhong_candidates()),
        };
        // '2' picks 钟; the unconverted tail stays in the composition.
        script->commit_script = {
            committed(U"钟", "guo", {"guo"},
                      {candidate(U"国", 999992), candidate(U"果", 999800)}),
        };
        script->backspace_script = {
            absorb("gu", {"gu"}, {candidate(U"果", 999800)}),
            absorb("g", {"g"}),
        };

        RecordingHost host;

        // The driver is what IMM32 does: asks ownership, and calls the worker
        // only for keys it claimed. Reproducing that gate here is half the
        // point — a refactor that starts calling the worker for unclaimed
        // keys would otherwise look harmless.
        struct Event {
            imfw::KeyEvent event;
            bool claimed;
        };
        const Event sequence[] = {
            {letter_event('Z'), true},   {letter_event('H'), true},
            {letter_event('O'), true},   {letter_event('N'), true},
            {letter_event('G'), true},   {letter_event('G'), true},
            {letter_event('U'), true},   {letter_event('O'), true},
            {char_event(U'2'), true},
            {kind_event(imfw::KeyKind::Backspace), true},
            {kind_event(imfw::KeyKind::Backspace), true},
            {kind_event(imfw::KeyKind::Escape), true},
            {shift_event(false), false},
            {shift_event(true), false},
        };

        std::vector<std::u32string> commits;
        for (const Event& step : sequence) {
            const bool claimed = imfw::wants_key(session, step.event, host);
            check(claimed == step.claimed,
                  step.claimed ? "a key that belongs to the IME was not claimed"
                               : "a key that belongs to the application was claimed");
            if (!claimed) continue;
            const imfw::KeyResult result =
                imfw::handle_key(session, step.event, host);
            if (!result.commit.empty()) commits.push_back(result.commit);
        }

        check_log(host.log,
                  {
                      "show comp=z segs=[z] cands=0 page=1/0",
                      "show comp=zh segs=[zh] cands=0 page=1/0",
                      "show comp=zho segs=[zho] cands=0 page=1/0",
                      "show comp=zhon segs=[zhon] cands=0 page=1/0",
                      "show comp=zhong segs=[zhong] cands=3 page=1/1",
                      "show comp=zhongg segs=[zhong] cands=3 page=1/1",
                      "show comp=zhonggu segs=[zhong] cands=3 page=1/1",
                      "show comp=zhongguo segs=[zhong][guo] cands=3 page=1/1",
                      // 钟 committed: the tail is offered next.
                      "show comp=guo segs=[guo] cands=2 page=1/1",
                      "show comp=gu segs=[gu] cands=1 page=1/1",
                      // Composition is still non-empty, so the host still
                      // refreshes; the candidate window hides itself on an
                      // empty pager. Pinned because it is the exact shape of
                      // the "candidate window blinks" report.
                      "show comp=g segs=[g] cands=0 page=1/0",
                      // Esc: cancel the engine, then close both windows.
                      "hide",
                      // Lone Shift: closed = English.
                      "set_open(0)",
                  },
                  "the host calls for zhongguo/2/backspace/escape/shift");

        check_ops(script->ops,
                  {"feed:z", "feed:h", "feed:o", "feed:n", "feed:g", "feed:g",
                   "feed:u", "feed:o", "commit:钟", "backspace", "backspace",
                   "cancel"});
        check(!script->underrun, "the host did not over-call the engine");

        check(commits.size() == 1, "exactly one commit went out");
        check(!commits.empty() && commits[0] == U"钟", "the commit is 钟");

        check(!host.open, "the lone Shift closed the IME");

        // After the composition is gone and the IME is closed, the keys belong
        // to the application again -- and must NOT reach the engine.
        check(!imfw::wants_key(session, char_event(U'1'), host),
              "a digit with no composition passes through");
        check(!imfw::wants_key(session, letter_event('A'), host),
              "a letter in English mode passes through");
        check(script->ops.size() == 12,
              "unclaimed keys never reach the engine");
        check(host.log.size() == 13, "unclaimed keys produce no host calls");
    }

    // ------------------------------------------------------------------
    // X2-w3: paging survives typing. The pager is re-fed on every letter,
    // but replace() must keep the page; a shorter new list clamps instead of
    // producing an out-of-range page or silently jumping to page 1.
    // ------------------------------------------------------------------
    {
        imfw::ImeSessionState session;
        auto engine = std::make_unique<ScriptedEngine>();
        ScriptedEngine* script = engine.get();
        session.engine = std::move(engine);
        script->feed_script = {
            absorb("a", {"a"}, many_candidates(20)),
            absorb("ab", {"ab"}, many_candidates(20)),
            absorb("abc", {"abc"}, many_candidates(5)),
        };

        RecordingHost host;
        const imfw::KeyEvent steps[] = {
            letter_event('A'), char_event(U'='), letter_event('B'),
            letter_event('C'),
        };
        for (const imfw::KeyEvent& event : steps) {
            const bool claimed = imfw::wants_key(session, event, host);
            check(claimed, "paging/typing keys belong to the open IME");
            if (claimed) imfw::handle_key(session, event, host);
        }

        check_log(host.log,
                  {
                      "show comp=a segs=[a] cands=20 page=1/3",
                      "show comp=a segs=[a] cands=20 page=2/3",
                      "show comp=ab segs=[ab] cands=20 page=2/3",
                      "show comp=abc segs=[abc] cands=5 page=1/1",
                  },
                  "typing keeps the page, a shorter list clamps it");
        check_ops(script->ops, {"feed:a", "feed:b", "feed:c"});
        check(!script->underrun, "the page key did not call the engine");
        check(session.pager.page() == 0, "a one-page list clamps to page zero");
    }

    // ------------------------------------------------------------------
    // P1-2 inside the smoke: the host drops candidates the font cannot draw,
    // and keeps the list anyway when it would drop too many (a font problem
    // is not a candidate problem).
    // ------------------------------------------------------------------
    {
        imfw::ImeSessionState session;
        auto engine = std::make_unique<ScriptedEngine>();
        ScriptedEngine* script = engine.get();
        session.engine = std::move(engine);
        script->feed_script = {absorb("a", {"a"},
                                      {candidate(U"甲", 1), candidate(U"龘", 0)})};

        RecordingHost host;
        host.unrenderable = {U'龘'};

        check(imfw::wants_key(session, letter_event('A'), host),
              "the letter is claimed");
        imfw::handle_key(session, letter_event('A'), host);

        check(host.filter_calls == 1, "the filter reported once");
        check(host.filtered_dropped == 1 && host.filtered_total == 2 &&
                  !host.filtered_kept_all,
              "one undrawable candidate is dropped");
        check(session.pager.total() == 1, "one candidate survives the filter");

        // Two of three undrawable: more than half gone means the font has no
        // CJK at all, so the list is kept (fails open).
        host.unrenderable = {U'龘', U'乙'};
        script->feed_script = {absorb("ab", {"ab"},
                                      {candidate(U"甲", 1), candidate(U"龘", 0),
                                       candidate(U"乙", 0)})};
        imfw::handle_key(session, letter_event('B'), host);
        check(host.filtered_kept_all, "a mostly-undrawable list is kept");
        check(session.pager.total() == 3, "no candidate is lost to a font bug");
    }

    if (failures != 0) {
        std::fprintf(stderr, "%d host smoke test(s) failed\n", failures);
        return 1;
    }
    std::printf("RosPinyin host smoke OK (zhongguo/2/backspace/escape/shift)\n");
    return 0;
}
