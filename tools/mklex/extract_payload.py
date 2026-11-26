#!/usr/bin/env python3
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
"""Restore the embedded word list from the checked-in generated header.

The word-list binary is deliberately NOT stored in git; pack_payload.py
encodes it as zlib+hex escapes inside tools/install-rospinyin/payload_base64.hpp.
CI therefore has no source .dat to publish as a release asset — this script
reconstructs it byte-for-byte from the header and pins the size/SHA-256 before
writing anything out.

Round-trip contract (verified here):
  extract_payload.py -> out.dat == m0/share/rospinyin-lexicon.dat (when present)
  sha256(out.dat) == a89ec5e5...c1c6a, size == 6,530,601
"""

from __future__ import annotations

import argparse
import hashlib
import re
import zlib
from pathlib import Path

EXPECTED_SIZE = 6_530_601
EXPECTED_SHA256 = (
    "a89ec5e500c47c121687e9789fade9b8187c1ec2b69d25d5e82f1a680cfc1c6a"
)


def extract(header: Path, output: Path) -> None:
    text = header.read_text(encoding="utf-8")
    match = re.search(
        r"#define\s+ROSPINYIN_LEX_PAYLOAD_BYTES\s+(.*?)(?=\n\s*(?:#define|#if|#endif|\Z))",
        text,
        re.S,
    )
    if not match:
        raise SystemExit(
            "LEX payload macro not found — regenerate the header with "
            "tools/install-rospinyin/pack_payload.py"
        )
    body = match.group(1)
    byte_text = "".join(re.findall(r"0x([0-9A-Fa-f]{2})", body))
    if len(byte_text) % 2:
        raise SystemExit("LEX payload hex has an odd number of digits")
    compressed = bytes.fromhex(byte_text)
    try:
        raw = zlib.decompress(compressed)
    except zlib.error as exc:
        raise SystemExit(f"LEX payload is not a valid zlib stream: {exc}")

    if len(raw) != EXPECTED_SIZE:
        raise SystemExit(
            f"LEX size mismatch: got {len(raw)}, expected {EXPECTED_SIZE}")
    sha = hashlib.sha256(raw).hexdigest()
    if sha != EXPECTED_SHA256:
        raise SystemExit(
            f"LEX SHA-256 mismatch: got {sha}, expected {EXPECTED_SHA256}")

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(raw)
    print(f"restored {output} ({len(raw)} bytes, sha256={sha})")


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--header",
        type=Path,
        default=root / "tools/install-rospinyin/payload_base64.hpp",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=root / "out/rospinyin-lexicon.dat",
    )
    args = parser.parse_args()
    extract(args.header, args.output)


if __name__ == "__main__":
    main()
