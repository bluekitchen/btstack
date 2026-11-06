cmake_minimum_required(VERSION 3.16)

if( NOT DEFINED ENV{BTSTACK_ROOT} )
    message(FATAL_ERROR "Please set BTSTACK_ROOT environment variable")
endif()

# The skeleton always builds its local main/your_project.c application. This
# avoids an ambient EXAMPLE environment variable selecting a different BTstack
# source closure in the shared ESP32 component.
set(BTSTACK_ESP32_EXAMPLE your_project)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include($ENV{BTSTACK_ROOT}/port/esp32/components/btstack/cmake/BTstack_project.cmake)

project(Examples)

