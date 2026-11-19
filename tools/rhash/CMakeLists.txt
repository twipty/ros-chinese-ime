# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
#
# rhash: SHA-256 for the guest, because ReactOS has no sha256sum/certutil.
# Build only for the standalone (ReactOS) form, like the installer.

add_executable(rhash rhash.c)

if(MSVC)
    # No vcruntime140.dll on a bare ReactOS install.
    set_property(TARGET rhash PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # wmain needs the Unicode CRT entry point; -static keeps libgcc out.
    target_link_options(rhash PRIVATE -municode -static)
    target_compile_options(rhash PRIVATE -Wall -Wextra)
endif()
