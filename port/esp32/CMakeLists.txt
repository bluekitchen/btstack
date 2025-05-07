cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/idf.cmake)

if( NOT DEFINED $ENV{BTSTACK_ROOT} )
    set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)
else()
    set(BTSTACK_ROOT $ENV{BTSTACK_ROOT})
endif()

project(Examples)

idf_build_component( components/btstack )
idf_build_component( main )

idf_build_process(esp32
    SDKCONFIG_DEFAULTS ${CMAKE_SOURCE_DIR}/sdkconfig.defaults
    BUILD_DIR ${CMAKE_BINARY_DIR})

include(${BTSTACK_ROOT}/example/CMakeLists.txt)

set (EXAMPLES ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY} ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE})
list(REMOVE_DUPLICATES EXAMPLES)
list(REMOVE_ITEM EXAMPLES "mesh_node_demo")
list(REMOVE_ITEM EXAMPLES "led_counter")
set(HFP_EXAMPLES hfp_ag_demo hfp_hf_demo hsp_ag_demo hsp_hs_demo )
set(LE_AUDIO_SINK_EXAMPLES le_audio_unicast_headset le_audio_unicast_gateway le_audio_broadcast_sink)
set(LE_AUDIO_SOURCE_EXAMPLES le_audio_unicast_gateway le_audio_broadcast_source)
foreach(EXAMPLE ${EXAMPLES})
	# get c file
	set (SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE}.c)

	# add GATT DB creation
	if ( "${EXAMPLES_GATT_FILES}" MATCHES ${EXAMPLE} )
		message("example ${EXAMPLE} -- with GATT DB")
	    add_custom_command(
		    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
			DEPENDS ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt
			COMMAND ${Python_EXECUTABLE}
			ARGS ${BTSTACK_ROOT}/tool/compile_gatt.py ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
		)
		list(APPEND SOURCES_EXAMPLE ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)
	else()
		message("example ${EXAMPLE}")
	endif()
    if( ${EXAMPLE} IN_LIST HFP_EXAMPLES )
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/sco_demo_util.c )
    endif()
    if( ${EXAMPLE} IN_LIST LE_AUDIO_SINK_EXAMPLES )
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/le_audio_demo_util_sink.c )
    endif()
    if( ${EXAMPLE} IN_LIST LE_AUDIO_SOURCE_EXAMPLES )
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/le_audio_demo_util_source.c )
    endif()

    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})
    target_include_directories( ${EXAMPLE} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
	target_link_libraries(${EXAMPLE} idf::main)
    idf_build_executable(${EXAMPLE})
endforeach(EXAMPLE)

