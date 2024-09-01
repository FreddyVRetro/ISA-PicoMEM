# PicoMEM various Devices emulation code
add_library(isa_devices INTERFACE)

target_include_directories(isa_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/isa_devices
)

target_sources(isa_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/dev_memory.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_picomem_io.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_post.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_adlib.cpp    
    ${CMAKE_CURRENT_LIST_DIR}/dev_joystick.cpp
)