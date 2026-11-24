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

# Self-contained GUI shell. It embeds the two compressed payloads as RCDATA
# compiled from gui_res.rc, inflates them into a private %TEMP% directory and
# drives the unchanged CLI installer one /stepN at a time. No registry writes
# live in this translation unit.
add_executable(install-rospinyin-gui WIN32 gui.cpp gui_res.rc)

# windres does not report the generated payload header as a dependency.
set_source_files_properties(gui_res.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/payload_base64.hpp")
target_include_directories(install-rospinyin-gui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(install-rospinyin-gui PRIVATE
    advapi32 user32 gdi32)
rospinyin_static_runtime(install-rospinyin-gui CXX)
rospinyin_reproducible_pe(install-rospinyin-gui CXX)
if(NOT MSVC)
    target_link_options(install-rospinyin-gui PRIVATE -municode)
endif()
