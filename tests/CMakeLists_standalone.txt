cmake_minimum_required(VERSION 3.13)

# Set project for host testing
project(picocalc_sd_boot_tests C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add compile definitions
add_compile_definitions(
    NDEBUG
    TEST
    PICO_NO_HARDWARE=1
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../bootloader/include
    ${CMAKE_SOURCE_DIR}/../bootloader/src
)

# Common test sources
set(TEST_COMMON_SOURCES
    test_common.c
    uf2_test_utils.c
)

# Function to create a test executable
function(add_test_executable name source)
    add_executable(${name} ${source} ${TEST_COMMON_SOURCES})
endfunction()

# Add test executables
add_test_executable(test_uf2 test_uf2.c)
add_test_executable(test_critical_scenarios test_critical_scenarios.c)
add_test_executable(test_safety_gaps test_safety_gaps.c)

# Enable testing
enable_testing()
add_test(NAME test_uf2 COMMAND test_uf2)
add_test(NAME test_critical_scenarios COMMAND test_critical_scenarios)
add_test(NAME test_safety_gaps COMMAND test_safety_gaps) 