# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
#
# Assembly of rospinyin.ime: framework host + framework UI + the chosen scheme
# + branding resources. The scheme is the only part that differs per input
# method; a future shape-based IME assembles its own .ime from the same
# framework/ with a different scheme_reg*.cpp (see -DROSPINYIN_SCHEME below).

# Which scheme this .ime is built from (X2-a0).
#
# "pinyin" is the product. "toy" is the seam's canary: a letters-only scheme in
# samples/toy-scheme/ that exists to prove the claim in PLAN-MODULARIZATION G4 —
# a second input method is a folder plus one scheme_factory(), not a patch to
# framework/. Both branches are ordinary builds of this same target; the toy one
# is not shippable (no Chinese data at all) and says so at configure time.
set(ROSPINYIN_SCHEME "pinyin" CACHE STRING
    "Input-method scheme assembled into rospinyin.ime (pinyin|toy)")

if(ROSPINYIN_SCHEME STREQUAL "pinyin")
    set(ROSPINYIN_SCHEME_SOURCES
        scheme_reg.cpp
        ../pinyin/host/dat_source_win.cpp)
    set(ROSPINYIN_SCHEME_INCLUDE "${PROJECT_SOURCE_DIR}/pinyin/include")
    # rospinyin-engine brings imfw-engine transitively (PUBLIC link).
    set(ROSPINYIN_SCHEME_LIBS rospinyin-engine)
elseif(ROSPINYIN_SCHEME STREQUAL "toy")
    set(ROSPINYIN_SCHEME_SOURCES
        scheme_reg_toy.cpp
        ../samples/toy-scheme/toy_scheme.cpp)
    set(ROSPINYIN_SCHEME_INCLUDE "${PROJECT_SOURCE_DIR}/samples/toy-scheme")
    set(ROSPINYIN_SCHEME_LIBS imfw-engine)
    message(WARNING
        "ROSPINYIN_SCHEME=toy: building the letters-only injection-seam canary. "
        "It carries no Chinese data and must not be shipped.")
else()
    message(FATAL_ERROR
        "ROSPINYIN_SCHEME must be pinyin or toy, got '${ROSPINYIN_SCHEME}'")
endif()

# Diagnostic channel: when ON, every IMM32 entry-point call appends a line to
# C:\rospinyin-trace.log (see framework/platform/win32/imm32/imm_entry.cpp). Off by default so
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
    ${ROSPINYIN_SCHEME_SOURCES}
    ../framework/platform/win32/imm32/dllmain.cpp
    ../framework/platform/win32/imm32/imm_entry.cpp
    ../framework/platform/win32/imm32/key_translate.cpp
    ../framework/platform/win32/imm32/trace_line.cpp
    ../framework/platform/win32/imm32/win32_host.cpp
    ../framework/platform/win32/imm32/ui_server.cpp
    ../framework/platform/win32/imm32/session_store.cpp
    ../framework/platform/win32/ui/ime_ui.cpp
    ../framework/platform/win32/ui/ui_window.cpp
    ../framework/platform/win32/ui/composition_window.cpp
    ../framework/platform/win32/ui/candidate_window.cpp
    ../framework/platform/win32/ui/toolbar.cpp
    rospinyin.def
    rospinyin_res.rc)

target_include_directories(rospinyin-ime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/framework/platform/win32/imm32"
    "${PROJECT_SOURCE_DIR}/framework/include"
    "${PROJECT_SOURCE_DIR}/framework/platform/win32/ui"
    "${ROSPINYIN_SCHEME_INCLUDE}")

target_link_libraries(rospinyin-ime PRIVATE
    ${ROSPINYIN_SCHEME_LIBS} user32 gdi32 imm32)

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
