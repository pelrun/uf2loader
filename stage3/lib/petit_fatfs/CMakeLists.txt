pico_add_library(petit_fatfs)

target_sources(petit_fatfs INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/pff.c
        ${CMAKE_CURRENT_LIST_DIR}/diskio.c
)

target_include_directories(petit_fatfs SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

pico_mirrored_target_link_libraries(petit_fatfs INTERFACE hardware_spi)
