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

# Built-in fallback table switch (decision 2026-11-19). The option lives at
# the repository root; scheme.cpp compiles its kBuiltin data (and the real
# load_builtin_table body) only when the switch is ON, so the ~3 KB of
# embedded table bytes leave the release binary with the switch. The empty-
# table contract (tests/builtin_gate_test.cpp) is what both configurations
# share: OFF builds rely on it when the .dat is missing, ON builds pin it as
# the reference behaviour.
if(ROSPINYIN_BUILTIN_TABLE)
    target_compile_definitions(rospinyin-engine PRIVATE ROSPINYIN_BUILTIN_TABLE)
endif()

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

    # host/dat_source_win.cpp writes the `dat ...` lines of the trace channel.
    # Since X2-e6 the formatter is an imfw public header, reached through
    # rospinyin-engine -> imfw-engine's PUBLIC include path; the old direct
    # framework/platform/win32/imm32 path is gone.

    # Same macro the host libraries carry: the `dat ...` lines have to appear in
    # the trace build or a "wrong table" report becomes unattributable.
    if(ROSPINYIN_TRACE)
        target_compile_definitions(rospinyin-engine PRIVATE ROSPINYIN_TRACE)
    endif()
endif()
