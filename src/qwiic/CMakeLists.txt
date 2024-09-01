add_library(qwiic INTERFACE)

target_include_directories(qwiic INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/qwiic
)

target_sources(qwiic INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/qwiic.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ht16k33_i2c.cpp
)


