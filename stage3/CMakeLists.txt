project(stage3 C CXX ASM)

add_subdirectory(lib/petit_fatfs)

add_executable(stage3 stage3.c uf2.c)

pico_set_program_name(stage3 "stage3")
pico_set_program_version(stage3 "2.0")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(stage3 0)
pico_enable_stdio_usb(stage3 0)
pico_enable_stdio_rtt(stage3 0)

pico_set_float_implementation(stage3 none)

if (PICO_BOARD MATCHES "^pico(_w)?$")
  pico_define_boot_stage2(boot2_custom "${CMAKE_CURRENT_SOURCE_DIR}/boot2_custom.S")
  pico_set_boot_stage2(stage3 boot2_custom)
  set_target_properties(bs2_default bs2_default_library PROPERTIES EXCLUDE_FROM_ALL 1)

  pico_set_linker_script(stage3 ${CMAKE_CURRENT_SOURCE_DIR}/memmap_2040.ld)
  target_compile_definitions(boot2_custom PRIVATE BOOTLOADER_START=0x101fb000)
endif()

target_compile_definitions(stage3 PRIVATE
    #ENABLE_DEBUG=1 # requires at least 20k of flash
)

# Add the standard library to the build
target_link_libraries(stage3
    pico_stdlib
    boot_uf2_headers
    boot_picobin_headers
    hardware_gpio
    petit_fatfs
    i2ckbd
    bootloader
)

# Add the standard include files to the build
target_include_directories(stage3 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(stage3)

target_link_options(stage3 PRIVATE -Wl,--print-memory-usage)
