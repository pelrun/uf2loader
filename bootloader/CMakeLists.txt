add_library(pico_boot
    src/main.c
    src/uf2.c
    src/proginfo.c
    src/bldetect.c
    src/key_event.c
    src/text_directory_ui.c
)

# Add shared libraries
add_subdirectory(libs/i2ckbd)
add_subdirectory(libs/lcdspi)

# Link against the Pico SDK libraries
target_link_libraries(pico_boot INTERFACE pico_base pico_stdlib hardware_flash i2ckbd lcdspi)

# Add the INTERFACE library for common compiler options
target_link_libraries(pico_boot INTERFACE pico_boot_opts)

# Add the linker scripts
target_sources(pico_boot PRIVATE
    memmap_2040.ld
    memmap_2350.ld
) 