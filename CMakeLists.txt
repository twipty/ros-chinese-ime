cmake_minimum_required(VERSION 3.16)
# C is required for the fully-static HTTP download helper (tools/httpdl).
project(RosPinyin VERSION 0.0.1 LANGUAGES C CXX)

# Product form (decision D5): standalone installable package is the default.
# ROSPINYIN_IN_TREE reserves the conditional path for a future ReactOS
# modules/optional in-tree target. It costs nothing at M1 and keeps the
# option open without committing to it.
option(ROSPINYIN_IN_TREE     "Build as ReactOS modules/optional component" OFF)
option(ROSPINYIN_BUILD_TESTS "Build engine unit tests"                     ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files are UTF-8 without BOM (checked in CI) and contain non-ASCII
# string literals (Chinese layout names). MSVC on an en-US host defaults to
# CP1252, which turns every L"..." with Chinese text into mojibake at compile
# time (this showed up as wrong characters in kbswitch). GCC/Clang, including
# MinGW-w64, default to UTF-8 input already.
if(MSVC)
    add_compile_options(/utf-8)
endif()

# windres does not accept MSVC's "#pragma code_page" (it is guarded by
# _MSC_VER in rospinyin_res.rc), so hand it the codepage explicitly.
if(MINGW)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -c 65001")
endif()

if(WIN32)
    # .rc version/string resources of rospinyin.ime need the RC language.
    enable_language(RC)
endif()

# Platform-independent pinyin engine. Contains no Windows headers, so it
# builds and is unit-tested on Linux CI. M2 grows this into the real engine.
add_library(rospinyin-engine STATIC
    engine/src/session.cpp
    engine/src/pinyin.cpp
    engine/src/table.cpp
    engine/src/pinyin_session.cpp)
target_include_directories(rospinyin-engine PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/engine/include>")
target_compile_features(rospinyin-engine PUBLIC cxx_std_17)

# IMM32 layer and the standalone installer are Windows/ReactOS only.
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ime/CMakeLists.txt")
    add_subdirectory(ime)
endif()
if(WIN32 AND NOT ROSPINYIN_IN_TREE AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/install-rospinyin/CMakeLists.txt")
    add_subdirectory(tools/install-rospinyin)
endif()
# Fully-static HTTP download helper for the ReactOS VM (wget/curl shims).
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/httpdl/CMakeLists.txt")
    add_subdirectory(tools/httpdl)
endif()
# M2 data compiler: a host-side tool (runs on Linux CI), no Windows headers.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/mkdat/CMakeLists.txt")
    add_subdirectory(tools/mkdat)
endif()

if(ROSPINYIN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ROSPINYIN_IN_TREE)
    message(STATUS "RosPinyin target: ReactOS modules/optional (in-tree)")
else()
    message(STATUS "RosPinyin target: standalone installable package")
endif()
