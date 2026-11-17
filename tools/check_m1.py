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
    "framework", "pinyin", "ime", "tests",
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

    abi = read(root, "framework/host/imm32_abi.h")
    entry = read(root, "framework/host/imm_entry.cpp")
    abi_missing = [n for n in REQUIRED_EXPORTS
                   if not re.search(r"\b" + n + r"\s*\(", abi)]
    impl_missing = [n for n in REQUIRED_EXPORTS
                    if not re.search(r"\bWINAPI\s+" + n + r"\s*\(", entry)]
    c.check("all 16 exports declared in imm32_abi.h", not abi_missing,
            ", ".join(sorted(abi_missing)))
    c.check("all 16 exports implemented in imm_entry.cpp", not impl_missing,
            ", ".join(sorted(impl_missing)))

    # 4. IME key-handling contract.
    # M1 shipped an inert passthrough IME (WBS 2.4.1) where ImeProcessKey
    # always returned FALSE and ImeToAsciiEx returned 0. M2 replaces that
    # behaviour with real pinyin key handling: ImeProcessKey decides key
    # ownership via should_consume_key(), and ImeToAsciiEx dispatches to
    # process_key() which emits WM_IME_CHAR TRANSMESSAGEs on commit. The
    # historical M1 snapshot is no longer the shipping behaviour, so assert
    # the live M2 wiring instead of the obsolete unconditional returns.
    process_body = function_body(entry, r"BOOL\s+WINAPI\s+ImeProcessKey\s*\([^)]*\)")
    ascii_body = function_body(entry, r"UINT\s+WINAPI\s+ImeToAsciiEx\s*\([^)]*\)")
    c.check("ImeProcessKey delegates key ownership to should_consume_key",
            process_body is not None and "should_consume_key" in process_body)
    # The M1 stub was "return FALSE;" and nothing else. Detect that shape
    # precisely: the live function must return the decision, not a constant.
    c.check("ImeProcessKey no longer unconditionally returns FALSE",
            process_body is not None and "consume ? TRUE : FALSE" in process_body)
    # Key-up must be declined here and nowhere else: ReactOS routes WM_KEYUP
    # through IntImmProcessKey too, and ImeToAsciiEx gets no lParam, so a
    # claimed key-up feeds every keystroke to the engine twice (doubled or
    # missing characters). See HANDOVER §11.4.
    c.check("ImeProcessKey declines key-up (ImeToAsciiEx cannot see lParam)",
            process_body is not None and "kKeyUpBit" in process_body)
    c.check("ImeToAsciiEx dispatches key processing to process_key",
            ascii_body is not None and "process_key" in ascii_body)
    c.check("M2 commits text through WM_IME_CHAR TRANSMESSAGEs",
            "WM_IME_CHAR" in entry and "emit_text" in entry)
    c.check("G1: commit path handles multi-codepoint text (words)",
            "emit_text(lpTransMsgList, commit_text)" in entry and
            "chosen->text[0]" not in entry)
    # Audit 2026-11-17 §2.1 (P0): ImeToAsciiEx takes a TRANSMSGLIST whose
    # uMsgCount header is pre-filled with the capacity; messages live at
    # ->TransMsg[0]. Writing at offset 0 clobbers the header and shifts
    # every message, which is exactly what broke composition on the VM.
    abi = read(root, "framework/host/imm32_abi.h")
    c.check("G1 ABI: ImeToAsciiEx takes TRANSMSGLIST, not a bare TRANSMSG array",
            "struct TransMsgList" in abi and
            "rpimm::TransMsgList* lpTransMsgList" in entry and
            "rpimm::TransMsg* lpTransMsg" not in entry)
    c.check("G1 ABI: messages written via list->TransMsg with uMsgCount capacity",
            "lpTransMsgList->TransMsg" in entry and
            "lpTransMsgList->uMsgCount" in entry and
            "offsetof(rpimm_list, TransMsg) == sizeof(UINT)" in entry)
    # G1 hardening: the emission logic is host-testable in imfw-engine, and
    # the host delegates instead of re-implementing the UTF-16 conversion.
    c.check("G1: WM_IME_CHAR emission lives in imfw-engine (host-tested)",
            (root / "framework/include/imfw/emit_text.hpp").is_file() and
            (root / "framework/src/emit_text.cpp").is_file() and
            "imfw::emit_text" in entry and
            "sizeof(imfw::TransMsgSlot) == sizeof(rpimm::TransMsg)" in entry)

    # 4b. Framework gap dispositions (xingma/XINGMA-PLAN.md 3.4).
    configure_body = function_body(entry, r"BOOL\s+WINAPI\s+ImeConfigure\s*\([^)]*\)")
    c.check("G3: ImeConfigure delegates to the Scheme configure hook",
            configure_body is not None and
            "scheme_instance().configure" in configure_body)
    c.check("G3+G2: successful configure reloads data and sessions",
            configure_body is not None and
            "reload_sessions()" in configure_body and
            "return TRUE" in configure_body)
    c.check("G4: selection/page key policy comes from the Scheme",
            "scheme.is_selection_key(ch)" in entry and
            "scheme.page_key(ch)" in entry and
            "bool is_page_key" not in entry)
    store_hpp = read(root, "framework/host/session_store.hpp")
    c.check("G2: sessions reload at runtime via reload_sessions",
            "void reload_sessions()" in store_hpp)
    scheme_hpp = read(root, "framework/include/imfw/scheme.hpp")
    c.check("G2: framework no longer passes a code table to engines",
            "create_engine() const" in scheme_hpp and
            "CodeTable&" not in scheme_hpp and
            "reload_data" in scheme_hpp)
    c.check("G2: the framework host holds no shared code table",
            "shared_table" not in store_hpp and
            "CodeTable" not in store_hpp and
            not (root / "framework/host/dat_loader.hpp").is_file() and
            not (root / "framework/host/dat_loader.cpp").is_file())
    c.check("G2: reload lets the scheme refresh its data (reload_data hook)",
            "scheme_instance().reload_data()" in
            read(root, "framework/host/session_store.cpp"))
    # Audit 2026-11-17 §2.2: reload must refresh data BEFORE dropping
    # sessions (data-first), so a throwing reload_data leaves the old
    # sessions consistent with the old data.
    reload_body = function_body(
        read(root, "framework/host/session_store.cpp"),
        r"void\s+reload\s*\(\s*\)")
    c.check("G2: reload refreshes data before dropping sessions (data-first)",
            reload_body is not None and
            reload_body.index("reload_data()") < reload_body.index("clear()"))
    c.check("G2: the pinyin scheme owns its data (dat_source)",
            (root / "pinyin/include/rospinyin/dat_source.hpp").is_file() and
            (root / "pinyin/src/dat_source.cpp").is_file() and
            (root / "pinyin/host/dat_source_win.cpp").is_file() and
            "load_pinyin_table" in read(root, "pinyin/src/scheme.cpp") and
            "dat_filename" not in read(root, "pinyin/src/scheme.cpp"))
    composition_ui = read(root, "framework/ui/composition_window.cpp")
    c.check("G4: compositions decode as UTF-8 (CJK allowed)",
            "utf8_to_wide" in entry and "utf8_to_wide" in composition_ui)
    # Regression guard: PinyinSession::feed() is a no-op until on_select(true),
    # so a missing activation makes the IME silently swallow every pinyin key.
    select_body = function_body(entry, r"BOOL\s+WINAPI\s+ImeSelect\s*\([^)]*\)")
    c.check("ImeSelect activates the engine session on select",
            select_body is not None and "on_select(true)" in select_body)

    # 5. Version resource (WBS 2.2.3).
    rc = read(root, "ime/rospinyin_res.rc")
    version_checks = {
        "FILETYPE=VFT_DRV": bool(re.search(r"FILETYPE\s+0x3L?\b", rc)),
        "FILESUBTYPE=VFT2_DRV_INPUTMETHOD": bool(re.search(r"FILESUBTYPE\s+0xBL?\b", rc)),
        "zh-CN/Unicode StringFileInfo block": 'BLOCK "080404b0"' in rc,
        # The langid now comes from a macro so a 0409 A/B control build can be
        # produced; the zh-CN default is asserted by the gate below.
        "Translation langid/1200 from ROSPINYIN_TRANSLATION_LANGID":
            bool(re.search(r"Translation\",\s*ROSPINYIN_TRANSLATION_LANGID,\s*1200", rc)),
        "OriginalFilename=rospinyin.ime": 'VALUE "OriginalFilename", "rospinyin.ime' in rc,
        "FileDescription names RosPinyin": "RosPinyin" in rc,
    }
    for label, ok in version_checks.items():
        c.check(f"version resource: {label}", ok)

    # 6. DllMain harmlessness (M0 red line #1).
    dll = strip_cpp_comments(read(root, "framework/host/dllmain.cpp"))
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
    for rel in ["framework/include/imfw/engine.hpp",
                "framework/include/imfw/scheme.hpp",
                "framework/include/imfw/session.hpp",
                "framework/src/engine.cpp",
                "framework/host/dllmain.cpp",
                "framework/host/imm_entry.cpp",
                "framework/ui/README.md",
                "pinyin/include/rospinyin/scheme.hpp",
                "pinyin/src/scheme.cpp",
                "ime/scheme_reg.cpp",
                "tests/session_test.cpp",
                "tools/syntax_stub/windows.h"]:
        c.check(f"present: {rel}", (root / rel).is_file())

    # The reusable framework must stay free of pinyin knowledge: the plugin
    # seam is imfw::Scheme (see framework/README.md).
    scheme_cpp = read(root, "pinyin/src/scheme.cpp")
    ui_server = read(root, "framework/host/ui_server.cpp")
    c.check("UI server class name comes from the Scheme",
            "ui_server_class" in ui_server and
            "L\"RosPinyinUISrv\"" in scheme_cpp)
    # IMM32 truncates the ImeInquire class name into IMEINFOEX.wszUIClass[16]
    # and then looks it up; 16+ characters means imm32 rejects the IME with
    # fLoadFlag=1 and nothing else to show for it. Guard the contract here so
    # the failure cannot come back through a rename.
    uiclass = re.search(r'kUiServerClass\[\]\s*=\s*L"([^"]*)"', scheme_cpp)
    c.check("ImeInquire UI class name fits IMEINFOEX.wszUIClass[16]",
            uiclass is not None and len(uiclass.group(1)) <= 15,
            f"len={len(uiclass.group(1))}" if uiclass else "kUiServerClass not found")
    c.check("scheme_reg hands the framework its Scheme",
            "scheme_instance" in read(root, "ime/scheme_reg.cpp") and
            "PinyinScheme" in read(root, "ime/scheme_reg.cpp"))
    # The layout's language (which "keyboard" ImmInstallIMEW files it under)
    # comes solely from the first VarFileInfo\Translation entry, so it is a
    # build-time fact, not a runtime one. A Simplified Chinese IME must ship as
    # zh-CN (0804); 0409 exists only as an A/B control against the system's
    # built-in Japanese IME. Guard the default so a control build cannot ship.
    ime_cmake = read(root, "ime/CMakeLists.txt")
    res_rc = read(root, "ime/rospinyin_res.rc")
    c.check("ime version resource defaults to zh-CN, 0409 stays a control build",
            'set(ROSPINYIN_IME_LANGID "0804" CACHE STRING' in ime_cmake and
            "ROSPINYIN_IME_LANG_EN" in res_rc and
            "ROSPINYIN_TRANSLATION_LANGID 0x804" in res_rc)
    framework_pinyin_refs = []
    for rel in ["framework/src/code_table.cpp", "framework/host/imm_entry.cpp",
                "framework/src/engine.cpp", "framework/host/session_store.cpp"]:
        # The diagnostic channel is compiled in under ROSPINYIN_TRACE (the
        # product name, not the language); matching that against "pinyin"
        # would make this gate fire on the trace build alone, which is noise.
        # Strip the product token before looking for language knowledge.
        text = strip_cpp_comments(read(root, rel)).replace("ROSPINYIN", "")
        text = text.replace("rospinyin", "")
        if "pinyin" in text.lower():
            framework_pinyin_refs.append(rel)
    c.check("framework core contains no pinyin knowledge",
            not framework_pinyin_refs, ", ".join(framework_pinyin_refs))

    # 10. M2 invariants that cost a whole test round when they regress.
    #
    # P0-3: the composition window measured its own text and stored the new
    # width, but never handed it to Win32, so "zhongguo" stayed clipped to the
    # width of "zhong". Geometry (position AND size) must have exactly one
    # owner: ime_ui's place_windows(), which knows the caret anchor and the
    # screen edges.
    comp_cpp = strip_cpp_comments(read(root, "framework/ui/composition_window.cpp"))
    ime_ui_cpp = strip_cpp_comments(read(root, "framework/ui/ime_ui.cpp"))
    c.check("composition window geometry owned by place_windows (no SetWindowPos)",
            "SetWindowPos" not in comp_cpp and
            "apply_geometry" in ime_ui_cpp)
    # ...and the per-keystroke repaint must be conditional, otherwise every
    # IMM32 re-entry (1-3 per key) flickers the window.
    c.check("composition repaint is conditional on content change",
            "content_changed" in comp_cpp)

    # P1-1: an .ime without its .dat shows an empty candidate window. The
    # installer must place both and remove both.
    installer_code = strip_cpp_comments(read(
        root, "tools/install-rospinyin/install.cpp"))
    c.check("installer ships and removes the data table too",
            installer_code.count("kDatFileName") >= 3 and
            "kDatFileName" in installer_code.split("int uninstall()")[1])

    # P0-2: the OS-side open flag is aligned at activation and never used as a
    # key gate (ReactOS hands keys to ImeProcessKey whatever fOpen says, so
    # gating would disable the IME whenever the flag is stale).
    entry_code = strip_cpp_comments(entry)
    gate = function_body(entry_code, r"bool should_consume_key\([^)]*\)")
    c.check("open status is aligned once, never used as a key gate",
            "AlignOpenStatus(" in entry_code and
            gate is not None and "OpenStatus" not in gate,
            "should_consume_key not found" if gate is None else "")

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
