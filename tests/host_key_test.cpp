// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// X2-a: the key policy without the host.
//
// Before this file existed, process_key() had **zero** host-side coverage. The
// busiest code path in the IME — the one that decides whether a keystroke is
// selection, paging, backspace, escape or text — could only be exercised by
// typing into a ReactOS guest and reading C:\rospinyin-trace.log. It is 90
// lines of `if`; every bug in the M2 reports (doubled letters, frozen
// composition window, "extra / lost characters") was a bug in one of them.
//
// What is pinned here is the *policy*, not the plumbing:
//   * ownership: closed = English, Ctrl/Alt never consumed, key-up never
//     claimed, selection/paging keys only mid-composition;
//   * lone-Shift: Shift down + Shift up toggles, Shift down + letter does not;
//   * the four work branches: feed, selection, paging, backspace, escape;
//   * the two guards around candidate filtering (all dropped = font problem).
//
// The IME binary is not involved: no <windows.h>, no IMM32, no GUI. That is the
// point of the split — the same source compiles into the .ime and into this.

#include <imfw/host.hpp>
#include <imfw/scheme.hpp>

#include <cstdio>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "host key test failed: %s\n", what);
        ++failures;
    }
}

// ---- fake engine --------------------------------------------------------
// Composes letters and offers 11 candidates (two pages: 9 + 2), so selection
// and paging are both observable. commit_text() converts left to right like the
// pinyin engine: one candidate consumes one typed letter and leaves the rest in
// the composition, which is what makes the "keep composing" branch reachable.
class FakeEngine final : public imfw::Engine {
public:
    imfw::Result feed(char32_t codepoint) override {
        typed_.push_back(codepoint);
        return snapshot(imfw::Action::Absorb);
    }

    imfw::Result backspace() override {
        if (!typed_.empty()) typed_.pop_back();
        return snapshot(imfw::Action::Absorb);
    }

    imfw::Result cancel() override {
        typed_.clear();
        return imfw::Result{};
    }

    imfw::Result commit_text(const std::u32string& text) override {
        imfw::Result result;
        result.action = imfw::Action::Commit;
        result.commit_text = text;
        if (typed_.size() > 1) {
            typed_.erase(0, 1);
            result.composition = narrow(typed_);
            result.candidates = candidates();
        } else {
            typed_.clear();
        }
        return result;
    }

    void on_select(bool) override {}

    const std::u32string& typed() const { return typed_; }

private:
    static std::string narrow(const std::u32string& text) {
        std::string out;
        for (const char32_t cp : text) out.push_back(static_cast<char>(cp));
        return out;
    }

    std::vector<imfw::Candidate> candidates() const {
        std::vector<imfw::Candidate> out;
        out.reserve(11);
        for (int i = 0; i < 11; ++i) {
            imfw::Candidate candidate;
            candidate.text = typed_ + U"#" + static_cast<char32_t>(U'A' + i);
            candidate.score = 11 - i;
            out.push_back(candidate);
        }
        return out;
    }

    imfw::Result snapshot(imfw::Action action) const {
        imfw::Result result;
        result.action = action;
        result.composition = narrow(typed_);
        for (char32_t cp : typed_) result.segments.push_back(std::string(1, static_cast<char>(cp)));
        if (!typed_.empty()) result.candidates = candidates();
        return result;
    }

    std::u32string typed_;
};

// A scheme with a letter alphabet; digits select, `-`/`=` page (the defaults of
// imfw::Scheme, which is what the framework actually relies on).
class FakeScheme final : public imfw::Scheme {
public:
    const wchar_t* ui_server_class() const override { return L"FakeUISrv"; }
    const wchar_t* composition_class() const override { return L"FakeComp"; }
    const wchar_t* candidate_class() const override { return L"FakeCand"; }
    const wchar_t* toolbar_class() const override { return L"FakeBar"; }
    const wchar_t* toolbar_label() const override { return L"fake"; }

