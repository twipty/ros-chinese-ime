#!/usr/bin/env bash
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
#
# Reproducible end-to-end rospinyin.dat build:
#   1. fetch pinned raw inputs (sha256 verified)  2. run mkdat
#
# Usage:
#   tools/mkdat/build_dat.sh [build-dir] [mkdat-binary]
# Defaults: build/m2-data and build/tools/mkdat/mkdat

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="${1:-$ROOT/build/m2-data}"
MKDAT="${2:-$ROOT/build/tools/mkdat/mkdat}"

if [[ ! -x "$MKDAT" ]]; then
    echo "mkdat not found at $MKDAT" >&2
    echo "build it first: cmake --build build --target mkdat" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
python3 "$ROOT/tools/mkdat/fetch_data.py" --out-dir "$OUT_DIR"

"$MKDAT" \
    --unihan "$OUT_DIR/Unihan_Readings.txt" \
    --frequency "$OUT_DIR/hanziDB.csv" \
    --out "$OUT_DIR/rospinyin.dat" \
    --stats

# Output fingerprint. Inputs were already sha256-pinned, but nothing checked what
# they produced, so "byte-for-byte reproducible" rested on memory -- and indeed
# two different dat sizes (902,768 and 902,776) both ended up in the docs.
# Changing the table on purpose now costs one deliberate edit here, which is the
# point: a data change becomes a reviewable act.
EXPECTED_DAT_SHA256="f28b2606338431d2e7a9adc089f8f053728c2be18061bc066d8fdaba23e9b921"
ACTUAL_DAT_SHA256="$(sha256sum "$OUT_DIR/rospinyin.dat" | cut -d' ' -f1)"
if [[ "$ACTUAL_DAT_SHA256" != "$EXPECTED_DAT_SHA256" ]]; then
    echo "rospinyin.dat fingerprint mismatch" >&2
    echo "  expected $EXPECTED_DAT_SHA256" >&2
    echo "  actual   $ACTUAL_DAT_SHA256" >&2
    if [[ "${ROSPINYIN_ALLOW_NEW_DAT:-0}" == "1" ]]; then
        echo "ROSPINYIN_ALLOW_NEW_DAT=1: accepting the new table;" >&2
        echo "update EXPECTED_DAT_SHA256 in tools/mkdat/build_dat.sh" >&2
    else
        exit 1
    fi
fi

echo "rospinyin.dat ready at $OUT_DIR/rospinyin.dat (sha256 ${ACTUAL_DAT_SHA256:0:12}...)"
