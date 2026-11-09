# Standalone tests for the BTstack CMake source-group resolver.
#
# Run directly with:
#   cmake -P test/cmake-resolver/resolver_test.cmake

get_filename_component(BTSTACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${BTSTACK_ROOT}/tool/cmake/BTstack.cmake")

# btstack_declare_application writes its declarations to the caller scope. A macro
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
        btstack_set_port_configuration_macros(test-audio ENABLE_BLE)
        btstack_declare_port_capability(test-audio
            CAPABILITY audio
            GROUPS platform-posix-audio-portaudio)
    endmacro()
    declare_test_audio_port()
    macro(declare_test_led_port)
        btstack_declare_port(test-led)
        btstack_declare_port_capability(test-led
            CAPABILITY led
            PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded")
    endmacro()
    declare_test_led_port()
    macro(declare_test_le_port)
        btstack_declare_port(test-le)
        btstack_set_port_configuration_macros(test-le
            ENABLE_BLE
            ENABLE_LE_PERIPHERAL)
    endmacro()
    declare_test_le_port()
    macro(declare_test_classic_port)
        btstack_declare_port(test-classic)
        btstack_set_port_configuration_macros(test-classic ENABLE_CLASSIC)
    endmacro()
    declare_test_classic_port()

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

    btstack_resolve_example_for_port(test-led led_counter
        led_counter_groups led_counter_configurations)
    if(NOT "${led_counter_groups}" STREQUAL "core")
        message(FATAL_ERROR "Unexpected led_counter source groups: ${led_counter_groups}")
    endif()
    btstack_collect_example_port_public_include_directories(led_include_directories
        test-led led_counter)
    if(NOT "${led_include_directories}" STREQUAL "${BTSTACK_ROOT}/platform/embedded")
        message(FATAL_ERROR "Unexpected LED provider includes: ${led_include_directories}")
    endif()

    # A BLE-only configuration selects LE examples, while Classic examples
    # and examples needing an unavailable port provider are omitted.
    btstack_filter_examples_for_port(le_examples
        PORT test-le
        CANDIDATES spp_counter gatt_counter audio_duplex led_counter)
    if(NOT "${le_examples}" STREQUAL "gatt_counter")
        message(FATAL_ERROR "Unexpected LE-only example selection: ${le_examples}")
    endif()

    # Baseline groups are pruned to the active port configuration. This keeps
    # port CMakeLists.txt declarative while allowing main.c to guard optional
    # feature setup with the same ENABLE_* macros.
    btstack_filter_groups_for_port(classic_base_groups
        PORT test-classic
        GROUPS core ble le-device-db-tlv link-key-db-tlv)
    if(NOT "${classic_base_groups}" STREQUAL "core;link-key-db-tlv")
        message(FATAL_ERROR "Unexpected Classic baseline group selection: ${classic_base_groups}")
    endif()

    # LE Audio requires ISO support in addition to an otherwise available
    # audio provider and BLE configuration.
    btstack_filter_examples_for_port(le_audio_examples
        PORT test-audio
        CANDIDATES le_audio_broadcast_sink_lite)
    if(le_audio_examples)
        message(FATAL_ERROR "LE Audio was selected without ENABLE_LE_ISOCHRONOUS_STREAMS")
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

    btstack_resolve_example(spp_and_gatt_counter spp_and_gatt_counter_groups spp_and_gatt_counter_configurations)
    if(NOT "${spp_and_gatt_counter_groups}" STREQUAL "core;l2cap;classic-sdp-util;att-db;att-dispatch;ble;classic-rfcomm;classic-sdp-server;gatt-server;classic-spp-server")
        message(FATAL_ERROR "Unexpected spp_and_gatt_counter source groups: ${spp_and_gatt_counter_groups}")
    endif()

    btstack_resolve_example(hid_keyboard_demo hid_keyboard_demo_groups hid_keyboard_demo_configurations)
    if(NOT "${hid_keyboard_demo_groups}" STREQUAL "classic-sdp-util;core;classic-device-id-server;l2cap;classic-sdp-server;classic-hid-device")
        message(FATAL_ERROR "Unexpected hid_keyboard_demo source groups: ${hid_keyboard_demo_groups}")
    endif()

    btstack_resolve_example(hid_host_demo hid_host_demo_groups hid_host_demo_configurations)
    if(NOT "${hid_host_demo_groups}" STREQUAL "core;classic-sdp-util;l2cap;classic-sdp-client;classic-hid-host")
        message(FATAL_ERROR "Unexpected hid_host_demo source groups: ${hid_host_demo_groups}")
    endif()

    btstack_resolve_example(gatt_counter gatt_counter_groups gatt_counter_configurations)
    if(NOT "${gatt_counter_groups}" STREQUAL "core;l2cap;att-db;att-dispatch;ble;gatt-server")
        message(FATAL_ERROR "Unexpected gatt_counter source groups: ${gatt_counter_groups}")
    endif()
    if(NOT "${gatt_counter_configurations}" STREQUAL "ENABLE_LE_PERIPHERAL")
        message(FATAL_ERROR
            "Unexpected gatt_counter configuration requirements: ${gatt_counter_configurations}")
    endif()

    btstack_resolve_example_for_port(test-audio le_audio_broadcast_sink_lite
        le_audio_groups le_audio_configurations)
    if(NOT "${le_audio_configurations}" STREQUAL
            "ENABLE_LE_ISOCHRONOUS_STREAMS;ENABLE_LE_PERIODIC_ADVERTISING")
        message(FATAL_ERROR
            "Unexpected LE Audio configuration requirements: ${le_audio_configurations}")
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
    foreach(group IN ITEMS classic-a2dp-source classic-btstack-sbc third-party-bluedroid-sbc)
        list(FIND a2dp_source_groups ${group} group_index)
        if(group_index EQUAL -1)
            message(FATAL_ERROR "a2dp_source_demo did not resolve ${group}")
        endif()
    endforeach()
    if(a2dp_source_configurations)
        message(FATAL_ERROR
            "Unexpected a2dp_source_demo configuration requirements: ${a2dp_source_configurations}")
    endif()

    btstack_resolve_example_for_port(test-audio a2dp_sink_demo
        a2dp_sink_groups a2dp_sink_configurations)
    foreach(group IN ITEMS classic-btstack-sbc third-party-bluedroid-sbc)
        list(FIND a2dp_sink_groups ${group} group_index)
        if(group_index EQUAL -1)
            message(FATAL_ERROR "a2dp_sink_demo did not resolve ${group}")
        endif()
    endforeach()

    expect_failure(unknown "Unknown BTstack source group 'missing-group'")
    expect_failure(cycle "Cyclic BTstack source-group dependencies")
endif()
