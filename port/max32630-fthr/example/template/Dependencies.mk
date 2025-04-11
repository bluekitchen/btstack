SHELL := bash
# BTstack
VPATH += $(BTSTACK_ROOT)/chipset/cc256x
VPATH += $(BTSTACK_ROOT)/example
VPATH += $(BTSTACK_ROOT)/port/pegasus-max3263x
VPATH += $(BTSTACK_ROOT)/src
VPATH += $(BTSTACK_ROOT)/src/ble
VPATH += $(BTSTACK_ROOT)/src/classic
VPATH += $(BTSTACK_ROOT)/src/mesh
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv6
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/netif
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/apps/http
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server
VPATH += ${BTSTACK_ROOT}/3rd-party/md5
VPATH += ${BTSTACK_ROOT}/3rd-party/yxml
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += $(BTSTACK_ROOT)/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/lwip
VPATH += ${BTSTACK_ROOT}/platform/lwip/port
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service

PROJ_CFLAGS += \
    -I$(BTSTACK_ROOT)/src \
    -I$(BTSTACK_ROOT)/src/ble \
    -I$(BTSTACK_ROOT)/src/classic \
    -I$(BTSTACK_ROOT)/src/mesh \
    -I$(BTSTACK_ROOT)/chipset/cc256x \
    -I$(BTSTACK_ROOT)/platform/posix \
    -I$(BTSTACK_ROOT)/platform/embedded \
    -I$(BTSTACK_ROOT)/platform/lwip \
    -I$(BTSTACK_ROOT)/platform/lwip/port \
    -I${BTSTACK_ROOT}/port/pegasus-max3263x \
    -I${BTSTACK_ROOT}/src/ble/gatt-service/ \
    -I${BTSTACK_ROOT}/example \
    -I${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include \
    -I${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include \
    -I${BTSTACK_ROOT}/3rd-party/md5 \
    -I${BTSTACK_ROOT}/3rd-party/yxml \
    -I${BTSTACK_ROOT}/3rd-party/micro-ecc \
    -I${BTSTACK_ROOT}/3rd-party/hxcmod-player \
    -I${BTSTACK_ROOT}/3rd-party/lwip/core/src/include \
    -I${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server \

CORE = \
    btstack_audio.c \
    btstack_crypto.c \
    btstack_link_key_db_tlv.c \
    btstack_linked_list.c \
    btstack_memory.c \
    btstack_memory_pool.c \
    btstack_run_loop.c \
    btstack_run_loop_embedded.c \
    btstack_stdin_embedded.c \
    btstack_tlv.c \
    btstack_tlv_flash_bank.c \
    btstack_uart_block_embedded.c \
    btstack_util.c \

COMMON = \
    ad_parser.c \
    hci.c \
    hci_cmd.c \
    hci_dump.c \
    hci_dump_embedded_stdout.c \
    hci_event_builder.c \
    hci_transport_h4.c \
    l2cap.c \
    l2cap_signaling.c \
    le_device_db_tlv.c \
    sm.c \
    uECC.c \

CLASSIC = \
    sdp_util.c \
    gatt_sdp.c \
    spp_server.c \
    rfcomm.c \
    bnep.c \
    sdp_server.c \
    device_id_server.c \

BLE = \
    att_db.c \
    att_server.c \
    le_device_db_tlv.c \
    att_dispatch.c \
    ancs_client.c \
    gatt_client.c \
    hid_device.c \
    battery_service_server.c \

SDP_CLIENT_OBJ = \
    sdp_client.o \
    sdp_client_rfcomm.o \

ATT = \
    att_dispatch.c \

GATT_SERVER = \
    att_db.c \
    att_server.c \

GATT_CLIENT = \
    gatt_client.c \
    gatt_service_client.c \
    battery_service_client.c \
    device_information_service_client.c \
    scan_parameters_service_client.c \
    hids_client.c \

PAN = \
    pan.c \

MBEDTLS = \
    bignum.c \
    ecp.c \
    ecp_curves.c \
    sm_mbedtls_allocator.c \
    memory_buffer_alloc.c \
    platform.c \


LWIP_CORE_SRC  = init.c mem.c memp.c netif.c udp.c ip.c pbuf.c inet_chksum.c def.c tcp.c tcp_in.c tcp_out.c timeouts.c sys_arch.c
LWIP_IPV4_SRC  = acd.c dhcp.c etharp.c icmp.c ip4.c ip4_frag.c ip4_addr.c
LWIP_NETIF_SRC = ethernet.c
LWIP_HTTPD = altcp_proxyconnect.c fs.c httpd.c
LWIP_SRC = ${LWIP_CORE_SRC} ${LWIP_IPV4_SRC} ${LWIP_NETIF_SRC} ${LWIP_HTTPD} dhserver.c

# List of files for Bluedroid SBC codec
include ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/Makefile.inc

SBC_CODEC = \
    ${SBC_DECODER} \
    btstack_sbc_plc.c \
    btstack_sbc_decoder_bluedroid.c \
    ${SBC_ENCODER} \
    btstack_sbc_encoder_bluedroid.c \
    hfp_msbc.c \
    hfp_codec.c \
    btstack_sbc_bluedroid.c

CVSD_PLC = \
    btstack_cvsd_plc.c \

AVDTP = \
    avdtp_util.c \
    avdtp.c \
    avdtp_initiator.c \
    avdtp_acceptor.c \
    avdtp_source.c \
    avdtp_sink.c \
    a2dp.c \
    a2dp_source.c \
    a2dp_sink.c \
    btstack_ring_buffer.c \
    btstack_resample.c \

AVRCP = \
    avrcp.c \
    avrcp_controller.c \
    avrcp_target.c \
    avrcp_browsing.c \
    avrcp_browsing_controller.c \
    avrcp_browsing_target.c \
    avrcp_media_item_iterator.c \

HXCMOD_PLAYER = \
    hxcmod.c \
    nao-deceased_by_disease.c \

MESH = \
    adv_bearer.c \
    beacon.c \
    gatt_bearer.c \
    mesh.c \
    mesh_access.c \
    mesh_configuration_client.c \
    mesh_configuration_server.c \
    mesh_crypto.c \
    mesh_foundation.c \
    mesh_generic_default_transition_time_client.c \
    mesh_generic_default_transition_time_server.c \
    mesh_generic_level_client.c \
    mesh_generic_level_server.c \
    mesh_generic_on_off_client.c \
    mesh_generic_on_off_server.c \
    mesh_health_server.c \
    mesh_iv_index_seq_number.c \
    mesh_keys.c \
    mesh_lower_transport.c \
    mesh_network.c \
    mesh_node.c \
    mesh_peer.c \
    mesh_provisioning_service_server.c \
    mesh_proxy.c \
    mesh_proxy_service_server.c \
    mesh_upper_transport.c \
    mesh_virtual_addresses.c \
    pb_adv.c \
    pb_gatt.c \
    provisioning.c \
    provisioning_device.c \
    provisioning_provisioner.c \

# List of GATT files used by either LE_ONLY or DUAL_MODE examples

EXAMPLES_GATT_FILES = \
    ancs_client_demo.ga\
    att_delayed_response.ga\
    gatt_battery_query.ga\
    gatt_browser.ga\
    gatt_counter.ga\
    gatt_device_information_query.ga\
    gatt_streamer_server.ga\
    hog_host_demo.ga\
    hog_keyboard_demo.ga\
    hog_mouse_demo.ga\
    le_credit_based_flow_control_mode_server.ga\
    nordic_spp_le_counter.ga\
    nordic_spp_le_streamer.ga\
    sm_pairing_central.ga\
    sm_pairing_peripheral.ga\
    spp_and_gatt_counter.ga\
    spp_and_gatt_streamer.ga\
    ublox_spp_le_counter.ga

include ${PORT_DIR}/example/template/Categories.mk
# define target variant for chipset/cc256x/Makefile.inc
ifneq ($(filter $(PROJECT),$(EXAMPLES_GENERAL)),)
    $(info $(PROJECT) in EXAMPLES_GENERAL)
else
    $(info $(PROJECT) not in EXAMPLES_GENERAL: $(EXAMPLES_GENERAL))
    include ${BTSTACK_ROOT}/chipset/cc256x/Makefile.inc
    CORE += btstack_chipset_cc256x.c
    COMMON += $(cc256x_init_script)
    PROJ_CFLAGS += -DENABLE_HCI_INIT
endif

# .o for .c
ATT_OBJ           = $(ATT:.c=.o)
AVDTP_OBJ         = $(AVDTP:.c=.o)
AVRCP_OBJ         = $(AVRCP:.c=.o)
BLE_OBJ           = $(BLE:.c=.o)
CLASSIC_OBJ       = $(CLASSIC:.c=.o)
COMMON_OBJ        = $(COMMON:.c=.o)
CORE_OBJ          = $(CORE:.c=.o)
CVSD_PLC_OBJ      = $(CVSD_PLC:.c=.o)
GATT_CLIENT_OBJ   = $(GATT_CLIENT:.c=.o)
GATT_SERVER_OBJ   = $(GATT_SERVER:.c=.o)
HXCMOD_PLAYER_OBJ = $(HXCMOD_PLAYER:.c=.o)
LWIP_OBJ          = $(LWIP_SRC:.c=.o)
MESH_OBJ          = $(MESH:.c=.o)
PAN_OBJ           = $(PAN:.c=.o)
SBC_CODEC_OBJ     = $(SBC_CODEC:.c=.o)

# examples

# general - no bluetooth
GENERAL_DEPS = ${CORE_OBJ} ${COMMON_OBJ}
audio_duplex_deps = ${GENERAL_DEPS} btstack_audio.o btstack_ring_buffer.o audio_duplex.c
led_counter_deps = ${GENERAL_DEPS} led_counter.c
mod_player_deps = ${GENERAL_DEPS} ${HXCMOD_PLAYER_OBJ} btstack_audio.o mod_player.c
sine_player_deps = ${GENERAL_DEPS} btstack_audio.o sine_player.c
# general - END

# le - bluetooth le
BLE_DEPS = ${BLE_OBJ} ${GENERAL_DEPS}
ancs_client_demo_deps = ${BLE_DEPS} ${CLASSIC_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} ancs_client.o ancs_client_demo.o
att_delayed_response_deps = ${BLE_DEPS} $(CLASSIC_OBJ) ${ATT_OBJ} ${GATT_SERVER_OBJ} att_delayed_response.o
gap_le_advertisements_deps = ${BLE_DEPS}  gap_le_advertisements.c
gatt_battery_query_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${GATT_SERVER_OBJ} gatt_battery_query.o
gatt_browser_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${GATT_SERVER_OBJ} gatt_browser.o
gatt_device_information_query_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${GATT_SERVER_OBJ} gatt_device_information_query.o
gatt_heart_rate_client_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_CLIENT_OBJ} gatt_heart_rate_client.c
hog_boot_host_demo_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_CLIENT_OBJ} hog_boot_host_demo.o
hog_host_demo_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${GATT_SERVER_OBJ} btstack_hid_parser.o btstack_hid.o hog_host_demo.o
hog_keyboard_demo_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} battery_service_server.o device_information_service_server.o btstack_hid_parser.o hids_device.o btstack_ring_buffer.o hog_keyboard_demo.o
hog_mouse_demo_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} battery_service_server.o device_information_service_server.o btstack_hid_parser.o hids_device.o hog_mouse_demo.o
le_credit_based_flow_control_mode_client_deps = ${BLE_DEPS} le_credit_based_flow_control_mode_client.c
le_credit_based_flow_control_mode_server_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} le_credit_based_flow_control_mode_server.o
le_mitm_deps = ${BLE_DEPS} le_mitm.c
le_streamer_client_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_CLIENT_OBJ} le_streamer_client.c
mesh_node_demo_deps = ${BLE_DEPS} ${MESH_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} mesh_node_demo.o
nordic_spp_le_counter_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} nordic_spp_service_server.o nordic_spp_le_counter.o
nordic_spp_le_streamer_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} nordic_spp_service_server.o nordic_spp_le_streamer.o
sm_pairing_central_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} sm_pairing_central.o
sm_pairing_peripheral_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} sm_pairing_peripheral.o
ublox_spp_le_counter_deps = ${BLE_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} device_information_service_server.o ublox_spp_service_server.o ublox_spp_le_counter.o
# le - END

