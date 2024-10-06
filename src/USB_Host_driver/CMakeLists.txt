add_library(USB_Host_driver INTERFACE)
target_include_directories(USB_Host_driver INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(USB_Host_driver INTERFACE tinyusb_board)
target_sources(USB_Host_driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/hid_app.c
    ${CMAKE_CURRENT_LIST_DIR}/usb.c  
    ${CMAKE_CURRENT_LIST_DIR}/hid_devices/keyboard.c
    ${CMAKE_CURRENT_LIST_DIR}/hid_devices/joystick.c
    )    