    char32_t input_char(std::uint32_t vk) const override {
        if (vk >= 'A' && vk <= 'Z') return static_cast<char32_t>('a' + (vk - 'A'));
        return 0;
    }
    bool is_input_char(char32_t cp) const override {
        return cp >= U'a' && cp <= U'z';
    }

    std::unique_ptr<imfw::Engine> create_engine() const override {
        return std::make_unique<FakeEngine>();
    }
};

FakeScheme g_scheme;

// ---- spy host -----------------------------------------------------------
class SpyHost final : public imfw::Host {
public:
    bool open = true;
    int show_calls = 0;
    int hide_calls = 0;
    int open_writes = 0;
    std::string last_composition;
    std::vector<char32_t> feeds;
    int filter_events = 0;
    std::size_t filter_dropped = 0;
    std::size_t filter_total = 0;
    bool filter_kept_all = false;
    std::vector<char32_t> unrenderable;

    bool is_open() override { return open; }
    void set_open(bool value) override {
        open = value;
        ++open_writes;
    }

    void show(const std::string& composition,
              const std::vector<std::string>& segments,
              const imfw::CandidatePager& pager) override {
        (void)segments;
        (void)pager;
        ++show_calls;
        last_composition = composition;
    }
    void hide() override { ++hide_calls; }

    bool can_render(char32_t codepoint) override {
        for (const char32_t blocked : unrenderable) {
            if (blocked == codepoint) return false;
        }
        return true;
    }

    void on_feed(char32_t input, const imfw::Result& result) override {
        (void)result;
        feeds.push_back(input);
    }
    void on_candidates_filtered(std::size_t dropped, std::size_t total,
                                bool kept_all) override {
        ++filter_events;
        filter_dropped = dropped;
        filter_total = total;
        filter_kept_all = kept_all;
    }
};

// ---- event builders (what the Win32 adapter produces) --------------------
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

imfw::KeyEvent kind_event(imfw::KeyKind kind, unsigned vk) {
    imfw::KeyEvent event;
    event.vk = vk;
    event.kind = kind;
    return event;
}

imfw::KeyEvent shift_event(bool key_up) {
    imfw::KeyEvent event = kind_event(imfw::KeyKind::Shift, 0x10);
    event.key_up = key_up;
    return event;
}

}  // namespace

// The test binary plays the assembling module: the registry needs a factory to
// fall back to, and nothing here links a real IME.
namespace imfw {

Scheme* scheme_factory() { return &g_scheme; }

}  // namespace imfw

