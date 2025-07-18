cmake_minimum_required(VERSION 3.13...3.27)

# Add option to select board
option(BUILD_PICO2 "Build for Pico2 W (RP2350) instead of Pico (RP2040)" OFF)

if(BUILD_PICO2)
  set(PICO_BOARD pico2_w)
else()
  set(PICO_BOARD pico)
endif()

include(pico_sdk_import.cmake)

if(BUILD_PICO2)
  # Add M33 specific flags
  add_compile_definitions(__ARM_FEATURE_CMSE=3)
  add_compile_options(-mcpu=cortex-m33)
else()
  set(PICO_DEFAULT_BOOT_STAGE2_FILE "${CMAKE_SOURCE_DIR}/boot2/boot2_custom.S")
endif()

project(picocalc_sd_boot C CXX ASM)
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR} CACHE PATH "Install prefix set to project root" FORCE)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE "MinSizeRel")
set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "-specs=nano.specs")

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_sdk_init()

add_subdirectory(i2ckbd)
add_subdirectory(lcdspi)
add_subdirectory(lib/pico-vfs)

if(BUILD_PICO2)
  set(TARGET_NAME picocalc_sd_boot_pico2_w)
else()
  set(TARGET_NAME picocalc_sd_boot_pico)
endif()

add_executable(${TARGET_NAME}
  main.c
  uf2.c
  proginfo.c
  key_event.c
  text_directory_ui.c
)

target_link_libraries(${TARGET_NAME}
  pico_stdlib
  hardware_sync
  hardware_flash
  hardware_irq
  hardware_i2c
  hardware_spi
  hardware_dma
  i2ckbd
  lcdspi
  blockdevice_sd
  filesystem_fat
  filesystem_vfs
  boot_uf2_headers
)

pico_enable_stdio_usb(${TARGET_NAME} 0)
pico_enable_stdio_uart(${TARGET_NAME} 1)

pico_add_extra_outputs(${TARGET_NAME})

target_link_options(${TARGET_NAME} PRIVATE -Wl,--print-memory-usage)

if (PICO_BOARD STREQUAL "pico")
  pico_set_linker_script(${TARGET_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/memmap_2040.ld)
elseif (PICO_BOARD STREQUAL "pico2_w")
  pico_set_linker_script(${TARGET_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/memmap_2350.ld)
endif()

# Define the output directory relative to the project root directory
if(BUILD_PICO2)
  set(output_dir prebuild_output/pico2_w)
else()
  set(output_dir prebuild_output/pico)
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.elf
  DESTINATION ${output_dir}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.uf2
  DESTINATION ${output_dir}
)

# Add custom target for running tests
add_custom_target(test
    COMMAND ${CMAKE_COMMAND} -E echo "Running UF2 implementation tests..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} ./test_integration.sh
    COMMENT "Running test suite"
    VERBATIM
)

# Add custom target for just unit tests
add_custom_target(unit-test
    COMMAND ${CMAKE_COMMAND} -E echo "Running unit tests..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} make -f Makefile.test clean all
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} ./test_uf2
    COMMENT "Running unit tests only"
    VERBATIM
)
