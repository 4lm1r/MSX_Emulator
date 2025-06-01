cmake_minimum_required(VERSION 3.10)
project(MSXEmulator)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Include directories
include_directories(src)
include_directories(src/cpu)
include_directories(src/vdp)
include_directories(src/memory)

# Source files
set(SOURCES
    src/main.cpp
    src/debug.cpp
    src/cpu/z80a.cpp
    src/cpu/opcodesHandler.cpp
    src/vdp/vdp.cpp
    src/memory/memory.cpp
)

# Test source files
set(TEST_SOURCES
    tests/cpu/test_z80a.cpp
    src/debug.cpp
    src/cpu/z80a.cpp
    src/cpu/opcodesHandler.cpp
    src/vdp/vdp.cpp
    src/memory/memory.cpp
)

# Create main executable
add_executable(msx_emulator ${SOURCES})
target_link_libraries(msx_emulator ${SDL2_LIBRARIES})

# Enable testing
enable_testing()

# Find Google Test
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Create test executable
add_executable(msx_emulator_tests ${TEST_SOURCES})
target_link_libraries(msx_emulator_tests ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread ${SDL2_LIBRARIES})

# Add test
add_test(NAME Z80ATest COMMAND msx_emulator_tests)
