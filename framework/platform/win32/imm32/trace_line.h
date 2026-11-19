// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// One-line-at-a-time writer for the ROSPINYIN_TRACE channel.
//
// Why a file instead of DbgPrint: ReactOS only forwards DbgPrint to the serial
// port while the kernel debugger is enabled (a FreeLoader boot option the
// default entry does not pass), and ntdll!DbgPrint would add an import the
// shipping .ime does not otherwise need. Appending a line to
// C:\rospinyin-trace.log needs nothing but kernel32, is readable from inside
// the guest, and survives a crash because each line is opened, written and
// closed.
//
// Split out of imm_entry.cpp in X2-d. It is *not* in the build when
// ROSPINYIN_TRACE is undefined: the class below is then an inert twin, so
// instrumented code compiles unchanged and the optimiser drops every call.
// DllMain keeps its own append-style writer (dllmain.cpp) because it runs
// before anything here could be relied on and spells its line differently; the
// two are the only trace producers in the module.

#pragma once

#include <cstddef>

#if defined(ROSPINYIN_TRACE)

namespace imfw {
namespace trace {

class Line {
public:
    Line();
    ~Line();  // commits (opens, writes, closes) -- nothing is buffered across lines

    Line& s(const char* text);     // append text
    Line& d(long long value);      // append decimal
    Line& h(unsigned long value);  // append 0x-prefixed hex

private:
    Line& number(long long value, bool hex);
    void commit();

    char line_[320];
    std::size_t len_;
};

}  // namespace trace
}  // namespace imfw

#define IMFW_TRACE(tag) ::imfw::trace::Line().s(tag)
#define IMFW_TRACE_ON 1

#else

namespace imfw {
namespace trace {

// Inert twin: same chainable syntax, no work. Deliberately a class rather than
// a macro-only stub so `IMFW_TRACE(x).s(..).d(..)` keeps parsing identically
// in both configurations -- instrumented code must not need an #ifdef of its
// own just to keep compiling.
class Line {
public:
    Line() = default;
    Line& s(const char*) { return *this; }
    Line& d(long long) { return *this; }
    Line& h(unsigned long) { return *this; }
};

}  // namespace trace
}  // namespace imfw

#define IMFW_TRACE(tag) ::imfw::trace::Line()
#define IMFW_TRACE_ON 0

#endif
