# imfw — reusable IMM32 input-method framework for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
#
# The Windows half of the framework, split into the two layers it actually has
# (audit A-P1-4, X2-e1). Before this file the layer was a list of 12 .cpp files
# inside ime/CMakeLists.txt, so "what belongs to the IMM32 host" and "what
# belongs to the windows" existed only as directory names.
#
#   imfw-win32-imm32   the host: the 16 entry points, key translation, the
#                      trace channel, the session store, the UI server window
#   imfw-win32-ui      the windows those entry points draw into
#
# The two are mutually dependent and that is not an accident: the host calls
# UiShow/UiHide/UiToolbarShow/..., the UI reads imfw::g_hInst and the class
# names from the Scheme. CMake allows a cycle between STATIC libraries (it
# repeats the whole component on the link line), and stating the cycle here is
# more honest than inventing a third "common" target whose only content would
# be a global variable.
#
# Neither library carries a scheme. rospinyin.ime links both plus exactly one
# scheme_reg*.cpp — see ime/CMakeLists.txt and samples/toy-scheme/.

add_library(imfw-win32-imm32 STATIC
    imm32/dllmain.cpp
    imm32/imm_entry.cpp
    imm32/key_translate.cpp
    imm32/trace_line.cpp
    imm32/win32_host.cpp
    imm32/ui_server.cpp
    imm32/session_store.cpp)

# PUBLIC: ime_ui.h / ime_host.h live here and ui/ consumes them.
target_include_directories(imfw-win32-imm32 PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/imm32>")
target_link_libraries(imfw-win32-imm32 PUBLIC imfw-engine imm32 user32)
target_compile_features(imfw-win32-imm32 PUBLIC cxx_std_17)

add_library(imfw-win32-ui STATIC
    ui/ime_ui.cpp
    ui/ui_window.cpp
    ui/composition_window.cpp
    ui/candidate_window.cpp
    ui/toolbar.cpp)

target_include_directories(imfw-win32-ui PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ui>")
# ime_ui.cpp implements the bridge declared in imm32/ime_ui.h and reads
# imfw::g_hInst from imm32/ime_host.h. Private: callers of the UI layer have no
# business seeing the host's headers.
target_include_directories(imfw-win32-ui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/imm32")
target_link_libraries(imfw-win32-ui PUBLIC gdi32 user32)
target_compile_features(imfw-win32-ui PUBLIC cxx_std_17)

# The cycle, written down twice on purpose: CMake reads a static-library
# dependency graph edge as "link this too", and an edge in one direction only
# would make the other layer's symbols look unreachable to a fresh reader.
target_link_libraries(imfw-win32-imm32 PUBLIC imfw-win32-ui)
target_link_libraries(imfw-win32-ui PUBLIC imfw-win32-imm32)

# The diagnostic channel is a property of the host (dllmain/imm_entry/trace_line
# all read ROSPINYIN_TRACE), so the define travels with the library instead of
# with whichever target happens to assemble it.
if(ROSPINYIN_TRACE)
    target_compile_definitions(imfw-win32-imm32 PUBLIC ROSPINYIN_TRACE)
endif()
