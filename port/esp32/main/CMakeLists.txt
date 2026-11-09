if(DEFINED BTSTACK_ESP32_EXAMPLE AND NOT "${BTSTACK_ESP32_EXAMPLE}" STREQUAL "")
    set(EXAMPLE "${BTSTACK_ESP32_EXAMPLE}")
elseif(DEFINED ENV{EXAMPLE} AND NOT "$ENV{EXAMPLE}" STREQUAL "")
    set(EXAMPLE "$ENV{EXAMPLE}")
else()
    set(EXAMPLE gatt_counter)
endif()

include("${BTSTACK_ROOT}/tool/cmake/BTstack.cmake")
include("${BTSTACK_ROOT}/example/requirements.cmake")

list(FIND btstack_declared_examples "${EXAMPLE}" EXAMPLE_INDEX)
if(EXAMPLE_INDEX EQUAL -1 AND NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/${EXAMPLE}.c")
    message(FATAL_ERROR "Unknown ESP32 example '${EXAMPLE}'")
elseif(NOT EXAMPLE_INDEX EQUAL -1)
    set(BUILD_MESSAGE  "Building example: \"${EXAMPLE}\"")
    foreach(source IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND EXAMPLE_SOURCES "${BTSTACK_ROOT}/example/${source}")
    endforeach()
    set(GATT_DB "${btstack_example_${EXAMPLE}_gatt_db}")
    if(GATT_DB)
        set(GATT_PATH "${BTSTACK_ROOT}/example/${GATT_DB}")
    endif()
else()
    set(BUILD_MESSAGE  "Building custom application \"${EXAMPLE}\"")
    set(EXAMPLE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/${EXAMPLE}.c")
endif()

idf_component_register(
    SRCS "main.c" ${EXAMPLE_SOURCES}
    REQUIRES btstack esp_phy bt esp_driver_gpio esp_driver_i2s)

include(BTstackUtils)

# add GATT DB creation
message("\n===")
if ( DEFINED GATT_PATH )
    message("${BUILD_MESSAGE} with GATT DB")
    target_gatt(${COMPONENT_LIB} ${GATT_PATH})
else()
    message(${BUILD_MESSAGE})
endif()
message("===\n")
