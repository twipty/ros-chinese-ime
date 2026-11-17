// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Deliberately uses Win32 CRITICAL_SECTION instead of std::mutex: MinGW's
// POSIX threading model would otherwise drag libwinpthread into the IME PE,
// which the ReactOS compatibility check in CI rejects.
//
// Data policy: the framework holds no code table. Every scheme owns its
// data and builds engines against it (see imfw/scheme.hpp); the store only
// tracks per-HIMC sessions.

#include "session_store.hpp"

#include <windows.h>

#include <map>
#include <memory>
#include <utility>

namespace imfw {
namespace {

class Store {
public:
    Store() { InitializeCriticalSection(&cs_); }
    ~Store() { DeleteCriticalSection(&cs_); }

    ImeSessionState* acquire(rpimm::InputContext hIMC) {
        if (hIMC == nullptr) return nullptr;
        EnterCriticalSection(&cs_);
        auto& slot = states_[hIMC];
        if (!slot) {
            slot = std::make_unique<ImeSessionState>();
            slot->engine = scheme_instance().create_engine();
        }
        ImeSessionState* result = slot.get();
        LeaveCriticalSection(&cs_);
        return result;
    }

    void release(rpimm::InputContext hIMC) {
        EnterCriticalSection(&cs_);
        states_.erase(hIMC);
        LeaveCriticalSection(&cs_);
    }

    // Let the scheme refresh its own data first, then drop every session so
    // the next key rebuilds engines against the new data. In-flight
    // compositions are discarded — acceptable after a configuration change.
    void reload() {
        EnterCriticalSection(&cs_);
        states_.clear();
        scheme_instance().reload_data();
        LeaveCriticalSection(&cs_);
    }

private:
    CRITICAL_SECTION cs_;
    std::map<rpimm::InputContext, std::unique_ptr<ImeSessionState>> states_;
};

Store& store() {
    static Store instance;  // constructed on first key, never in DllMain
    return instance;
}

}  // namespace

ImeSessionState* acquire_session(rpimm::InputContext hIMC) {
    return store().acquire(hIMC);
}

void release_session(rpimm::InputContext hIMC) { store().release(hIMC); }

void reload_sessions() { store().reload(); }

}  // namespace imfw
