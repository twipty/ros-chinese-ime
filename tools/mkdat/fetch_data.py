#!/usr/bin/env python3
"""Fetch the pinned raw inputs for mkdat, with sha256 verification.

All conclusions come from docs/milestones/m2/M2-DATA-PLAN.md. The files are NOT checked into
the repository (8 MB zip); this script makes the build reproducible instead.

Usage:
    python3 tools/mkdat/fetch_data.py --out-dir build/data
    python3 tools/mkdat/fetch_data.py --out-dir build/data --force

Produces:
    <out-dir>/Unihan_Readings.txt
    <out-dir>/hanziDB.csv
"""

import argparse
import hashlib
import shutil
import sys
import urllib.request
import zipfile
from pathlib import Path

# Pinned Unicode version (18.0.0). 'latest' is only a fallback in case the
# versioned path is moved; the sha256 check still guards reproducibility.
UNIHAN_URLS = (
    "https://www.unicode.org/Public/18.0.0/ucd/Unihan.zip",
    "https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip",
)
UNIHAN_SHA256 = "4c93ea9c1f636451729a840978f1667a53886af37ba854fdcce109721c63d43e"

HANZIDB_URL = (
    "https://raw.githubusercontent.com/rtang18/hanziDB.csv/master/data/hanziDB.csv"
)
HANZIDB_SHA256 = "03f809543a2bd0d32c8f65f98567cbaa01527f2eefbdb0ea214a1abddc0b03a0"

READINGS_MEMBER = "Unihan_Readings.txt"


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download(url: str, destination: Path, expected_sha256: str, force: bool) -> None:
    if destination.exists() and not force:
        actual = sha256_of(destination)
        if actual == expected_sha256:
            print(f"reuse {destination.name} ({actual[:12]}...)")
            return
        print(f"warning: {destination.name} sha256 mismatch; re-downloading")

    print(f"download {url}")
    request = urllib.request.Request(url, headers={"User-Agent": "RosPinyin-mkdat/1.0"})
    with urllib.request.urlopen(request, timeout=120) as response:
        data = response.read()

    actual = hashlib.sha256(data).hexdigest()
    if actual != expected_sha256:
        print(
            f"error: sha256 mismatch for {url}\n"
            f"  expected {expected_sha256}\n"
            f"  actual   {actual}",
            file=sys.stderr,
        )
        sys.exit(1)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)
    print(f"  ok: {len(data)} bytes, sha256 {actual[:12]}...")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", required=True, type=Path)
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--keep-zip", action="store_true",
                        help="keep Unihan.zip in the output directory")
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    zip_path = args.out_dir / "Unihan.zip"

    if zip_path.exists() and sha256_of(zip_path) == UNIHAN_SHA256 and not args.force:
        print(f"reuse Unihan.zip ({UNIHAN_SHA256[:12]}...)")
    else:
        last_error = None
        for url in UNIHAN_URLS:
            try:
                download(url, zip_path, UNIHAN_SHA256, args.force)
                last_error = None
                break
            except Exception as error:  # noqa: BLE001 - report and try fallback
                last_error = error
                print(f"warning: {url} failed: {error}", file=sys.stderr)
        if last_error is not None:
            print("error: cannot fetch Unihan.zip from any configured URL",
                  file=sys.stderr)
            return 1

    readings_path = args.out_dir / READINGS_MEMBER
    with zipfile.ZipFile(zip_path) as archive:
        if READINGS_MEMBER not in archive.namelist():
            print(f"error: {READINGS_MEMBER} missing from Unihan.zip",
                  file=sys.stderr)
            return 1
        with archive.open(READINGS_MEMBER) as source, readings_path.open("wb") as target:
            shutil.copyfileobj(source, target)
    print(f"extracted {readings_path.name} ({readings_path.stat().st_size} bytes)")

    csv_path = args.out_dir / "hanziDB.csv"
    download(HANZIDB_URL, csv_path, HANZIDB_SHA256, args.force)

    if not args.keep_zip:
        zip_path.unlink(missing_ok=True)

    print("raw inputs ready in", args.out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
