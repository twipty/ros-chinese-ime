#!/usr/bin/env python3
"""End-to-end checker for mkdat / rospinyin.dat (M2, D2=A).

Runs mkdat on the checked-in fixtures, decodes the resulting binary using
tools/mkdat/dat_format.hpp's layout and asserts:

  * header magic / version / offsets are consistent
  * the v-key normalization happened in the data layer (nv3/lv4/lve4)
  * tone digits sit immediately after the vowel (zho1ng, not zhong1)
  * kHanyuPinlu breaks polyphonic ties (重 -> chong)
  * tier bit is graded from the checked-in Droid cmap (中 -> 0, 嗯 -> 1)
  * syllable ranges are contiguous and ordered, readings are canonical
  * repeated runs are byte-for-byte deterministic

Usage:
    python3 tests/check_mkdat.py --mkdat <path-to-mkdat> --root <repo-root>
"""

import argparse
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

MAGIC = b"ROSPYDAT"
VERSION = 2
# 80 bytes: magic + 17 u32 + max_code_len/auto_commit/reserved[2].
HEADER_FMT = "<8s17I4B"
CHAR_FMT = "<IIIIII"           # 24 bytes
SYL_FMT = "<IIII"              # 16 bytes
LEX_FMT = "<IIIIII"            # 24 bytes, same shape as CHAR_FMT
LEXCODE_FMT = "<IIII"          # 16 bytes, same shape as SYL_FMT
HEADER_SIZE = struct.calcsize(HEADER_FMT)
CHAR_SIZE = struct.calcsize(CHAR_FMT)
SYL_SIZE = struct.calcsize(SYL_FMT)
UNKNOWN_RANK = 0xFFFFFFFF
LEX_FMT_SIZE = struct.calcsize(LEX_FMT)
LEXCODE_FMT_SIZE = struct.calcsize(LEXCODE_FMT)


