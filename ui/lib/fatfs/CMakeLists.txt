pico_add_library(fatfs)

target_sources(fatfs INTERFACE
        ff.c
        ffsystem.c
        ffunicode.c
        diskio.c
)

target_include_directories(fatfs SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

pico_mirrored_target_link_libraries(fatfs INTERFACE sdmmc)
