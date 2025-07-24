add_library(pr_binary_info_reader bi_reader.c)
target_link_libraries(pr_binary_info_reader PRIVATE
        pico_stdlib
        pico_binary_info
        )
target_include_directories (pr_binary_info_reader PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)
