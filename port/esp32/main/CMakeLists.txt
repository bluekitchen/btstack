if( NOT DEFINED ENV{EXAMPLE} )
    set(EXAMPLE gatt_counter)
else()
    set(EXAMPLE $ENV{EXAMPLE})
endif()

if( NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/${EXAMPLE}.c" )
    message( "builtin sample" )
    list(TRANSFORM EXAMPLE PREPEND ${BTSTACK_ROOT}/example/ OUTPUT_VARIABLE EXAMPLE_SOURCE)
else()
    message( "local app" )
    list(TRANSFORM EXAMPLE PREPEND ${CMAKE_CURRENT_LIST_DIR}/ OUTPUT_VARIABLE EXAMPLE_SOURCE)
endif()

set( GATT_FILE_NAME "${EXAMPLE_SOURCE}.gatt" )
string( CONCAT EXAMPLE_SOURCE ${EXAMPLE_SOURCE} ".c" )
if( NOT EXISTS "${GATT_FILE_NAME}" )
    unset( GATT_FILE_NAME )
endif()


idf_component_register(
    SRCS "main.c" "${EXAMPLE_SOURCE}"
    REQUIRES btstack esp_phy bt)

include(BTstackUtils)

set(HFP_EXAMPLES hfp_ag_demo hfp_hf_demo hsp_ag_demo hsp_hs_demo )
set(LE_AUDIO_SINK_EXAMPLES le_audio_unicast_headset le_audio_unicast_gateway le_audio_broadcast_sink)
set(LE_AUDIO_SOURCE_EXAMPLES le_audio_unicast_gateway le_audio_broadcast_source)

# add GATT DB creation
if ( DEFINED GATT_FILE_NAME )
    message("${EXAMPLE} -- with GATT DB")
    target_gatt(${COMPONENT_LIB} ${GATT_FILE_NAME})
endif()
if( ${EXAMPLE} IN_LIST HFP_EXAMPLES )
    list(APPEND SRCS ${BTSTACK_ROOT}/example/sco_demo_util.c )
endif()
if( ${EXAMPLE} IN_LIST LE_AUDIO_SINK_EXAMPLES )
    list(APPEND SRCS ${BTSTACK_ROOT}/example/le_audio_demo_util_sink.c )
endif()
if( ${EXAMPLE} IN_LIST LE_AUDIO_SOURCE_EXAMPLES )
    list(APPEND SRCS ${BTSTACK_ROOT}/example/le_audio_demo_util_source.c )
endif()

if( DEFINED SRCS )
    target_sources(${COMPONENT_LIB} PRIVATE ${SRCS})
endif()
