# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
#
# Assembly of rospinyin.ime: framework host + framework UI + pinyin plugin +
# branding resources. A future shape-based IME assembles its own .ime from the
# same framework/ and pinyin/ directories with a different scheme_reg.cpp.

# Diagnostic channel: when ON, every IMM32 entry-point call appends a line to
# C:\rospinyin-trace.log (see framework/host/imm_entry.cpp). Off by default so
# a shipped .ime carries no trace code at all.
option(ROSPINYIN_TRACE "Trace IMM32 entry points to C:\\rospinyin-trace.log" OFF)

# Version-resource language of rospinyin.ime. This is not cosmetic: imm32's
# ImmInstallIMEW derives the layout's HKL from the first VarFileInfo\Translation
# entry (imm32_install.c: `pInfoEx->hkl = UlongToHandle(*pw)`), so it decides
# which "keyboard" the layout lands under — 0804 -> Chinese keyboard,
# 0409 -> English keyboard, which is where the system's mzimeja.ime lives.
# 0409 exists purely to build a control binary for A/B comparisons; the product
# default stays 0804 (a Simplified Chinese IME must sit under a zh-CN layout).
set(ROSPINYIN_IME_LANGID "0804" CACHE STRING
    "Version-resource language of rospinyin.ime (0804=zh-CN, 0409=en-US)")

add_library(rospinyin-ime SHARED
    scheme_reg.cpp
    ../framework/host/dllmain.cpp
    ../framework/host/imm_entry.cpp
    ../framework/host/ui_server.cpp
    ../framework/host/session_store.cpp
    ../framework/ui/ime_ui.cpp
    ../framework/ui/ui_window.cpp
    ../framework/ui/composition_window.cpp
    ../framework/ui/candidate_window.cpp
    ../framework/ui/toolbar.cpp
    ../pinyin/host/dat_source_win.cpp
    rospinyin.def
    rospinyin_res.rc)

target_include_directories(rospinyin-ime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/framework/host"
    "${PROJECT_SOURCE_DIR}/framework/include"
    "${PROJECT_SOURCE_DIR}/framework/ui"
    "${PROJECT_SOURCE_DIR}/pinyin/include")

# rospinyin-engine brings imfw-engine transitively (PUBLIC link).
target_link_libraries(rospinyin-ime PRIVATE rospinyin-engine user32 gdi32 imm32)

# Output must be exactly "rospinyin.ime" (no lib prefix, no .dll suffix).
set_target_properties(rospinyin-ime PROPERTIES
    PREFIX ""
    SUFFIX ""
    OUTPUT_NAME "rospinyin.ime")

if(ROSPINYIN_TRACE)
    target_compile_definitions(rospinyin-ime PRIVATE ROSPINYIN_TRACE)
endif()

# Feed the language choice to windres (it honours -D). Only the MinGW build is
# supported for the .ime, so COMPILE_OPTIONS on the .rc is enough.
if(ROSPINYIN_IME_LANGID STREQUAL "0409")
    set_source_files_properties(rospinyin_res.rc PROPERTIES
        COMPILE_OPTIONS "-DROSPINYIN_IME_LANG_EN")
    message(STATUS "rospinyin.ime version resource: en-US (0409 control build)")
elseif(NOT ROSPINYIN_IME_LANGID STREQUAL "0804")
    message(FATAL_ERROR
        "ROSPINYIN_IME_LANGID must be 0804 or 0409, got '${ROSPINYIN_IME_LANGID}'")
endif()

# Do not drag a compiler runtime DLL onto a ReactOS installation.
if(MSVC)
    # Kept for the historical MSVC toolchain only. ReactOS 0.4.16's kernel32
    # lacks InitializeCriticalSectionEx, which MSVC's static CRT imports, so
    # the supported build is MinGW (see .github/workflows/ci.yml).
    set_property(TARGET rospinyin-ime PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # A bare ReactOS has none of libstdc++-6.dll / libgcc_s_dw2-1.dll /
    # libwinpthread-1.dll, so link the whole runtime statically. The resulting
    # PE then only imports system DLLs (kernel32/user32/gdi32/imm32).
    target_link_options(rospinyin-ime PRIVATE
        -static-libgcc -static-libstdc++ -static)
endif()
