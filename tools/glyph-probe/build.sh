#!/usr/bin/env bash
# RosPinyin P1 verification-stage GDI font-fallback probe.
# Builds glyph-probe.exe with the pinned MinGW-w64 i686 toolchain, prints its
# SHA256, and optionally installs it into m0/share/ for guest HTTP download.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GXX="${MINGW_GXX:-/home/twipty/.workbuddy/binaries/mingw-i686/usr/bin/i686-w64-mingw32-g++-posix}"
OUT="$ROOT/m0/share/glyph-probe.exe"

if [ ! -x "$GXX" ]; then
    echo "ERROR: MinGW compiler not found: $GXX" >&2
    echo "Set MINGW_GXX=/path/to/i686-w64-mingw32-g++ and retry." >&2
    exit 2
fi

# /tmp on this host is 10 MB; keep MinGW's temp files under the workbuddy dir.
export TMPDIR="${TMPDIR:-/home/twipty/.workbuddy/tmp}"

echo "== compiling glyph-probe.exe (static runtime: guest has no MinGW DLLs) =="
# ReactOS ships no libgcc_s_dw2-1.dll / libstdc++-6.dll. A dynamic C++ runtime
# link makes the exe fail to load in the guest with a misleading "missing DLL"
# popup. The installer GUI uses the same -static policy (cmake/StaticRuntime.cmake).
"$GXX" -mwindows -municode -std=c++17 -O2 \
    -I"$SCRIPT_DIR" -I"$SCRIPT_DIR/../../framework/include" \
    -static-libgcc -static-libstdc++ -static \
    "$SCRIPT_DIR/glyph_probe.cpp" -o "$OUT"

# PE import gate: fail the build if a dynamic MinGW runtime DLL slipped back in.
OBJDUMP="${MINGW_OBJDUMP:-${GXX%/*}/i686-w64-mingw32-objdump}"
if [ -x "$OBJDUMP" ]; then
    BAD=$("$OBJDUMP" -p "$OUT" 2>/dev/null \
          | grep -iE 'DLL Name: (libgcc|libstdc\+\+|libwinpthread)' || true)
    if [ -n "$BAD" ]; then
        echo "ERROR: glyph-probe.exe has dynamic MinGW runtime dependency:" >&2
        echo "$BAD" >&2
        echo "Guest ReactOS does not ship these DLLs." >&2
        exit 3
    fi
    echo "== PE import gate: no dynamic MinGW runtime DLLs =="
fi

echo "== artifact =="
ls -la "$OUT"
file "$OUT"
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$OUT"
fi

echo "== installed to: $OUT =="
echo "guest download: http://10.0.2.2:8000/glyph-probe.exe"
