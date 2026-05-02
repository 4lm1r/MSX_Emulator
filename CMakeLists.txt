cmake_minimum_required(VERSION 3.14)
project(MSXEmulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# ImGui via FetchContent
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)
set(IMGUI_INCLUDE_DIRS
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Include directories
include_directories(src src/cpu src/vdp src/memory src/psg)

# Source files
set(SOURCES
    src/main.cpp
    src/debug.cpp
    src/cpu/z80a.cpp
    src/cpu/opcodesHandler.cpp
    src/vdp/vdp.cpp
    src/memory/memory.cpp
    src/ppi/ppi.cpp
    src/keyboard/keyboard.cpp
    src/psg/psg.cpp
)

# Test source files
set(TEST_SOURCES
    tests/cpu/test_z80a.cpp
    tests/vdp/test_vdp.cpp
    tests/memory/test_memory.cpp
    src/debug.cpp
    src/cpu/z80a.cpp
    src/cpu/opcodesHandler.cpp
    src/vdp/vdp.cpp
    src/memory/memory.cpp
    src/ppi/ppi.cpp
    src/keyboard/keyboard.cpp
    src/psg/psg.cpp
)

# Main executable (includes ImGui)
add_executable(msx_emulator ${SOURCES} ${IMGUI_SOURCES})
target_include_directories(msx_emulator PRIVATE ${IMGUI_INCLUDE_DIRS})
target_link_libraries(msx_emulator ${SDL2_LIBRARIES})

# Tests (no ImGui needed)
enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(msx_emulator_tests ${TEST_SOURCES})
target_link_libraries(msx_emulator_tests ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread ${SDL2_LIBRARIES})

add_test(NAME Z80ATest COMMAND msx_emulator_tests)
