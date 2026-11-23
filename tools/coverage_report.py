#!/usr/bin/env python3
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
"""M2 exit criterion: how good is the first page for the highest-frequency chars?

    tools/coverage_report.py                    # top 500, the M2 exit criterion
    tools/coverage_report.py --top 1000 --fail-under 95

The M2 exit criterion in ROADMAP.md says "字准率以 Jun Da 高频前 500 字覆盖率自评
（目标 ≥95%）". Until now nothing produced that number: M2-ACCEPTANCE.md verifies
behaviour (keys, paging, undo), not whether the character a user wants is on the
first page. This script measures exactly that, from the shipped `rospinyin.dat`.

Method (mirrors the engine, no reimplementation of the ranking):

  * `CharEntry` records are stored sorted by (syl_id, rank, codepoint), and
    `pinyin_session.cpp` scores candidates as `1000000 - rank` (higher wins).
    Every entry is offered; tier-0 entries with no frequency data carry rank
    1,000,000 (score 0), so they sort after the ranked ones. For a syllable the
    on-screen order is simply rank-ascending order within the record range.
    [快] then drops tier-1 entries, which never changes the relative position
    of the top-ranked characters this report measures.
  * "top 500 by frequency" = the 500 characters with the lowest `rank` field,
    which mkdat copies from the frequency table (hanziDB, itself ranked by the
    Jun Da ordering). The original Jun Da table is not redistributed -- using
    our own derived ranks keeps this inside the licence we actually ship.

Reported numbers:

  first-choice  the wanted character is candidate #1 for its pinyin
  first-page    among the first 9 (what a user sees without paging)
  contiguous    chars reachable by typing the full toneless syllable

Exit code is 0 unless --fail-under is given and the result is below it.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# 80 B: magic + 17 u32 + max_code_len/auto_commit/reserved[2]. The format has
# one version only; a version 1 table (52-byte header) is rejected rather than
# guessed at, because its entries carry no tier bit.
HEADER = struct.Struct("<8s17I4B")
CHAR_ENTRY = struct.Struct("<IIIIII")   # 24 B: codepoint/rank/syl/reading/alt/flags
SYL_ENTRY = struct.Struct("<IIII")      # 16 B: name_off/first_entry/entry_count/flags
K_UNKNOWN_RANK = 0xFFFFFFFF
K_TIER_BIT = 0x2
PAGE_SIZE = 9


def read_dat(path: Path):
    data = path.read_bytes()
    (magic, version, _flags, char_count, syl_count, _lex_count, _lex_key_count,
     char_off, syl_off, _lex_off, _lex_key_off, pool_off, _pool_size,
     _name_off, _alphabet_off, _source_off, entry_size, code_size,
     _max_code_len, _auto_commit, _r0, _r1) = HEADER.unpack_from(data, 0)
    if magic != b"ROSPYDAT":
        raise SystemExit(f"{path}: not a ROSPYDAT file")
    if version != 2:
        raise SystemExit(
            f"{path}: unsupported version {version} (this build reads v2 only; "
            f"rebuild the table with tools/mkdat/build_dat.sh)")
    if entry_size != CHAR_ENTRY.size or code_size != SYL_ENTRY.size:
        raise SystemExit(f"{path}: header record sizes disagree with the layout")

    chars = []
    for i in range(char_count):
        cp, rank, syl_id, reading_off, _alt, flags = CHAR_ENTRY.unpack_from(
            data, char_off + i * CHAR_ENTRY.size)
        chars.append((cp, rank, syl_id, reading_off, flags))

    syllables = []
    for i in range(syl_count):
        name_off, first, count, _res = SYL_ENTRY.unpack_from(
            data, syl_off + i * SYL_ENTRY.size)
        end = data.index(b"\0", pool_off + name_off)
        syllables.append((data[pool_off + name_off:end].decode("ascii"),
                          first, count))
    return chars, syllables


def main() -> int:
    ap = argparse.ArgumentParser(description="first-page coverage of top-N chars")
    ap.add_argument("--dat", type=Path,
                    default=Path(__file__).resolve().parent.parent
                    / "m0" / "share" / "rospinyin.dat")
    ap.add_argument("--top", type=int, default=500,
                    help="how many highest-frequency characters to score")
    ap.add_argument("--fail-under", type=float, default=None,
                    help="exit 1 when first-page coverage is below this percentage")
    ap.add_argument("--list-failures", type=int, default=0,
                    help="print this many characters that miss the first page")
    a = ap.parse_args()

    chars, syllables = read_dat(a.dat)
    ranked = [c for c in chars if c[1] != K_UNKNOWN_RANK]
    if not ranked:
        raise SystemExit(f"{a.dat}: no ranked characters")

    # Position of every character inside its syllable, in engine order, plus
    # the syllable name so the report can say which pinyin to type.
    position: dict[int, int] = {}
    pinyin_of: dict[int, str] = {}
    for name, first, count in syllables:
        seen = 0
        for i in range(first, first + count):
            cp, rank = chars[i][0], chars[i][1]
            pinyin_of[cp] = name
            seen += 1  # every entry is an offered candidate; tier filters later
            position[cp] = seen

    top = sorted(ranked, key=lambda c: (c[1], c[0]))[: a.top]

    first_choice = 0
    first_page = 0
    misses = []
    for cp, rank, syl_id, _reading, _flags in top:
        pos = position.get(cp, 0)
        name = pinyin_of.get(cp, "?")
        if pos == 1:
            first_choice += 1
        if 0 < pos <= PAGE_SIZE:
            first_page += 1
        else:
            misses.append((rank, chr(cp), name, pos))

    n = len(top)
    print(f"dat: {a.dat}")
    print(f"characters: {len(chars)} ({len(ranked)} ranked, "
          f"{len(chars) - len(ranked)} unranked/hidden by default)")
    # Tier is a build-time fact, not a coverage metric: tier1 characters need a
    # font beyond the bundled one, so a table that is all tier0 is one the
    # default (glyphs = common) mode can render end to end.
    tier1 = sum(1 for c in chars if c[4] & K_TIER_BIT)
    print(f"tier: {len(chars) - tier1} rendable by the bundled font, "
          f"{tier1} need an extra font")
    print(f"scoring the {n} highest-frequency characters (rank 1..{top[-1][1]})\n")
    print(f"  first-choice  {first_choice:5d} / {n}  "
          f"= {first_choice * 100.0 / n:5.1f}%")
    print(f"  first-page    {first_page:5d} / {n}  "
          f"= {first_page * 100.0 / n:5.1f}%   (target >= 95%)")

    if misses and a.list_failures:
        print(f"\noff the first page ({len(misses)}):")
        for rank, ch, name, pos in misses[: a.list_failures]:
            where = "hidden (unranked)" if pos == 0 else f"position {pos}"
            print(f"  rank {rank:5d}  {ch}  pinyin '{name}'  {where}")

    if a.fail_under is not None:
        pct = first_page * 100.0 / n
        if pct < a.fail_under:
            print(f"\nFAIL: {pct:.1f}% < {a.fail_under}%")
            return 1
        print(f"\nPASS: {pct:.1f}% >= {a.fail_under}%")
    return 0


if __name__ == "__main__":
    sys.exit(main())
