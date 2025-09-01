cmake_minimum_required(VERSION 3.16)

if( NOT DEFINED ENV{BTSTACK_ROOT} )
    message(FATAL_ERROR "Please set BTSTACK_ROOT environment variable")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include($ENV{BTSTACK_ROOT}/port/esp32/components/btstack/cmake/BTstack_project.cmake)

project(Examples)

