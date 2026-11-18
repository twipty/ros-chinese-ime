// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Scheme injection seam (architecture audit 2026-11-18, A-P0-2).
//
// Before this header the framework reached its scheme through
// scheme_instance() alone — a contract whose *implementation* the assembling
// module had to supply at link time. A test, or a second input method in the
// same binary, therefore had exactly two options: link a different .cpp, or
// edit the framework. The audit called that "链接期硬接线": the plugin
// contract was real, the plugin *seam* was not.
//
// SchemeRegistry splits the two halves that were fused into that one symbol,
// because they have different lifetimes and different owners:
//
//   creation  scheme_factory(), defined by the assembling module
//             (ime/scheme_reg.cpp for pinyin, ime/scheme_reg_toy.cpp for the
//             toy scheme). It runs on the first query, not at load time, so a
//             bare LoadLibrary still costs nothing (M0 red line #1).
//   query     SchemeRegistry::get(), called by the framework. Returns an
//             installed scheme when install() put one there, otherwise the
//             factory's.
//
// install() is the injection point: a test hands the framework a fake without
// touching framework sources and without linking an assembling module that
// knows about pinyin. It is also what lets one binary serve more than one
// scheme (X3-b's "two schemes in one process" rehearsal) — the registry never
// knows either type.
//
// Precedence is installed > factory, stated here rather than inferred from
// the code. The registry owns nothing: an installed scheme is never deleted,
// the caller keeps ownership.

#pragma once

#include <imfw/scheme.hpp>

#include <atomic>

namespace imfw {

// Supplied by the assembling module. Must return a process-lifetime scheme,
// constructed on the first call (never from DllMain's prologue: IMM32's first
// key is the earliest legitimate moment). Called at most once per process
// under contention — implementations must be idempotent, the idiomatic form is
// a function-local static. Returning nullptr is a build error in disguise:
// get() aborts unless install() already supplied a scheme, which is why a test
// binary that has no built-in scheme defines this to return its own fake.
Scheme* scheme_factory();

class SchemeRegistry {
public:
    static SchemeRegistry& instance();

    // Query side. Installed scheme first, factory second. Never returns null:
    // with neither, it writes one line to stderr and aborts. That state cannot
    // occur in a correctly assembled .ime (the factory is linked in), and
    // failing loudly beats a half-working IME inside csrss.
    Scheme& get();

    // Injection side. Pass nullptr to drop back to the factory. Returns the
    // previously installed scheme — not owned, never deleted here.
    Scheme* install(Scheme* scheme) noexcept;

    // The installed scheme, or nullptr while the factory is in use.
    Scheme* installed() const noexcept;

private:
    SchemeRegistry() = default;

    std::atomic<Scheme*> installed_{nullptr};
    std::atomic<Scheme*> created_{nullptr};
};

}  // namespace imfw
