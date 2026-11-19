#!/usr/bin/env python3
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
"""Count the five seams that would need touching if the front end changed.

The point is not to freeze an API. It is to make coupling visible: each line
below answers "if I replace IMM32/Win32, how many call sites do I have to
rewrite?" Today every number is small. If one climbs, that is evidence that a
real abstraction is owed, not a reason to add an interface nobody uses (X2-f,
plan §4 mechanism M2).

Run directly:

    python3 tools/seams.py --root .

check_m1.py imports count() and fails when any seam exceeds its cap, so the
numbers cannot drift upward unnoticed in CI.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# (name, glob paths relative to root, call-site pattern, ceiling)
SEAMS = (
    (
        "engine",
        ("framework/src/host_key.cpp",),
        r"(?:state|session)\.engine->",
        4,
    ),
    (
        "scheme-policy",
        ("framework/src/host_key.cpp", "framework/src/key_route.cpp"),
        r"\bscheme\.|\bscheme_instance\(\)",
        12,
    ),
    (
        "ui-exit",
        (
            "framework/platform/win32/imm32/imm_entry.cpp",
            "framework/platform/win32/imm32/win32_host.cpp",
        ),
        r"\bUi[A-Z][A-Za-z0-9_]*\(",
        12,
    ),
    (
        "imm32-abi",
        ("framework/platform/win32/imm32/imm_entry.cpp",),
        r"\bWINAPI\s+(?:Ime[A-Za-z0-9_]+|NotifyIME)\s*\(",
        16,
    ),
    (
        "can-render",
        (
            "framework/src/host_key.cpp",
            "framework/platform/win32/imm32/win32_host.cpp",
        ),
        r"\.can_render\(|\bUiCanRender\(",
        2,
    ),
)


def strip_comments(text: str) -> str:
    """Drop comments so a prose mention cannot be counted as a call site."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def count(root: Path) -> dict[str, int]:
    """Return the current call-site count for each seam."""
    result: dict[str, int] = {}
    for name, paths, pattern, _ in SEAMS:
        body = ""
        for rel in paths:
            body += strip_comments((root / rel).read_text(encoding="utf-8"))
        result[name] = len(re.findall(pattern, body))
    return result


def over_cap(root: Path) -> dict[str, tuple[int, int]]:
    """Return {seam: (count, cap)} for every seam above its ceiling."""
    counts = count(root)
    return {name: (counts[name], cap)
            for name, _, _, cap in SEAMS if counts[name] > cap}


def main() -> int:
    ap = argparse.ArgumentParser(description="count front-end seam call sites")
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    args = ap.parse_args()
    root = args.root.resolve()
    counts = count(root)
    for name, _, _, cap in SEAMS:
        value = counts[name]
        print(f"{name:14s} {value:3d} / {cap:3d}  {'OK' if value <= cap else 'OVER CAP'}")
    bad = over_cap(root)
    if bad:
        print("seam cap exceeded: " + ", ".join(
            f"{name}={value}>{cap}" for name, (value, cap) in sorted(bad.items())))
        return 1
    print("all seams within cap")
    return 0


if __name__ == "__main__":
    sys.exit(main())
