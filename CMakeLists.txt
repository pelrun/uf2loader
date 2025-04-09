cmake_minimum_required(VERSION 3.13...3.27)

include(pico_sdk_import.cmake)

# Setup board for Pico 1
set(PICO_BOARD pico)

# Setup board for Pico 2W
# set(PICO_BOARD pico2)

project(picocalc_sd_boot C CXX ASM)
set(FAMILY rp2040)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
)

add_subdirectory(i2ckbd)
add_subdirectory(lcdspi)
add_subdirectory(pico-vfs)

add_executable(picocalc_sd_boot
  main.c
  key_event.c
)

target_link_libraries(picocalc_sd_boot
  pico_stdlib
  hardware_sync
  hardware_flash
  hardware_irq
  hardware_adc
  hardware_pwm
  hardware_i2c
  hardware_spi
  hardware_dma
  hardware_exception
  hardware_pio
  pico_multicore
  i2ckbd
  lcdspi
  blockdevice_sd
  filesystem_fat
  filesystem_vfs
)

pico_enable_stdio_usb(picocalc_sd_boot 0)
pico_enable_stdio_uart(picocalc_sd_boot 1)

pico_add_extra_outputs(picocalc_sd_boot)

target_link_options(picocalc_sd_boot PRIVATE -Wl,--print-memory-usage)

# This function sets the custom linker script for SD card applications
# The memmap_sdcard_app.ld script configures the application to start at
# FLASH origin = 0x10000000 + 256k, which aligns with the SD_BOOT_FLASH_OFFSET
# defined in main.c (256 * 1024). This ensures the bootloader (first 256KB)
# is preserved when flashing the application.
function(enable_sdcard_app target)
  pico_set_linker_script(${target} ${CMAKE_SOURCE_DIR}/memmap_sdcard_app.ld)
endfunction()
