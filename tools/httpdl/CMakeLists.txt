# RosPinyin - Simplified Chinese IME for ReactOS
# SPDX-License-Identifier: MIT
#
# Minimal, fully-static HTTP downloader for the ReactOS test VM. Built twice
# (wget.exe / curl.exe) from one C source so guest-side downloads never depend
# on wget/curl being present on bare ReactOS 0.4.16.

add_executable(ros-wget http_dl.c)
add_executable(ros-curl http_dl.c)

foreach(t ros-wget ros-curl)
    target_link_libraries(${t} PRIVATE ws2_32)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # Only KERNEL32/ADVAPI32/WS2_32/msvcrt survive in the final PE.
        target_link_options(${t} PRIVATE -static)
    endif()
endforeach()

set_target_properties(ros-wget PROPERTIES
    OUTPUT_NAME "wget" SUFFIX ".exe" PREFIX "")
set_target_properties(ros-curl PROPERTIES
    OUTPUT_NAME "curl" SUFFIX ".exe" PREFIX "")
