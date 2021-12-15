cmake_minimum_required (VERSION 3.5)
project(gatt_server)

# fink pkgconfig
find_package(PkgConfig REQUIRED)

# add CppUTest
include_directories("/usr/local/include")
link_directories("/usr/local/lib")
link_libraries( CppUTest )
link_libraries( CppUTestExt )

include_directories(.)
include_directories(../mock/)
include_directories(../../src)
include_directories(../../3rd-party/rijndael/)
include_directories(../../3rd-party/micro-ecc/)
include_directories(../../platform/posix/)
include_directories( ${CMAKE_CURRENT_BINARY_DIR})

set(SOURCES
    ../mock/mock_btstack_tlv.c
    ../../3rd-party/rijndael/rijndael.c
    ../../3rd-party/micro-ecc/uECC.c
    ../../src/ad_parser.c
    ../../src/ble/att_db.c
    ../../src/ble/att_db_util.c
    ../../src/ble/att_server.c
    ../../src/ble/gatt-service/battery_service_server.c 
    ../../src/ble/gatt-service/cycling_power_service_server.c 
    ../../src/ble/gatt-service/cycling_speed_and_cadence_service_server.c 
    ../../src/ble/gatt-service/device_information_service_server.c 
    ../../src/ble/gatt-service/heart_rate_service_server.c 
    ../../src/ble/gatt-service/hids_device.c 
    ../../src/ble/gatt-service/nordic_spp_service_server.c 
    ../../src/ble/gatt-service/ublox_spp_service_server.c 
    ../../src/ble/le_device_db_memory.c       
    ../../src/btstack_crypto.c            
    ../../src/btstack_linked_list.c       
    ../../src/btstack_memory.c            
    ../../src/btstack_memory_pool.c       
    ../../src/btstack_tlv.c
    ../../platform/posix/btstack_tlv_posix.c
    ../../src/btstack_util.c
    ../../src/hci_cmd.c                   
    ../../src/hci_dump.c                  
    mock.c                                
)

# Enable ASAN
add_compile_options( -g -fsanitize=address)
add_link_options(       -fsanitize=address)

# create static lib
add_library(btstack STATIC ${SOURCES})

# create targets
foreach(EXAMPLE_FILE gatt_server_test.cpp)
    get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)
    set (SOURCE_FILES ${EXAMPLE_FILE} mock.c ../mock/mock_btstack_tlv.c ../mock/mock_btstack_tlv.h)
    # profile.h
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/profile.h
        COMMAND ${CMAKE_SOURCE_DIR}/../../tool/compile_gatt.py
        ARGS ${CMAKE_SOURCE_DIR}/profile.gatt ${CMAKE_CURRENT_BINARY_DIR}/profile.h
    )
    list(APPEND SOURCE_FILES ${CMAKE_CURRENT_BINARY_DIR}/profile.h)
    add_executable(${EXAMPLE} ${SOURCE_FILES} )
    target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE_FILE)
