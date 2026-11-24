#!/usr/bin/env python3
"""make_tools_iso.py - Build a small data ISO for fresh ReactOS test guests.

Fresh ReactOS installs have no wget/curl. This ISO carries a wget.exe plus the
current candidate installer and helper tools, so a guest can copy them off the
CD drive with Explorer (no network, no keyboard-injection by the host).

All file names are ASCII 8.3-style uppercase for maximum ReactOS compatibility.

Usage:
    python3 m0/vm/make_tools_iso.py [--out m0/vm/tools.iso]
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

import pycdlib

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent                 # m0/
SHARE = ROOT / "share"

# (source relative to m0/, ISO path, description)
PAYLOAD = [
    ("share/wget.exe",                         "/WGET.EXE",                  "command-line HTTP downloader"),
    ("share/rhash.exe",                        "/RHASH.EXE",                 "SHA256 checksum utility"),
    ("share/install-rospinyin-gui.exe",        "/INSTALL-ROSPINYIN-GUI.EXE", "candidate self-contained GUI installer"),
    ("share/install-rospinyin.exe",            "/INSTALL-ROSPINYIN.EXE",     "CLI installer (/step1../step4)"),
    # 0.4.17-dev nightly dropped DroidSansFallback.ttf from its font set.
    # Without a CJK fallback font, all Chinese in the GUI renders as '?' boxes
    # and the IME's tier0 ([fast] mode) glyph coverage is also lost.
    ("probe/reactos/Fonts/DroidSansFallback.ttf", "/DROID.TTF",
     "CJK fallback font missing from 0.4.17 nightly"),
    ("vm/droid-font.reg", "/DROID.REG",
     "registry entry that registers DROID.TTF with GDI"),
]

README = """ROSPINYIN TOOLS CD - for fresh ReactOS test guests
====================================================

This CD is a one-shot bootstrap: a clean ReactOS has no wget/curl, so copy
the tools you need from this CD to your C: drive with Explorer.

Contents (all files are in the CD root):
  WGET.EXE                   - download files over HTTP/FTP from a command prompt
  RHASH.EXE                  - compute SHA256 to verify downloads
  INSTALL-ROSPINYIN-GUI.EXE  - candidate self-contained GUI installer
  INSTALL-ROSPINYIN.EXE      - CLI installer (arguments /step1 .. /step4)
  DROID.TTF                  - CJK fallback font for 0.4.17-dev nightly
  DROID.REG                  - registers DROID.TTF with GDI (import via regedit)
  SHA256SUMS.TXT             - SHA256 of every binary on this CD

CJK FONT FIX FOR NIGHTLY GUESTS (Chinese shows as '?' without it):
  ReactOS 0.4.17-dev nightly no longer ships DroidSansFallback.ttf, so all
  Chinese text in the installer GUI renders as '?' boxes. 0.4.16 stable has
  this font and is unaffected. To fix a nightly guest:
  1. Copy DROID.TTF from D:\\ to C:\\ReactOS\\Fonts\\.
  2. Double-click DROID.REG to import it (regedit), confirm the prompt.
  3. REBOOT the guest, then re-open the installer - Chinese should render.
  This also restores the IME's [fast] mode glyph coverage on nightly.

How to bootstrap wget (then download anything):
  1. Open "My Computer" -> the CD drive (usually D:).
  2. Copy WGET.EXE (and RHASH.EXE) to C:\\ (e.g. drag into C:\\).
  3. Open "Command Prompt" from the desktop.
  4. cd \\
     wget http://10.0.2.2:8000/install-rospinyin-gui.exe
  5. Verify: rhash --sha256 install-rospinyin-gui.exe
     Expected SHA256 is listed in SHA256SUMS.TXT on this CD.

Host HTTP server: http://10.0.2.2:8000/  (SLIRP guest -> host 10.0.2.2).
The installer inside this CD and on the HTTP server are the same build.

Note: after copying, you may eject/swap this CD; the guest keeps its own copy.
"""


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(HERE / "tools.iso"))
    args = ap.parse_args()
    out = Path(args.out)

    # Read all payload bytes first (so we can build SHA256SUMS.TXT).
    blobs: list[tuple[str, bytes, str]] = []
    for src_name, iso_path, desc in PAYLOAD:
        src = ROOT / src_name
        if not src.is_file():
            raise SystemExit(f"missing source: {src}")
        blobs.append((iso_path, src.read_bytes(), desc))

    sums = "".join(f"{sha256(data)}  {iso_path.lstrip('/')}\n" for iso_path, data, _ in blobs)
    extra = [("/SHA256SUMS.TXT", sums.encode("utf-8"), "SHA256 checksums"),
             ("/README.TXT", README.encode("utf-8"), "usage notes")]

    import io

    iso = pycdlib.PyCdlib()
    iso.new(interchange_level=1, joliet=2, vol_ident="ROSPINYIN_TOOLS")
    # ISO9660 base name must be pure A-Z/0-9/_ and 8.3; Joliet keeps readable names.
    short_names = {
        "/WGET.EXE": "/WGET.EXE",
        "/RHASH.EXE": "/RHASH.EXE",
        "/INSTALL-ROSPINYIN-GUI.EXE": "/INSGUI.EXE",
        "/INSTALL-ROSPINYIN.EXE": "/INSCLI.EXE",
        "/SHA256SUMS.TXT": "/SHASUM.TXT",
        "/README.TXT": "/README.TXT",
        "/DROID.TTF": "/DROID.TTF",
        "/DROID.REG": "/DROID.REG",
    }
    for iso_path, data, _ in blobs + extra:
        iso.add_fp(io.BytesIO(data), len(data),
                   joliet_path=iso_path,
                   iso_path=short_names[iso_path])
    iso.write(str(out))
    iso.close()

    print(f"wrote {out} ({out.stat().st_size} bytes)")
    for iso_path, data, desc in blobs + extra:
        print(f"  {iso_path.lstrip('/'):28} {len(data):>9}  {sha256(data)[:12]}  {desc}")


if __name__ == "__main__":
    main()
