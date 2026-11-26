#!/usr/bin/env python3
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
"""Build the OPTIONAL Droid fallback-font pack for ReactOS nightly.

The original DroidSansFallback.ttf is not stored in this repository and is not
shipped with the installer or a release. This script takes a locally obtained
copy (e.g. from the official ReactOS 0.4.16 ISO), refuses anything whose bytes
differ from the pinned original, and emits a deterministic zip that can be
hosted separately as an optional download.

Usage:
  python3 tools/droid-font-pack/build_pack.py \
      --ttf /path/to/DroidSansFallback.ttf --out droid-font-pack.zip
"""
from __future__ import annotations

import argparse
import hashlib
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]

EXPECTED_SHA256 = (
    "82a74d0ef369ec7979172023cfff87f04f0a617a2871c39ef6aeb0f399c5cdaf"
)
EXPECTED_SIZE = 3_081_908

README = """ROSPINYIN OPTIONAL DROID FONT PACK (nightly only)
====================================================

ReactOS 0.4.17-dev nightly removed DroidSansFallback.ttf but the UI font
substitution still names that family, so every Chinese window (including the
RosPinyin installer) renders as '?' boxes. ReactOS 0.4.16 stable already ships
the font and needs nothing from this pack.

Contents:
  DROID.TTF     unmodified Droid Sans Fallback from ReactOS 0.4.16
  DROID.REG     registers the font AND marks it as owned by this pack
  UNREG.REG     manual registry cleanup (only if not using /u)
  SHA256SUMS.TXT
  SOURCE.TXT

Install on a nightly guest (choose ONE path):
  A. Copy DROID.TTF to C:\\, then:
       install-rospinyin.exe /fontinstall DROID.TTF
     The tool verifies the exact file size, installs the font, registers it,
     records ownership and asks you to reboot.
  B. Manual: copy DROID.TTF to C:\\ReactOS\\Fonts\\, import DROID.REG with
     regedit, then reboot.

Verify afterwards:
  install-rospinyin.exe /status
  -> "droid font : present (installed by RosPinyin font pack)"
  -> "droid font reg: present"
  Chinese in the GUI installer must render as characters, not '?' boxes, and
  the IME's [fast] mode must show normal glyphs.

Uninstall:
  install-rospinyin.exe /u
  removes the font, its Fonts registry value and the ownership marker ONLY if
  the marker says this pack installed it. A font shipped by the OS is never
  replaced and never removed. UNREG.REG is only a manual fallback.

This pack is optional, is not bundled with the installer, and is not a GitHub
Release asset. Font license: Apache-2.0 (as shipped in ReactOS 0.4.16).
"""


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def add_text(zf: zipfile.ZipFile, name: str, text: str) -> None:
    info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o644 << 16
    zf.writestr(info, text.encode("utf-8"))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--ttf", type=Path,
        default=ROOT / "m0/probe/reactos/Fonts/DroidSansFallback.ttf",
        help="locally obtained original DroidSansFallback.ttf",
    )
    ap.add_argument("--out", type=Path, default=HERE / "droid-font-pack.zip")
    args = ap.parse_args()

    font = args.ttf.read_bytes()
    if len(font) != EXPECTED_SIZE or sha256(font) != EXPECTED_SHA256:
        raise SystemExit(
            "refusing to pack: the TTF is not the pinned original\n"
            f"  size:     {len(font)} (expected {EXPECTED_SIZE})\n"
            f"  sha256:   {sha256(font)}\n"
            f"  expected: {EXPECTED_SHA256}"
        )

    files = {
        "DROID.TTF": font,
        "DROID.REG": (HERE / "DROID.REG").read_bytes(),
        "UNREG.REG": (HERE / "UNREG.REG").read_bytes(),
        "README.TXT": README.encode("utf-8"),
        "SOURCE.TXT": (
            "Droid Sans Fallback (TrueType)\n"
            "Source: ReactOS 0.4.16 official ISO / Fonts\n"
            "License: Apache-2.0\n"
            f"Size: {EXPECTED_SIZE}\n"
            f"SHA256: {EXPECTED_SHA256}\n"
            "Policy: unmodified, optional, not bundled into the installer or release.\n"
        ).encode("utf-8"),
    }
    sums = "".join(
        f"{sha256(data)}  {name}\n" for name, data in sorted(files.items())
    )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.out, "w", zipfile.ZIP_DEFLATED) as zf:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            zf.writestr(info, data)
        add_text(zf, "SHA256SUMS.TXT", sums)

    print(f"wrote {args.out} ({args.out.stat().st_size} bytes)")
    print(f"font sha256 verified: {EXPECTED_SHA256}")


if __name__ == "__main__":
    main()
