cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include("${CMAKE_CURRENT_SOURCE_DIR}/components/btstack/cmake/BTstack_project.cmake")

project(esp32-example)

