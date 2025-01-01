add_library(USB_Host_driver INTERFACE)
target_include_directories(USB_Host_driver INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(USB_Host_driver INTERFACE tinyusb_board)
target_sources(USB_Host_driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/hid_app.c
    ${CMAKE_CURRENT_LIST_DIR}/usb.c  
    ${CMAKE_CURRENT_LIST_DIR}/hid_devices/keyboard.c
    ${CMAKE_CURRENT_LIST_DIR}/hid_devices/joystick.c
    ${CMAKE_CURRENT_LIST_DIR}/msc_devices/fatfs_usb.c
    )

add_library(tinyusb_config INTERFACE)
target_include_directories(tinyusb_config INTERFACE ${CMAKE_SOURCE_DIR}../lib/tinyusb/src)
include(${CMAKE_SOURCE_DIR}/lib/tinyusb/src/CMakeLists.txt)
target_compile_definitions(tinyusb_config INTERFACE
    CFG_TUSB_CONFIG_FILE="../../../USB_Host_driver/tusb_config.h"
)
