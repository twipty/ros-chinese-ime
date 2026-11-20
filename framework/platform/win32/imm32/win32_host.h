// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// The platform half of the key policy (X2-a). imfw::Host is the port through
// which the *portable* policy (framework/src/host_key.cpp) does the four
// things it cannot do by itself: read/write the Chinese/English flag, put text
// on screen, read/flip the session's glyph mode, and write the trace channel.
// This class is the only implementation of that port.

#pragma once

#include "imm32_abi.h"

#include <imfw/host.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace imfw {
namespace win32 {

class Win32Host final : public imfw::Host {
public:
    explicit Win32Host(rpimm::InputContext hIMC) : hIMC_(hIMC) {}

    // Chinese/English state, owned by the IME (see AlignOpenStatus).
    bool is_open() override;
    void set_open(bool open) override;

    void show(const std::string& composition,
              const std::vector<std::string>& segments,
              const imfw::CandidatePager& pager) override;
    void hide() override;
    bool show_all_glyphs() const override;
    void toggle_glyph_mode() override;

    void on_feed(char32_t input, const imfw::Result& result) override;
    void on_candidates_filtered(std::size_t dropped, std::size_t total,
                                bool kept_all) override;

private:
    rpimm::InputContext hIMC_;
};

}  // namespace win32
}  // namespace imfw
