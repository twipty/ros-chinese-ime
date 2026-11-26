#!/usr/bin/env python3
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
"""Release gate: the tag must match the version stamped into the .ime.

    tools/check_release_version.py alpha0.0.2

Checks three things, all of which used to be "remembered by hand":

  1. `ime/version.h` numbers equal the tag           (alpha0.0.2 -> 0,0,2)
  2. `ROSPINYIN_VERSION_STRING` equals the tag       (shows up in Explorer)
  3. `docs/releases/<tag>.md` exists                 (release notes are written
     before tagging, not improvised afterwards)

Why a gate and not a checklist: `alpha0.0.2` shipped while the version resource
still said `0,0,1,0` / `alpha0.0.1`, because nothing compared the two. A user
reading file properties would have been told the wrong version, and "the
published binary is the build of this tag" would have been uncheckable.

Exit code 0 when everything matches, 1 otherwise (the CI release job and
`make_release.py` both rely on that).

Pure standard library, no third-party dependencies, same as check_m1.py.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# alpha*/beta* are prereleases; v* is the formal release tag.
TAG_RE = re.compile(r"^(alpha|beta|v)(\d+)\.(\d+)\.(\d+)$")

RT_VERSION = 16


def _u16(b: bytes, off: int) -> int:
    return int.from_bytes(b[off:off + 2], "little")


def _u32(b: bytes, off: int) -> int:
    return int.from_bytes(b[off:off + 4], "little")


def pe_file_version(path: Path) -> tuple[int, int, int, int] | None:
    """Read FILEVERSION out of a PE's version resource.

    Checks the artifact instead of the source, which is the part that matters:
    a release note saying "built from this tag" is worth nothing if the .ime
    in the archive carries a different number. Deliberately dependency-free --
    the same reason check_m1.py avoids third-party imports.
    """
    data = path.read_bytes()
    if data[:2] != b"MZ":
        return None
    pe = _u32(data, 0x3C)
    if data[pe:pe + 4] != b"PE\0\0":
        return None
    coff = pe + 4
    nsections = _u16(data, coff + 2)
    opt_size = _u16(data, coff + 16)
    opt = coff + 20
    magic = _u16(data, opt)
    dd = opt + (96 if magic == 0x10B else 112)
    rsrc_rva, _rsrc_size = _u32(data, dd + 2 * 8), _u32(data, dd + 2 * 8 + 4)
    if rsrc_rva == 0:
        return None

    sections = []
    sec = opt + opt_size
    for i in range(nsections):
        base = sec + i * 40
        va = _u32(data, base + 12)
        vsize = _u32(data, base + 8)
        raw = _u32(data, base + 20)
        sections.append((va, max(vsize, _u32(data, base + 16)), raw))

    def rva_to_off(rva: int) -> int | None:
        for va, size, raw in sections:
            if va <= rva < va + size:
                return rva - va + raw
        return None

    def first_entry(dir_off: int, wanted: int | None = None) -> tuple[int, int]:
        """First (or matching) directory entry -> (name_id, data_rva_or_off)."""
        named = _u16(data, dir_off + 12)
        ids = _u16(data, dir_off + 14)
        for i in range(named + ids):
            ent = dir_off + 16 + i * 8
            name = _u32(data, ent)
            target = _u32(data, ent + 4)
            if wanted is not None and name != wanted:
                continue
            return name, target & 0x7FFFFFFF
        return -1, -1

    # Directory offsets are relative to the start of the resource directory.
    # The leaf entry (the one carrying IMAGE_RESOURCE_DATA_ENTRY) is relative in
    # practice -- windres and link.exe both emit it that way -- but the spec is
    # read differently by different tools, so the leaf is tried both ways.
    off = rva_to_off(rsrc_rva)
    if off is None:
        return None
    _, t = first_entry(off, RT_VERSION)
    if t < 0:
        return None
    cur = rva_to_off(rsrc_rva + t)
    if cur is None:
        return None
    _, t = first_entry(cur)          # name level entry
    cur = rva_to_off(rsrc_rva + t)
    if cur is None:
        return None
    _, t = first_entry(cur)          # language level entry -> data entry
    for cand in (rsrc_rva + t, t):
        data_entry = rva_to_off(cand)
        if data_entry is None:
            continue
        ver_off = rva_to_off(_u32(data, data_entry))
        if ver_off is None:
            continue
        fixed = _fixed_info_offset(data, ver_off)
        if fixed is not None:
            ms = _u32(data, fixed + 8)
            ls = _u32(data, fixed + 12)
            return ms >> 16, ms & 0xFFFF, ls >> 16, ls & 0xFFFF
    return None


def _fixed_info_offset(data: bytes, ver_off: int) -> int | None:
    """Offset of VS_FIXEDFILEINFO inside a VS_VERSIONINFO block."""
    key = ver_off + 6  # wLength, wValueLength, wType
    while key + 1 < len(data) and _u16(data, key) != 0:
        key += 2
    fixed = (key + 2 + 3) & ~3
    if fixed + 16 > len(data) or _u32(data, fixed) != 0xFEEF04BD:
        return None
    return fixed


def read_version_header(root: Path) -> dict[str, str]:
    header = (root / "ime" / "version.h").read_text(encoding="utf-8")
    values: dict[str, str] = {}
    for name in ("MAJOR", "MINOR", "PATCH", "BUILD"):
        m = re.search(rf"#define\s+ROSPINYIN_VERSION_{name}\s+(\d+)", header)
        if m:
            values[name] = m.group(1)
    m = re.search(r'#define\s+ROSPINYIN_VERSION_STRING\s+"([^"]+)"', header)
    if m:
        values["STRING"] = m.group(1)
    return values


def main() -> int:
    ap = argparse.ArgumentParser(description="tag vs version.h gate")
    ap.add_argument("tag", help="release tag, e.g. alpha0.0.2")
    ap.add_argument("--root", type=Path,
                    default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--no-notes", action="store_true",
                    help="skip the docs/releases/<tag>.md check (dry runs)")
    ap.add_argument("--binary", type=Path, default=None,
                    help="also verify FILEVERSION inside this built .ime/.exe")
    a = ap.parse_args()

    tag = a.tag.strip()
    m = TAG_RE.match(tag)
    if not m:
        print(f"[FAIL] tag {tag!r} does not look like alphaX.Y.Z / betaX.Y.Z / vX.Y.Z")
        return 1

    want = {"MAJOR": m.group(2), "MINOR": m.group(3), "PATCH": m.group(4)}
    got = read_version_header(a.root.resolve())
    if not got:
        print("[FAIL] ime/version.h has no ROSPINYIN_VERSION_* defines")
        return 1

    failures = 0

    def check(label: str, ok: bool, detail: str) -> None:
        nonlocal failures
        print(f"[{'PASS' if ok else 'FAIL'}] {label} -- {detail}")
        if not ok:
            failures += 1

    for part, expected in want.items():
        actual = got.get(part, "<missing>")
        check(f"version.h {part} matches the tag", actual == expected,
              f"tag={expected} header={actual}")

    check("version string equals the full tag", got.get("STRING") == tag,
          f"tag={tag} header={got.get('STRING', '<missing>')}")

    if not a.no_notes:
        notes = a.root.resolve() / "docs" / "releases" / f"{tag}.md"
        check("release notes exist before tagging", notes.is_file(), str(notes))

    if a.binary is not None:
        got_bin = pe_file_version(a.binary)
        if got_bin is None:
            check("artifact carries a readable version resource", False,
                  str(a.binary))
        else:
            want_bin = tuple(int(want[p]) for p in ("MAJOR", "MINOR", "PATCH"))
            check("artifact FILEVERSION matches the tag",
                  got_bin[:3] == want_bin,
                  "tag=" + ".".join(map(str, want_bin)) +
                  " binary=" + ".".join(map(str, got_bin)))

    if failures:
        print(f"\n{failures} release gate(s) failed -- bump ime/version.h in the "
              f"commit you tag, and write docs/releases/{tag}.md first.")
        return 1
    print(f"\nrelease gate ok for {tag}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
