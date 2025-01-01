cmake_minimum_required(VERSION 3.13)

add_library(usb_midi_host INTERFACE)
target_sources(usb_midi_host INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/usb_midi_host.c
)
target_include_directories(usb_midi_host INTERFACE
 ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(usb_midi_host INTERFACE pico_stdlib tinyusb_host)

add_library(usb_midi_host_app_driver INTERFACE)
target_sources(usb_midi_host_app_driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/usb_midi_host_app_driver.c
)
target_include_directories(usb_midi_host_app_driver INTERFACE
 ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(usb_midi_host_app_driver INTERFACE usb_midi_host)

