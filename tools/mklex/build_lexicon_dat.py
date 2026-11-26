#!/usr/bin/env python3
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT (script); CC-CEDICT data is CC-BY-SA.
"""Build the OPTIONAL M4 word dat (rospinyin-lexicon.dat) from CC-CEDICT.

The character dat stays untouched: this tool emits a word-only v2 dat under
the same ROSPYDAT magic, with zero CharEntry/SylEntry records. The runtime
loads it on top of rospinyin.dat; a missing file simply leaves M2 behaviour.

The source snapshot is pinned by SHA-256. The generated dat is deliberately
not stored in git: CC-CEDICT is CC-BY-SA and must be distributed separately.

Usage:
  python3 tools/mklex/build_lexicon_dat.py --fetch \
      --out m0/share/rospinyin-lexicon.dat
  python3 tools/mklex/build_lexicon_dat.py \
      --cedict /path/to/cedict_1_0_ts_utf-8_mdbg.txt.gz --out lexicon.dat
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import re
import struct
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]

SOURCE_URL = (
    "https://www.mdbg.net/chinese/export/cedict/"
    "cedict_1_0_ts_utf-8_mdbg.txt.gz"
)
SOURCE_SHA256 = (
    "9594e10b88f5dde3003133f20f216ff9ae66e9f4441ee94f13b56d14e4043118"
)
WEIGHTS = HERE / "data/weights.txt"

HEADER_FMT = "<8s17I4B"
HEADER_SIZE = 80
LEX_ENTRY_FMT = "<6I"       # text_off, rank, syl_count, reading_off, alt, flags
LEX_CODE_FMT = "<4I"        # key_off, first_entry, entry_count, flags
ENTRY_SIZE = struct.calcsize(LEX_ENTRY_FMT)   # 24
CODE_SIZE = struct.calcsize(LEX_CODE_FMT)     # 16
ENTRY_FMT = LEX_ENTRY_FMT                     # CharEntry has the same width
SYL_FMT = LEX_CODE_FMT                        # SylEntry has the same width

LINE_RE = re.compile(r"^(\S+)\s+(\S+)\s+\[([^\]]+)\]\s+/")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def normalize_reading(raw: str) -> str:
    """Turn a CC-CEDICT reading into the dat's key alphabet."""
    out = raw.strip().lower().replace("ü", "v").replace(" ", "")
    if not out:
        return ""
    for ch in out:
        if not (ch.isascii() and (ch.isalpha() or ch.isdigit()) or ch == "'"):
            return ""
    # Key digits are tone numbers 1-5; reading sanity only allows 0-9, so 5
    # (neutral tone) is kept. syllable_of() strips them at lookup time.
    return out


def load_weights() -> dict[str, int]:
    weights: dict[str, int] = {}
    for line in WEIGHTS.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        word, rank = line.split("\t", 1)
        weights[word] = int(rank)
    return weights


def collect(source: Path) -> tuple[dict[str, list[tuple[str, int, int]]], int]:
    """Return key -> [(word, rank, syl_count)] and the source line count."""
    weights = load_weights()
    raw = source.read_bytes()
    if source.suffix == ".gz":
        if sha256(raw) != SOURCE_SHA256:
            raise SystemExit(
                f"CC-CEDICT snapshot mismatch: {sha256(raw)}\n"
                f"expected {SOURCE_SHA256}\n"
                "Update SOURCE_SHA256 deliberately if you really mean to "
                "upgrade the dictionary."
            )
        text = gzip.decompress(raw).decode("utf-8")
    else:
        text = raw.decode("utf-8")

    by_key: dict[str, list[tuple[str, int, int]]] = {}
    seen: set[tuple[str, str]] = set()
    order = 0
    lines = 0
    for line in text.splitlines():
        lines += 1
        match = LINE_RE.match(line)
        if not match:
            continue
        simplified, reading = match.group(2), match.group(3)
        key = normalize_reading(reading)
        tokens = [t for t in reading.strip().lower().replace("ü", "v").split(" ") if t]
        if not key or len(tokens) < 2:
            continue
        # Keep words whose character count matches the syllable count: those
        # are the predictable multi-character phrases the engine can commit.
        if len(simplified) != len(tokens):
            continue
        if (key, simplified) in seen:
            continue
        seen.add((key, simplified))
        order += 1
        rank = weights.get(simplified, 1000 + order)
        by_key.setdefault(key, []).append((simplified, rank, len(tokens)))
    return by_key, lines


