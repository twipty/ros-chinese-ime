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
    "${CMAKE_SOURCE_DIR}/samples/toy-scheme")
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

# X2-b: the same layers, but asserting the *sequence* instead of single
# decisions. One fixed key run (zhongguo -> 2 -> Backspace x2 -> Esc -> lone
# Shift), one exact host-call log. This is the net X2-c/X2-d must not move:
# both promise "the call sequence is unchanged", which is otherwise only
# checkable by reading a diff.
add_executable(rospinyin-host-smoke host_smoke_test.cpp)
target_link_libraries(rospinyin-host-smoke PRIVATE imfw-engine)
add_test(NAME host-smoke COMMAND rospinyin-host-smoke)

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

# Static M1 contract checks (exports, version resource, M0 red lines).
find_program(ROSPINYIN_PYTHON NAMES python3 python QUIET)
if(ROSPINYIN_PYTHON)
    add_test(NAME m1-contract
             COMMAND "${ROSPINYIN_PYTHON}"
                     "${CMAKE_SOURCE_DIR}/tools/check_m1.py"
                     --root "${CMAKE_SOURCE_DIR}")
else()
    message(STATUS "Python3 not found; M1 contract check skipped")
endif()

# M2: end-to-end mkdat test on checked-in fixtures (no network needed).
if(TARGET mkdat AND ROSPINYIN_PYTHON)
    add_test(NAME mkdat-e2e
             COMMAND "${ROSPINYIN_PYTHON}"
                     "${CMAKE_SOURCE_DIR}/tests/check_mkdat.py"
                     --mkdat $<TARGET_FILE:mkdat>
                     --root "${CMAKE_SOURCE_DIR}")
else()
    message(STATUS "mkdat or Python3 unavailable; mkdat-e2e skipped")
endif()
