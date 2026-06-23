cmake_minimum_required(VERSION 3.16)

#PMBIOS object Library
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pmbios.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${CMAKE_LINKER} -r -b binary -o ${CMAKE_CURRENT_BINARY_DIR}/pmbios.o pmbios.bin
    #places in data
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/pmbios.bin
)

add_custom_target(pmbiosobj ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/pmbios.o)
add_library(pmbios OBJECT IMPORTED GLOBAL)
add_dependencies(pmbios pmbiosobj)
set_target_properties(pmbios PROPERTIES IMPORTED_OBJECTS "${CMAKE_CURRENT_BINARY_DIR}/pmbios.o")

#PM2BIOS object Library
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pm2bios.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${CMAKE_LINKER} -r -b binary -o ${CMAKE_CURRENT_BINARY_DIR}/pm2bios.o pm2bios.bin
    #places in data
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/pm2bios.bin
)

add_custom_target(pm2biosobj ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/pm2bios.o)
add_library(pm2bios OBJECT IMPORTED GLOBAL)
add_dependencies(pm2bios pm2biosobj)
set_target_properties(pm2bios PROPERTIES IMPORTED_OBJECTS "${CMAKE_CURRENT_BINARY_DIR}/pm2bios.o")