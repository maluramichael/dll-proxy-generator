cmake_minimum_required(VERSION 3.15)
project(d3d9_overlay CXX C)

# Static CRT so the drop-in d3d9.dll does not depend on the VC++ redist being
# installed on the target machine (WoW 3.3.5a box may not have VCRUNTIME140).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
cmake_policy(SET CMP0091 NEW)

# x86 only:  cmake -A Win32 -B build ;  cmake --build build --config Release

set(TP "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
set(IMGUI "${TP}/imgui")
set(MINHOOK "${TP}/minhook")

add_library(d3d9 SHARED
    d3d9_proxy.cpp           # generator output: 15 forward pragmas
    d3d9_overlay.cpp         # our overrides + ImGui hook
    d3d9_overlay.def         # clean undecorated export names

    ${IMGUI}/imgui.cpp
    ${IMGUI}/imgui_draw.cpp
    ${IMGUI}/imgui_tables.cpp
    ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/imgui_demo.cpp
    ${IMGUI}/backends/imgui_impl_dx9.cpp
    ${IMGUI}/backends/imgui_impl_win32.cpp

    ${MINHOOK}/src/buffer.c
    ${MINHOOK}/src/hook.c
    ${MINHOOK}/src/trampoline.c
    ${MINHOOK}/src/hde/hde32.c
    ${MINHOOK}/src/hde/hde64.c
)

target_include_directories(d3d9 PRIVATE
    ${IMGUI}
    ${IMGUI}/backends
    ${MINHOOK}/include
)

set_target_properties(d3d9 PROPERTIES
    OUTPUT_NAME "d3d9"   # produces d3d9.dll
    PREFIX "")

# No d3d9.lib: the real entry points come via LoadLibrary + GetProcAddress, so
# there is no import dependency on d3d9.dll (which would be a self-import).
set_target_properties(d3d9 PROPERTIES LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/d3d9_overlay.def")
