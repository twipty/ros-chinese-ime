# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
#
# rhash: SHA-256 for the guest, because ReactOS has no sha256sum/certutil.
# Build only for the standalone (ReactOS) form, like the installer.

add_executable(rhash rhash.c)

# Static CRT + reproducible header (cmake/StaticRuntime.cmake, X2-e3). The
# reproducible half matters most here: this tool is what the guest uses to
# answer "is this the build I meant?", so the binaries it hashes must not carry
# a link timestamp either.
rospinyin_static_runtime(rhash C)
rospinyin_reproducible_pe(rhash C)

if(NOT MSVC)
    # wmain needs the Unicode CRT entry point (entry point, not runtime).
    target_link_options(rhash PRIVATE -municode)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rhash PRIVATE -Wall -Wextra)
endif()
