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

if(WIN32)
    # .rc version/string resources of rospinyin.ime need the RC language.
    enable_language(RC)
endif()

# Platform-independent pinyin engine. Contains no Windows headers, so it
# builds and is unit-tested on Linux CI. M2 grows this into the real engine.
add_library(rospinyin-engine STATIC engine/src/session.cpp)
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

if(ROSPINYIN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ROSPINYIN_IN_TREE)
    message(STATUS "RosPinyin target: ReactOS modules/optional (in-tree)")
else()
    message(STATUS "RosPinyin target: standalone installable package")
endif()
