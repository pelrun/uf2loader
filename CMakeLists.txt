cmake_minimum_required(VERSION 3.13)

# Set PICO_SDK_PATH to the location of the Pico SDK
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/pico-sdk")

# Include the Pico SDK (must be before project)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set the project name and languages
project(picocalc_sd_boot C CXX ASM)

# Initialize the Pico SDK
pico_sdk_init()

# Set the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optimization flags for size
add_compile_options(-Os -ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections)

# Custom libraries
add_subdirectory(bootloader/libs/i2ckbd)
add_subdirectory(bootloader/libs/lcdspi)

# pico-vfs library - include as interface library
include(pico-vfs/pico_vfs.cmake)

# Create pico-vfs library
add_library(pico-vfs INTERFACE)
target_sources(pico-vfs INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/src/blockdevice/sd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/src/blockdevice/heap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/src/blockdevice/loopback.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/src/filesystem/fat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/src/filesystem/littlefs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/src/filesystem/vfs.c
    # Add FatFS source files
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/vendor/ff15/source/ff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/vendor/ff15/source/ffsystem.c
    # Note: ffunicode.c removed to save space
    # Add littlefs source files
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/vendor/littlefs/lfs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/vendor/littlefs/lfs_util.c
)
target_include_directories(pico-vfs INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/include
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/include/filesystem/ChaN
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/vendor/littlefs
    ${CMAKE_CURRENT_SOURCE_DIR}/pico-vfs/vendor/ff15/source
)
target_link_libraries(pico-vfs INTERFACE
    hardware_spi
    pico_sync
)

# Main executable
add_executable(picocalc_sd_boot
    bootloader/src/main.c
    bootloader/src/uf2.c
    bootloader/src/proginfo.c
    bootloader/src/bldetect.c
    bootloader/src/key_event.c
    bootloader/src/text_directory_ui.c
    bootloader/src/ff_minimal_unicode.c
    bootloader/boot2/boot2_custom.S
)

# Set linker script based on target board
if(PICO_BOARD STREQUAL "pico2")
    pico_set_linker_script(picocalc_sd_boot ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/memmap_2350.ld)
else()
    pico_set_linker_script(picocalc_sd_boot ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/memmap_2040.ld)
endif()

# Link SDK and custom libs
target_link_libraries(picocalc_sd_boot
    pico_stdlib
    hardware_flash
    pico_unique_id
    i2ckbd
    lcdspi
    pico-vfs
)

# Include paths
target_include_directories(picocalc_sd_boot PRIVATE
    bootloader/include
    pico-vfs/include
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/include
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/src
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/libs
    ${PICO_SDK_PATH}/src/common/boot_uf2_headers/include
    ${PICO_SDK_PATH}/src/rp2040/boot_stage2/include
    ${PICO_SDK_PATH}/src/common/boot_uf2/include
    ${PICO_SDK_PATH}/src/common/pico_stdlib/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_gpio/include
    ${PICO_SDK_PATH}/src/common/pico_base/include
    ${PICO_SDK_PATH}/src/rp2_common/pico_platform/include
    ${PICO_SDK_PATH}/src/rp2040/hardware_regs/include
    ${PICO_SDK_PATH}/src/rp2040/hardware_structs/include
    ${PICO_SDK_PATH}/src/rp2040/boot_stage2/asminclude
)

# Enable testing and add tests
enable_testing()
# TODO: Fix test build configuration for cross-compilation
# add_subdirectory(tests)

pico_add_extra_outputs(picocalc_sd_boot) 