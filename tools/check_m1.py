#!/usr/bin/env python3
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
"""M1 contract checker (no third-party dependencies).

Verifies WBS 2.2 and the M0 red lines without needing a Windows toolchain:

  * every new source file carries the MIT SPDX header
  * no Japanese-specific logic, names or kana survived the skeleton rewrite
  * all 16 mandatory IMM32 exports are declared, exported and implemented
  * the version resource satisfies the ImmLoadLayout checks (WBS 2.2.3)
  * DllMain stays harmless (M0 red line #1)
  * the installer never writes a preload/default layout (M0 red line #2)
    and exposes step-by-step registration (M0 red line #4)
  * the layout name is identical in the DLL, resources and installer

Run directly or via ctest (target name: m1-contract).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REQUIRED_EXPORTS = {
    "ImeConversionList", "ImeConfigure", "ImeDestroy", "ImeEscape",
    "ImeInquire", "ImeProcessKey", "ImeSelect", "ImeSetActiveContext",
    "ImeToAsciiEx", "NotifyIME", "ImeRegisterWord", "ImeUnregisterWord",
    "ImeGetRegisterWordStyle", "ImeEnumRegisterWord",
    "ImeSetCompositionString", "ImeGetImeMenuItems",
}

SCAN_DIRS = [
    "ime", "engine", "ui", "tests",
    "tools/install-rospinyin", "tools/syntax_stub",
]
SOURCE_SUFFIXES = {".cpp", ".h", ".hpp", ".rc", ".def"}
JAPANESE_WORDS = [
    "mzimeja", "MZ-IME", "Katayama", "片山",
    "Hiragana", "Katakana", "Roma-ji", "日本語", "カタカナ",
]
LAYOUT_TEXT = "中文(简体) - RosPinyin"


def strip_cpp_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def def_symbols(text: str) -> set[str]:
    names: set[str] = set()
    in_exports = False
    for raw in text.splitlines():
        line = raw.split(";")[0].strip()
        if not line:
            continue
        if line.upper() == "EXPORTS":
            in_exports = True
            continue
        if in_exports and not line.upper().startswith(("LIBRARY", "EXPORTS")):
            names.add(line.split()[0])
    return names


def function_body(src: str, signature_re: str) -> str | None:
    m = re.search(signature_re + r"\s*\{(.*?)\n\}", src, re.S)
    return m.group(1) if m else None


class Checker:
    def __init__(self) -> None:
        self.total = 0
        self.failures: list[str] = []

    def check(self, label: str, ok: bool, detail: str = "") -> None:
        self.total += 1
        if ok:
            print(f"[PASS] {label}")
        else:
            suffix = f" -- {detail}" if detail else ""
            print(f"[FAIL] {label}{suffix}")
            self.failures.append(label)

    def summary(self) -> int:
        print(f"\n{self.total - len(self.failures)}/{self.total} checks passed")
        if self.failures:
            for name in self.failures:
                print(f"  failed: {name}")
            return 1
        return 0


def read(root: Path, rel: str) -> str:
    return (root / rel).read_text(encoding="utf-8")


def run(root: Path) -> int:
    c = Checker()

    # 1. MIT headers on every new source file.
    missing_spdx: list[str] = []
    source_files: list[Path] = []
    for d in SCAN_DIRS:
        for p in sorted((root / d).rglob("*")):
            if p.is_file() and p.suffix in SOURCE_SUFFIXES:
                source_files.append(p)
                head = "\n".join(p.read_text(encoding="utf-8").splitlines()[:6])
                if "SPDX-License-Identifier: MIT" not in head:
                    missing_spdx.append(str(p.relative_to(root)))
    c.check("MIT SPDX header on all source files", not missing_spdx,
            ", ".join(missing_spdx[:5]))

    # 2. No Japanese-specific residue in the rewritten M1 tree.
    kana = re.compile(r"[\u3040-\u30ff\u31f0-\u31ff]")
    residue: list[str] = []
    for p in source_files:
        text = p.read_text(encoding="utf-8")
        hits = [w for w in JAPANESE_WORDS if w in text]
        if kana.search(text):
            hits.append("kana characters")
        if hits:
            residue.append(f"{p.relative_to(root)}: {', '.join(hits)}")
    c.check("no Japanese logic / names / kana in the M1 tree", not residue,
            "; ".join(residue[:5]))

    # 3. Export set.
    def_text = read(root, "ime/rospinyin.def")
    exports = def_symbols(def_text)
    c.check("rospinyin.def exports exactly the 16 IMM32 entry points",
            exports == REQUIRED_EXPORTS,
            f"missing={sorted(REQUIRED_EXPORTS - exports)} "
            f"extra={sorted(exports - REQUIRED_EXPORTS)}")

    abi = read(root, "ime/imm32_abi.h")
    entry = read(root, "ime/imm_entry.cpp")
    abi_missing = [n for n in REQUIRED_EXPORTS
                   if not re.search(r"\b" + n + r"\s*\(", abi)]
    impl_missing = [n for n in REQUIRED_EXPORTS
                    if not re.search(r"\bWINAPI\s+" + n + r"\s*\(", entry)]
    c.check("all 16 exports declared in imm32_abi.h", not abi_missing,
            ", ".join(sorted(abi_missing)))
    c.check("all 16 exports implemented in imm_entry.cpp", not impl_missing,
            ", ".join(sorted(impl_missing)))

    # 4. M1 passthrough semantics (WBS 2.4.1).
    process_body = function_body(entry, r"BOOL\s+WINAPI\s+ImeProcessKey\s*\([^)]*\)")
    ascii_body = function_body(entry, r"UINT\s+WINAPI\s+ImeToAsciiEx\s*\([^)]*\)")
    c.check("ImeProcessKey consumes no key (returns FALSE)",
            process_body is not None and "return FALSE;" in process_body)
    c.check("ImeToAsciiEx generates no messages (returns 0)",
            ascii_body is not None and "return 0;" in ascii_body)

    # 5. Version resource (WBS 2.2.3).
    rc = read(root, "ime/rospinyin_res.rc")
    version_checks = {
        "FILETYPE=VFT_DRV": bool(re.search(r"FILETYPE\s+0x3L?\b", rc)),
        "FILESUBTYPE=VFT2_DRV_INPUTMETHOD": bool(re.search(r"FILESUBTYPE\s+0xBL?\b", rc)),
        "zh-CN/Unicode StringFileInfo block": 'BLOCK "080404b0"' in rc,
        "Translation 0x804/1200": bool(re.search(r"Translation\",\s*0x804,\s*1200", rc)),
        "OriginalFilename=rospinyin.ime": 'VALUE "OriginalFilename", "rospinyin.ime' in rc,
        "FileDescription names RosPinyin": "RosPinyin" in rc,
    }
    for label, ok in version_checks.items():
        c.check(f"version resource: {label}", ok)

    # 6. DllMain harmlessness (M0 red line #1).
    dll = strip_cpp_comments(read(root, "ime/dllmain.cpp"))
    forbidden_dll = ["LoadLibrary", "RegOpenKey", "RegCreateKey", "RegQueryValue",
                     "CreateThread", "CreateWindow", "fopen", "ImmInstallIME",
                     "ImmGetContext", "LoadImage"]
    dll_hits = [w for w in forbidden_dll if w in dll]
    c.check("DllMain only registers the UI class (bare LoadLibrary is harmless)",
            not dll_hits, ", ".join(dll_hits))

    # 7. Installer red lines (M0 #2, #4 and D3).
    installer = read(root, "tools/install-rospinyin/install.cpp")
    code = strip_cpp_comments(installer)
    forbidden_installer = ["Preload", "ActivateKeyboardLayout",
                           "SystemParametersInfo", "LoadKeyboardLayoutW",
                           "E0100804"]
    install_hits = [w for w in forbidden_installer if w in code]
    c.check("installer never writes Preload / default layout / fixed HKL",
            not install_hits, ", ".join(install_hits))
    c.check("installer uses the HKL returned by ImmInstallIMEW",
            "ImmInstallIMEW" in code and "save_hkl(hkl)" in code)
    steps = ["/step1", "/step2", "/step3", "/step4", "/status", "/u", "/i"]
    c.check("installer exposes step-by-step registration commands",
            all(s in code for s in steps),
            ", ".join(s for s in steps if s not in code))

    # 8. One layout name across DLL, resources and installer.
    header = read(root, "ime/rospinyin.h")
    places = {
        "ime/rospinyin.h": LAYOUT_TEXT in header,
        "ime/rospinyin_res.rc": LAYOUT_TEXT in rc,
        "tools/install-rospinyin/install.cpp": LAYOUT_TEXT in installer,
    }
    c.check("layout text is identical in header, resources and installer",
            all(places.values()),
            ", ".join(k for k, v in places.items() if not v))

    # 9. Core files exist and the engine contract is host-testable.
    for rel in ["engine/include/rospinyin/engine.hpp",
                "engine/include/rospinyin/session.hpp",
                "engine/src/session.cpp",
                "ui/README.md",
                "tests/session_test.cpp",
                "tools/syntax_stub/windows.h"]:
        c.check(f"present: {rel}", (root / rel).is_file())

    c.check("UI server class name is centrally defined",
            "kUIServerClassName" in header and
            "kUIServerClassName" in read(root, "ime/ui_server.cpp"))

    return c.summary()


def main() -> int:
    parser = argparse.ArgumentParser(description="M1 contract checker")
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parent.parent,
                        help="repository root (defaults to the parent of tools/)")
    args = parser.parse_args()
    return run(args.root.resolve())


if __name__ == "__main__":
    sys.exit(main())
