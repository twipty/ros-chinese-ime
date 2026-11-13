cmake_minimum_required(VERSION 3.16)
project(RosPinyin VERSION 0.0.1 LANGUAGES CXX)

# Product form (decision D5): standalone installable package is the default.
# ROSPINYIN_IN_TREE reserves the conditional path for a future ReactOS
# modules/optional in-tree target. It costs nothing at M1 and keeps the
# option open without committing to it.
option(ROSPINYIN_IN_TREE     "Build as ReactOS modules/optional component" OFF)
option(ROSPINYIN_BUILD_TESTS "Build engine unit tests"                     ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-independent pinyin engine (M2). Header-only placeholder at M1:
# no platform APIs, no Windows headers, so it builds and tests on Linux CI.
add_library(rospinyin-engine INTERFACE)
target_include_directories(rospinyin-engine INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/engine/include>")
target_compile_features(rospinyin-engine INTERFACE cxx_std_17)

# IMM32 layer is Windows/ReactOS only and is added in M1 tasks 2.2/2.4.
# Guarded by EXISTS so the M0 baseline tree configures cleanly everywhere.
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ime/CMakeLists.txt")
    add_subdirectory(ime)
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
