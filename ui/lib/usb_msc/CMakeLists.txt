pico_add_library(usb_msc)

target_sources(usb_msc INTERFACE
  usb_msc.c
  usb_descriptors.c
)

target_include_directories(sdmmc SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

pico_mirrored_target_link_libraries(sdmmc INTERFACE
  tinyusb_device
  tinyusb_board
)
