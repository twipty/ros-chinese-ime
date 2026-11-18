// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// The scheme injection seam (PLAN-MODULARIZATION X2-a0, audit A-P0-2). Until
// this test existed, "a scheme can be replaced without editing the framework"
// was a claim in a plan. What is pinned here:
//
//   * the query side falls back to the assembling module's factory,
//   * install() overrides it and install(nullptr) restores it,
//   * the factory runs exactly once (the scheme is created lazily and cached),
//   * scheme_instance() is an alias, so existing call sites keep meaning.
//
// What is deliberately not tested: the "neither factory nor injection" abort.
// It can only be observed by killing the process, and in a real .ime the state
// is unreachable because the factory is linked in.

#include <imfw/scheme.hpp>
#include <imfw/scheme_registry.hpp>

#include <cstdio>
#include <memory>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "scheme registry test failed: %s\n", what);
        ++failures;
    }
}

// Minimal Scheme: the registry never calls anything but the vtable, so the
// payload only has to be distinguishable between the two instances.
class FakeScheme final : public imfw::Scheme {
public:
    explicit FakeScheme(const wchar_t* label) : label_(label) {}

    const wchar_t* ui_server_class() const override { return L"FakeUISrv"; }
    const wchar_t* composition_class() const override { return L"FakeComp"; }
    const wchar_t* candidate_class() const override { return L"FakeCand"; }
    const wchar_t* toolbar_class() const override { return L"FakeBar"; }
    const wchar_t* toolbar_label() const override { return label_; }

    char32_t input_char(std::uint32_t vk) const override {
        if (vk >= 'A' && vk <= 'Z') {
            return static_cast<char32_t>('a' + (vk - 'A'));
        }
        return 0;
    }
    bool is_input_char(char32_t cp) const override {
        return cp >= U'a' && cp <= U'z';
    }

    std::unique_ptr<imfw::Engine> create_engine() const override { return nullptr; }

private:
    const wchar_t* label_;
};

FakeScheme g_assembled{L"assembled"};
FakeScheme g_injected{L"injected"};
int g_factory_calls = 0;

}  // namespace

// This test binary has no assembling module, and the creation side is exactly
// what an assembly must supply. Standing in for it here is the point: the
// framework is not edited, no pinyin is linked, and the seam still works.
namespace imfw {

Scheme* scheme_factory() {
    ++g_factory_calls;
    return &g_assembled;
}

}  // namespace imfw

int main() {
    imfw::SchemeRegistry& registry = imfw::SchemeRegistry::instance();

    check(registry.installed() == nullptr, "nothing is installed before the first query");
    check(&registry.get() == &g_assembled, "get() falls back to the assembly's factory");
    check(g_factory_calls == 1, "the factory runs once");
    check(&imfw::scheme_instance() == &g_assembled,
          "scheme_instance() is an alias for get()");
    check(g_factory_calls == 1, "the created scheme is cached, not rebuilt per query");

    // Injection: what a host smoke test does instead of linking a fake .cpp.
    check(registry.install(&g_injected) == nullptr,
          "install() returns the previous injection (none yet)");
    check(registry.installed() == &g_injected, "installed() reports the injection");
    check(&registry.get() == &g_injected, "get() prefers the injected scheme");
    check(&imfw::scheme_instance() == &g_injected,
          "scheme_instance() follows the injection too");
    check(g_factory_calls == 1, "injection does not disturb the factory");

    // Dropping the injection returns to the assembly's scheme, not to "none".
    check(registry.install(nullptr) == &g_injected,
          "install(nullptr) returns what it replaced");
    check(registry.installed() == nullptr, "nothing is installed again");
    check(&registry.get() == &g_assembled, "get() falls back after the injection is dropped");
    check(g_factory_calls == 1, "the fallback is the cached instance, not a new one");

    if (failures == 0) {
        std::printf("scheme registry seam OK\n");
        return 0;
    }
    return 1;
}
