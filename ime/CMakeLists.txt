# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
#
# Windows/ReactOS only. Produces rospinyin.ime: a real IMM32 DLL with the
# 16 mandatory exports and a version resource that ImmLoadLayout accepts.
# M1 was an inert passthrough IME; M2 drives the pinyin engine and renders
# composition/candidate windows.

add_library(rospinyin-ime SHARED
    dllmain.cpp
    imm_entry.cpp
    ui_server.cpp
    dat_loader.cpp
    session_store.cpp
    ../ui/ime_ui.cpp
    ../ui/ui_window.cpp
    ../ui/composition_window.cpp
    ../ui/candidate_window.cpp
    rospinyin.def
    rospinyin_res.rc)

target_include_directories(rospinyin-ime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/ui"
    "${PROJECT_SOURCE_DIR}/engine/include")

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

# M2 links the platform-independent engine above. It was deliberately kept
# out of the M1 build so registration bugs could not be confounded with
# engine bugs; that isolation is no longer needed once M1 is signed off.