# classic - no le
CLASSIC_DEPS = ${GENERAL_DEPS} ${CLASSIC_OBJ}
a2dp_sink_demo_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} ${SBC_CODEC_OBJ} ${AVDTP_OBJ} avrcp.o avrcp_controller.o avrcp_target.o avrcp_cover_art_client.o  obex_srm_client.o goep_client.o obex_parser.o obex_message_builder.o btstack_sample_rate_compensation.o a2dp_sink_demo.c
a2dp_source_demo_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} ${SBC_CODEC_OBJ} ${AVDTP_OBJ} ${HXCMOD_PLAYER_OBJ} avrcp.o avrcp_controller.o avrcp_target.o a2dp_source_demo.c
ant_test_deps = ${CLASSIC_DEPS} ant_test.c
avrcp_browsing_client_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} ${AVRCP_OBJ} ${AVDTP_OBJ} avrcp_browsing_client.c
dut_mode_classic_deps = ${CLASSIC_DEPS} dut_mode_classic.c
gap_dedicated_bonding_deps = ${CLASSIC_DEPS} gap_dedicated_bonding.c
gap_inquiry_deps = ${CLASSIC_DEPS} gap_inquiry.c
gap_link_keys_deps = ${CLASSIC_DEPS} gap_link_keys.c
hfp_ag_demo_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} ${SBC_CODEC_OBJ} ${CVSD_PLC_OBJ} wav_util.o sco_demo_util.o btstack_ring_buffer.o hfp.o hfp_gsm_model.o hfp_ag.o hfp_ag_demo.c
hfp_hf_demo_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} ${SBC_CODEC_OBJ} ${CVSD_PLC_OBJ} wav_util.o sco_demo_util.o btstack_ring_buffer.o hfp.o hfp_hf.o hfp_hf_demo.c
hid_host_demo_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} btstack_hid_parser.o hid_host.o hid_host_demo.o
hid_keyboard_demo_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} btstack_ring_buffer.o hid_device.o btstack_hid_parser.o hid_keyboard_demo.o
hid_mouse_demo_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} btstack_ring_buffer.o hid_device.o btstack_hid_parser.o hid_mouse_demo.o
hsp_ag_demo_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} ${SBC_CODEC_OBJ} ${CVSD_PLC_OBJ} wav_util.o sco_demo_util.o btstack_ring_buffer.o hsp_ag.o hsp_ag_demo.c
hsp_hs_demo_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} ${SBC_CODEC_OBJ} ${CVSD_PLC_OBJ} wav_util.o sco_demo_util.o btstack_ring_buffer.o hsp_hs.o hsp_hs_demo.c
panu_demo_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} ${PAN_OBJ} panu_demo.c
pan_lwip_http_server_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} ${PAN_OBJ} ${LWIP_SRC} btstack_ring_buffer.o bnep_lwip.o pan_lwip_http_server.o
pbap_client_demo_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} md5.o obex_iterator.o obex_parser.o obex_message_builder.o obex_srm_client.o goep_client.o yxml.o pbap_client.o pbap_client_demo.o
sdp_bnep_query_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} sdp_bnep_query.c
sdp_general_query_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} sdp_general_query.c
sdp_rfcomm_query_deps = ${CLASSIC_DEPS} ${PAN_OBJ} ${SDP_CLIENT_OBJ} sdp_rfcomm_query.c
spp_counter_deps = ${CLASSIC_DEPS} spp_counter.c
spp_flowcontrol_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} spp_flowcontrol.c
spp_streamer_client_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} spp_streamer_client.c
spp_streamer_deps = ${CLASSIC_DEPS} ${SDP_CLIENT_OBJ} spp_streamer.c
# classic - END

