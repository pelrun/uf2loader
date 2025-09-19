project(diag C CXX ASM)

add_executable(diag diag.c)

pico_set_program_name(diag "bl-diagnostic")
pico_set_program_description(diag "Diagnostics for uf2loader")
pico_set_program_version(diag "2.3")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(diag 0)
pico_enable_stdio_usb(diag 0)
pico_enable_stdio_rtt(diag 0)

pico_minimize_runtime(diag)

if (PICO_BOARD MATCHES "^pico(_w)?$")
  set_target_properties(diag PROPERTIES OUTPUT_NAME "diag_pico")
else()
  set_target_properties(diag PROPERTIES OUTPUT_NAME "diag_pico2")
endif()

pico_add_extra_outputs(diag)

target_compile_definitions(diag PRIVATE
    #ENABLE_DEBUG=1 # requires at least 20k of flash
)

# Add the standard library to the build
target_link_libraries(diag
    pico_stdlib
    hardware_gpio
    petit_fatfs
    i2ckbd
    lcdspi
)

# Add the standard include files to the build
target_include_directories(diag PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_options(diag PRIVATE -Wl,--print-memory-usage)
