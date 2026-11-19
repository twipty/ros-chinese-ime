# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
#
# The standalone installer tool. Skipped for the in-tree product form:
# ReactOS's own build/install machinery would own registration there.

add_executable(install-rospinyin install.cpp)

target_link_libraries(install-rospinyin PRIVATE imm32 advapi32)

# Static CRT + reproducible header (cmake/StaticRuntime.cmake, X2-e3).
rospinyin_static_runtime(install-rospinyin CXX)
rospinyin_reproducible_pe(install-rospinyin CXX)

if(NOT MSVC)
    # wmain requires the Unicode CRT entry point on MinGW-w64. This is about
    # which entry point the PE declares, not about the runtime, so it stays here
    # while the runtime flags live in cmake/StaticRuntime.cmake.
    target_link_options(install-rospinyin PRIVATE -municode)
endif()
