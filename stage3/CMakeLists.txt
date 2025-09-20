project(stage3 C CXX ASM)

add_executable(stage3 stage3.c uf2.c)

pico_set_program_name(stage3 "stage3")
pico_set_program_description(stage3 "BL for UF2 Loader")
pico_set_program_version(stage3 "2.3")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(stage3 0)
pico_enable_stdio_usb(stage3 0)
pico_enable_stdio_rtt(stage3 0)

pico_minimize_runtime(stage3)

set_target_properties(stage3 PROPERTIES OUTPUT_NAME "bootloader_${PICO_BOARD}")

math(EXPR part_size_kb "${PICO_FLASH_SIZE_BYTES} / 1024 - 8")

# Partition has to avoid the top 4k of a 16MB flash due to RP2350-E10 workaround
if(part_size_kb GREATER 16372)
  set(part_size_kb 16372)
endif()

if (PICO_PLATFORM MATCHES "^rp2040")

  pico_define_boot_stage2(boot2_custom "${CMAKE_CURRENT_SOURCE_DIR}/boot2_custom.S")
  pico_set_boot_stage2(stage3 boot2_custom)
  set_target_properties(bs2_default bs2_default_library PROPERTIES EXCLUDE_FROM_ALL 1)

  pico_set_linker_script(stage3 ${CMAKE_CURRENT_SOURCE_DIR}/memmap_2040.ld)
  target_compile_definitions(boot2_custom PRIVATE BOOTLOADER_START=0x101fc000)

elseif(PICO_PLATFORM MATCHES "^rp2350")

  pico_set_linker_script(stage3 ${CMAKE_CURRENT_SOURCE_DIR}/memmap_2350.ld)

  configure_file(partitions.json.in partitions.json @ONLY)
  pico_embed_pt_in_binary(stage3 ${CMAKE_CURRENT_BINARY_DIR}/partitions.json)

  # create absolute uf2, and package in flash
  pico_set_uf2_family(stage3 "absolute")
  pico_package_uf2_output(stage3 0x10000000)

else()
  message(FATAL_ERROR "PICO_PLATFORM not supported: " ${PICO_PLATFORM})
endif()

message("PICO_BOARD: " ${PICO_BOARD})
message("PICO_PLATFORM: " ${PICO_PLATFORM})
message("PICO_FLASH_SIZE_BYTES: " ${PICO_FLASH_SIZE_BYTES})
message("part_size_kb: " ${part_size_kb})

pico_add_extra_outputs(stage3)

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

target_link_options(stage3 PRIVATE -Wl,--print-memory-usage)
