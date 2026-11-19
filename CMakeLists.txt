cmake_minimum_required(VERSION 3.16)
# C is required for the fully-static HTTP download helper (tools/httpdl) and the
# guest-side SHA-256 tool (tools/rhash).
#
# X2-e5: the release version has one owner, ime/version.h, which is also what
# the .ime resource reads. A literal here used to say 0.0.1 while the shipped
# binary said 0,0,2; nothing consumed PROJECT_VERSION, so the lie was free.
# Read the numbers instead and fail loudly if the header is inconsistent.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/ime/version.h" _rpy_version_header)
foreach(_rpy_part MAJOR MINOR PATCH)
    string(REGEX MATCH
        "#[ \t]*define[ \t]+ROSPINYIN_VERSION_${_rpy_part}[ \t]+([0-9]+)"
        _rpy_match "${_rpy_version_header}")
    set(_rpy_${_rpy_part} "${CMAKE_MATCH_1}")
    if(NOT _rpy_${_rpy_part} MATCHES "^[0-9]+$")
        message(FATAL_ERROR
            "Cannot read ROSPINYIN_VERSION_${_rpy_part} from ime/version.h")
    endif()
endforeach()
string(REGEX MATCH
    "#[ \t]*define[ \t]+ROSPINYIN_VERSION_STRING[ \t]+\"([^\"]+)\""
    _rpy_match "${_rpy_version_header}")
set(_rpy_version_string "${CMAKE_MATCH_1}")
if(NOT _rpy_version_string MATCHES
        "^(alpha|beta)${_rpy_MAJOR}\\.${_rpy_MINOR}\\.${_rpy_PATCH}$")
    message(FATAL_ERROR
        "ime/version.h: ROSPINYIN_VERSION_STRING '${_rpy_version_string}' "
        "does not match ${_rpy_MAJOR}.${_rpy_MINOR}.${_rpy_PATCH}")
endif()
set(ROSPINYIN_PROJECT_VERSION "${_rpy_MAJOR}.${_rpy_MINOR}.${_rpy_PATCH}")
project(RosPinyin VERSION ${ROSPINYIN_PROJECT_VERSION} LANGUAGES C CXX)

# Product form (decision D5): standalone installable package is the default.
# ROSPINYIN_IN_TREE reserves the conditional path for a future ReactOS
# modules/optional in-tree target. It costs nothing at M1 and keeps the
# option open without committing to it.
option(ROSPINYIN_IN_TREE     "Build as ReactOS modules/optional component" OFF)
option(ROSPINYIN_BUILD_TESTS "Build engine unit tests"                     ON)
# Diagnostic channel: when ON, every IMM32 entry-point call appends a line to
# C:\rospinyin-trace.log (framework/platform/win32/imm32). Off by default so a
# shipped .ime carries no trace code at all. Declared here, not in ime/: the
# platform libraries are added before ime/ and they are the ones that read the
# macro (X2-e1).
option(ROSPINYIN_TRACE       "Trace IMM32 entry points to C:\\rospinyin-trace.log" OFF)

# Language levels are stated once, for both languages (X2-e3). Before this the
# C++ level was pinned and C was left to the compiler default, which is gnu17
# on GCC 11+ and gnu11 on older ones: a C tool that compiles today could stop
# compiling after a toolchain upgrade with no change in this repository.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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

# rospinyin_static_runtime() / rospinyin_reproducible_pe(): what a shipped PE
# looks like. Defined at the top level so every subdirectory sees the same two
# functions instead of restating the flags (X2-e3, audit A-P1-6).
include("${PROJECT_SOURCE_DIR}/cmake/StaticRuntime.cmake")

# Reusable IMM32 framework: engine contract, generic code table, candidate
# paging (framework/). Contains no pinyin knowledge, so a shape-based IME can
# link the same library.
add_subdirectory(framework)

# Pinyin plugin: reading normalization, segmentation and the PinyinScheme.
add_subdirectory(pinyin)

# No EXISTS guards below (X2-e3). A guard turned "this subdirectory is missing"
# into a configure that succeeds and silently ships a smaller product: the same
# failure mode X2-e2 removed from the data source, one layer up. add_subdirectory
# on a missing directory is a hard configure error, which is what we want.

# IMM32 layer: Windows/ReactOS only.
if(WIN32)
    add_subdirectory(ime)
endif()
# Standalone package extras. The in-tree form does not build them: ReactOS's own
# build machinery owns registration there.
if(WIN32 AND NOT ROSPINYIN_IN_TREE)
    # Step-by-step installer (register/unregister without touching the default
    # input language).
    add_subdirectory(tools/install-rospinyin)
    # SHA-256 helper for the guest: ReactOS ships no sha256sum, and byte counts
    # cannot tell two same-sized builds apart.
    add_subdirectory(tools/rhash)
endif()
if(WIN32)
    # Fully-static HTTP download helper for the ReactOS VM (wget/curl shims).
    add_subdirectory(tools/httpdl)
endif()
# M2 data compiler: a host-side tool (runs on Linux CI), no Windows headers.
add_subdirectory(tools/mkdat)

if(ROSPINYIN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ROSPINYIN_IN_TREE)
    message(STATUS "RosPinyin target: ReactOS modules/optional (in-tree)")
else()
    message(STATUS "RosPinyin target: standalone installable package")
endif()
