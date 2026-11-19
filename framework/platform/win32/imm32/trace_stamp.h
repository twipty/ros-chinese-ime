// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Shared timestamp formatter for the ROSPINYIN_TRACE channel.
//
// Why two clocks per line instead of one:
//   * wall clock (HH:MM:SS.mmm) is the only field that survives a reboot, and
//     C:\rospinyin-trace.log is opened with OPEN_ALWAYS and never truncated —
//     without it, lines from two separate boots are indistinguishable, and the
//     log of a "before" run cannot be told apart from the "after" run;
//   * the tick count (+Nms since boot) needs no date arithmetic and stays
//     monotonic, so a delta between two lines is a subtraction. It is also the
//     same clock imhost.exe writes in its own [tick] prefix, which is what
//     lets a guest-side probe call be lined up with a line the .ime wrote.
//
// No CRT: the format is a fixed width, so it is cheaper (and safer inside
// DllMain) to place the digits by hand than to reach for sprintf. Header-only
// and inline so both translation units (dllmain.cpp, imm_entry.cpp) produce the
// same prefix without adding a source file to the .ime build.
#ifndef IMFW_HOST_TRACE_STAMP_H
#define IMFW_HOST_TRACE_STAMP_H

#if defined(ROSPINYIN_TRACE)

#include <windows.h>

namespace imfw {
namespace trace {

// One definition for both translation units: DllMain (which answers "did the
// .ime load at all") and the IMM32 entry points (which answer "did IMM32 start
// calling us") must append to the same file, or the two halves of a bring-up
// session cannot be read in order.
constexpr char kTracePath[] = "C:\\rospinyin-trace.log";

// Room needed by stamp(): "[" + "HH:MM:SS.mmm" (12) + " +" (2) + up to
// 10 digits + "ms]" (3) + " " (1) = 29. 32 keeps a byte of slack.
constexpr unsigned kStampRoom = 32;

inline unsigned put_two(char* out, unsigned len, unsigned value) {
    out[len] = static_cast<char>('0' + (value / 10) % 10);
    out[len + 1] = static_cast<char>('0' + value % 10);
    return len + 2;
}

inline unsigned put_three(char* out, unsigned len, unsigned value) {
    out[len] = static_cast<char>('0' + (value / 100) % 10);
    out[len + 1] = static_cast<char>('0' + (value / 10) % 10);
    out[len + 2] = static_cast<char>('0' + value % 10);
    return len + 3;
}

// The one decimal formatter of the trace channel (X2-d): DllMain, the
// timestamp prefix and the per-line builder all reach the same digits instead
// of each carrying a hand-written divide-by-ten loop that can disagree about
// zero or about running out of room.
//
// `cap` is what makes it safe to call next to a payload of unknown length: the
// digits are dropped rather than written past the end, exactly like the
// hand-written loop it replaced (which guarded `len + 2 < sizeof(out)`).
inline unsigned put_uint(char* out, unsigned len, unsigned cap,
                         unsigned long value) {
    char tmp[12];
    unsigned count = 0;
    do {
        tmp[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value != 0 && count < 11);
    while (count > 0 && len + 1 < cap) out[len++] = tmp[--count];
    return len;
}

// Appends "[HH:MM:SS.mmm +Nms] " and returns the new length. When there is no
// room the buffer is returned untouched, so a full line degrades to the old
// "no stamp" shape instead of being written half a timestamp.
inline unsigned stamp(char* out, unsigned len, unsigned cap) {
    if (len + kStampRoom > cap) return len;

    SYSTEMTIME now;
    GetLocalTime(&now);

    out[len++] = '[';
    len = put_two(out, len, now.wHour);
    out[len++] = ':';
    len = put_two(out, len, now.wMinute);
    out[len++] = ':';
    len = put_two(out, len, now.wSecond);
    out[len++] = '.';
    len = put_three(out, len, now.wMilliseconds);
    out[len++] = ' ';
    out[len++] = '+';
    len = put_uint(out, len, cap, static_cast<unsigned long>(GetTickCount()));
    out[len++] = 'm';
    out[len++] = 's';
    out[len++] = ']';
    out[len++] = ' ';
    return len;
}

}  // namespace trace
}  // namespace imfw

#endif  // ROSPINYIN_TRACE

#endif  // IMFW_HOST_TRACE_STAMP_H
