# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.

add_executable(rospinyin-smoke smoke_test.cpp)
target_link_libraries(rospinyin-smoke PRIVATE rospinyin-engine)
add_test(NAME smoke COMMAND rospinyin-smoke)

# WBS 2.4 host-side counterpart: passthrough semantics + 50 switch cycles.
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

# M2 UI logic: 9-candidate paging and digit selection.
add_executable(rospinyin-candidate-pager candidate_pager_test.cpp)
target_link_libraries(rospinyin-candidate-pager PRIVATE rospinyin-engine)
add_test(NAME candidate-pager COMMAND rospinyin-candidate-pager)

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
