# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
#
# The standalone installer tool. Skipped for the in-tree product form:
# ReactOS's own build/install machinery would own registration there.

add_executable(install-rospinyin install.cpp)

target_link_libraries(install-rospinyin PRIVATE imm32 advapi32)

if(MSVC)
    # ReactOS has no vcruntime140.dll: link the CRT statically so the
    # installer runs on a bare ReactOS installation.
    set_property(TARGET install-rospinyin PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # wmain requires the Unicode CRT entry point on MinGW-w64.
    target_link_options(install-rospinyin PRIVATE -municode)
endif()
