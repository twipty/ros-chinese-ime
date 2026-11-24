#!/usr/bin/env bash
# Build the minimal FTP uploader for ReactOS guest -> host log transfer.
# Statically links MinGW runtimes; imports only system DLLs present on ReactOS.
set -euo pipefail
export TMPDIR=/home/twipty/.workbuddy/tmp

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/../.." && pwd)"
GXX=/home/twipty/.workbuddy/binaries/mingw-i686/usr/bin/i686-w64-mingw32-g++-posix
OBJDUMP=/home/twipty/.workbuddy/binaries/mingw-i686/usr/bin/i686-w64-mingw32-objdump
OUT="$REPO/m0/share/ftpput.exe"

echo "== compiling ftpput.exe =="
"$GXX" -mwindows -municode -std=c++17 -O2 -I"$SCRIPT_DIR" \
    -static-libgcc -static-libstdc++ -static \
    "$SCRIPT_DIR/ftpput.cpp" -lws2_32 -o "$OUT"

echo "== PE imports =="
DLLS="$($OBJDUMP -p "$OUT" 2>/dev/null | grep 'DLL Name' | awk -F': ' '{print $2}' | tr '\n' ' ')"
echo "  $DLLS"
for bad in libgcc libstdc++ libwinpthread; do
    if echo "$DLLS" | grep -qi "$bad"; then
        echo "ERROR: dynamic MinGW runtime dependency ($bad)" >&2
        exit 1
    fi
done

echo "== artifact =="
ls -la "$OUT"
sha256sum "$OUT"
