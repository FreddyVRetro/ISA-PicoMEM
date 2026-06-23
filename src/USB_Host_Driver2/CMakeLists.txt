# !! Remove link of tinyusb in xinput CMakefile !!
add_library(USB_Host_Driver INTERFACE)
target_include_directories(USB_Host_Driver INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(USB_Host_Driver INTERFACE tinyusb_board)
target_sources(USB_Host_Driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/usbh.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/hid.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/hid_registry.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/hid_utils.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/generic/hid_gamepad.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/generic/hid_keyboard.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/generic/hid_mouse.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/generic/hid_parser.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/generic/sinput_host.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/sony/sony_ds3.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/sony/sony_ds4.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/sony/sony_ds5.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/sony/sony_psc.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/8bitdo/8bitdo_bta.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/8bitdo/8bitdo_m30.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/8bitdo/8bitdo_pce.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/nintendo/gamecube_adapter.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/nintendo/switch_pro.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/nintendo/switch2_pro.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/hori/hori_horipad.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/hori/hori_pokken.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/logitech/logitech_wingman.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/sega/sega_astrocity.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/google/google_stadia.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/raphnet/raphnet_pce.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/microsoft/ms_sidewinder_dualstrike.c
    ${CMAKE_CURRENT_LIST_DIR}/hid/devices/vendors/microsoft/ms_sidewinder_commander.c
    ${CMAKE_CURRENT_LIST_DIR}/xinput/oldxinput.c
    ${CMAKE_CURRENT_LIST_DIR}/../USB_Host_Driver/msc/fatfs_usb.c
    ${CMAKE_CURRENT_LIST_DIR}/../USB_Host_Driver/usb.c    
    )

add_library(tinyusb_config INTERFACE)
target_include_directories(tinyusb_config INTERFACE ${CMAKE_SOURCE_DIR}../lib/tinyusb/src)
include(${CMAKE_SOURCE_DIR}/lib/tinyusb/src/CMakeLists.txt)
target_compile_definitions(tinyusb_config INTERFACE
    CFG_TUSB_CONFIG_FILE="../../../USB_Host_Driver/tusb_config.h"
)
