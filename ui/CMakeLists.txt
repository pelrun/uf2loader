project(ui C CXX ASM)

add_subdirectory(lib/pico-vfs)
add_subdirectory(lib/binary_info_reader)

# Use our ffconf.h and not the one in pico-vfs
# Saves 60k of code!
target_include_directories(filesystem_fat BEFORE INTERFACE ${CMAKE_CURRENT_LIST_DIR}/fatfs)

add_executable(ui
  main.c
  uf2.c
  key_event.c
  text_directory_ui.c
)

if (PICO_BOARD MATCHES "^pico(_w)?$")
  set_target_properties(ui PROPERTIES OUTPUT_NAME "BOOT2040")
else ()
  set_target_properties(ui PROPERTIES OUTPUT_NAME "BOOT2350")
endif()

target_compile_options(ui PRIVATE -mno-unaligned-access)

pico_set_program_name(ui "uf2load")
pico_set_program_description(ui "UI for UF2 Loader")
pico_set_program_version(ui "2.1")

target_link_libraries(ui
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

pico_enable_stdio_usb(ui 0)
pico_enable_stdio_uart(ui 0)

pico_set_binary_type(ui no_flash)

pico_add_extra_outputs(ui)

target_link_options(ui PRIVATE -Wl,--print-memory-usage)
