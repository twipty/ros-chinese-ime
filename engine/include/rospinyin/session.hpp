// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// M1 passthrough session. Keeps no composition state, never consumes a key,
// and survives arbitrary select/deselect cycles. This is the host-testable
// counterpart of the IMM32 passthrough layer in ime/imm_entry.cpp.

#pragma once

#include <rospinyin/engine.hpp>
#include <cstddef>

namespace rospinyin {

class PassthroughSession final : public Engine {
public:
    Result feed(char32_t codepoint) override {
        (void)codepoint;
        if (active_) {
            ++passthrough_keys_;
        }
        return Result{};
    }

    Result backspace() override { return Result{}; }
    Result cancel() override { return Result{}; }

    void on_select(bool selected) override {
        active_ = selected;
    }

    bool active() const noexcept { return active_; }
    std::size_t passthrough_keys() const noexcept { return passthrough_keys_; }
    const std::string& composition() const noexcept { return empty_composition_; }

private:
    bool active_ = false;
    std::size_t passthrough_keys_ = 0;
    const std::string empty_composition_{};
};

}  // namespace rospinyin
