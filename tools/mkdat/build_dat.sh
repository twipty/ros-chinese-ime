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

echo "rospinyin.dat ready at $OUT_DIR/rospinyin.dat"
