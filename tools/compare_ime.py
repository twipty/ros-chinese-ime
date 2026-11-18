#!/usr/bin/env python3
"""Byte-for-byte comparison of two .ime builds.

X1 (directory relocation) and any other *refactor with no logic change* has a
harder acceptance criterion than any unit test: rebuild with the same
toolchain and compare the PE byte by byte. "All tests pass" proves nothing
here — if a source file is dropped from the CMake source list by a bad path,
the link still succeeds and the tests still pass, but the image changes.

Two builds of identical sources are not bit-identical, because the linker
stamps each image with:
  * TimeDateStamp (COFF header, offset 0x88) — wall clock, 4 bytes
  * CheckSum      (optional header, 0xD8..0xD9) — derived from the whole image
  * debug directory contents (CodeView/PDB path or build id, around 0x60A04
    for the current image; the debug *directory entry* holds a pointer, so a
    single changed stamp shifts the whole payload)

Everything else must match exactly. This tool prints every differing run and
classifies it, so a report can quote "4 regions, all linker stamps" instead of
"the files differ".

Usage:
    tools/compare_ime.py OLD.ime NEW.ime [--report FILE]
Exit code 0 = equivalent (only linker stamps differ), 1 = real difference.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

# Offsets of the fields the linker is allowed to rewrite. Derived from the PE
# layout, not hard-coded per image: we read the COFF header to find them.
PE_SIG_OFF = 0x3C  # e_lfanew: DWORD offset of the PE signature


def pe_offsets(blob: bytes) -> dict[str, object]:
    """Locate the linker-stamped fields using the real PE layout.

    All three are structural, not magic numbers: the COFF TimeDateStamp, the
    optional-header CheckSum, and every IMAGE_DEBUG_DIRECTORY entry's own
    TimeDateStamp (which ld copies from the COFF header, so it moves whenever
    the clock does). Finding the third one by walking data directory 6 means
    this works for any image instead of hard-coding 0x60A04.
    """
    pe = struct.unpack_from("<I", blob, PE_SIG_OFF)[0]
    if blob[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE image")
    coff = pe + 4
    opt = coff + 20
    magic = struct.unpack_from("<H", blob, opt)[0]
    # PE32 and PE32+ share the data-directory offset (opt+96) because the extra
    # 8 bytes of a PE32+ optional header are consumed by ImageBase/BaseOfData.
    dd = opt + 96
    nrva = struct.unpack_from("<I", blob, dd - 4)[0]
    exp_rva, _ = struct.unpack_from("<II", blob, dd + 0) if nrva > 0 else (0, 0)
    dbg_rva, dbg_size = (struct.unpack_from("<II", blob, dd + 6 * 8)
                         if nrva > 6 else (0, 0))
    offs: dict[str, object] = {
        # TimeDateStamp lives in the COFF file header.
        "timestamp": coff + 4,
        # CheckSum lives in the optional header, same offset for PE32/PE32+.
        "checksum": opt + 64,
        "magic": magic,
        "debug_dirs": [(dbg_rva, dbg_size)] if dbg_rva else [],
        "export_dir_rva": exp_rva,
        "sections": section_table(blob, opt, magic, coff),
    }
    # ld also stamps the export directory with the same clock value, so a
    # rebuild of byte-identical sources differs there too. This is the field
    # PLAN-MODULARIZATION §3 called "the debug field at 0x60A04"; it is not a
    # debug directory entry — this image has none (data dir #6 is empty) — but
    # IMAGE_EXPORT_DIRECTORY.TimeDateStamp, 4 bytes into .edata.
    exp_off = rva_to_off(offs, exp_rva) if exp_rva else None
    offs["export_ts"] = exp_off + 4 if exp_off is not None else None
    return offs


def section_table(blob: bytes, opt: int, magic: int, coff: int):
    """(virtual address, virtual size, raw pointer, raw size) per section."""
    nsec = struct.unpack_from("<H", blob, coff + 2)[0]
    opt_size = struct.unpack_from("<H", blob, coff + 16)[0]
    sec = opt + opt_size
    out = []
    for i in range(nsec):
        base = sec + i * 40
        vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", blob, base + 8)
        out.append((vaddr, vsize, rptr, rsize))
    return out


def rva_to_off(offs: dict, rva: int) -> int | None:
    for vaddr, vsize, rptr, rsize in offs["sections"]:  # type: ignore[union-attr]
        if vaddr <= rva < vaddr + max(vsize, rsize):
            return rptr + (rva - vaddr)
    return None


def debug_timestamps(blob: bytes, offs: dict) -> list[int]:
    """File offsets of each IMAGE_DEBUG_DIRECTORY.TimeDateStamp field."""
    out: list[int] = []
    for rva, size in offs["debug_dirs"]:  # type: ignore[union-attr]
        base = rva_to_off(offs, rva)
        if base is None:
            continue
        for i in range(size // 28):
            # 28 B entry; TimeDateStamp is the second DWORD.
            out.append(base + i * 28 + 4)
    return out


def runs(old: bytes, new: bytes) -> list[tuple[int, int]]:
    """Maximal runs of differing bytes as (offset, length)."""
    out: list[tuple[int, int]] = []
    start = None
    for i in range(min(len(old), len(new))):
        if old[i] != new[i]:
            if start is None:
                start = i
        elif start is not None:
            out.append((start, i - start))
            start = None
    if start is not None:
        out.append((start, min(len(old), len(new)) - start))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("old")
    ap.add_argument("new")
    ap.add_argument("--report", help="write a markdown report here")
    a = ap.parse_args()

    old = Path(a.old).read_bytes()
    new = Path(a.new).read_bytes()
    lines: list[str] = []

    def say(s: str = "") -> None:
        print(s)
        lines.append(s)

    say(f"old  {a.old}  {len(old):,} B  {hashlib.sha256(old).hexdigest()[:16]}")
    say(f"new  {a.new}  {len(new):,} B  {hashlib.sha256(new).hexdigest()[:16]}")

    if len(old) != len(new):
        say(f"SIZE MISMATCH: {len(new) - len(old):+,} B — a source file was added or dropped")
        # Size already proves non-equivalence, but still show where it diverges.
    else:
        say("size identical")

    offs = pe_offsets(old)
    ts, cks, exp_ts = offs["timestamp"], offs["checksum"], offs["export_ts"]
    dbg_ts = debug_timestamps(old, offs)
    say(f"PE32{'+' if offs['magic'] == 0x20B else ''}  "
        f"TimeDateStamp @ 0x{ts:X}  "
        f"CheckSum @ 0x{cks:X}  "
        f"export TimeDateStamp @ {f'0x{exp_ts:X}' if exp_ts else 'none'}  "
        f"debug TimeDateStamp @ "
        f"{', '.join(f'0x{o:X}' for o in dbg_ts) or 'none'}")

    diff = runs(old, new)
    if not diff:
        # Only reachable with equal sizes, or with a truncated tail.
        say("no differing bytes within the common prefix"
            if len(old) != len(new) else "IDENTICAL: zero differing bytes")
        real: list[tuple[int, int]] = []
    else:
        say(f"{len(diff)} differing region(s):")
        real = []
        for off, ln in diff:
            if ts <= off < ts + 4:
                kind = "TimeDateStamp (linker clock)"
            elif cks <= off < cks + 2:
                kind = "CheckSum (derived from image)"
            elif exp_ts is not None and exp_ts <= off < exp_ts + 4:
                kind = "export dir TimeDateStamp (copy of the clock)"
            elif any(d <= off < d + 4 for d in dbg_ts):
                kind = "debug dir TimeDateStamp (copy of the clock)"
            else:
                kind = "**payload**"
                real.append((off, ln))
            old_b = old[off:off + ln].hex()
            new_b = new[off:off + ln].hex()
            show = 48
            say(f"  0x{off:06X} +{ln:4d}  {kind}")
            say(f"      old {old_b[:show]}{'…' if ln * 2 > show else ''}")
            say(f"      new {new_b[:show]}{'…' if ln * 2 > show else ''}")

    # A size change is a real difference even if every remaining byte matches.
    ok = not real and len(old) == len(new)
    say()
    say("VERDICT: equivalent" if ok else "VERDICT: NOT equivalent")

    if a.report:
        Path(a.report).write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
