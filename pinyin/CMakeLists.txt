# RosPinyin pinyin plugin (engine + scheme)
# SPDX-License-Identifier: MIT
#
# Pinyin-specific logic: reading normalization, syllable segmentation and the
# PinyinScheme implementing the framework's plugin contract. Host-independent
# (no Windows headers) so Linux CI builds and unit-tests it.

add_library(rospinyin-engine STATIC
    src/pinyin.cpp
    src/pinyin_session.cpp
    src/scheme.cpp
    src/dat_source.cpp)

target_include_directories(rospinyin-engine PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(rospinyin-engine PUBLIC imfw-engine)
target_compile_features(rospinyin-engine PUBLIC cxx_std_17)