def fail(message):
    print(f"check_mkdat: FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def decode(path: Path) -> dict:
    blob = path.read_bytes()
    (magic, version, flags, char_count, syl_count, lex_entry_count,
     lex_code_count, char_off, syl_off, lex_off, lex_code_off,
     pool_off, pool_size, name_off, alphabet_off, source_off,
     entry_size, code_size, max_code_len, auto_commit, r0, r1) = \
        struct.unpack_from(HEADER_FMT, blob, 0)

    if magic != MAGIC:
        fail(f"bad magic {magic!r}")
    if version != VERSION:
        hint = " (version 1 is the retired 52-byte-header layout; rebuild the table)" \
            if version == 1 else ""
        fail(f"bad version {version}{hint}")
    if (flags, name_off, alphabet_off, source_off, max_code_len,
            auto_commit, r0, r1) != (0, 0, 0, 0, 0, 0, 0, 0):
        fail("reserved / shape-only header fields must be zero for pinyin")
    if entry_size != CHAR_SIZE or code_size != SYL_SIZE:
        fail(f"header record sizes {entry_size}/{code_size} disagree with the layout")
    if char_off != HEADER_SIZE:
        fail(f"bad entry table offset {char_off}")
    if syl_off != char_off + char_count * CHAR_SIZE:
        fail("bad syllable table offset")
    if lex_off != syl_off + syl_count * SYL_SIZE:
        fail("bad word table offset")
    if lex_code_off != lex_off + lex_entry_count * LEX_FMT_SIZE:
        fail("bad word-key table offset")
    if pool_off != lex_code_off + lex_code_count * LEXCODE_FMT_SIZE:
        fail("bad pool offset")
    if pool_off + pool_size != len(blob):
        fail("pool size does not match file length")

    pool = blob[pool_off:]

    def string_at(offset: int) -> str:
        if offset >= len(pool):
            fail(f"string offset {offset} outside pool")
        end = pool.find(b"\0", offset)
        if end < 0:
            fail(f"unterminated string at offset {offset}")
        return pool[offset:end].decode("utf-8")

    chars = []
    for i in range(char_count):
        cp, rank, syl_id, reading_off, alt_off, entry_flags = struct.unpack_from(
            CHAR_FMT, blob, char_off + i * CHAR_SIZE)
        if entry_flags & ~0x2:
            fail(f"U+{cp:04X}: reserved flag bits are set")
        chars.append({
            "cp": cp,
            "rank": rank,
            "syl_id": syl_id,
            "reading": string_at(reading_off),
            "alt": string_at(alt_off) if alt_off else "",
            "tier": (entry_flags >> 1) & 1,
        })

    syllables = []
    for i in range(syl_count):
        name_off, first, count, reserved = struct.unpack_from(
            SYL_FMT, blob, syl_off + i * SYL_SIZE)
        if reserved != 0:
            fail("syllable reserved field must be zero")
        syllables.append({
            "name": string_at(name_off),
            "first": first,
            "count": count,
        })

    # mkdat emits no word list yet; M4 will. Validate whatever is there so the
    # checker keeps working on the day the table gains words.
    words = []
    expected = 0
    for i in range(lex_code_count):
        key_off, first, count, key_flags = struct.unpack_from(
            LEXCODE_FMT, blob, lex_code_off + i * LEXCODE_FMT_SIZE)
        if key_flags != 0:
            fail("word-key flags must be zero")
        if first != expected:
            fail("word-key runs do not tile the word table")
        expected += count
        words.append({"key": string_at(key_off), "first": first, "count": count})
    if expected != lex_entry_count:
        fail(f"word keys cover {expected} of {lex_entry_count} words")

    return {"chars": chars, "syllables": syllables, "words": words,
            "pool_size": pool_size}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mkdat", required=True, type=Path)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--cmap", type=Path, default=None,
                        help="Droid cmap list (default: tools/mkdat/data/droid_cmap.txt)")
    args = parser.parse_args()

    if not args.mkdat.exists():
        fail(f"mkdat not found: {args.mkdat}")

    cmap = args.cmap or (args.root / "tools" / "mkdat" / "data" /
                         "droid_cmap.txt")
    if not cmap.exists():
        fail(f"Droid cmap list not found: {cmap}")

    fixtures = args.root / "tools" / "mkdat" / "fixtures"
    unihan = fixtures / "unihan_mini.txt"
    frequency = fixtures / "hanzidb_mini.csv"
    if not unihan.exists() or not frequency.exists():
        fail(f"fixtures missing in {fixtures}")

    with tempfile.TemporaryDirectory(prefix="ros-mkdat-check-") as tmp:
        out1 = Path(tmp) / "one.dat"
        out2 = Path(tmp) / "two.dat"
        command = [
            str(args.mkdat),
            "--unihan", str(unihan),
            "--frequency", str(frequency),
            "--cmap", str(cmap),
            "--quiet",
        ]
        for out in (out1, out2):
            result = subprocess.run(command + ["--out", str(out)],
                                    capture_output=True, text=True)
            if result.returncode != 0:
                fail(f"mkdat exited {result.returncode}: {result.stderr}")

        if out1.read_bytes() != out2.read_bytes():
            fail("mkdat output is not deterministic")

        data = decode(out1)

    by_cp = {item["cp"]: item for item in data["chars"]}

    expected = {
        0x5973: ("nv3", "nv"),       # 女 nǚ -> v key, tone after vowel
        0x5F8B: ("lv4", "lv"),       # 律 lǜ
        0x7565: ("lve4", "lve"),     # 略 lüè
        0x4E2D: ("zho1ng", "zhong"), # 中 zhōng, digit not at end
        0x56FD: ("guo2", "guo"),     # 国 guó
        0x91CD: ("cho2ng", "chong"), # 重: kHanyuPinlu picks chóng
        0x54EF: ("n4g", "ng"),       # 嗯 ǹg (syllabic nasal)
    }
    for cp, (reading, syllable) in expected.items():
        item = by_cp.get(cp)
        if item is None:
            fail(f"missing codepoint U+{cp:04X}")
        if item["reading"] != reading:
            fail(f"U+{cp:04X}: reading {item['reading']!r} != {reading!r}")

    # Every reading must be canonical: no raw umlaut or tone mark survived.
    for item in data["chars"]:
        if not re.fullmatch(r"[a-z']+(?:[1-4][a-z']*)?", item["reading"]):
            fail(f"non-canonical reading {item['reading']!r} for U+{item['cp']:04X}")

    # Polyphonic 重 must retain the other reading as an alternative.
    zhong = by_cp[0x91CD]
    if "zho4ng" not in zhong["alt"].split():
        fail(f"重 lost its alternate reading: {zhong['alt']!r}")

    # Frequency ranking from hanziDB: 的 is rank 1, 一 is rank 2.
    if by_cp[0x7684]["rank"] != 1:
        fail("的 should have rank 1")
    if by_cp[0x4E00]["rank"] != 2:
        fail("一 should have rank 2")
    # A char present only in Unihan has unknown rank.
    if by_cp[0x4E48]["rank"] != UNKNOWN_RANK:
        fail("么 (Unihan-only) should have unknown rank")

    # Tier is graded from the real cmap at build time: 中 is covered, 嗯 is
    # not. These two fixtures would both flip to 0 if --cmap were ignored.
    if by_cp[0x4E2D]["tier"] != 0:
        fail("中 should be tier0 (Droid cmap covers U+4E2D)")
    if by_cp[0x54EF]["tier"] != 1:
        fail("嗯 should be tier1 (Droid cmap misses U+54EF)")

    # Syllable ranges must be contiguous, cover all chars, and be sorted.
    covered = 0
    previous_name = ""
    for syllable in data["syllables"]:
        name = syllable["name"]
        if previous_name and name <= previous_name:
            fail(f"syllables out of order: {previous_name!r} then {name!r}")
        previous_name = name
        start = syllable["first"]
        end = start + syllable["count"]
        if syllable["count"] == 0:
            fail(f"empty syllable range for {name!r}")
        for index in range(start, end):
            item = data["chars"][index]
            numeric = item["reading"]
            plain = re.sub(r"[1-4]", "", numeric)
            if plain != name:
                fail(f"syllable {name!r} range contains {numeric!r}")
        covered += syllable["count"]
    if covered != len(data["chars"]):
        fail(f"syllable ranges cover {covered} of {len(data['chars'])} chars")

    print(
        f"check_mkdat OK: {len(data['chars'])} chars, "
        f"{len(data['syllables'])} syllables, "
        f"{data['pool_size']}-byte pool; v-key + tone positions + tier verified"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
