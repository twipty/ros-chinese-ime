#!/usr/bin/env python3
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
"""Deployment artifact ledger: what exactly did I ship?

The recurring failure in this project is not "the build is wrong", it is
"the file I tested is not the file I built".  Three shapes of that:

  * the shared folder still holds the previous build;
  * two builds differ but happen to have the same byte count
    (`rospinyin.ime` varies by a few hundred bytes per refactor, so a
    size-only check is a coin flip);
  * the guest copies something by name and nobody checks where it came from.

So this tool never trusts names or timestamps: every artifact is hashed.
Timestamps in particular are useless once a file crosses into ReactOS,
whose clock drifts (see HANDOVER 6.4.1).

Usage:

  python3 tools/artifact_hashes.py
      List every .ime / .dat / deploy .exe under the repo plus the shared
      folder, newest first, with size and sha256.

  python3 tools/artifact_hashes.py --expect build-x2a1-pinyin/ime/rospinyin.ime
      Mark that file as the intended version (★) and check the shared
      folder against it.  Exit code 1 if the shared copy does not match.

  python3 tools/artifact_hashes.py --share-dir ~/.workbuddy/tmp/vmshare
      Point at another shared folder.

  python3 tools/artifact_hashes.py --json
      Machine-readable output.

  python3 tools/artifact_hashes.py --write-txt
      Additionally drop `VERSIONS.txt` into the shared folder so the
      guest side can compare byte counts with `dir` (ReactOS has no
      sha256 tool, so size is all it can check -- which is exactly why
      this side has to be exact).

No third-party dependencies.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_SHARE = Path.home() / ".workbuddy" / "tmp" / "vmshare"

# Extensions worth versioning.  Everything else in build*/ is object files.
SUFFIXES = {".ime", ".dat"}
# Deployment binaries, matched by name because a build tree holds hundreds of
# unrelated .exe files.
EXE_NAMES = {"install-rospinyin.exe", "rhash.exe", "imhost.exe", "imhost_dyn.exe"}
SKIP_DIRS = {".git", "xingma", "vendor", "node_modules"}


@dataclass
class Artifact:
    path: Path
    size: int
    sha256: str
    mtime: float

    @property
    def short(self) -> str:
        return self.sha256[:16]

    def when(self) -> str:
        return datetime.fromtimestamp(self.mtime).strftime("%m-%d %H:%M")


def wanted(path: Path) -> bool:
    name = path.name.lower()
    if name.startswith("rospinyin") and path.suffix.lower() in SUFFIXES:
        return True
    return name in EXE_NAMES


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def collect(root: Path, recursive: bool = True, with_exe: bool = True) -> list[Artifact]:
    out: list[Artifact] = []
    if not root.is_dir():
        return out
    it = root.rglob("*") if recursive else root.glob("*")
    for path in it:
        if not path.is_file() or not wanted(path):
            continue
        if not with_exe and path.suffix.lower() == ".exe":
            continue
        if any(part in SKIP_DIRS for part in path.parts):
            continue
        st = path.stat()
        out.append(Artifact(path, st.st_size, sha256_of(path), st.st_mtime))
    out.sort(key=lambda a: a.mtime, reverse=True)
    return out


def rel(path: Path) -> str:
    for base in (REPO, Path.home()):
        try:
            return str(path.relative_to(base))
        except ValueError:
            continue
    return str(path)


def main() -> int:
    ap = argparse.ArgumentParser(description="Hash and reconcile deploy artifacts.")
    ap.add_argument("--root", default=str(REPO), help="repo root to scan (default: this repo)")
    ap.add_argument("--share-dir", default=str(DEFAULT_SHARE), help="shared folder served to the guest")
    ap.add_argument("--expect", default="", help="path (or suffix of a path) of the intended build")
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    ap.add_argument("--with-exe", action="store_true", help="also list deploy .exe files")
    ap.add_argument("--share-all", action="store_true",
                    help="scan the shared folder recursively (default: top level only)")
    ap.add_argument("--write-txt", action="store_true", help="write VERSIONS.txt into the shared folder")
    args = ap.parse_args()

    root = Path(args.root)
    share = Path(args.share_dir).expanduser()

    repo_files = collect(root, recursive=True, with_exe=args.with_exe)
    # The guest fetches from the top of the share; older releases live in
    # dated subfolders and would only add noise.
    share_files = collect(share, recursive=args.share_all, with_exe=args.with_exe)

    # Which artifact is the intended one?
    expected: Artifact | None = None
    if args.expect:
        key = args.expect.replace("\\", "/")
        cands = [a for a in repo_files if key in a.path.as_posix()]
        if not cands:
            print(f"[error] no artifact matches --expect {args.expect!r}", file=sys.stderr)
            return 2
        expected = cands[0]

    # Same byte count, different content: the trap a size check cannot see.
    by_size: dict[int, set[str]] = {}
    for a in repo_files + share_files:
        by_size.setdefault(a.size, set()).add(a.sha256)
    collisions = {size: hashes for size, hashes in by_size.items() if len(hashes) > 1}

    # Shared copies matched to the exact build they came from.
    repo_by_hash = {a.sha256: a for a in repo_files}
    share_rows = []
    for a in share_files:
        peer = repo_by_hash.get(a.sha256)
        share_rows.append((a, peer, "match" if peer else "orphan"))

    mismatches = 0
    if expected is not None:
        for a, _peer, _state in share_rows:
            if a.path.suffix.lower() in SUFFIXES and a.path.name.lower() == expected.path.name.lower():
                # Content, not provenance: the share may hold a legitimate
                # older build of the same name, which is exactly the mistake.
                if a.sha256 != expected.sha256:
                    mismatches += 1

    if args.json:
        print(json.dumps({
            "expect": asdict(expected) | {"path": rel(expected.path)} if expected else None,
            "repo": [asdict(a) | {"path": rel(a.path)} for a in repo_files],
            "share": [
                {
                    "path": rel(a.path), "size": a.size, "sha256": a.sha256,
                    "mtime": a.mtime, "peer": rel(peer.path) if peer else None, "state": state,
                }
                for a, peer, state in share_rows
            ],
            "size_collisions": {str(k): sorted(v) for k, v in collisions.items()},
            "mismatch": mismatches,
        }, ensure_ascii=False, indent=2))
        return 1 if mismatches else 0

    width = max((len(f"{a.size:,}") for a in repo_files + share_files), default=5)

    print(f"repo  {root}")
    print(f"share {share}{'' if share.is_dir() else '   (missing)'}\n")

    print("== 构建产物（含共享副本，按修改时间降序）==")
    if not repo_files and not share_files:
        print("  (no artifacts found -- build first)")
    for a in repo_files:
        mark = "★" if expected and a.path == expected.path else " "
        print(f" {mark} {a.size:>{width},}  {a.short}  {rel(a.path):<52} {a.when()}")
    for a, peer, state in share_rows:
        if state == "match":
            tag = "= " + rel(peer.path)
        else:
            tag = "NOT from any build here"
        print(f"   {a.size:>{width},}  {a.short}  {rel(a.path):<52} {a.when()}  <- {tag}")

    if expected is not None:
        print(f"\n★ 预期版本：{rel(expected.path)}  {expected.size:,} B  sha256 {expected.sha256}")
        for a, _peer, _state in share_rows:
            if a.path.name.lower() == expected.path.name.lower() and a.path.suffix.lower() in SUFFIXES:
                if a.sha256 == expected.sha256:
                    print(f"  ✓ 共享目录的 {a.path.name} 与 ★ 逐字节一致")
                else:
                    print(f"  ✗ 共享目录的 {a.path.name} 不是 ★：{a.size:,} B {a.short}"
                          f"（guest 若从这里取，装的就是另一版）")
        if expected.path.suffix.lower() == ".ime":
            print("\n  guest 内核对（ReactOS 没有 sha256sum，用我们自己的 rhash.exe）：")
            print(r"    rhash.exe C:\ReactOS\system32\rospinyin.ime")
            print(f"    期望 {expected.size:,} B 与上面的 sha256；")
            print("    若 rhash.exe 不在 guest 里，先 wget http://10.0.2.2:8000/rhash.exe")

    if collisions:
        print("\n⚠ 同字节数、不同内容 —— 只比大小的检查在这里会失效：")
        for size, hashes in sorted(collisions.items()):
            print(f"  {size:,} B: {len(hashes)} 种内容 ({', '.join(h[:12] for h in sorted(hashes))})")

    if args.write_txt and share.is_dir():
        txt = share / "VERSIONS.txt"
        lines = [
            "RosPinyin deploy artifacts -- compare with `dir` inside the guest",
            "ReactOS has no sha256; byte count is the only guest-side check.",
            "mtime is NOT reliable inside ReactOS (clock drifts).",
            "",
            f"generated {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            "",
        ]
        for a in repo_files[:20]:
            star = "*" if expected and a.path == expected.path else " "
            lines.append(f"{star} {a.size:>12,}  {a.short}  {rel(a.path)}")
        txt.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"\n已写 {rel(txt)}")

    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
