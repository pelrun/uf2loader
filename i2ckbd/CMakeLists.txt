# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)


project(i2ckbd
        VERSION 0.0.1
        DESCRIPTION "i2ckbd for picocalc."
        )

add_library(i2ckbd INTERFACE)

target_sources(i2ckbd INTERFACE
        i2ckbd.c
        )

target_link_libraries(i2ckbd INTERFACE  pico_stdlib hardware_spi hardware_i2c)

target_include_directories(i2ckbd INTERFACE ${CMAKE_CURRENT_LIST_DIR})