# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE "MinSizeRel")
set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "-specs=nano.specs")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../output)

set(PROJECT_VERSION "2.3")

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(sdboot)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(common/i2ckbd i2ckbd)
add_subdirectory(common/bootloader bootloader)
add_subdirectory(common/lcdspi lcdspi)
add_subdirectory(common/petit_fatfs petit_fatfs)

add_subdirectory(stage3)
add_subdirectory(ui)
add_subdirectory(diag)