# dual - classic + le
DUAL_DEPS = ${GENERAL_DEPS} ${BLE_OBJ} ${CLASSIC_OBJ}
gatt_counter_deps = ${DUAL_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} battery_service_server.o gatt_counter.o
gatt_streamer_server_deps = ${DUAL_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} gatt_streamer_server.o
spp_and_gatt_counter_deps = ${DUAL_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} spp_and_gatt_counter.o
spp_and_gatt_streamer_deps = ${DUAL_DEPS} ${ATT_OBJ} ${GATT_SERVER_OBJ} spp_and_gatt_streamer.o
# dual - END

# Expands PROJECT_FEATURES when $(PROJECT)_deps is empty.
# Expected values are the XX_DEPS and XX_OBJ values defined in Dependencies.mk
# e.g. PROJECT_FEATURES = DUAL_DEPS ATT_OBJ GATT_SERVER_OBJ
ifeq ($($(PROJECT)_deps),)
  ifeq ($(PROJECT_FEATURES),)
    $(warning $(PROJECT)_deps is empty)
  else
    # Add dependencies
    $(foreach dep, $(PROJECT_FEATURES), $(eval $(PROJECT)_deps += $($(dep))))
	# Add main project file to build $(PROJECT).o from $(PROJECT).c
	$(PROJECT)_deps += $(PROJECT)
  endif
endif