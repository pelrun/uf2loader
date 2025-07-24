project(ui C CXX ASM)

add_subdirectory(lib/lcdspi)
add_subdirectory(lib/pico-vfs)
add_subdirectory(lib/binary_info_reader)

# Use our ffconf.h and not the one in pico-vfs
# Saves 60k of code!
target_include_directories(filesystem_fat BEFORE INTERFACE ${CMAKE_CURRENT_LIST_DIR}/fatfs)

add_executable(BOOT2040
  main.c
  uf2.c
  key_event.c
  text_directory_ui.c
)

target_compile_options(BOOT2040 PRIVATE -mno-unaligned-access)

pico_set_program_name(BOOT2040 "uf2load")
pico_set_program_version(BOOT2040 "2.0")

target_compile_definitions(BOOT2040 PRIVATE VERSION=${PROJECT_VERSION})

target_link_libraries(BOOT2040
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
  boot_picobin_headers
  bootloader
  pr_binary_info_reader
)

pico_enable_stdio_usb(BOOT2040 0)
pico_enable_stdio_uart(BOOT2040 0)

pico_set_binary_type(BOOT2040 no_flash)

pico_add_extra_outputs(BOOT2040)

target_link_options(BOOT2040 PRIVATE -Wl,--print-memory-usage)
