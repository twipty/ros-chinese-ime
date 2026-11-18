// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Scheme registry: see imfw/scheme_registry.hpp for why it exists.
//
// Two atoms of state, no locks. std::mutex is banned framework-wide because
// MinGW's POSIX threading model would pull libwinpthread into the IME PE
// (CI rejects that import), and a CRITICAL_SECTION would drag windows.h into
// the host-independent core. Neither is needed: the values are pointers and
// the only ordering rule is "publish, then read".

#include <imfw/scheme_registry.hpp>

#include <cstdio>
#include <cstdlib>

namespace imfw {
namespace {

[[noreturn]] void abort_without_scheme() {
    std::fputs(
        "imfw: no scheme — the assembling module must define "
        "imfw::scheme_factory() (or install() one before the first query)\n",
        stderr);
    std::abort();
}

}  // namespace

SchemeRegistry& SchemeRegistry::instance() {
    // Function-local static: constructed on the first query, never in
    // DllMain's prologue. Two atomics, so construction is loader-safe.
    static SchemeRegistry registry;
    return registry;
}

Scheme* SchemeRegistry::install(Scheme* scheme) noexcept {
    return installed_.exchange(scheme, std::memory_order_acq_rel);
}

Scheme* SchemeRegistry::installed() const noexcept {
    return installed_.load(std::memory_order_acquire);
}

Scheme& SchemeRegistry::get() {
    if (Scheme* injected = installed_.load(std::memory_order_acquire)) {
        return *injected;
    }

    Scheme* created = created_.load(std::memory_order_acquire);
    if (created != nullptr) return *created;

    // First query. Two threads may get here together (IMM32 does not promise a
    // single caller); the factory is idempotent, so both may construct and the
    // exclusive store decides which pointer is published.
    Scheme* made = scheme_factory();
    if (made == nullptr) abort_without_scheme();

    Scheme* expected = nullptr;
    if (created_.compare_exchange_strong(expected, made,
                                         std::memory_order_acq_rel)) {
        return *made;
    }
    return *expected;  // lost the race; the winner's scheme is the equivalent
}

// The historical entry point, kept so the six framework call sites and any
// out-of-tree host do not have to change. It is now a pure alias for the
// query side, and the framework — not the assembling module — defines it.
Scheme& scheme_instance() { return SchemeRegistry::instance().get(); }

}  // namespace imfw
