pico_add_library(sdmmc)

target_sources(sdmmc INTERFACE
        sdmmc.c
)

target_include_directories(sdmmc SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

pico_mirrored_target_link_libraries(sdmmc INTERFACE hardware_spi)
