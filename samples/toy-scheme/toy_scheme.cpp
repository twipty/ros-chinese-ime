// RosPinyin / toy scheme — SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.

#include "toy_scheme.hpp"

namespace toy {
namespace {

// The only place UTF-8 happens: the alphabet is ASCII, so one code point is
// one byte. A scheme with a non-ASCII alphabet must encode properly (pinyin
// does it through imfw::utf8); this one would rather stay obvious.
char ascii_of(char32_t cp) { return static_cast<char>(cp & 0x7F); }

bool is_letter(char32_t cp) { return cp >= U'a' && cp <= U'z'; }

}  // namespace

imfw::Result ToyEngine::snapshot(imfw::Action action) const {
    imfw::Result result;
    result.action = action;
    if (composition_.empty()) return result;

    result.composition.reserve(composition_.size());
    result.segments.reserve(composition_.size());
    for (char32_t cp : composition_) {
        result.composition.push_back(ascii_of(cp));
        result.segments.emplace_back(1, ascii_of(cp));
    }

    // Two candidates: the text as typed, and the text with a marker. Digit 1
    // and digit 2 therefore commit visibly different strings, which is the
    // whole of what this scheme has to prove.
    result.candidates.push_back({composition_, 100});
    std::u32string marked = composition_;
    marked.push_back(U'!');
    result.candidates.push_back({marked, 50});
    return result;
}

imfw::Result ToyEngine::feed(char32_t codepoint) {
    if (!is_letter(codepoint)) {
        return snapshot(imfw::Action::Passthrough);
    }
    composition_.push_back(codepoint);
    return snapshot(imfw::Action::Absorb);
}

imfw::Result ToyEngine::backspace() {
    if (composition_.empty()) return imfw::Result{};
    composition_.pop_back();
    return snapshot(imfw::Action::Absorb);
}

imfw::Result ToyEngine::cancel() {
    composition_.clear();
    return imfw::Result{};
}

// No IME-open state of its own: the framework owns Shift/open status, and a
// stateless engine has nothing to arm or disarm. Kept as an explicit no-op so
// the contract (idempotent, called repeatedly by IMM32) is visible here rather
// than inherited by silence.
void ToyEngine::on_select(bool selected) { (void)selected; }

char32_t ToyScheme::input_char(std::uint32_t vk) const {
    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<char32_t>('a' + (vk - 'A'));
    }
    return 0;
}

std::unique_ptr<imfw::Engine> ToyScheme::create_engine() const {
    return std::make_unique<ToyEngine>();
}

}  // namespace toy
