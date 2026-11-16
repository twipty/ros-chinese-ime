# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
#
# mkdat is a HOST tool: it runs on the build machine (Linux CI) to compile
# rospinyin.dat. It reuses the platform-independent normalization routines
# from rospinyin-engine so build-time and run-time agree on canonical forms.

add_executable(mkdat mkdat.cpp)
target_link_libraries(mkdat PRIVATE rospinyin-engine)
