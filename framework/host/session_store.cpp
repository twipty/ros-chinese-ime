// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Deliberately uses Win32 CRITICAL_SECTION instead of std::mutex: MinGW's
// POSIX threading model would otherwise drag libwinpthread into the IME PE,
// which the ReactOS compatibility check in CI rejects.

#include "session_store.hpp"

#include "dat_loader.hpp"

#include <windows.h>

#include <map>
#include <memory>

namespace imfw {
namespace {

class Store {
public:
    Store() { InitializeCriticalSection(&cs_); }
    ~Store() { DeleteCriticalSection(&cs_); }

    const CodeTable& table() {
        EnterCriticalSection(&cs_);
        if (!table_loaded_) {
            load_ime_table(table_, scheme_instance());
            table_loaded_ = true;
        }
        LeaveCriticalSection(&cs_);
        return table_;
    }

    ImeSessionState* acquire(rpimm::InputContext hIMC) {
        if (hIMC == nullptr) return nullptr;
        EnterCriticalSection(&cs_);
        // Make sure the table exists before an engine can hold a reference
        // to it.
        if (!table_loaded_) {
            load_ime_table(table_, scheme_instance());
            table_loaded_ = true;
        }
        auto& slot = states_[hIMC];
        if (!slot) {
            slot = std::make_unique<ImeSessionState>();
            slot->engine = scheme_instance().create_engine(table_);
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

private:
    CRITICAL_SECTION cs_;
    bool table_loaded_ = false;
    CodeTable table_;
    std::map<rpimm::InputContext, std::unique_ptr<ImeSessionState>> states_;
};

Store& store() {
    static Store instance;  // constructed on first key, never in DllMain
    return instance;
}

}  // namespace

const CodeTable& shared_table() { return store().table(); }

ImeSessionState* acquire_session(rpimm::InputContext hIMC) {
    return store().acquire(hIMC);
}

void release_session(rpimm::InputContext hIMC) { store().release(hIMC); }

}  // namespace imfw
