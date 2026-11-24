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

echo "== compiling glyph-probe.exe =="
"$GXX" -mwindows -std=c++17 -O2 -I"$SCRIPT_DIR" \
    "$SCRIPT_DIR/glyph_probe.cpp" -o "$OUT"

echo "== artifact =="
ls -la "$OUT"
file "$OUT"
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$OUT"
fi

echo "== installed to: $OUT =="
echo "guest download: http://10.0.2.2:8000/glyph-probe.exe"
