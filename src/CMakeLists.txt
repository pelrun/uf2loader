cmake_minimum_required(VERSION 3.13...3.27)

include(pico_sdk_import.cmake)

project(picocalc_sd_boot C CXX ASM)
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR} CACHE PATH "Install prefix set to project root" FORCE)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE "MinSizeRel")
set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "-specs=nano.specs")

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
)

function(build_for_board board_name)

  if(board_name STREQUAL "pico2_w")
    set(PICO_BOARD pico2_w)
  else()
    set(PICO_BOARD pico)
    set(PICO_DEFAULT_BOOT_STAGE2_FILE "${CMAKE_SOURCE_DIR}/boot2/boot2_custom.S")
  endif()

  pico_sdk_init()

  add_subdirectory(i2ckbd)
  add_subdirectory(lcdspi)
  add_subdirectory(lib/pico-vfs)

  add_executable(picocalc_sd_boot_${board_name}
    main.c
    uf2.c
    proginfo.c
    key_event.c
    text_directory_ui.c
  )

  target_link_libraries(picocalc_sd_boot_${board_name}
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

  pico_enable_stdio_usb(picocalc_sd_boot_${board_name} 0)
  pico_enable_stdio_uart(picocalc_sd_boot_${board_name} 1)

  pico_add_extra_outputs(picocalc_sd_boot_${board_name})

  target_link_options(picocalc_sd_boot_${board_name} PRIVATE -Wl,--print-memory-usage)

  if (PICO_BOARD STREQUAL "pico")
    pico_set_linker_script(picocalc_sd_boot_${board_name} ${CMAKE_CURRENT_SOURCE_DIR}/memmap_2040.ld)
  endif()

  # Define the output directory relative to the project root directory
  set(output_dir prebuild_output/${board_name})

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/picocalc_sd_boot_${board_name}.elf
    DESTINATION ${output_dir}
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/picocalc_sd_boot_${board_name}.uf2
    DESTINATION ${output_dir}
  )
endfunction()


# Build for Pico
build_for_board(pico)

# Build for Pico 2
#build_for_board(pico2_w)
