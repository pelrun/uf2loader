project(ui C CXX ASM)

add_subdirectory(lib/sdmmc)
add_subdirectory(lib/fatfs)
add_subdirectory(lib/usb_msc)
add_subdirectory(lib/binary_info_reader)

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
pico_set_program_version(ui "2.3")

target_include_directories(ui PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(ui
  pico_stdlib
  pico_unique_id
  hardware_sync
  hardware_flash
  hardware_irq
  hardware_i2c
  hardware_spi
  hardware_dma
  i2ckbd
  lcdspi
  fatfs
  boot_uf2_headers
  boot_picobin_headers
  bootloader
  pr_binary_info_reader
  usb_msc
)

pico_enable_stdio_usb(ui 0)
pico_enable_stdio_uart(ui 0)

pico_set_binary_type(ui no_flash)

pico_add_extra_outputs(ui)

target_link_options(ui PRIVATE -Wl,--print-memory-usage)

target_compile_options(ui PRIVATE -DENABLE_RAM_APPS=1 -DENABLE_USB=1)
