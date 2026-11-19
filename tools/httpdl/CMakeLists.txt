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
endforeach()

# Only KERNEL32/ADVAPI32/WS2_32/msvcrt survive in the final PE.
# Static runtime + reproducible header (cmake/StaticRuntime.cmake, X2-e3).
# This file used to state only the first half. The calls are written out per
# target instead of sitting in the loop above: a policy applied through ${t} is
# a policy a static check cannot see being applied, and these two lines are the
# whole difference between "has it" and "looks like it has it".
rospinyin_static_runtime(ros-wget C)
rospinyin_reproducible_pe(ros-wget C)
rospinyin_static_runtime(ros-curl C)
rospinyin_reproducible_pe(ros-curl C)

set_target_properties(ros-wget PROPERTIES
    OUTPUT_NAME "wget" SUFFIX ".exe" PREFIX "")
set_target_properties(ros-curl PROPERTIES
    OUTPUT_NAME "curl" SUFFIX ".exe" PREFIX "")
