# Standalone tests for the BTstack CMake source-group resolver.
#
# Run directly with:
#   cmake -P test/cmake-resolver/resolver_test.cmake

get_filename_component(BTSTACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${BTSTACK_ROOT}/tool/cmake/BTstack.cmake")

# btstack_declare_example writes its declarations to the caller scope. A macro
# preserves this script's scope while loading the manifest.
macro(load_example_requirements)
    include("${BTSTACK_ROOT}/example/requirements.cmake")
endmacro()
load_example_requirements()

function(expect_failure mode expected_diagnostic)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DTEST_MODE=${mode}" "-P" "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(result EQUAL 0)
        message(FATAL_ERROR "Resolver ${mode} test unexpectedly succeeded")
    endif()

    set(diagnostic "${output}${error}")
    string(FIND "${diagnostic}" "${expected_diagnostic}" diagnostic_index)
    if(diagnostic_index EQUAL -1)
        message(FATAL_ERROR
            "Resolver ${mode} test failed without the expected diagnostic: ${expected_diagnostic}\n"
            "Actual output:\n${diagnostic}")
    endif()
endfunction()

if(DEFINED TEST_MODE)
    if(TEST_MODE STREQUAL "unknown")
        btstack_resolve_groups(resolved GROUPS missing-group)
    elseif(TEST_MODE STREQUAL "cycle")
        # classic-rfcomm -> l2cap -> core. Add the final edge only for this test.
        set(btstack_group_dependencies_core classic-rfcomm)
        btstack_resolve_groups(resolved GROUPS classic-rfcomm)
    else()
        message(FATAL_ERROR "Unknown resolver test mode: ${TEST_MODE}")
    endif()
else()
    macro(declare_test_audio_port)
        btstack_declare_port(test-audio)
        btstack_declare_port_provider(test-audio
            CAPABILITY audio
            GROUPS platform-posix-audio-portaudio)
    endmacro()
    declare_test_audio_port()

    btstack_collect_public_include_directories(public_include_directories
        ROOT "${BTSTACK_ROOT}")
    set(expected_public_include_directories
        "${BTSTACK_ROOT}/src"
        "${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include"
        "${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include"
        "${BTSTACK_ROOT}/3rd-party/hxcmod-player"
        "${BTSTACK_ROOT}/3rd-party/lc3-google/include"
        "${BTSTACK_ROOT}/3rd-party/yxml"
    )
    if(NOT "${public_include_directories}" STREQUAL "${expected_public_include_directories}")
        message(FATAL_ERROR
            "Unexpected public include directories: ${public_include_directories}")
    endif()

    btstack_resolve_groups(resolved GROUPS classic-rfcomm)
    if(NOT "${resolved}" STREQUAL "core;l2cap;classic-rfcomm")
        message(FATAL_ERROR "Unexpected resolver closure: ${resolved}")
    endif()

    # Repeated requested groups must not occur in the resolved closure.
    btstack_resolve_groups(resolved GROUPS classic-rfcomm l2cap classic-rfcomm)
    if(NOT "${resolved}" STREQUAL "core;l2cap;classic-rfcomm")
        message(FATAL_ERROR "Resolver did not remove duplicate groups: ${resolved}")
    endif()

    btstack_resolve_example(spp_counter spp_counter_groups spp_counter_configurations)
    if(NOT "${spp_counter_groups}" STREQUAL "classic-sdp-util;core;l2cap;classic-rfcomm;classic-sdp-server;classic-spp-server")
        message(FATAL_ERROR "Unexpected spp_counter source groups: ${spp_counter_groups}")
    endif()
    if(spp_counter_configurations)
        message(FATAL_ERROR
            "Unexpected spp_counter configuration requirements: ${spp_counter_configurations}")
    endif()

    btstack_resolve_example(sdp_general_query sdp_general_query_groups sdp_general_query_configurations)
    if(NOT "${sdp_general_query_groups}" STREQUAL "classic-sdp-util;core;l2cap;classic-sdp-client")
        message(FATAL_ERROR "Unexpected sdp_general_query source groups: ${sdp_general_query_groups}")
    endif()

    btstack_resolve_example(spp_streamer_client spp_streamer_client_groups spp_streamer_client_configurations)
    if(NOT "${spp_streamer_client_groups}" STREQUAL "core;classic-sdp-util;l2cap;classic-sdp-client;classic-rfcomm;classic-sdp-client-rfcomm")
        message(FATAL_ERROR "Unexpected spp_streamer_client source groups: ${spp_streamer_client_groups}")
    endif()

    btstack_resolve_example(gatt_counter gatt_counter_groups gatt_counter_configurations)
    if(NOT "${gatt_counter_groups}" STREQUAL "core;l2cap;att-db;att-dispatch;ble;gatt-server")
        message(FATAL_ERROR "Unexpected gatt_counter source groups: ${gatt_counter_groups}")
    endif()
    if(NOT "${gatt_counter_configurations}" STREQUAL "ENABLE_LE_PERIPHERAL")
        message(FATAL_ERROR
            "Unexpected gatt_counter configuration requirements: ${gatt_counter_configurations}")
    endif()

    btstack_resolve_groups(gatt_service_client_groups GROUPS gatt-service-client)
    if(NOT "${gatt_service_client_groups}" STREQUAL "core;l2cap;att-dispatch;ble;gatt-client;gatt-service-client")
        message(FATAL_ERROR
            "Unexpected gatt-service-client source groups: ${gatt_service_client_groups}")
    endif()

    btstack_resolve_example(gatt_battery_query gatt_battery_query_groups gatt_battery_query_configurations)
    if(NOT "${gatt_battery_query_groups}" STREQUAL "core;l2cap;att-db;att-dispatch;ble;gatt-client;gatt-server;gatt-service-battery-service-client")
        message(FATAL_ERROR
            "Unexpected gatt_battery_query source groups: ${gatt_battery_query_groups}")
    endif()
    if(gatt_battery_query_configurations)
        message(FATAL_ERROR
            "Unexpected gatt_battery_query configuration requirements: ${gatt_battery_query_configurations}")
    endif()

    btstack_resolve_example(hog_host_demo hog_host_demo_groups hog_host_demo_configurations)
    if(NOT "${hog_host_demo_groups}" STREQUAL "core;l2cap;att-db;att-dispatch;ble;gatt-client;gatt-server;gatt-service-client;gatt-service-hids-host")
        message(FATAL_ERROR
            "Unexpected hog_host_demo source groups: ${hog_host_demo_groups}")
    endif()
    if(hog_host_demo_configurations)
        message(FATAL_ERROR
            "Unexpected hog_host_demo configuration requirements: ${hog_host_demo_configurations}")
    endif()

    set(gatt_service_client_leaves
        gatt-service-hids-host
        gatt-service-immediate-alert-service-client
        gatt-service-link-loss-service-client
        gatt-service-object-transfer-service-client
        gatt-service-scan-parameters-service-client
        gatt-service-tx-power-service-client
    )
    set(gatt_client_leaves
        gatt-service-ancs-client
        gatt-service-battery-service-client
        gatt-service-device-information-service-client
    )
    set(gatt_server_leaves
        gatt-service-battery-service-server
        gatt-service-battery-service-v1-server
        gatt-service-bond-management-service-server
        gatt-service-cycling-power-service-server
        gatt-service-cycling-speed-and-cadence-service-server
        gatt-service-device-information-service-server
        gatt-service-heart-rate-service-server
        gatt-service-hids-device
        gatt-service-immediate-alert-service-server
        gatt-service-link-loss-service-server
        gatt-service-nordic-spp-service-server
        gatt-service-object-transfer-service-server
        gatt-service-scan-parameters-service-server
        gatt-service-tx-power-service-server
        gatt-service-ublox-spp-service-server
    )
    foreach(group IN LISTS gatt_service_client_leaves)
        btstack_resolve_groups(resolved GROUPS ${group})
        list(FIND resolved gatt-service-client dependency_index)
        if(dependency_index EQUAL -1)
            message(FATAL_ERROR "${group} did not resolve gatt-service-client")
        endif()
    endforeach()
    foreach(group IN LISTS gatt_client_leaves)
        btstack_resolve_groups(resolved GROUPS ${group})
        list(FIND resolved gatt-client dependency_index)
        if(dependency_index EQUAL -1)
            message(FATAL_ERROR "${group} did not resolve gatt-client")
        endif()
    endforeach()
    foreach(group IN LISTS gatt_server_leaves)
        btstack_resolve_groups(resolved GROUPS ${group})
        list(FIND resolved gatt-server dependency_index)
        if(dependency_index EQUAL -1)
            message(FATAL_ERROR "${group} did not resolve gatt-server")
        endif()
    endforeach()

    btstack_resolve_example_for_port(test-audio a2dp_source_demo
        a2dp_source_groups a2dp_source_configurations)
    if(NOT "${a2dp_source_groups}" STREQUAL "classic;platform-posix-audio-portaudio")
        message(FATAL_ERROR "Unexpected a2dp_source_demo source groups: ${a2dp_source_groups}")
    endif()
    if(a2dp_source_configurations)
        message(FATAL_ERROR
            "Unexpected a2dp_source_demo configuration requirements: ${a2dp_source_configurations}")
    endif()

    expect_failure(unknown "Unknown BTstack source group 'missing-group'")
    expect_failure(cycle "Cyclic BTstack source-group dependencies")
endif()
