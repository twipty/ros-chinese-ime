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
        scheme_reg.cpp)
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
# C:\rospinyin-trace.log. The option is declared at the repository root
# (X2-e1) because the platform libraries read the macro and they are configured
# before this directory.

# Version-resource language of rospinyin.ime. This is not cosmetic: imm32's
# ImmInstallIMEW derives the layout's HKL from the first VarFileInfo\Translation
# entry (imm32_install.c: `pInfoEx->hkl = UlongToHandle(*pw)`), so it decides
# which "keyboard" the layout lands under — 0804 -> Chinese keyboard,
# 0409 -> English keyboard, which is where the reference Japanese IME ships
# under on a stock system.
# 0409 exists purely to build a control binary for A/B comparisons; the product
# default stays 0804 (a Simplified Chinese IME must sit under a zh-CN layout).
set(ROSPINYIN_IME_LANGID "0804" CACHE STRING
    "Version-resource language of rospinyin.ime (0804=zh-CN, 0409=en-US)")

# Assembly only: one scheme + branding + the two framework libraries. The
# Win32 host and UI are libraries now (framework/platform/win32/CMakeLists.txt,
# X2-e1), so this file names no platform source file. That is the point of the
# split: a new input method assembles an .ime without touching the host layer.
add_library(rospinyin-ime SHARED
    ${ROSPINYIN_SCHEME_SOURCES}
    rospinyin.def
    rospinyin_res.rc)

# windres does not emit a header dependency for #include "version.h". Without
# this, bumping ime/version.h silently leaves the old FILEVERSION in the .ime
# and the release gate fails only after a full rebuild.
set_source_files_properties(rospinyin_res.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/version.h")

target_include_directories(rospinyin-ime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${ROSPINYIN_SCHEME_INCLUDE}")

# imfw-win32-imm32 brings imfw-win32-ui through the (static) dependency cycle;
# both are named anyway so the link line states what the module contains.
#
# --whole-archive on the host library is not belt-and-braces: GNU ld pulls an
# archive member only to satisfy an *undefined reference*, and the 16 names in
# rospinyin.def are export entries, not references. Without it the link fails
# with "cannot export ImeProcessKey: symbol not defined" — the entry points are
# reachable from imm32.dll at runtime, so nothing in the module references them
# at link time. The UI library needs no such treatment: the host calls into it.
target_link_libraries(rospinyin-ime PRIVATE
    ${ROSPINYIN_SCHEME_LIBS}
    "-Wl,--whole-archive" imfw-win32-imm32 "-Wl,--no-whole-archive"
    imfw-win32-ui)

# Output must be exactly "rospinyin.ime" (no lib prefix, no .dll suffix).
set_target_properties(rospinyin-ime PROPERTIES
    PREFIX ""
    SUFFIX ""
    OUTPUT_NAME "rospinyin.ime")

# No ROSPINYIN_TRACE here on purpose (X2-e2): nothing compiled into this target
# reads the macro any more. The trace channel is a property of the host library
# (framework/platform/win32/CMakeLists.txt) and of the pinyin data source
# (pinyin/CMakeLists.txt), and each declares it where it is read. A define on
# the assembly target would look like it worked while changing nothing.

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

# Do not drag a compiler runtime DLL onto a ReactOS installation, and do not
# let ld stamp the link time into the header. Both are stated once, in
# cmake/StaticRuntime.cmake (X2-e3): the same two promises apply to the
# installer, rhash and the download shims, and restating the flag set per build
# file is exactly how they drifted before.
rospinyin_static_runtime(rospinyin-ime CXX)
rospinyin_reproducible_pe(rospinyin-ime CXX)
