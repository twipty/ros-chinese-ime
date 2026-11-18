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
  * the loader path stays quiet: DLL_PROCESS_ATTACH may only call the class
    registration allowlist, that registration may only call RegisterClassExW,
    and ImeInquire must be able to refuse (A1 / A1b)
  * the installer's registry writes stay inside a two-key allowlist and it
    never sets the default input language (deployment preference)
  * the installer exposes step-by-step registration (M0 red line #4)
  * the layout name is identical in the DLL, resources and installer

Why allowlists instead of blocklists: until 2026-11-18 the contract read "the
installer never writes Preload", implemented as a scan for the substring
"Preload". That asserted a property nobody can honour (ReactOS writes the
Preload entry itself inside ImmInstallIMEW) while a new spelling such as
L"Keyboard Layout\\" L"Preload" would have slipped through. The N/N' probe
runs showed delivery is harmless and loader-path behaviour is what matters,
so the checks below assert what call sites are reachable, not what words
appear in the file.

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


TRACE_GUARD_RE = re.compile(
    r"#\s*if\s+defined\s*\(\s*ROSPINYIN_TRACE\s*\)(.*?)#\s*endif", re.S)


def strip_trace_blocks(text: str) -> str:
    """Remove `#if defined(ROSPINYIN_TRACE) ... #endif` sections.

    The diagnostic trace opens a file from DllMain, which is fine because it
    only exists in the trace build. Shipping behaviour is what the loader-path
    checks assert, so the guarded blocks must be removed before scanning --
    otherwise the checker would fail on code that the release build never
    compiles.
    """
    return TRACE_GUARD_RE.sub("", text)


def calls_in(src: str) -> set[str]:
    """Identifiers used as `Name(` -- a cheap call-site extractor.

    Only capitalised names are collected, which is what Win32 APIs and our own
    free functions look like; language keywords (`if`, `switch`, `break`) and
    `return` are lowercase and therefore ignored.
    """
    return set(re.findall(r"\b([A-Z][A-Za-z0-9_]*)\s*\(", src))


def call_arguments(src: str, open_paren: int) -> str:
    """Text between the parenthesis at `open_paren` and its match."""
    depth = 0
    for i in range(open_paren, len(src)):
        ch = src[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return src[open_paren + 1:i]
    return ""


def split_top_level(text: str) -> list[str]:
    parts: list[str] = []
    depth = 0
    current: list[str] = []
    for ch in text:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
        else:
            current.append(ch)
    parts.append("".join(current).strip())
    return parts


REG_KEY_CALL_RE = re.compile(r"\bReg(?:Open|Create|Delete)Key(?:Ex)?[AW]?\s*\(")


def reg_key_writes(src: str) -> list[tuple[str, str]]:
    """(hive, subkey-expression) for every Reg{Open,Create,Delete}Key call."""
    out: list[tuple[str, str]] = []
    for m in REG_KEY_CALL_RE.finditer(src):
        args = split_top_level(call_arguments(src, m.end() - 1))
        if len(args) >= 2:
            out.append((args[0].strip(), args[1].strip()))
    return out


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

    abi = read(root, "framework/platform/win32/imm32/imm32_abi.h")
    entry = read(root, "framework/platform/win32/imm32/imm_entry.cpp")
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
            "apply_commit" in entry and
            "emit_text(lpTransMsgList, text)" in entry and
            "chosen->text[0]" not in entry)
    # Audit 2026-11-17 §2.1 (P0): ImeToAsciiEx takes a TRANSMSGLIST whose
    # uMsgCount header is pre-filled with the capacity; messages live at
    # ->TransMsg[0]. Writing at offset 0 clobbers the header and shifts
    # every message, which is exactly what broke composition on the VM.
    abi = read(root, "framework/platform/win32/imm32/imm32_abi.h")
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
    store_hpp = read(root, "framework/platform/win32/imm32/session_store.hpp")
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
            not (root / "framework/platform/win32/imm32/dat_loader.hpp").is_file() and
            not (root / "framework/platform/win32/imm32/dat_loader.cpp").is_file())
    c.check("G2: reload lets the scheme refresh its data (reload_data hook)",
            "scheme_instance().reload_data()" in
            read(root, "framework/platform/win32/imm32/session_store.cpp"))
    # Audit 2026-11-17 §2.2: reload must refresh data BEFORE dropping
    # sessions (data-first), so a throwing reload_data leaves the old
    # sessions consistent with the old data.
    reload_body = function_body(
        read(root, "framework/platform/win32/imm32/session_store.cpp"),
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
    composition_ui = read(root, "framework/platform/win32/ui/composition_window.cpp")
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

    # 6. Loader-path behaviour (A1 / A1b, replacing the M0 #1 word blocklist).
    #    The N/N' probe runs (6 boots, release binary) showed that being
    #    delivered into the logon path is harmless; what hurts is doing
    #    failable work there and swallowing the failure. So the assertion is
    #    now "what may the attach path call", which also covers calls added
    #    later under a different spelling.
    dllmain = strip_cpp_comments(
        strip_trace_blocks(read(root, "framework/platform/win32/imm32/dllmain.cpp")))
    attach = dllmain.split("DLL_PROCESS_ATTACH", 1)[-1].split("break;", 1)[0]
    attach_calls = calls_in(attach)
    allowed_attach = {"DisableThreadLibraryCalls", "RegisterUIServerClass"}
    c.check("DLL_PROCESS_ATTACH calls only the class-registration allowlist",
            attach_calls <= allowed_attach,
            f"unexpected: {sorted(attach_calls - allowed_attach)}")

    ui_server = strip_cpp_comments(read(root, "framework/platform/win32/imm32/ui_server.cpp"))
    reg_body = function_body(ui_server, r"ATOM RegisterUIServerClass\([^)]*\)")
    reg_calls = calls_in(reg_body or "")
    c.check("UI class registration only calls RegisterClassExW "
            "(no window, registry or file work at load)",
            reg_body is not None and reg_calls <= {"RegisterClassExW"},
            "body missing" if reg_body is None else
            f"unexpected: {sorted(reg_calls - {'RegisterClassExW'})}")

    # A1b: refusing beats half-initialising. ImeInquire is the one entry point
    # imm32 must be able to trust; if it cannot fill the structure it has to
    # say so instead of letting the loader use garbage -- that silent-success
    # pattern is exactly what the reference IME did before the 0xF4.
    imm_src = strip_cpp_comments(read(root, "framework/platform/win32/imm32/imm_entry.cpp"))
    inquire_body = function_body(imm_src, r"BOOL WINAPI ImeInquire\([^)]*\)")
    dangerous_inquire = {"ImmInstallIME", "ImmInstallIMEW", "CreateFileA",
                         "CreateFileW", "CreateThread", "LoadLibraryA",
                         "LoadLibraryW", "RegisterClassExW"}
    inquire_calls = calls_in(inquire_body or "")
    c.check("ImeInquire can refuse and touches no loader/registry API",
            inquire_body is not None and "return FALSE" in inquire_body and
            not (inquire_calls & dangerous_inquire),
            "body missing" if inquire_body is None else
            ("no rejection path" if "return FALSE" not in inquire_body else
             f"unexpected: {sorted(inquire_calls & dangerous_inquire)}"))

    # 7. Installer contract (M0 #4, D3) and the surviving deployment
    #    preference: never make ourselves the default input language.
    installer = read(root, "tools/install-rospinyin/install.cpp")
    code = strip_cpp_comments(installer)

    # Positive allowlist: every registry key the installer can reach is either
    # our own HKCU bookkeeping key or the layout key it just created. A new
    # write -- whatever it is called -- needs a new subkey expression and
    # fails here, which is what a blocklist of words could not deliver.
    allowed_subkeys = {"kRegInstall", "sub"}
    allowed_hives = {"HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE"}
    writes = reg_key_writes(code)
    bad_writes = [(h, s) for h, s in writes
                  if h not in allowed_hives or s not in allowed_subkeys]
    c.check("installer registry writes stay in the two-key allowlist "
            "(own HKCU key + the layout key)",
            bool(writes) and not bad_writes,
            "no Reg*Key call found" if not writes else str(bad_writes))
    # Every place that formats a registry path must start from kKeyboardLayouts.
    # `sub` appears in both the install and the uninstall path, so a rule that
    # only looked at the first occurrence would let the second one drift.
    path_builds = []
    for m in re.finditer(r"\bswprintf\s*\(", code):
        args = split_top_level(call_arguments(code, m.end() - 1))
        if len(args) >= 4 and "\\\\" in args[2]:
            path_builds.append(args[3])
    c.check("every formatted layout subkey starts from kKeyboardLayouts",
            bool(path_builds) and all(b == "kKeyboardLayouts" for b in path_builds),
            f"prefixes: {sorted(set(path_builds))}")

    # Negative: the API that would make us the default input language. This is
    # a preference now, not a red line -- ReactOS writes the Preload entry
    # itself inside ImmInstallIMEW -- but we still must not do it ourselves.
    default_layout_apis = ["ActivateKeyboardLayout", "SystemParametersInfo",
                           "ImmSetDefaultLayout", "LoadKeyboardLayoutW",
                           "LoadKeyboardLayoutA"]
    api_hits = [w for w in default_layout_apis if w in code]
    c.check("installer never sets the default input language",
            not api_hits, ", ".join(api_hits))

    c.check("installer uses the HKL returned by ImmInstallIMEW "
            "instead of a hard-coded id",
            "ImmInstallIMEW" in code and "save_hkl(hkl)" in code and
            "E0100804" not in code)
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
                "framework/platform/win32/imm32/dllmain.cpp",
                "framework/platform/win32/imm32/imm_entry.cpp",
                "framework/platform/win32/ui/README.md",
                "pinyin/include/rospinyin/scheme.hpp",
                "pinyin/src/scheme.cpp",
                "ime/scheme_reg.cpp",
                "tests/session_test.cpp",
                "tools/syntax_stub/windows.h"]:
        c.check(f"present: {rel}", (root / rel).is_file())

    # The reusable framework must stay free of pinyin knowledge: the plugin
    # seam is imfw::Scheme (see framework/README.md).
    scheme_cpp = read(root, "pinyin/src/scheme.cpp")
    ui_server = read(root, "framework/platform/win32/imm32/ui_server.cpp")
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
    # X2-a0 (audit A-P0-2): the assembly supplies the *factory*; the framework
    # owns scheme_instance(). Assert the definition form rather than the bare
    # word, because the word legitimately appears in the file's comments ("it
    # does not define scheme_instance()") — the first version of this check
    # passed on that comment alone, which is exactly the kind of false green
    # this gate exists to prevent.
    scheme_reg = strip_cpp_comments(read(root, "ime/scheme_reg.cpp"))
    c.check("scheme_reg hands the framework its Scheme (X2-a0: factory, not the entry point)",
            "Scheme* scheme_factory()" in scheme_reg and
            "PinyinScheme" in scheme_reg and
            "Scheme& scheme_instance" not in scheme_reg)
    c.check("X2-a0: the injection seam lives in the reusable core (imfw-engine)",
            (root / "framework/include/imfw/scheme_registry.hpp").is_file() and
            "src/scheme_registry.cpp" in read(root, "framework/CMakeLists.txt"))
    # The seam's second consumer: a letters-only scheme in samples/ assembled by
    # -DROSPINYIN_SCHEME=toy. G4 says a second input method needs no framework
    # edit; this is the cheapest standing proof that the claim still holds.
    scheme_reg_toy = read(root, "ime/scheme_reg_toy.cpp")
    c.check("X2-a0: the toy scheme is assembled through the same three lines",
            "Scheme* scheme_factory()" in scheme_reg_toy and
            "ToyScheme" in scheme_reg_toy and
            "ROSPINYIN_SCHEME" in read(root, "ime/CMakeLists.txt"))
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
    for rel in ["framework/src/code_table.cpp", "framework/platform/win32/imm32/imm_entry.cpp",
                "framework/src/engine.cpp", "framework/platform/win32/imm32/session_store.cpp"]:
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
    comp_cpp = strip_cpp_comments(read(root, "framework/platform/win32/ui/composition_window.cpp"))
    ime_ui_cpp = strip_cpp_comments(read(root, "framework/platform/win32/ui/ime_ui.cpp"))
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

    # P0-2 / M2 Shift toggle: `open` is now a state the IME owns. It is aligned
    # at every activation (AlignOpenStatus), toggled by VK_SHIFT via exactly one
    # helper (set_ime_open), and read through exactly one helper (ime_is_open).
    # Before the toggle existed, gating on it was forbidden precisely because
    # nothing ever set the flag; that is no longer true, so the gate is back --
    # but only through the helper, so no call site can invent its own meaning.
    entry_code = strip_cpp_comments(entry)
    gate = function_body(entry_code, r"bool should_consume_key\([^)]*\)")
    toggle = function_body(entry_code, r"void set_ime_open\([^)]*\)")
    c.check("open status is owned by the IME: aligned, toggled on lone Shift, "
            "read through one helper",
            "AlignOpenStatus(" in entry_code and
            toggle is not None and "ImmSetOpenStatus(" in toggle and
            "VK_SHIFT" in entry_code and
            gate is not None and "ime_is_open(" in gate,
            "should_consume_key/set_ime_open not found"
            if gate is None or toggle is None else "")

    # U1 (2026-11-18 真机): the alignment must hang off BOTH entry points.
    # ReactOS can deliver ImeSelect without a preceding ImeSetActiveContext;
    # when only the latter aligned, the very first switch to this layout stayed
    # "closed" -- the tray showed EN and the user's first Shift *opened* the
    # IME instead of switching language.
    select_body = function_body(entry_code, r"BOOL WINAPI ImeSelect\([^)]*\)")
    active_body = function_body(entry_code,
                                r"BOOL WINAPI ImeSetActiveContext\([^)]*\)")
    c.check("open status aligned on both activation entry points "
            "(ImeSelect and ImeSetActiveContext)",
            select_body is not None and "AlignOpenStatus(" in select_body and
            active_body is not None and "AlignOpenStatus(" in active_body,
            "AlignOpenStatus missing from one of the two entry points")

    # U3 (2026-11-18 真机): dock to the work area, never to the raw screen.
    # SM_CYSCREEN puts the toolbar under the taskbar, and on ReactOS
    # HWND_TOPMOST does not win against it (the taskbar is topmost too and
    # keeps the higher z-order), so the label was clipped to a few pixels.
    toolbar_cpp = strip_cpp_comments(read(root, "framework/platform/win32/ui/toolbar.cpp"))
    c.check("toolbar docks to the work area, not the raw screen",
            "SPI_GETWORKAREA" in toolbar_cpp and
            "screen_h - kToolbarHeight" not in toolbar_cpp)

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
