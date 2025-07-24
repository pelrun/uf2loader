
project(bootloader
        VERSION 0.0.1
        DESCRIPTION "bootloader info for picocalc."
        )

pico_add_library(bootloader)

target_sources(bootloader INTERFACE
        proginfo.c
)

target_include_directories(bootloader INTERFACE ${CMAKE_CURRENT_LIST_DIR})

pico_mirrored_target_link_libraries(bootloader INTERFACE hardware_flash)
