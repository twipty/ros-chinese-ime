# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.

add_executable(rospinyin-smoke smoke_test.cpp)
target_link_libraries(rospinyin-smoke PRIVATE rospinyin-engine)
add_test(NAME smoke COMMAND rospinyin-smoke)

# WBS 2.4 host-side counterpart: passthrough semantics + 50 switch cycles.
# The passthrough session and build id are framework-owned (imfw-engine).
add_executable(rospinyin-session session_test.cpp)
target_link_libraries(rospinyin-session PRIVATE rospinyin-engine)
add_test(NAME session COMMAND rospinyin-session)

# M2: shared pinyin normalization (tone digits + u-umlaut -> v key).
add_executable(rospinyin-pinyin pinyin_test.cpp)
target_link_libraries(rospinyin-pinyin PRIVATE rospinyin-engine)
add_test(NAME pinyin COMMAND rospinyin-pinyin)

# M2 engine skeleton: segmentation, v-key candidates, commit paths.
add_executable(rospinyin-pinyin-session pinyin_session_test.cpp)
target_link_libraries(rospinyin-pinyin-session PRIVATE rospinyin-engine)
add_test(NAME pinyin-session COMMAND rospinyin-pinyin-session)

# M2 UI logic: 9-candidate paging and digit selection (framework-owned).
add_executable(rospinyin-candidate-pager candidate_pager_test.cpp)
target_link_libraries(rospinyin-candidate-pager PRIVATE imfw-engine)
add_test(NAME candidate-pager COMMAND rospinyin-candidate-pager)

# Missing-.dat abnormal state (decision 2026-11-19): the empty-table
# contract. Pins that a release build without the built-in table (and with a
# missing/unreadable .dat) keeps the session alive: letters accumulate,
# candidates stay empty, Backspace/Esc exit, nothing crashes. Runs in BOTH
# configurations: the empty table is built by hand, so the test means the
# same thing whether or not kBuiltin is compiled in.
add_executable(rospinyin-builtin-gate builtin_gate_test.cpp)
target_link_libraries(rospinyin-builtin-gate PRIVATE rospinyin-engine)
add_test(NAME builtin-gate COMMAND rospinyin-builtin-gate)

# M2.5 framework extraction: the Scheme plugin contract (reuse seam).
add_executable(rospinyin-scheme scheme_test.cpp)
target_link_libraries(rospinyin-scheme PRIVATE rospinyin-engine)
add_test(NAME pinyin-scheme COMMAND rospinyin-scheme)

# X2-a0: the scheme *injection* seam. No pinyin, no Windows: a fake scheme goes
# in through SchemeRegistry::install() and the framework is not edited to allow
# it. This is the 30-line test that proves the plugin seam is a seam.
add_executable(rospinyin-scheme-registry scheme_registry_test.cpp)
target_link_libraries(rospinyin-scheme-registry PRIVATE imfw-engine)
add_test(NAME scheme-registry COMMAND rospinyin-scheme-registry)

# X2-a0: the seam's second consumer. samples/toy-scheme is what a new input
# method looks like, so it is exercised on the host instead of being a folder
# nobody links. Links imfw-engine only — no pinyin, no data.
add_executable(rospinyin-toy-scheme toy_scheme_test.cpp ../samples/toy-scheme/toy_scheme.cpp)
target_include_directories(rospinyin-toy-scheme PRIVATE
    "${PROJECT_SOURCE_DIR}/samples/toy-scheme")
target_link_libraries(rospinyin-toy-scheme PRIVATE imfw-engine)
add_test(NAME toy-scheme COMMAND rospinyin-toy-scheme)

# X2-a: the key policy, compiled without <windows.h>. Before this target the
# IME's busiest branch (which key means what) had no host-side coverage at all;
# the fake engine/host here are also the base X2-b extends.
add_executable(rospinyin-host-key host_key_test.cpp)
target_link_libraries(rospinyin-host-key PRIVATE imfw-engine)
add_test(NAME host-key COMMAND rospinyin-host-key)

# X2-c: the routing decision alone. One key in, one Intent out — no engine, no
# host, so every branch is asked directly instead of being inferred from a
# smoke test's call log. This is what "M1 Intent 中间语言" buys: a vocabulary a
# second frontend can consume without inheriting ours.
add_executable(rospinyin-key-route key_route_test.cpp)
target_link_libraries(rospinyin-key-route PRIVATE imfw-engine)
add_test(NAME key-route COMMAND rospinyin-key-route)