int main() {
    {
        // ---- ownership: closed = English -------------------------------
        imfw::ImeSessionState session;
        session.engine = std::make_unique<FakeEngine>();
        SpyHost host;
        host.open = false;
        check(!imfw::wants_key(session, letter_event('A'), host),
              "a closed IME lets letters reach the application");
        host.open = true;
        check(imfw::wants_key(session, letter_event('A'), host),
              "an open IME claims its input alphabet");

        // Modifiers are never consumed (application shortcuts must work).
        imfw::KeyEvent ctrl = letter_event('A');
        ctrl.ctrl = true;
        check(!imfw::wants_key(session, ctrl, host), "Ctrl+letter is the application's");
        imfw::KeyEvent alt = letter_event('A');
        alt.alt = true;
        check(!imfw::wants_key(session, alt, host), "Alt+letter is the application's");

        // A key-up is never claimed: claiming it makes every letter arrive twice.
        imfw::KeyEvent up = letter_event('A');
        up.key_up = true;
        check(!imfw::wants_key(session, up, host), "a key-up is never claimed");

        // Nothing on screen: selection / paging keys belong to the application.
        check(!imfw::wants_key(session, char_event(U'1'), host), "digit with no composition passes through");
        check(!imfw::wants_key(session, char_event(U'='), host), "page key with no composition passes through");
        check(session.pager.empty(), "pager starts empty");

        // A key the framework neither translates nor the scheme accepts.
        check(!imfw::wants_key(session, letter_event(0x70 /* F1 */), host),
              "an unmanaged key is left to the application");
    }

    {
        // ---- lone Shift toggles; Shift+letter does not -----------------
        imfw::ImeSessionState session;
        session.engine = std::make_unique<FakeEngine>();
        SpyHost host;

        check(!imfw::wants_key(session, shift_event(false), host), "Shift-down is never claimed");
        check(session.shift_latched, "Shift-down latches");
        check(!imfw::wants_key(session, shift_event(true), host), "Shift-up is never claimed");
        check(!session.shift_latched, "Shift-up clears the latch");
        check(host.open_writes == 1 && !host.open, "a lone Shift closes the IME");
        check(host.open == false, "host open flag follows");

        // Latin: Shift + A must stay a capital A, not switch the language.
        imfw::ImeSessionState other;
        other.engine = std::make_unique<FakeEngine>();
        SpyHost other_host;
        check(!imfw::wants_key(other, shift_event(false), other_host), "shift-down again");
        check(imfw::wants_key(other, letter_event('A'), other_host),
              "the letter after Shift is still ours");
        check(!other.shift_latched, "a non-Shift key clears the latch");
        check(!imfw::wants_key(other, shift_event(true), other_host), "shift-up again");
        check(other_host.open_writes == 0, "Shift+A does not toggle the language");
    }

    {
        // ---- feed, select, page, backspace, escape ---------------------
        imfw::ImeSessionState session;
        auto engine = std::make_unique<FakeEngine>();
        FakeEngine* engine_ptr = engine.get();
        session.engine = std::move(engine);
        SpyHost host;

        imfw::KeyResult result = imfw::handle_key(session, letter_event('A'), host);
        check(result.commit.empty(), "feed inserts nothing");
        check(engine_ptr->typed() == U"a", "the engine saw the letter, lowercased");
        check(session.composing, "a non-empty composition is composing");
        check(host.show_calls == 1, "the composition window is refreshed");
        check(host.last_composition == "a", "the composition text reaches the host");
        check(host.feeds.size() == 1 && host.feeds[0] == U'a', "the feed is traced once");
        check(session.pager.total() == 11, "the candidate list is adopted");
        check(session.pager.page_count() == 2, "11 candidates are two pages");

        // Ownership follows the composition: the same keys that passed through
        // a moment ago are ours now.
        check(imfw::wants_key(session, char_event(U'1'), host),
              "a digit selects while candidates are on screen");
        check(imfw::wants_key(session, kind_event(imfw::KeyKind::Backspace, 0x08), host),
              "backspace is ours mid-composition");
        check(imfw::wants_key(session, kind_event(imfw::KeyKind::Escape, 0x1B), host),
              "escape is ours mid-composition");

        // Paging: page 1 -> page 2 moves, page 2 -> page 2 does not.
        const int shows_before_page = host.show_calls;
        imfw::handle_key(session, char_event(U'='), host);
        check(session.pager.page() == 1, "the page key moves to page 2");
        check(host.show_calls == shows_before_page + 1, "a real page move refreshes");
        imfw::handle_key(session, char_event(U'='), host);
        check(session.pager.page() == 1, "paging past the end stays put");
        check(host.show_calls == shows_before_page + 1, "a no-op page move does not refresh");
        imfw::handle_key(session, char_event(U'-'), host);
        check(session.pager.page() == 0, "the back page key returns to page 1");

        // Digit selection: candidate 1 on page 1 is typed + "#A".
        result = imfw::handle_key(session, char_event(U'1'), host);
        check(result.commit == U"a#A", "digit 1 commits the first candidate");
        check(engine_ptr->typed().empty(), "one typed letter was consumed");
        check(!session.composing, "nothing left to convert closes the session");
        check(host.hide_calls == 1, "the windows are hidden after the commit");

        // One more letter, then select with a two-letter composition: the
        // remainder stays composing (the pinyin left-to-right rule).
        imfw::handle_key(session, letter_event('A'), host);
        imfw::handle_key(session, letter_event('B'), host);
        check(engine_ptr->typed() == U"ab", "two letters are composing");
        result = imfw::handle_key(session, char_event(U'1'), host);
        check(result.commit == U"ab#A", "the first candidate of the new list is committed");
        check(session.composing, "the remainder keeps composing");
        check(engine_ptr->typed() == U"b", "one letter was consumed, one remains");
        check(host.show_calls > 0 && host.last_composition == "b",
              "the composition window shows the remainder");

        // Backspace down to empty hides the windows instead of drawing an
        // empty popup.
        const int hides_before = host.hide_calls;
        imfw::handle_key(session, kind_event(imfw::KeyKind::Backspace, 0x08), host);
        check(!session.composing, "backspace on the last letter ends the composition");
        check(host.hide_calls == hides_before + 1, "an empty composition hides the windows");

        // Escape cancels an in-flight composition.
        imfw::handle_key(session, letter_event('A'), host);
        imfw::handle_key(session, letter_event('B'), host);
        const int hides_before_esc = host.hide_calls;
        imfw::handle_key(session, kind_event(imfw::KeyKind::Escape, 0x1B), host);
        check(!session.composing, "escape ends the composition");
        check(engine_ptr->typed().empty(), "escape cancels the engine");
        check(host.hide_calls == hides_before_esc + 1, "escape hides the windows");

        // A non-input key mid-composition changes nothing.
        const std::size_t total_before = session.pager.total();
        result = imfw::handle_key(session, letter_event(0x70 /* F1 */), host);
        check(result.commit.empty(), "an unmanaged key commits nothing");
        check(session.pager.total() == total_before, "an unmanaged key does not touch the pager");
    }

    {
        // ---- candidate filtering (P1-2 guards) -------------------------
        imfw::ImeSessionState session;
        session.engine = std::make_unique<FakeEngine>();
        SpyHost host;

        // One letter whose candidates all carry '#'; marking '#' unrenderable
        // drops all 11 — that is the "font cannot draw CJK" guard, and the list
        // must survive untouched.
        host.unrenderable.push_back(U'#');
        imfw::handle_key(session, letter_event('A'), host);
        check(host.filter_events == 1, "the filter reports once");
        check(host.filter_kept_all, "dropping everything is treated as a font problem");
        check(host.filter_dropped == 11 && host.filter_total == 11, "all 11 were dropped");
        check(session.pager.total() == 11, "the list is kept when the font cannot draw CJK");

        // Exactly one candidate is undrawable: it is dropped, the rest stay.
        imfw::ImeSessionState one;
        one.engine = std::make_unique<FakeEngine>();
        SpyHost one_host;
        one_host.unrenderable.push_back(U'A');  // only "a#A" uses this letter
        imfw::handle_key(one, letter_event('A'), one_host);
        check(one_host.filter_events == 1, "a partial drop is reported");
        check(!one_host.filter_kept_all, "a partial drop is not a font problem");
        check(one_host.filter_dropped == 1, "exactly one candidate was dropped");
        check(one.pager.total() == 10, "the remaining candidates are kept");

        // More than half dropped is still a font problem (11 -> 5 is < 50%):
        // a font that draws 5 of 11 CJK candidates is not a font that should
        // silently shrink the list.
        imfw::ImeSessionState half;
        half.engine = std::make_unique<FakeEngine>();
        SpyHost half_host;
        for (char32_t letter = U'A'; letter <= U'F'; ++letter) {
            half_host.unrenderable.push_back(letter);
        }
        imfw::handle_key(half, letter_event('A'), half_host);
        check(half_host.filter_dropped == 6, "6 of 11 were dropped");
        check(half_host.filter_kept_all, "losing more than half is a font problem");
        check(half.pager.total() == 11, "the list is kept when most of it is undrawable");
    }

    if (failures == 0) {
        std::printf("host key policy OK\n");
        return 0;
    }
    return 1;
}
