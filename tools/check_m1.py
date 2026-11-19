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
import json
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
    "framework", "pinyin", "ime", "tests", "cmake",
    "tools/install-rospinyin", "tools/syntax_stub",
]
SOURCE_SUFFIXES = {".cpp", ".h", ".hpp", ".rc", ".def", ".cmake"}
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


def strip_cmake_comments(text: str) -> str:
    """Drop `#` comments and `#[[ ]]` bracket comments.

    Needed since X2-e1: the platform CMakeLists explains *why* a TU sits in
    one library and not the other, and those explanations name the very paths
    the checks below look for. Matching against the commented text would let a
    file "belong" to a library it is only mentioned in.
    """
    text = re.sub(r"#\[\[.*?\]\]", "", text, flags=re.S)
    return re.sub(r"#[^\n]*", "", text)


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
    # X2-d split the IMM32 host layer into four translation units. Claims about
    # the *layer* (what it may call, what it links, what it converts) read
    # `entry_all`; claims about *ownership* read one named file and, where a
    # second copy is the failure mode, say so explicitly -- "the string is
    # somewhere" cannot distinguish "moved" from "copied", which is the false
    # green this project hit three times during X2-a0/c.
    IMM32 = "framework/platform/win32/imm32/"
    entry = read(root, IMM32 + "imm_entry.cpp")
    # Function-body assertions read the comment-stripped text: every one of them
    # asks "is this call here", and a comment that names the call answers yes.
    entry_code = strip_cpp_comments(entry)
    key_translate = read(root, IMM32 + "key_translate.cpp")
    win32_host = read(root, IMM32 + "win32_host.cpp")
    trace_line = read(root, IMM32 + "trace_line.cpp")
    entry_all = "\n".join([entry, key_translate, win32_host, trace_line])
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
    # ownership through imfw::wants_key() and ImeToAsciiEx dispatches to
    # imfw::handle_key(), which emits WM_IME_CHAR TRANSMESSAGEs on commit.
    # Since X2-a both policy functions live in framework/src/host_key.cpp, so
    # the assertions below are split: the entry file must only *translate*, the
    # policy file must *decide*. The historical M1 snapshot is not the shipping
    # behaviour either way.
    policy = read(root, "framework/src/host_key.cpp")
    process_body = function_body(entry_code, r"BOOL\s+WINAPI\s+ImeProcessKey\s*\([^)]*\)")
    ascii_body = function_body(entry_code, r"UINT\s+WINAPI\s+ImeToAsciiEx\s*\([^)]*\)")
    c.check("ImeProcessKey delegates key ownership to wants_key (host-tested)",
            process_body is not None and
            "const bool consume = state != nullptr && imfw::wants_key(*state, event, host);"
            in process_body)
    # Both call sites, pinned individually: the shift branch answers "not ours"
    # but still has to run the lone-Shift latch, so a substring test that only
    # asks "is wants_key mentioned" passes while one of the two is deleted --
    # observed while writing this assertion.
    c.check("Shift reaches the same policy from the ownership path",
            process_body is not None and
            "if (state != nullptr) imfw::wants_key(*state, event, host);"
            in process_body)
    # The M1 stub was "return FALSE;" and nothing else. Detect that shape
    # precisely: the live function must return the decision, not a constant.
    c.check("ImeProcessKey no longer unconditionally returns FALSE",
            process_body is not None and "consume ? TRUE : FALSE" in process_body)
    # Key-up must be declined here and nowhere else: ReactOS routes WM_KEYUP
    # through IntImmProcessKey too, and ImeToAsciiEx gets no lParam, so a
    # claimed key-up feeds every keystroke to the engine twice (doubled or
    # missing characters). See HANDOVER §11.4. X2-a split this into "the
    # adapter carries the bit" + "the policy refuses it"; both halves are
    # required, so both are asserted.
    c.check("ImeProcessKey declines key-up (ImeToAsciiEx cannot see lParam)",
            "kKeyUpBit" in key_translate and
            process_body is not None and "event.key_up" in process_body and
            "if (event.key_up) return false;" in policy)
    c.check("ImeToAsciiEx dispatches key processing to handle_key",
            ascii_body is not None and "imfw::handle_key(" in ascii_body)
    c.check("M2 commits text through WM_IME_CHAR TRANSMESSAGEs",
            "WM_IME_CHAR" in key_translate and "imfw::emit_text(" in key_translate)
    c.check("G1: commit path handles multi-codepoint text (words)",
            ascii_body is not None and
            "imfw::win32::emit_commit(lpTransMsgList, result.commit)" in ascii_body and
            "apply_commit" in policy and
            "chosen->text[0]" not in entry_all and
            "chosen->text[0]" not in policy)
    # Audit 2026-11-17 §2.1 (P0): ImeToAsciiEx takes a TRANSMSGLIST whose
    # uMsgCount header is pre-filled with the capacity; messages live at
    # ->TransMsg[0]. Writing at offset 0 clobbers the header and shifts
    # every message, which is exactly what broke composition on the VM.
    abi = read(root, "framework/platform/win32/imm32/imm32_abi.h")
    c.check("G1 ABI: ImeToAsciiEx takes TRANSMSGLIST, not a bare TRANSMSG array",
            "struct TransMsgList" in abi and
            "rpimm::TransMsgList* lpTransMsgList" in key_translate and
            "rpimm::TransMsg* lpTransMsg" not in entry_all)
    c.check("G1 ABI: messages written via list->TransMsg with uMsgCount capacity",
            "lpTransMsgList->TransMsg" in key_translate and
            "lpTransMsgList->uMsgCount" in key_translate and
            "offsetof(rpimm_list, TransMsg) == sizeof(UINT)" in key_translate)
    # G1 hardening: the emission logic is host-testable in imfw-engine, and
    # the host delegates instead of re-implementing the UTF-16 conversion.
    c.check("G1: WM_IME_CHAR emission lives in imfw-engine (host-tested)",
            (root / "framework/include/imfw/emit_text.hpp").is_file() and
            (root / "framework/src/emit_text.cpp").is_file() and
            "imfw::emit_text" in key_translate and
            "sizeof(imfw::TransMsgSlot) == sizeof(rpimm::TransMsg)" in key_translate)

    # 4b. Framework gap dispositions (xingma/XINGMA-PLAN.md 3.4).
    configure_body = function_body(entry_code, r"BOOL\s+WINAPI\s+ImeConfigure\s*\([^)]*\)")
    c.check("G3: ImeConfigure delegates to the Scheme configure hook",
            configure_body is not None and
            "scheme_instance().configure" in configure_body)
    c.check("G3+G2: successful configure reloads data and sessions",
            configure_body is not None and
            "reload_sessions()" in configure_body and
            "return TRUE" in configure_body)
    # X2-c moved the two questions into the router (key_route.cpp); asking them
    # again in handle_key() would let the two answers drift apart.
    route_cpp = strip_cpp_comments(read(root, "framework/src/key_route.cpp"))
    c.check("G4: selection/page key policy comes from the Scheme",
            "#include <imfw/host.hpp>" in policy and
            "scheme.is_selection_key(ch)" in route_cpp and
            "scheme.page_key(ch)" in route_cpp and
            "bool is_page_key" not in policy and
            "is_selection_key" not in entry_all)
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
            "utf8_to_wide" in win32_host and "utf8_to_wide" in composition_ui)
    # Regression guard: PinyinSession::feed() is a no-op until on_select(true),
    # so a missing activation makes the IME silently swallow every pinyin key.
    # X2-d moved that call into the shared activation path; the assertion follows
    # it instead of pinning a line that no longer means anything.
    activate_body = function_body(entry_code, r"void activate_session\([^)]*\)")
    select_body = function_body(entry_code, r"BOOL\s+WINAPI\s+ImeSelect\s*\([^)]*\)")
    c.check("ImeSelect activates the engine session on select",
            activate_body is not None and "on_select(true)" in activate_body and
            select_body is not None and "activate_session(hIMC)" in select_body)

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
                "framework/platform/win32/imm32/key_translate.cpp",
                "framework/platform/win32/imm32/trace_line.cpp",
                "framework/platform/win32/imm32/win32_host.cpp",
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
    # X2-b: the host smoke test must exist *and be registered*, because a test
    # file nobody runs is worse than no file (it looks like coverage). The
    # static half checks registration and that the expected log really pins the
    # toggle; ctest listing it is the runtime half.
    smoke_src = read(root, "tests/host_smoke_test.cpp")
    c.check("X2-b: host smoke is present and registered in ctest",
            (root / "tests/host_smoke_test.cpp").is_file() and
            "add_test(NAME host-smoke" in read(root, "tests/CMakeLists.txt") and
            "check_log(" in smoke_src and
            "set_open(0)" in smoke_src and
            "feed:z" in smoke_src)

    # X2-a: the key policy must stay portable. The entire point of moving it out
    # of imm_entry.cpp is that tests/host_key_test.cpp links it on the build
    # host; one <windows.h> in this layer and that test stops being possible --
    # which is exactly how the policy ended up with zero coverage before.
    c.check("X2-a: the key policy and its state stay Windows-free",
            (root / "framework/src/host_key.cpp").is_file() and
            (root / "framework/include/imfw/host.hpp").is_file() and
            (root / "framework/include/imfw/session_state.hpp").is_file() and
            "#include <windows.h>" not in policy and
            "#include <windows.h>" not in read(root, "framework/include/imfw/host.hpp") and
            "#include <windows.h>" not in read(root, "framework/include/imfw/session_state.hpp") and
            "src/host_key.cpp" in read(root, "framework/CMakeLists.txt"))
    # X2-c: "what does this key mean" has exactly one owner. Pinned in opposite
    # directions on purpose -- the file that must answer (key_route.cpp) and the
    # file that must no longer answer (handle_key). Checking only the first
    # would stay green with both copies present, and two copies of a routing
    # decision drift the moment one of them is edited: the same false-green
    # shape as the two X2-a0 rewrites.
    intent_hpp = read(root, "framework/include/imfw/intent.hpp")
    handle_body = function_body(policy, r"KeyResult handle_key\([^)]*\)")
    c.check("X2-c: key routing is a pure function of (scheme, key)",
            "Intent route_key(const Scheme& scheme, const KeyEvent& event)"
            in route_cpp and
            "src/key_route.cpp" in read(root, "framework/CMakeLists.txt") and
            "add_test(NAME key-route" in read(root, "tests/CMakeLists.txt") and
            "#include <windows.h>" not in route_cpp and
            "#include <windows.h>" not in intent_hpp)
    # The forbidden half is spelled as "no policy call at all", not as
    # "no `scheme.`": the first version of this check tested for the literal
    # `scheme.`, and a re-added `scheme_instance().page_key(...)` stayed green
    # because the underscore sits between the two -- the same false-green shape
    # as the X2-a0 ones, this time caught by mutation rather than by review.
    # One scheme_instance() is allowed here: the argument to route_key().
    c.check("X2-c: handle_key dispatches on Intent and asks the scheme nothing",
            handle_body is not None and "route_key(" in handle_body and
            handle_body.count("scheme_instance()") == 1 and
            "page_key(" not in handle_body and
            "is_selection_key(" not in handle_body and
            "input_char(" not in handle_body,
            "handle_key not found" if handle_body is None else "")
    # Every kind needs a branch: a missing one is a key that silently does
    # nothing, and a switch over an enum only warns (no -Werror here).
    kinds_block = re.search(r"enum class IntentKind[^{]*\{(.*?)\};",
                            intent_hpp, re.S)
    intent_kinds = (re.findall(r"(\w+)\s*(?:=[^,]*)?,", kinds_block.group(1))
                    if kinds_block else [])
    unhandled = [k for k in intent_kinds
                 if f"case IntentKind::{k}:" not in policy]
    c.check("X2-c: every Intent kind has a branch in handle_key",
            bool(intent_kinds) and not unhandled,
            ", ".join(unhandled) or "no IntentKind enum found")

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
    for rel in ["framework/src/code_table.cpp", IMM32 + "imm_entry.cpp",
                IMM32 + "key_translate.cpp", IMM32 + "win32_host.cpp",
                IMM32 + "trace_line.cpp",
                "framework/src/engine.cpp", IMM32 + "session_store.cpp"]:
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
    # helper (set_ime_open), and read through exactly one query (host.is_open()).
    # Before the toggle existed, gating on it was forbidden precisely because
    # nothing ever set the flag; that is no longer true, so the gate is back --
    # but only through the helpers, so no call site can invent its own meaning.
    # X2-a split the helpers across the boundary: set_ime_open() (policy) calls
    # Host::set_open(), and the one ImmSetOpenStatus lives in the adapter.
    gate = function_body(policy, r"bool wants_key\([^)]*\)")
    toggle = function_body(policy, r"void set_ime_open\([^)]*\)")
    # X2-d moved the adapter to win32_host.cpp; the assertion follows it and
    # also pins that the copy is gone from the entry file (a moved call that
    # left its old body behind is exactly the drift this check is for).
    open_writer = function_body(strip_cpp_comments(win32_host),
                                r"void Win32Host::set_open\(bool open\)[^{]*")
    c.check("open status is owned by the IME: aligned, toggled on lone Shift, "
            "read through one helper",
            "AlignOpenStatus(" in entry_code and
            "VK_SHIFT" in key_translate and
            toggle is not None and "host.set_open(" in toggle and
            open_writer is not None and "ImmSetOpenStatus(" in open_writer and
            gate is not None and "host.is_open()" in gate,
            "wants_key/set_ime_open/set_open not found"
            if gate is None or toggle is None or open_writer is None else "")

    # U1 (2026-11-18 真机): both entry points must activate, and since X2-d they
    # do it through one helper. The point of the helper is that the sequence
    # exists once: two copies in two orders is precisely what produced U1.
    active_body = function_body(entry_code,
                                r"BOOL WINAPI ImeSetActiveContext\([^)]*\)")
    c.check("open status aligned on both activation entry points "
            "(ImeSelect and ImeSetActiveContext)",
            select_body is not None and "activate_session(hIMC)" in select_body and
            active_body is not None and "activate_session(hIMC)" in active_body,
            "activate_session missing from one of the two entry points")

    # X2-d. Two claims, both pinned in the failing direction:
    #  * imm_entry.cpp is an entry-point file again -- the four things that made
    #    it 600 lines live in their own TUs now, and the guards below name the
    #    most characteristic symbol of each, so re-merging one fails here;
    #  * the activation sequence exists once. A copy left behind in either entry
    #    point is the U1 bug waiting to re-appear in a different order.
    entry_lines = len(entry.splitlines())
    c.check("X2-d: imm_entry.cpp holds entry points, not a god object",
            entry_lines <= 400 and
            "class Win32Host" not in entry and
            "class Win32Host" in read(root, IMM32 + "win32_host.h") and
            "trace::Line" not in entry and
            "framework_key_char" not in entry and
            "offsetof(" not in entry,
            f"{entry_lines} lines")
    c.check("X2-d: the activation sequence exists once (activate_session)",
            activate_body is not None and
            "AlignOpenStatus(" in activate_body and
            "on_select(true)" in activate_body and
            "UiToolbarShow(" in activate_body and
            select_body is not None and
            active_body is not None and
            "AlignOpenStatus(" not in select_body and
            "AlignOpenStatus(" not in active_body and
            "on_select(true)" not in select_body and
            "on_select(true)" not in active_body and
            "UiToolbarShow(" not in select_body and
            "UiToolbarShow(" not in active_body)
    # The trace channel had two hand-written divide-by-ten loops (pid, tid)
    # inside commit(); both now go through trace_stamp.h's put_uint().
    c.check("X2-d: one decimal formatter in the trace channel",
            "put_uint(" in trace_line and
            "% 10" not in trace_line and
            "unsigned put_uint(char* out, unsigned len, unsigned cap" in
            read(root, "framework/include/imfw/trace_stamp.hpp"))

    # U3 (2026-11-18 真机): dock to the work area, never to the raw screen.
    # SM_CYSCREEN puts the toolbar under the taskbar, and on ReactOS
    # HWND_TOPMOST does not win against it (the taskbar is topmost too and
    # keeps the higher z-order), so the label was clipped to a few pixels.
    toolbar_cpp = strip_cpp_comments(read(root, "framework/platform/win32/ui/toolbar.cpp"))
    c.check("toolbar docks to the work area, not the raw screen",
            "SPI_GETWORKAREA" in toolbar_cpp and
            "screen_h - kToolbarHeight" not in toolbar_cpp)

    # Guest-side hashing (2026-11-19). ReactOS ships no sha256sum and no
    # certutil, while two of our rospinyin.ime builds are both 940,888 bytes
    # with different content -- so a byte count cannot verify a deployment.
    # rhash fills that gap; the constraints are the same as the installer's:
    # CryptoAPI is incomplete on ReactOS (own SHA-256 instead), and msvcrt
    # does not guarantee %llu (HANDOVER 11.4), hence plain %lu sizes.
    rhash_src = strip_cpp_comments(read(root, "tools/rhash/rhash.c"))
    c.check("rhash: own SHA-256, no CryptoAPI, no %llu",
            "0x6a09e667u" in rhash_src and
            "Crypt" not in rhash_src and
            "%llu" not in rhash_src and
            "FILE_SHARE_DELETE" in rhash_src)

    # X2-e1 (audit A-P1-4): the Windows half became two libraries. The failure
    # this has to catch is not "a file moved" but "a file is in both lists":
    # a TU compiled into two libraries means duplicate symbols, and for the
    # stateful ones (the host's session map, the UI's per-HIMC windows) it
    # means two copies of the same cache inside one module -- which links
    # cleanly and misbehaves at runtime. Hence ownership is checked in both
    # directions, and comments are stripped first (they name the paths too).
    plat_cmake = strip_cmake_comments(
        read(root, "framework/platform/win32/CMakeLists.txt"))
    host_block, _, ui_block = plat_cmake.partition("add_library(imfw-win32-ui")
    host_tus = ["imm32/dllmain.cpp", "imm32/imm_entry.cpp",
                "imm32/key_translate.cpp", "imm32/trace_line.cpp",
                "imm32/win32_host.cpp", "imm32/ui_server.cpp",
                "imm32/session_store.cpp"]
    ui_tus = ["ui/ime_ui.cpp", "ui/ui_window.cpp", "ui/composition_window.cpp",
              "ui/candidate_window.cpp", "ui/toolbar.cpp"]
    misplaced = ([t for t in host_tus if t not in host_block or t in ui_block] +
                 [t for t in ui_tus if t not in ui_block or t in host_block])
    c.check("X2-e1: every platform TU belongs to exactly one library",
            bool(ui_block) and not misplaced, ", ".join(misplaced))

    # ...and the assembling .ime compiles none of them. The `/../` form is the
    # one this file used before the split; matching the bare path would also
    # hit the comment that points at the new CMakeLists.
    c.check("X2-e1: ime/ assembles the platform layer instead of compiling it",
            not re.search(r"\.\./framework/platform/win32/", ime_cmake) and
            "imfw-win32-imm32" in ime_cmake and
            "imfw-win32-ui" in ime_cmake,
            "ime/CMakeLists.txt still names platform sources")

    # GNU ld pulls an archive member only for an *undefined reference*, and the
    # 16 names in rospinyin.def are export entries, not references. Without
    # whole-archive on the host library the link dies with "cannot export
    # ImeProcessKey: symbol not defined" -- measured, not guessed (X2-e1).
    c.check("X2-e1: .def exports force the host archive members in",
            re.search(r"--whole-archive\"?\s+imfw-win32-imm32", ime_cmake) and
            "--no-whole-archive" in ime_cmake,
            "whole-archive missing: ld resolves exports against linked objects only")

    # The trace macro is read by the platform libraries, which are configured
    # before ime/. Declared in ime/ it would leave the libraries untraced and
    # the flag would look like it worked.
    c.check("X2-e1: ROSPINYIN_TRACE is declared before the libraries that read it",
            "option(ROSPINYIN_TRACE" in read(root, "CMakeLists.txt") and
            "option(ROSPINYIN_TRACE" not in ime_cmake,
            "option lives in ime/CMakeLists.txt, i.e. after framework/platform/win32")

    # ld's default PE timestamp makes two builds of identical sources differ in
    # a handful of bytes, so "same sha256" stops being an answer to "is this the
    # build I shipped". The whole deployment story (artifact_hashes.py, the
    # guest-side rhash.exe) depends on that answer, hence the flag is pinned.
    #
    # Since X2-e3 the flag lives in cmake/StaticRuntime.cmake and every shipped
    # PE gets it; here the assertion only has to prove this target *asks for it*
    # (a call, not the literal flag -- the literal moved out on purpose).
    static_rt = read(root, "cmake/StaticRuntime.cmake")
    c.check("X2-e1: the .ime links reproducibly (no PE timestamp)",
            "-Wl,--no-insert-timestamp" in static_rt and
            re.search(r"rospinyin_reproducible_pe\(\s*rospinyin-ime\s+CXX\s*\)",
                      ime_cmake) is not None and
            "--no-insert-timestamp" not in ime_cmake,
            "ld stamps link time into the header; hashes stop identifying a build")

    # X2-e2 (audit A-P1-5): the plug-in owns its data end to end, so the Win32
    # half of the .dat read belongs to rospinyin-engine and not to the assembly.
    # The failure this has to catch is the one measured before the move: with
    # the file dropped from the source list, ld resolved load_system_table() to
    # the always-present weak stub and produced a 942,989 B .ime that silently
    # ran on the built-in table (the real one is 948,481 B, and nothing said so).
    #
    # Matched as a *call* (`target_sources(rospinyin-engine PRIVATE ...)`), not
    # as a path: the comment above the block names the same file, and an
    # assertion that matches words has already gone green on a mutant three
    # times in this project.
    pinyin_cmake = strip_cmake_comments(read(root, "pinyin/CMakeLists.txt"))
    c.check("X2-e2: the pinyin data source is a rospinyin-engine source",
            re.search(r"target_sources\(\s*rospinyin-engine\s+PRIVATE\s+"
                      r"host/dat_source_win\.cpp\s*\)", pinyin_cmake) is not None and
            "dat_source_win" not in strip_cmake_comments(ime_cmake),
            "dat_source_win.cpp must be added by pinyin/CMakeLists.txt and not "
            "re-listed by ime/CMakeLists.txt")

    # ...and the Windows build must be unable to do without it. The macro is
    # what removes the stub; without it the stub is present everywhere and the
    # link above succeeds again.
    c.check("X2-e2: Windows builds require a real load_system_table()",
            re.search(r"target_compile_definitions\(\s*rospinyin-engine\s+PRIVATE\s+"
                      r"ROSPINYIN_REQUIRE_SYSTEM_TABLE\s*\)",
                      pinyin_cmake, re.S) is not None,
            "without ROSPINYIN_REQUIRE_SYSTEM_TABLE a missing data source links "
            "cleanly and ships the built-in table")

    # The guard has to cover the stub and nothing else: load_pinyin_table is the
    # Linux/host-test path and must survive.
    src_dat = read(root, "pinyin/src/dat_source.cpp")
    stub_guard = re.search(
        r"#if\s+!defined\(ROSPINYIN_REQUIRE_SYSTEM_TABLE\)(.*?)#endif",
        src_dat, re.S)
    c.check("X2-e2: the guard removes the weak stub only",
            stub_guard is not None and
            "load_system_table" in stub_guard.group(1) and
            "load_pinyin_table" not in stub_guard.group(1))

    # Nothing compiled into the .ime reads the macro any more, so a define here
    # would be the kind of flag that looks applied and changes no byte. It is
    # declared by the two targets that do read it (platform host, pinyin data
    # source) -- see the ROSPINYIN_TRACE checks above.
    c.check("X2-e2: ROSPINYIN_TRACE is not re-declared on the assembly target",
            "ROSPINYIN_TRACE" not in strip_cmake_comments(ime_cmake),
            "ime/CMakeLists.txt defines a macro nothing in it reads")

    # X2-e6: the trace formatter is an imfw public service, not a host private
    # header. The data source used to name a file inside platform/win32/imm32,
    # which forced rospinyin-engine to carry a path into the host layer. The
    # formatter now lives under framework/include and reaches both clients via
    # imfw-engine's PUBLIC include path -- no new implementation, no second copy.
    ds_code = strip_cpp_comments(read(root, "pinyin/host/dat_source_win.cpp"))
    c.check("X2-e6: pinyin data source uses the public trace header",
            "#include <imfw/trace_stamp.hpp>" in ds_code and
            "#include \"trace_stamp.h\"" not in ds_code,
            "a plugin must not name a file in the host's platform directory")

    c.check("X2-e6: rospinyin-engine has no platform include path",
            "framework/platform/win32/imm32" not in pinyin_cmake,
            "the engine reaches the formatter through imfw-engine, not by "
            "reaching into the host layer")

    dllmain_code = strip_cpp_comments(read(root, IMM32 + "dllmain.cpp"))
    trace_line_code = strip_cpp_comments(trace_line)
    c.check("X2-e6: host callers still share the public formatter",
            "#include <imfw/trace_stamp.hpp>" in dllmain_code and
            "#include <imfw/trace_stamp.hpp>" in trace_line_code,
            "moving the header must not remove it from its original callers")

    trace_pub = read(root, "framework/include/imfw/trace_stamp.hpp")
    c.check("X2-e6: one formatter remains, no private header is resurrected",
            not (root / (IMM32 + "trace_stamp.h")).exists() and
            "kTracePath[]" in trace_pub and
            "unsigned put_uint(char* out, unsigned len, unsigned cap" in trace_pub,
            "a leftover private copy would split the one-decimal-formatter "
            "contract that X2-d established")

    # ------------------------------------------------------------------
    # X2-e3 (audit A-P1-6): the build configuration itself, not one file in it.
    #
    # Everything below is about *where a fact is stated*. The failures they have
    # to catch are the two that already happened in this repository:
    #   * a guard (EXISTS) that turned "this directory is missing" into a
    #     configure that succeeds and ships less -- X2-e2 found the same shape
    #     one layer down, where a missing data source linked cleanly against a
    #     weak stub and produced a 942,989 B .ime instead of 948,481 B;
    #   * the same flag set restated per consumer, which is how tools/httpdl
    #     ended up with -static but no reproducible header while the .ime had
    #     both.
    # ------------------------------------------------------------------
    root_cmake = strip_cmake_comments(read(root, "CMakeLists.txt"))
    c.check("X2-e3: no EXISTS guard can hide a missing subdirectory",
            "EXISTS" not in root_cmake,
            "an EXISTS guard makes a missing directory a silent no-op; "
            "add_subdirectory fails loudly, which is the point")

    c.check("X2-e3: the C language level is stated, not inherited",
            "set(CMAKE_C_STANDARD 11)" in root_cmake and
            "set(CMAKE_C_STANDARD_REQUIRED ON)" in root_cmake and
            "set(CMAKE_C_EXTENSIONS OFF)" in root_cmake,
            "C targets (httpdl, rhash) would follow whatever the compiler "
            "defaults to, which changes across GCC versions")

    # One root variable. CMAKE_SOURCE_DIR and PROJECT_SOURCE_DIR are equal here
    # only because no subdirectory calls project(); the day one does, every
    # CMAKE_SOURCE_DIR silently starts pointing somewhere else.
    stray_source_dir = []
    for cmake_file in sorted(root.rglob("CMakeLists.txt")):
        if any(part in {"build", "build-standalone", "build-trace",
                        "build-intree", "build-toy", "build-cross",
                        "xingma", "vendor", ".git"}
               for part in cmake_file.relative_to(root).parts):
            continue
        if "${CMAKE_SOURCE_DIR}" in cmake_file.read_text(encoding="utf-8"):
            stray_source_dir.append(str(cmake_file.relative_to(root)))
    c.check("X2-e3: subdirectories use PROJECT_SOURCE_DIR, not CMAKE_SOURCE_DIR",
            not stray_source_dir, ", ".join(stray_source_dir))

    c.check("X2-e3: StaticRuntime.cmake defines the two shipped-PE policies",
            "function(rospinyin_static_runtime" in static_rt and
            "function(rospinyin_reproducible_pe" in static_rt and
            "include(\"${PROJECT_SOURCE_DIR}/cmake/StaticRuntime.cmake\")"
            in root_cmake,
            "the include is what makes the functions visible to every target")

    # Every PE that runs on ReactOS asks for both policies by *call*, and none
    # of them restates the flags. A literal -static-libstdc++ in one of these
    # files is the drift itself, not a style issue: the flag it lacks is the
    # one the other files forgot last time.
    shipped = [
        ("ime/CMakeLists.txt", "rospinyin-ime"),
        ("tools/install-rospinyin/CMakeLists.txt", "install-rospinyin"),
        ("tools/rhash/CMakeLists.txt", "rhash"),
        ("tools/httpdl/CMakeLists.txt", "ros-wget"),
        ("tools/httpdl/CMakeLists.txt", "ros-curl"),
    ]
    missing_calls, restated = [], []
    for rel, target in shipped:
        body = strip_cmake_comments(read(root, rel))
        if re.search(rf"rospinyin_static_runtime\(\s*{re.escape(target)}\b",
                     body) is None:
            missing_calls.append(f"{rel}: static runtime")
        if re.search(rf"rospinyin_reproducible_pe\(\s*{re.escape(target)}\b",
                     body) is None:
            missing_calls.append(f"{rel}: reproducible PE")
        for literal in ("-static-libstdc++", "-static-libgcc",
                        "--no-insert-timestamp"):
            if literal in body:
                restated.append(f"{rel}: {literal}")
    c.check("X2-e3: every shipped PE asks for both policies by call",
            not missing_calls, ", ".join(missing_calls))
    c.check("X2-e3: no build file restates the flags instead of calling",
            not restated, ", ".join(restated))

    # Presets are what CI and humans share. Without them the same five
    # configurations live in a workflow file and in someone's shell history,
    # and they diverge silently (measured before X2-e3: build-win had tests OFF
    # while build-win-trace-en had them ON).
    presets = json.loads(read(root, "CMakePresets.json"))
    preset_names = {p["name"] for p in presets["configurePresets"]}
    required = {"linux-ci", "win32-mingw", "win32-trace"}
    ci_yml = read(root, ".github/workflows/ci.yml")
    c.check("X2-e3: presets exist and CI configures through them",
            required <= preset_names and
            "cmake --preset linux-ci" in ci_yml and
            "cmake --preset win32-mingw" in ci_yml and
            required <= {p["name"] for p in presets.get("buildPresets", [])},
            f"missing presets: {sorted(required - preset_names)}")

    # A-P2-11: the Win32 UI files used to guess SDK constants in production.
    # The real <windows.h> always supplied them, so every #ifndef block was a
    # no-op in a shipped PE and only existed to make the host syntax stub happy.
    # That is the stub's job, not the product's: move the values there.
    ui_tus = [
        "framework/platform/win32/ui/ui_window.cpp",
        "framework/platform/win32/ui/ime_ui.cpp",
        "framework/platform/win32/ui/candidate_window.cpp",
        "framework/platform/win32/ui/composition_window.cpp",
        "framework/platform/win32/ui/toolbar.cpp",
    ]
    # Match the *shape* (a constant guarded by its own definition), not a word
    # like "SWP_", and cover both common spellings of the guard.
    sdk_patch = re.compile(
        r"#\s*ifndef\s+([A-Z][A-Z0-9_]*)\s*\n\s*#\s*define\s+\1\b"
        r"|#\s*if\s+!defined\s*\(\s*([A-Z][A-Z0-9_]*)\s*\)"
        r"\s*\n\s*#\s*define\s+\2\b")
    patched = [rel for rel in ui_tus
               if sdk_patch.search(strip_cpp_comments(read(root, rel)))]
    c.check("X2-e4: no UI production TU re-defines an SDK constant",
            not patched, ", ".join(patched))

    # The stub must supply every SDK identifier the UI code references. Types
    # are declared with using/struct, constants with #define; both are part of
    # the stub surface. Project-owned prefixes are exempt, and the exemptions
    # are explicit rather than "any uppercase word".
    stub_code = strip_cpp_comments(read(root, "tools/syntax_stub/windows.h"))
    stub_surface = set(re.findall(r"^#\s*define\s+([A-Z][A-Z0-9_]*)",
                                  stub_code, re.M))
    stub_surface |= set(re.findall(
        r"^(?:using|struct|class|enum)\s+([A-Z][A-Z0-9_]*)", stub_code, re.M))
    used_ids = set()
    for rel in ui_tus:
        used_ids |= set(re.findall(
            r"\b([A-Z][A-Z0-9_]{2,})\b",
            strip_cpp_comments(read(root, rel))))
    exempt_prefixes = ("ROSPINYIN_", "IMFW_", "SPDX_")
    missing_ids = {name for name in used_ids - stub_surface
                   if not name.startswith(exempt_prefixes)}
    c.check("X2-e4: syntax_stub declares every SDK identifier the UI uses",
            not missing_ids, ", ".join(sorted(missing_ids)))

    # Text assertions alone cannot prove the stub is actually used; the CI
    # syntax step is the compiler-backed half of this contract.
    c.check("X2-e4: CI syntax-checks every UI TU against the stub",
            "-Itools/syntax_stub" in ci_yml and
            all(rel in ci_yml for rel in ui_tus),
            "the stub is only honest if the compiler checks it against the UI TUs")

    # A-P1-7 (version part): PROJECT_VERSION used to be 0.0.1 while the .ime
    # resource said 0,0,2. Nothing consumed the CMake value, so the drift was
    # invisible until a tag was cut. There is one owner now, and three ways to
    # notice if anything tries to become a second one.
    version_h = read(root, "ime/version.h")
    nums = {}
    for part in ("MAJOR", "MINOR", "PATCH"):
        m = re.search(r"#\s*define\s+ROSPINYIN_VERSION_" + part + r"\s+(\d+)",
                      version_h)
        nums[part] = m.group(1) if m else None
    string_m = re.search(
        r'#\s*define\s+ROSPINYIN_VERSION_STRING\s+"([^"]+)"', version_h)
    version_string = string_m.group(1) if string_m else ""
    # This check reads the raw file on purpose: strip_cmake_comments() treats
    # the '#' inside our regex strings as a comment and would erase the very
    # evidence we are looking for. The project-literal pattern is specific
    # enough that an explanatory comment cannot satisfy it.
    root_cmake_raw = read(root, "CMakeLists.txt")
    derived_ok = (
        "file(READ" in root_cmake_raw and
        "ime/version.h" in root_cmake_raw and
        "ROSPINYIN_VERSION_${_rpy_part}" in root_cmake_raw and
        "foreach(_rpy_part MAJOR MINOR PATCH)" in root_cmake_raw and
        "VERSION ${ROSPINYIN_PROJECT_VERSION}" in root_cmake_raw and
        re.search(r"\bproject\s*\([^)]*\bVERSION\s+[0-9]", root_cmake_raw) is None and
        all(v is not None for v in nums.values()) and
        re.fullmatch(r"(?:alpha|beta)" + ".".join(re.escape(nums[p] or "")
                                                  for p in ("MAJOR", "MINOR", "PATCH")),
                     version_string) is not None
    )
    c.check("X2-e5: CMake version is derived from ime/version.h",
            derived_ok,
            "a literal project(VERSION) quietly names artifacts after a "
            "version the binary does not carry")

    # Only prose/history and the two release gates may quote a release tag.
    # Match the two harmful shapes: a tag literal and a bare FILEVERSION tuple.
    release_literal = re.compile(r"\b(?:alpha|beta)\d+\.\d+\.\d+\b")
    comma4 = re.compile(r"(?<![\d.])\d,\d,\d,\d(?![\d.])")
    version_exempt = {
        "ime/version.h", "tools/check_release_version.py",
        "tools/make_release.py",
    }
    code_suffixes = {".c", ".cpp", ".h", ".hpp", ".rc", ".def", ".cmake",
                     ".py", ".yml", ".yaml", ".json", ".txt"}
    skip_dirs = {".git", "vendor", "xingma", "docs"}
    version_hits = []
    for path in sorted(root.rglob("*")):
        rel = path.relative_to(root).as_posix()
        if not path.is_file() or rel in version_exempt:
            continue
        if any(part in skip_dirs or part.startswith("build")
               for part in path.relative_to(root).parts):
            continue
        if path.suffix not in code_suffixes and path.name != "CMakeLists.txt":
            continue
        body = path.read_text(encoding="utf-8")
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
        body = re.sub(r"(?<![:\w])//[^\n]*", "", body)
        body = re.sub(r"#[^\n]*", "", body)
        for lineno, line in enumerate(body.splitlines(), 1):
            if release_literal.search(line) or comma4.search(line):
                version_hits.append(f"{rel}:{lineno}")
    c.check("X2-e5: no second release-version literal in code or build files",
            not version_hits, ", ".join(version_hits[:8]))

    rc_text = read(root, "ime/rospinyin_res.rc")
    c.check("X2-e5: version resource expands macros, not literals",
            "#include \"version.h\"" in rc_text and
            re.search(r"FILEVERSION\s+ROSPINYIN_VERSION_COMMA", rc_text) and
            re.search(r"FILEVERSION\s+\d", rc_text) is None,
            "the .ime must carry the same version.h truth, not a copy")

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
