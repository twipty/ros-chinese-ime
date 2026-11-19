// imfw — reusable IMM32 input-method framework for ReactOS
// SPDX-License-Identifier: MIT
//
// Body of the trace channel (see trace_line.h for why it is a file at all).
// Compiled to nothing unless ROSPINYIN_TRACE is defined.

#include "trace_line.h"

#if defined(ROSPINYIN_TRACE)

#include <imfw/trace_stamp.hpp>

#include <windows.h>

namespace imfw {
namespace trace {
namespace {

// "p=" / "t=" + the number. One helper because there are two of them and the
// only thing that ever differed was the tag character -- those two hand-written
// copies are what X2-d merged.
unsigned put_tagged(char* out, unsigned len, unsigned cap, char tag,
                    unsigned long value) {
    if (len + 2 >= cap) return len;
    out[len++] = tag;
    out[len++] = '=';
    return put_uint(out, len, cap, value);
}

}  // namespace

Line::Line() : len_(0) { line_[0] = '\0'; }

Line::~Line() { commit(); }

Line& Line::s(const char* text) {
    while (text != nullptr && *text != '\0' && len_ + 1 < sizeof(line_)) {
        line_[len_++] = *text++;
    }
    line_[len_] = '\0';
    return *this;
}

Line& Line::d(long long value) { return number(value, false); }

Line& Line::h(unsigned long value) {
    return number(static_cast<long long>(value), true);
}

Line& Line::number(long long value, bool hex) {
    const unsigned base = hex ? 16u : 10u;
    unsigned long long magnitude = value < 0
        ? 0ULL - static_cast<unsigned long long>(value)
        : static_cast<unsigned long long>(value);
    char tmp[24];
    int count = 0;

    if (hex) s("0x");
    do {
        const unsigned digit = static_cast<unsigned>(magnitude % base);
        tmp[count++] = static_cast<char>(digit < 10 ? '0' + digit
                                                    : 'a' + digit - 10);
        magnitude /= base;
    } while (magnitude != 0 && count < 23);
    if (value < 0 && !hex) tmp[count++] = '-';
    while (count > 0 && len_ + 1 < sizeof(line_)) line_[len_++] = tmp[--count];
    line_[len_] = '\0';
    return *this;
}

void Line::commit() {
    if (len_ == 0) return;

    // Prefix the line with wall clock + tick count (see trace_stamp.hpp for why
    // both) so interleaved lines keep their order and lines from different
    // boots stay distinguishable.
    char out[400];
    unsigned n = stamp(out, 0, sizeof(out));

    // pid/tid: several processes probe the .ime during bring-up (our imhost,
    // the shell, the app under test). Without them "ImeInquire ran" cannot be
    // attributed to the instance imm32 was supposed to load.
    //
    // Room: kStampRoom (32) + "p=" + 10 digits + ' ' + "t=" + 10 digits + ' '
    // + payload (319) + '\n' = 376 < sizeof(out). Every step is capped as well,
    // so a longer field in the future truncates a digit instead of walking off
    // the buffer.
    n = put_tagged(out, n, sizeof(out), 'p',
                   static_cast<unsigned long>(GetCurrentProcessId()));
    if (n + 1 < sizeof(out)) out[n++] = ' ';
    n = put_tagged(out, n, sizeof(out), 't',
                   static_cast<unsigned long>(GetCurrentThreadId()));
    if (n + 1 < sizeof(out)) out[n++] = ' ';

    for (std::size_t i = 0; i < len_ && n + 2 < sizeof(out); ++i) {
        out[n++] = line_[i];
    }
    out[n++] = '\n';

    const HANDLE file = CreateFileA(kTracePath, FILE_APPEND_DATA,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    // FILE_APPEND_DATA alone was not enough: a run that should have produced
    // three lines (attach, attach, detach) left only the last one, which
    // silently drops evidence. Seek explicitly before writing.
    SetFilePointer(file, 0, nullptr, FILE_END);
    DWORD written = 0;
    WriteFile(file, out, static_cast<DWORD>(n), &written, nullptr);
    CloseHandle(file);
}

}  // namespace trace
}  // namespace imfw

#endif  // ROSPINYIN_TRACE
