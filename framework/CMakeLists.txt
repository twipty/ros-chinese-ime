# imfw — reusable IMM32 input-method framework for ReactOS
# SPDX-License-Identifier: MIT
#
# Host-independent core: the engine contract, the generic code table (dat
# reader), candidate paging and the build identifier. No Windows headers,
# so Linux CI builds and unit-tests it directly.

add_library(imfw-engine STATIC
    src/engine.cpp
    src/code_table.cpp
    src/candidate_pager.cpp
    src/emit_text.cpp
    src/host_key.cpp
    src/key_route.cpp
    src/scheme_registry.cpp
    src/utf8.cpp)

target_include_directories(imfw-engine PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(imfw-engine PUBLIC cxx_std_17)
