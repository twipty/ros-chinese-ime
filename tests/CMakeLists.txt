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