class Pool:
    def __init__(self) -> None:
        self.bytes = bytearray()
        self.offsets: dict[str, int] = {}

    def intern(self, value: str) -> int:
        if value in self.offsets:
            return self.offsets[value]
        offset = len(self.bytes)
        self.bytes.extend(value.encode("utf-8"))
        self.bytes.append(0)
        self.offsets[value] = offset
        return offset


def build(by_key: dict[str, list[tuple[str, int, int]]]) -> bytes:
    pool = Pool()
    words: list[tuple[int, int, int]] = []   # (text_off, rank, syl_count)
    keys: list[tuple[int, int, int]] = []    # (key_off, first_entry, count)

    for key in sorted(by_key):
        entries = sorted(by_key[key], key=lambda item: (item[1], item[0]))
        first = len(words)
        key_off = pool.intern(key)
        for word, rank, syl_count in entries:
            words.append((pool.intern(word), rank, syl_count))
        keys.append((key_off, first, len(entries)))

    lex_off = HEADER_SIZE                     # no char/syllable tables
    lex_code_off = lex_off + len(words) * ENTRY_SIZE
    pool_off = lex_code_off + len(keys) * CODE_SIZE
    header = struct.pack(
        HEADER_FMT,
        b"ROSPYDAT", 2, 0,
        0, 0, len(words), len(keys),
        lex_off, lex_off, lex_off, lex_code_off,
        pool_off, len(pool.bytes),
        0, 0, 0, ENTRY_SIZE, CODE_SIZE,
        0, 0, 0, 0,
    )
    assert len(header) == HEADER_SIZE

    blob = bytearray(header)
    for text_off, rank, syl_count in words:
        blob.extend(struct.pack(LEX_ENTRY_FMT, text_off, rank, syl_count,
                                0, 0, 0))
    for key_off, first, count in keys:
        blob.extend(struct.pack(LEX_CODE_FMT, key_off, first, count, 0))
    blob.extend(pool.bytes)
    return bytes(blob)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--cedict", type=Path,
                    help="local CC-CEDICT .gz (hash pinned) or decoded .txt")
    ap.add_argument("--fetch", action="store_true",
                    help="download the pinned snapshot to a temp file")
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    if args.fetch:
        if args.cedict is not None:
            raise SystemExit("--fetch and --cedict are mutually exclusive")
        cache = Path.home() / ".workbuddy/tmp/cedict-pinned.txt.gz"
        cache.parent.mkdir(parents=True, exist_ok=True)
        if not cache.is_file() or sha256(cache.read_bytes()) != SOURCE_SHA256:
            print(f"fetching {SOURCE_URL}")
            urllib.request.urlretrieve(SOURCE_URL, cache)
        source = cache
    else:
        if args.cedict is None:
            raise SystemExit("give --cedict PATH or use --fetch")
        source = args.cedict

    by_key, lines = collect(source)
    blob = build(by_key)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(blob)
    print(f"source lines : {lines}")
    print(f"word keys    : {len(by_key)}")
    print(f"words        : {sum(len(v) for v in by_key.values())}")
    print(f"output       : {args.out} ({len(blob)} bytes)")
    print(f"sha256       : {sha256(blob)}")
    # Acceptance probes before shipping: the key must be reachable after the
    # runtime deriver strips tones from "ni3hao3" / "ji4suan4ji1".
    for key in ("ni3hao3", "ji4suan4ji1"):
        found = [w for w, _, _ in by_key.get(key, [])]
        print(f"{key:12} -> {found[:3] or 'MISSING'}")


if __name__ == "__main__":
    main()
