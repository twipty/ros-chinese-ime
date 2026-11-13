# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
#
# Windows/ReactOS only. Produces rospinyin.ime: a real IMM32 DLL with the
# 16 mandatory exports and a version resource that ImmLoadLayout accepts.
# At M1 the module is a passthrough IME (no key is consumed).

add_library(rospinyin-ime SHARED
    dllmain.cpp
    imm_entry.cpp
    ui_server.cpp
    rospinyin.def
    rospinyin_res.rc)

target_include_directories(rospinyin-ime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/engine/include")

target_link_libraries(rospinyin-ime PRIVATE user32 gdi32 imm32)

# Output must be exactly "rospinyin.ime" (no lib prefix, no .dll suffix).
set_target_properties(rospinyin-ime PROPERTIES
    PREFIX ""
    SUFFIX ""
    OUTPUT_NAME "rospinyin.ime")

# Do not drag a compiler runtime DLL onto a ReactOS installation.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(rospinyin-ime PRIVATE -static-libgcc -static-libstdc++)
endif()

# M2 will link the platform-independent engine here. At M1 the IME layer
# deliberately has zero dependency on it so registration bugs cannot be
# confounded with engine bugs.
