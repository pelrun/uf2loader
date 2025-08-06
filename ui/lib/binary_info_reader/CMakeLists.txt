add_library(pr_binary_info_reader INTERFACE)

target_sources(pr_binary_info_reader INTERFACE
        bi_reader.c
)

target_link_libraries(pr_binary_info_reader INTERFACE
        pico_stdlib
        pico_binary_info
)

target_include_directories (pr_binary_info_reader INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}
)