# M3: the shared ASCII -> fullwidth Chinese punctuation table. One table in the
# framework, reused by every Chinese scheme (pinyin now, shape IME later).
add_executable(rospinyin-punctuation punctuation_test.cpp)
target_link_libraries(rospinyin-punctuation PRIVATE imfw-engine)
add_test(NAME punctuation COMMAND rospinyin-punctuation)

# M3 shift fix: the vk -> char table as a Windows-free pure function. Without
# this target the shift half of every punctuation key is untestable on Linux.
add_executable(rospinyin-key-char key_char_test.cpp)
target_link_libraries(rospinyin-key-char PRIVATE imfw-engine)
add_test(NAME key-char COMMAND rospinyin-key-char)

# X2-b: the same layers, but asserting the *sequence* instead of single
# decisions. One fixed key run (zhongguo -> 2 -> Backspace x2 -> Esc -> lone
# Shift), one exact host-call log. This is the net X2-c/X2-d must not move:
# both promise "the call sequence is unchanged", which is otherwise only
# checkable by reading a diff.
add_executable(rospinyin-host-smoke host_smoke_test.cpp)
target_link_libraries(rospinyin-host-smoke PRIVATE imfw-engine)
add_test(NAME host-smoke COMMAND rospinyin-host-smoke)

# 状态指示器: the toolbar caption carries fOpen ("拼:开" / "拼:关"). The window
# needs GDI and cannot be tested here; the caption is composed in
# imfw/toolbar_text.cpp and is exactly what this pins.
add_executable(rospinyin-toolbar-text toolbar_text_test.cpp)
target_link_libraries(rospinyin-toolbar-text PRIVATE rospinyin-engine)
add_test(NAME toolbar-text COMMAND rospinyin-toolbar-text)

# G1 regression: multi-codepoint commits (WM_IME_CHAR per UTF-16 unit).
add_executable(rospinyin-emit-text emit_text_test.cpp)
target_link_libraries(rospinyin-emit-text PRIVATE imfw-engine)
add_test(NAME emit-text COMMAND rospinyin-emit-text)

# G4 connected conclusion: UTF-8 compositions with byte-wise fallback.
add_executable(rospinyin-utf8 utf8_test.cpp)
target_link_libraries(rospinyin-utf8 PRIVATE imfw-engine)
add_test(NAME utf8 COMMAND rospinyin-utf8)

# Generic CodeTable utility: synthetic dat parsing, candidate ordering and
# corruption rejection (framework/src/code_table.cpp, pinyin-agnostic).
add_executable(rospinyin-code-table code_table_test.cpp)
target_link_libraries(rospinyin-code-table PRIVATE imfw-engine)
add_test(NAME code-table COMMAND rospinyin-code-table)

# GUI 安装器的平台白名单 / step1..4 编排 / payload 校验，全部为 Windows-free
# 纯逻辑。它们是路线 C 与 Windows 误装闸门在 Linux CI 上唯一可直接验的部分。
add_executable(rospinyin-installer-logic installer_logic_test.cpp)
target_include_directories(rospinyin-installer-logic PRIVATE
    "${PROJECT_SOURCE_DIR}/tools/install-rospinyin")
add_test(NAME installer-logic COMMAND rospinyin-installer-logic)

# P1 verification stage: pure codepoint -> WenJin plane mapping, shared by the
# guest GDI probe tool. No <windows.h>; host-testable.
add_executable(rospinyin-glyph-probe-logic glyph_probe_logic_test.cpp)
target_include_directories(rospinyin-glyph-probe-logic PRIVATE
    "${PROJECT_SOURCE_DIR}/tools/glyph-probe")
add_test(NAME glyph-probe-logic COMMAND rospinyin-glyph-probe-logic)

# Static M1 contract checks (exports, version resource, M0 red lines).
find_program(ROSPINYIN_PYTHON NAMES python3 python QUIET)
if(ROSPINYIN_PYTHON)
    add_test(NAME m1-contract
             COMMAND "${ROSPINYIN_PYTHON}"
                     "${PROJECT_SOURCE_DIR}/tools/check_m1.py"
                     --root "${PROJECT_SOURCE_DIR}")
else()
    message(STATUS "Python3 not found; M1 contract check skipped")
endif()

# M2: end-to-end mkdat test on checked-in fixtures (no network needed).
if(TARGET mkdat AND ROSPINYIN_PYTHON)
    add_test(NAME mkdat-e2e
             COMMAND "${ROSPINYIN_PYTHON}"
                     "${PROJECT_SOURCE_DIR}/tests/check_mkdat.py"
                     --mkdat $<TARGET_FILE:mkdat>
                     --root "${PROJECT_SOURCE_DIR}")
else()
    message(STATUS "mkdat or Python3 unavailable; mkdat-e2e skipped")
endif()
