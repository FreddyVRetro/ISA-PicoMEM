add_library(qwiic INTERFACE)

target_include_directories(qwiic INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/qwiic
)

target_sources(qwiic INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/pm_i2c.c
    ${CMAKE_CURRENT_LIST_DIR}/i2c_ht16k33.c     #LED Display
    ${CMAKE_CURRENT_LIST_DIR}/i2c_pfc85263.cpp  #RTC
    )

if(USE_FRONTPANEL)
target_sources(qwiic INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/dev_oled.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pm_display.c      #OLED Display code
    ${CMAKE_CURRENT_LIST_DIR}/pm_menu.c         #Menu system for OLED display
    )
endif()    


