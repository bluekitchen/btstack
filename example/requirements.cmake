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

# General examples use the minimal BTstack core requirement. Host ports may
# still link their normal controller setup for convenience.
foreach(example IN ITEMS
    audio_duplex
    led_counter
    sine_player
)
    btstack_declare_example(${example}
        SOURCES ${example}.c
        NEEDS core
    )
endforeach()

btstack_declare_example(mod_player
    SOURCES mod_player.c
    NEEDS core third-party-hxcmod
)

btstack_declare_example(audio_player
    SOURCES audio_player.c ${audio_demo_util_sources}
    NEEDS core audio-demo third-party-hxcmod
)

# Classic-only examples. ublox_spp_le_counter is declared with the LE GATT
# examples below because its default configuration is LE.
btstack_declare_example(a2dp_sink_demo SOURCES a2dp_sink_demo.c
    NEEDS classic-a2dp-sink classic-avrcp-controller classic-avrcp-target
          classic-avrcp-cover-art-client classic-btstack-sbc-bluedroid
          classic-btstack-sbc-decoder-bluedroid classic-btstack-sbc-plc
          third-party-bluedroid-sbc classic-sdp-server classic-device-id-server)

btstack_declare_example(spp_counter
    SOURCES spp_counter.c
    NEEDS classic-spp-server
)

btstack_declare_example(spp_streamer
    SOURCES spp_streamer.c
    NEEDS classic-spp-server
)

foreach(example IN ITEMS
    sdp_bnep_query
    sdp_general_query
)
    btstack_declare_example(${example}
        SOURCES ${example}.c
        NEEDS classic-sdp-client
    )
endforeach()

btstack_declare_example(sdp_rfcomm_query
    SOURCES sdp_rfcomm_query.c
    NEEDS classic-sdp-client-rfcomm
)

btstack_declare_example(spp_streamer_client
    SOURCES spp_streamer_client.c
    NEEDS classic-rfcomm classic-sdp-client-rfcomm
)

foreach(example IN ITEMS
    hid_keyboard_demo
    hid_mouse_demo
)
    btstack_declare_example(${example}
        SOURCES ${example}.c
        NEEDS classic-hid-device classic-device-id-server
    )
endforeach()

btstack_declare_example(hid_host_demo
    SOURCES hid_host_demo.c
    NEEDS classic-hid-host
)

foreach(example IN ITEMS dut_mode_classic gap_dedicated_bonding gap_inquiry gap_link_keys)
    btstack_declare_example(${example} SOURCES ${example}.c NEEDS core)
endforeach()

btstack_declare_example(pbap_client_demo SOURCES pbap_client_demo.c NEEDS classic-pbap-client)
btstack_declare_example(avrcp_browsing_client SOURCES avrcp_browsing_client.c
    NEEDS classic-a2dp-sink classic-avrcp-browsing-controller
          classic-avrcp-browsing-target classic-avrcp-media-item-iterator
          classic-sdp-server)

btstack_declare_example(hfp_ag_demo SOURCES hfp_ag_demo.c ${sco_demo_util_sources} NEEDS classic-hfp-ag)
btstack_declare_example(hfp_hf_demo SOURCES hfp_hf_demo.c ${sco_demo_util_sources} NEEDS classic-hfp-hf)
btstack_declare_example(hsp_ag_demo SOURCES hsp_ag_demo.c ${sco_demo_util_sources} NEEDS classic-hsp-ag)
btstack_declare_example(hsp_hs_demo SOURCES hsp_hs_demo.c ${sco_demo_util_sources} NEEDS classic-hsp-hs)

btstack_declare_example(a2dp_source_demo
    SOURCES a2dp_source_demo.c
    NEEDS classic-a2dp-source classic-avrcp-controller classic-avrcp-target
          classic-btstack-sbc-bluedroid classic-btstack-sbc-encoder-bluedroid
          classic-btstack-sbc-plc third-party-bluedroid-sbc audio-demo third-party-hxcmod audio
          classic-sdp-server classic-device-id-server
)


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
    NEEDS le-audio audio-demo third-party-hxcmod
)
btstack_declare_example(le_audio_unicast_gateway_lite
    SOURCES le_audio_unicast_gateway_lite.c
            ${audio_demo_util_sources}
            ${le_audio_demo_util_sink_sources}
            ${le_audio_demo_util_source_sources}
    NEEDS le-audio audio-demo third-party-hxcmod
)
btstack_declare_example(le_audio_unicast_headset_lite
    SOURCES le_audio_unicast_headset_lite.c
            ${le_audio_demo_util_sink_sources}
            ${le_audio_demo_util_source_sources}
    NEEDS le-audio audio-demo third-party-hxcmod
)

# Dual-mode examples compose the GATT server with the SPP server profile.
foreach(example IN ITEMS
    spp_and_gatt_counter
    spp_and_gatt_streamer
)
    btstack_declare_example(${example}
        SOURCES ${example}.c
        GATT_DB ${example}.gatt
        NEEDS gatt-server classic-spp-server
    )
endforeach()
