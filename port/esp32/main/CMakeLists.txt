# Set default example to build, update to build your own application
# e.g. if there is btstack/port/esp32/main/my_app.c, you can set DEFAULT_EXAMPLE to "my_app"
set (DEFAULT_EXAMPLE gatt_counter)

if( NOT DEFINED ENV{EXAMPLE} )
    set(EXAMPLE ${DEFAULT_EXAMPLE})
else()
    set(EXAMPLE $ENV{EXAMPLE})
endif()

if( NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/${EXAMPLE}.c" )
    set(BUILD_MESSAGE  "Building example: \"${EXAMPLE}\"")
    list(TRANSFORM EXAMPLE PREPEND ${BTSTACK_ROOT}/example/ OUTPUT_VARIABLE EXAMPLE_SOURCE)
else()
    set(BUILD_MESSAGE  "Building custom application")
    list(TRANSFORM EXAMPLE PREPEND ${CMAKE_CURRENT_LIST_DIR}/ OUTPUT_VARIABLE EXAMPLE_SOURCE)
endif()

# detect optional .gatt file
set( GATT_PATH "${EXAMPLE_SOURCE}.gatt" )
string( CONCAT EXAMPLE_SOURCE ${EXAMPLE_SOURCE} ".c" )
if( NOT EXISTS "${GATT_PATH}" )
    unset( GATT_PATH )
endif()

idf_component_register(
    SRCS "main.c" "${EXAMPLE_SOURCE}"
    REQUIRES btstack esp_phy bt)

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

# add example utils as needed
set(SCO_EXAMPLES hfp_ag_demo hfp_hf_demo hsp_ag_demo hsp_hs_demo )
if( ${EXAMPLE} IN_LIST SCO_EXAMPLES )
    list(APPEND SRCS ${BTSTACK_ROOT}/example/sco_demo_util.c )
endif()

set(LE_AUDIO_SINK_EXAMPLES le_audio_unicast_headset le_audio_unicast_gateway le_audio_broadcast_sink)
if( ${EXAMPLE} IN_LIST LE_AUDIO_SINK_EXAMPLES )
    list(APPEND SRCS ${BTSTACK_ROOT}/example/le_audio_demo_util_sink.c )
endif()

set(LE_AUDIO_SOURCE_EXAMPLES le_audio_unicast_gateway le_audio_broadcast_source)
if( ${EXAMPLE} IN_LIST LE_AUDIO_SOURCE_EXAMPLES )
    list(APPEND SRCS ${BTSTACK_ROOT}/example/le_audio_demo_util_source.c )
endif()

# define sources for main component
target_sources(${COMPONENT_LIB} PRIVATE ${SRCS})
