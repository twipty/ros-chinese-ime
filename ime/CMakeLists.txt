# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
#
# Assembly of rospinyin.ime: framework host + framework UI + pinyin plugin +
# branding resources. A future shape-based IME assembles its own .ime from the
# same framework/ and pinyin/ directories with a different scheme_reg.cpp.

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
