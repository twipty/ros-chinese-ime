#!/usr/bin/env python3
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
"""Documentation layout gate (no third-party dependencies).

Enforces the 2026-11-18 reorganisation, which moved 16 markdown files out of the
repository root into `docs/`. The rules exist because the problem was not "too
many files" but "no rule about where a new file goes": once one more report is
written next to README.md, the root is cluttered again and nobody notices.

Checks:

  1. the repository root holds no markdown other than README.md
  2. every markdown file under docs/ is listed in docs/README.md (the index)
  3. every relative markdown link outside xingma/ and vendor/ resolves
  4. docs/releases/ files are named <tag>.md, matching the release gate
  5. docs/README.md itself exists and keeps its index tables

Scope note: xingma/ is a read-only upstream drop and vendor/ is third-party
reference source; both keep their own conventions and are not checked.

Run directly or in CI (`Documentation layout` step in the engine-smoke job).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT_ALLOWED_MD = {"README.md"}
# Docs that live next to the code they describe on purpose (see docs/README.md §3).
SUBSYSTEM_MD_GLOBS = (
    "framework/README.md",
    "framework/ui/README.md",
    "pinyin/README.md",
    "libernt/PORTABILITY-ASSESSMENT.md",
    "m0/vm/*.md",
    "m0/vm/evidence/*/*.md",
)
SKIP_DIRS = {".git", "xingma", "vendor", "node_modules"}
SKIP_BUILD_PREFIX = "build"

LINK_RE = re.compile(r"\]\(([^)\s]+)")
RELEASE_RE = re.compile(r"^(alpha|beta)\d+\.\d+\.\d+\.md$")
DOUBLE_PAREN_RE = re.compile(r"\]\([^)\s]+\.md(?:#[^)\s]*)?\)\)")
FENCE_RE = re.compile(r"```.*?```", re.S)
INLINE_CODE_RE = re.compile(r"`[^`\n]*`")


def strip_code(text: str) -> str:
    """Drop fenced blocks and inline code spans.

    Prose legitimately *quotes* links (``[X](../a.md)``) and C++ signatures
    (`f](std::size_t)`) inside code spans; neither is a link to resolve.
    """
    text = FENCE_RE.sub("", text)
    return INLINE_CODE_RE.sub("", text)


class Checker:
    def __init__(self) -> None:
        self.total = 0
        self.failures: list[str] = []

    def check(self, label: str, ok: bool, detail: str = "") -> None:
        self.total += 1
        print(f"[PASS] {label}" if ok else f"[FAIL] {label}{f' -- {detail}' if detail else ''}")
        if not ok:
            self.failures.append(label)

    def summary(self) -> int:
        print(f"\n{self.total - len(self.failures)}/{self.total} checks passed")
        for name in self.failures:
            print(f"  failed: {name}")
        return 1 if self.failures else 0


def iter_md(root: Path) -> list[Path]:
    out = []
    for p in sorted(root.rglob("*.md")):
        rel = p.relative_to(root)
        if any(part in SKIP_DIRS for part in rel.parts):
            continue
        if rel.parts and rel.parts[0].startswith(SKIP_BUILD_PREFIX):
            continue
        out.append(p)
    return out


def broken_links(root: Path, p: Path) -> list[str]:
    bad = []
    text = strip_code(p.read_text(encoding="utf-8"))
    for target in LINK_RE.findall(text):
        if re.match(r"^(https?:|mailto:|#|/)", target):
            continue
        body = target.split("#", 1)[0]
        if not body:
            continue
        if not (p.parent / body).exists():
            bad.append(target)
    return bad


def run(root: Path) -> int:
    c = Checker()

    # 1. Root stays clean: README.md is the only markdown allowed there.
    stray = sorted(
        p.name for p in root.glob("*.md") if p.name not in ROOT_ALLOWED_MD
    )
    c.check("repository root holds no markdown other than README.md",
            not stray, f"stray: {', '.join(stray)}")

    # 2. The index lists every document under docs/.
    docs = root / "docs"
    index = docs / "README.md"
    c.check("docs/README.md (the document map) exists", index.is_file(), str(index))
    index_text = index.read_text(encoding="utf-8") if index.is_file() else ""
    unlisted = []
    if docs.is_dir():
        for p in sorted(docs.rglob("*.md")):
            if p == index:
                continue
            rel = p.relative_to(docs).as_posix()
            if p.name not in index_text and rel not in index_text:
                unlisted.append(rel)
    c.check("every document under docs/ is listed in the index",
            not unlisted, f"unlisted: {', '.join(unlisted)}")

    # 3. Relative links resolve everywhere we maintain prose.
    all_broken = {}
    for p in iter_md(root):
        bad = broken_links(root, p)
        if bad:
            all_broken[p.relative_to(root).as_posix()] = bad
    detail = "; ".join(f"{k}: {', '.join(v)}" for k, v in all_broken.items())
    c.check("every relative markdown link resolves", not all_broken, detail)

    # 4. Links are not double-closed. The 2026-11-18 migration rewrote 14 link
    #    targets with a regex whose group excluded the closing paren, so every
    #    rewritten link gained one `)`: `](x.md))`. The link checker above does
    #    not catch it (the target still resolves); the typo ships as text.
    doubled = []
    for p in iter_md(root):
        for lineno, line in enumerate(strip_code(p.read_text(encoding="utf-8")).splitlines(), 1):
            if DOUBLE_PAREN_RE.search(line):
                doubled.append(f"{p.relative_to(root).as_posix()}:{lineno}")
    c.check("no link is closed twice, e.g. '](x.md))'",
            not doubled, ", ".join(doubled))

    # 5. Release notes are named after a tag, because the gate maps tag -> file.
    rel_dir = docs / "releases"
    bad_names = []
    if rel_dir.is_dir():
        bad_names = [p.name for p in rel_dir.iterdir()
                     if p.is_file() and not RELEASE_RE.match(p.name)]
    c.check("docs/releases/<tag>.md naming matches the release gate",
            bool(rel_dir.is_dir()) and not bad_names,
            f"offending: {', '.join(bad_names) or 'missing directory'}")

    # 6. Subsystem docs stay next to their code (the exemption list above).
    literal = [g for g in SUBSYSTEM_MD_GLOBS if "*" not in g]
    missing = [g for g in literal if not (root / g).is_file()]
    c.check("subsystem documentation stays beside its code",
            not missing, f"missing: {', '.join(missing)}")

    return c.summary()


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) > 1:
        root = Path(sys.argv[1]).resolve()
    return run(root)


if __name__ == "__main__":
    raise SystemExit(main())
