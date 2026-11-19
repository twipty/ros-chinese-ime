# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
#
# What a PE we ship to ReactOS looks like (X2-e3, audit A-P1-6).
#
# Every binary here runs on a ReactOS 0.4.16 installation that has no
# libstdc++-6.dll, no libgcc_s_dw2-1.dll, no libwinpthread-1.dll and no
# vcruntime140.dll. Before this file that fact was stated four times, in four
# build files, with four slightly different flag sets -- and tools/httpdl only
# had half of it. A fact restated per consumer is a fact that drifts.
#
# Two separate functions on purpose: "the runtime is inside the binary" and
# "the binary is reproducible" are different promises, and a target may one day
# want the first without the second.
#
# Call sites pass the target's language (C or CXX) because the two need
# different flags: -static-libstdc++ on a pure C link is noise, and this
# repository builds both.

# rospinyin_static_runtime(<target> <C|CXX>)
#
# MSVC is kept for the historical toolchain only: its static CRT imports
# kernel32!InitializeCriticalSectionEx, which ReactOS 0.4.16 does not export.
# MinGW is the supported build (see .github/workflows/ci.yml).
function(rospinyin_static_runtime target lang)
    if(MSVC)
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        return()
    endif()
    if(NOT CMAKE_${lang}_COMPILER_ID MATCHES "GNU|Clang")
        return()
    endif()
    if(lang STREQUAL "CXX")
        target_link_options(${target} PRIVATE
            -static-libgcc -static-libstdc++ -static)
    else()
        target_link_options(${target} PRIVATE -static)
    endif()
endfunction()

# rospinyin_reproducible_pe(<target> <C|CXX>)
#
# ld stamps the link time into the PE header (TimeDateStamp + Checksum, plus
# two mirror bytes), so two builds of byte-identical sources get two different
# sha256 values. Measured on the .ime: 6 bytes apart, same size, same code.
#
# That is not tidiness. "Is the file in system32 the build I just made?" is
# answered by a hash -- tools/artifact_hashes.py on the host and the
# guest-side rhash.exe exist for exactly that question -- and a timestamp makes
# every rebuild answer "no". Nothing on ReactOS reads the field, and the
# version resource does not either.
function(rospinyin_reproducible_pe target lang)
    if(MSVC)
        # /Brepro is MSVC's equivalent (VS2015 Update 2+).
        target_link_options(${target} PRIVATE /Brepro)
        return()
    endif()
    if(CMAKE_${lang}_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(${target} PRIVATE -Wl,--no-insert-timestamp)
    endif()
endfunction()
