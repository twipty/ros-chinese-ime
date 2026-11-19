# RosPinyin pinyin plugin (engine + scheme)
# SPDX-License-Identifier: MIT
#
# Pinyin-specific logic: reading normalization, syllable segmentation and the
# PinyinScheme implementing the framework's plugin contract. Host-independent
# (no Windows headers) so Linux CI builds and unit-tests it. The one exception
# is the Windows data source below, which is why the base sources and the
# Windows-only source are two separate statements.

add_library(rospinyin-engine STATIC
    src/pinyin.cpp
    src/pinyin_session.cpp
    src/scheme.cpp
    src/dat_source.cpp)

target_include_directories(rospinyin-engine PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(rospinyin-engine PUBLIC imfw-engine)
target_compile_features(rospinyin-engine PUBLIC cxx_std_17)

# The plug-in owns its data end to end, including the Win32 half of the read
# (audit A-P1-5, X2-e2).
#
# load_system_table() has two implementations:
#   * host/dat_source_win.cpp -- (GetSystemDirectoryW + CreateFileW) and the
#     imfw::CodeTable parser. Windows only, and it belongs to *this* library:
#     the .dat is the pinyin scheme's data, not the framework's.
#   * src/dat_source.cpp -- a weak stub returning nullptr, so load_pinyin_table()
#     degrades to the built-in table on purpose on Linux/host tests.
#
# A weak stub that is always present turns "forgot to compile the real one" into
# a working binary: measured before this commit by deleting the file from
# ime/CMakeLists.txt, the link still succeeded and produced a 942,989 B .ime
# (vs 948,481 B) that shipped the built-in table only -- fewer candidates, no
# polyphones, and no message anywhere. ROSPINYIN_REQUIRE_SYSTEM_TABLE compiles
# the stub out, so the same mistake is now an undefined reference at link time.
if(WIN32)
    target_sources(rospinyin-engine PRIVATE host/dat_source_win.cpp)
    target_compile_definitions(rospinyin-engine PRIVATE
        ROSPINYIN_REQUIRE_SYSTEM_TABLE)

    # host/dat_source_win.cpp writes the `dat ...` lines of the trace channel,
    # formatted by the channel's one timestamp header (X2-d). That header lives
    # in the host layer; the path is the remaining cross-layer debt and X2-e6
    # replaces it with a minimal public interface.
    target_include_directories(rospinyin-engine PRIVATE
        "${PROJECT_SOURCE_DIR}/framework/platform/win32/imm32")

    # Same macro the host libraries carry: the `dat ...` lines have to appear in
    # the trace build or a "wrong table" report becomes unattributable.
    if(ROSPINYIN_TRACE)
        target_compile_definitions(rospinyin-engine PRIVATE ROSPINYIN_TRACE)
    endif()
endif()
