# Declarative requirements for the examples currently listed in
# example/CMakeLists.txt.
#
# Include tool/cmake/BTstack.cmake before including this file. Declarations
# state what an example needs; they do not enable BTstack features or create
# targets. The current CMakeLists.txt remains a compatibility list until ports
# are migrated to consume these declarations.

# Shared application-support sources. These stay with the example declarations
# rather than the generated BTstack source catalog because they are not BTstack
# implementation code and only a subset of examples uses each one.
set(audio_demo_util_sources
    audio_player_demo_util.c
)
set(le_audio_demo_util_sink_sources
    le_audio_demo_util_sink.c
)
set(le_audio_demo_util_source_sources
    le_audio_demo_util_source.c
)
set(sco_demo_util_sources
    sco_demo_util.c
)

# General examples without Bluetooth.
foreach(example IN ITEMS
    audio_duplex
    led_counter
    mod_player
    sine_player
)
    btstack_declare_example(${example}
        SOURCES ${example}.c
    )
endforeach()

btstack_declare_example(audio_player
    SOURCES audio_player.c ${audio_demo_util_sources}
)

# Classic-only examples. ublox_spp_le_counter is declared with the LE GATT
# examples below because its default configuration is LE.
foreach(example IN ITEMS
    a2dp_sink_demo
    avrcp_browsing_client
    dut_mode_classic
    gap_dedicated_bonding
    gap_inquiry
    gap_link_keys
    hid_host_demo
    hid_keyboard_demo
    hid_mouse_demo
    pbap_client_demo
    sdp_bnep_query
    sdp_general_query
    sdp_rfcomm_query
    spp_counter
    spp_streamer
    spp_streamer_client
)
    btstack_declare_example(${example}
        SOURCES ${example}.c
        NEEDS classic
    )
endforeach()

btstack_declare_example(a2dp_source_demo
    SOURCES a2dp_source_demo.c
    NEEDS classic audio
)

foreach(example IN ITEMS
    hfp_ag_demo
    hfp_hf_demo
    hsp_ag_demo
    hsp_hs_demo
)
    btstack_declare_example(${example}
        SOURCES ${example}.c ${sco_demo_util_sources}
        NEEDS classic
    )
endforeach()

# LE examples without an application-specific GATT database.
foreach(example IN ITEMS
    gap_le_advertisements
    le_credit_based_flow_control_mode_client
    le_mitm
)
    btstack_declare_example(${example}
        SOURCES ${example}.c
        NEEDS ble
    )
endforeach()

btstack_declare_example(hog_boot_host_demo
    SOURCES hog_boot_host_demo.c
    NEEDS gatt-client
)

btstack_declare_example(le_streamer_client
    SOURCES le_streamer_client.c
    NEEDS gatt-service-client
)

btstack_declare_example(gatt_heart_rate_client
    SOURCES gatt_heart_rate_client.c
    NEEDS gatt-client
)

# gatt_counter's normal configuration is a LE peripheral. Its optional
# ENABLE_GATT_OVER_CLASSIC code path is intentionally not represented here yet.
btstack_declare_example(gatt_counter
    SOURCES gatt_counter.c
    GATT_DB gatt_counter.gatt
    NEEDS gatt-server ENABLE_LE_PERIPHERAL
)

btstack_declare_example(gatt_battery_query
    SOURCES gatt_battery_query.c
    GATT_DB gatt_battery_query.gatt
    NEEDS gatt-server gatt-service-battery-service-client
)

btstack_declare_example(hog_host_demo
    SOURCES hog_host_demo.c
    GATT_DB hog_host_demo.gatt
    NEEDS gatt-server gatt-service-hids-host
)

btstack_declare_example(ancs_client_demo
    SOURCES ancs_client_demo.c
    GATT_DB ancs_client_demo.gatt
    NEEDS gatt-server gatt-service-ancs-client
)

btstack_declare_example(att_delayed_response
    SOURCES att_delayed_response.c
    GATT_DB att_delayed_response.gatt
    NEEDS gatt-server
)

btstack_declare_example(gatt_browser
    SOURCES gatt_browser.c
    GATT_DB gatt_browser.gatt
    NEEDS gatt-server gatt-client
)

btstack_declare_example(gatt_device_information_query
    SOURCES gatt_device_information_query.c
    GATT_DB gatt_device_information_query.gatt
    NEEDS gatt-server gatt-service-device-information-service-client
)

btstack_declare_example(gatt_streamer_server
    SOURCES gatt_streamer_server.c
    GATT_DB gatt_streamer_server.gatt
    NEEDS gatt-server
)

foreach(example IN ITEMS
    hog_keyboard_demo
    hog_mouse_demo
)
    btstack_declare_example(${example}
        SOURCES ${example}.c
        GATT_DB ${example}.gatt
        NEEDS
            gatt-server
            gatt-service-battery-service-server
            gatt-service-device-information-service-server
            gatt-service-hids-device
    )
endforeach()

btstack_declare_example(le_credit_based_flow_control_mode_server
    SOURCES le_credit_based_flow_control_mode_server.c
    GATT_DB le_credit_based_flow_control_mode_server.gatt
    NEEDS gatt-server
)

btstack_declare_example(mesh_node_demo
    SOURCES mesh_node_demo.c
    GATT_DB mesh_node_demo.gatt
    NEEDS gatt-server mesh
)

foreach(example IN ITEMS
    nordic_spp_le_counter
    nordic_spp_le_streamer
)
    btstack_declare_example(${example}
        SOURCES ${example}.c
        GATT_DB ${example}.gatt
        NEEDS gatt-server gatt-service-nordic-spp-service-server
    )
endforeach()

foreach(example IN ITEMS
    sm_pairing_central
    sm_pairing_peripheral
)
    btstack_declare_example(${example}
        SOURCES ${example}.c
        GATT_DB ${example}.gatt
        NEEDS gatt-server gatt-client
    )
endforeach()

btstack_declare_example(ublox_spp_le_counter
    SOURCES ublox_spp_le_counter.c
    GATT_DB ublox_spp_le_counter.gatt
    NEEDS
        gatt-server
        gatt-service-device-information-service-server
        gatt-service-ublox-spp-service-server
)

# LE Audio examples, enabled by selected ports.
btstack_declare_example(le_audio_broadcast_sink_lite
    SOURCES le_audio_broadcast_sink_lite.c ${le_audio_demo_util_sink_sources}
    NEEDS le-audio
)
btstack_declare_example(le_audio_broadcast_source_lite
    SOURCES le_audio_broadcast_source_lite.c
            ${audio_demo_util_sources}
            ${le_audio_demo_util_source_sources}
    NEEDS le-audio
)
btstack_declare_example(le_audio_unicast_gateway_lite
    SOURCES le_audio_unicast_gateway_lite.c
            ${audio_demo_util_sources}
            ${le_audio_demo_util_sink_sources}
            ${le_audio_demo_util_source_sources}
    NEEDS le-audio
)
btstack_declare_example(le_audio_unicast_headset_lite
    SOURCES le_audio_unicast_headset_lite.c
            ${le_audio_demo_util_sink_sources}
            ${le_audio_demo_util_source_sources}
    NEEDS le-audio
)

# Dual-mode examples not already declared above.
foreach(example IN ITEMS
    spp_and_gatt_counter
    spp_and_gatt_streamer
)
    btstack_declare_example(${example}
        SOURCES ${example}.c
        GATT_DB ${example}.gatt
        NEEDS ble classic
    )
endforeach()
