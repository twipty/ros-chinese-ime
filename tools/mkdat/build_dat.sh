#!/usr/bin/env bash
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
#
# Reproducible end-to-end rospinyin.dat build:
#   1. fetch pinned raw inputs (sha256 verified)  2. run mkdat
#
# Usage:
#   tools/mkdat/build_dat.sh [build-dir] [mkdat-binary] [--install[=dir]]
# Defaults: build/m2-data and build/tools/mkdat/mkdat; --install copies the
# verified table to m0/share (the VM/release staging dir).
#
# --install exists because that copy used to be a manual `cp`, and the two
# copies silently diverged: on 2026-11-20 m0/share still held a v1 table while
# the loader had become v2-only, so every VM run would have fallen back to the
# built-in array without saying why.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INSTALL_TO=""
POSITIONAL=()
for arg in "$@"; do
    case "$arg" in
        --install)     INSTALL_TO="$ROOT/m0/share" ;;
        --install=*)   INSTALL_TO="${arg#--install=}" ;;
        *)             POSITIONAL+=("$arg") ;;
    esac
done
OUT_DIR="${POSITIONAL[0]:-$ROOT/build/m2-data}"
MKDAT="${POSITIONAL[1]:-$ROOT/build/tools/mkdat/mkdat}"

if [[ ! -x "$MKDAT" ]]; then
    echo "mkdat not found at $MKDAT" >&2
    echo "build it first: cmake --build build --target mkdat" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
python3 "$ROOT/tools/mkdat/fetch_data.py" --out-dir "$OUT_DIR"

# The cmap is a checked-in data file (exported from the ReactOS bundled font
# by export_cmap.py), not a network input. Pin it here so a font swap is a
# deliberate, reviewable act -- the tier bit is only as honest as this list.
CMAP="$ROOT/tools/mkdat/data/droid_cmap.txt"
EXPECTED_CMAP_SHA256="cdee3c06bffba5114498ae25cd8182a927996ca1c2eb1fb2cb2d0e1f63f908d9"
ACTUAL_CMAP_SHA256="$(sha256sum "$CMAP" | cut -d' ' -f1)"
if [[ "$ACTUAL_CMAP_SHA256" != "$EXPECTED_CMAP_SHA256" ]]; then
    echo "droid_cmap.txt fingerprint mismatch" >&2
    echo "  expected $EXPECTED_CMAP_SHA256" >&2
    echo "  actual   $ACTUAL_CMAP_SHA256" >&2
    exit 1
fi

"$MKDAT" \
    --unihan "$OUT_DIR/Unihan_Readings.txt" \
    --frequency "$OUT_DIR/hanziDB.csv" \
    --cmap "$CMAP" \
    --out "$OUT_DIR/rospinyin.dat" \
    --stats

# Output fingerprint. Inputs were already sha256-pinned, but nothing checked what
# they produced, so "byte-for-byte reproducible" rested on memory -- and indeed
# two different dat sizes (902,768 and 902,776) both ended up in the docs.
# Changing the table on purpose now costs one deliberate edit here, which is the
# point: a data change becomes a reviewable act.
#
# Bumped 2026-11-20 with the v2 container (80-byte header, 24-byte entries, word
# tables). Second bump the same day carries the first real tier grading: 44,355
# characters stay, but slot-5 bit1 is now set for the 27,855 codepoints outside
# Droid's cmap instead of being all zero. A differing count here is a data bug.
EXPECTED_DAT_SHA256="8f8cdb332b425a460eb25e1e016ce06871c9eec0dd7f6de750f59521142756d3"
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

# Only install after the fingerprint gate, so a wrong table can never reach the
# staging dir and masquerade as "the build worked".
if [[ -n "$INSTALL_TO" ]]; then
    if [[ "$OUT_DIR/rospinyin.dat" -ef "$INSTALL_TO/rospinyin.dat" ]]; then
        echo "install target is the build output itself, nothing to copy"
    else
        mkdir -p "$INSTALL_TO"
        cp -f "$OUT_DIR/rospinyin.dat" "$INSTALL_TO/rospinyin.dat"
        echo "installed to $INSTALL_TO/rospinyin.dat"
    fi
fi
