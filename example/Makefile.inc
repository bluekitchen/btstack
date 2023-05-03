
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/src/mesh
VPATH += ${BTSTACK_ROOT}/src/mesh/gatt-service
VPATH += ${BTSTACK_ROOT}/example
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv6
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/netif
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/apps/http
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server
VPATH += ${BTSTACK_ROOT}/3rd-party/md5
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/yxml
VPATH += ${BTSTACK_ROOT}/platform/lwip
VPATH += ${BTSTACK_ROOT}/platform/lwip/port


CFLAGS += -I.
CFLAGS += -I${BTSTACK_ROOT}/src/ble
CFLAGS += -I${BTSTACK_ROOT}/src/classic
CFLAGS += -I${BTSTACK_ROOT}/src/mesh
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/hxcmod-player
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/lwip/core/src/include/
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/md5
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/yxml
CFLAGS += -I${BTSTACK_ROOT}/platform/lwip
CFLAGS += -I${BTSTACK_ROOT}/platform/lwip/port

# for CVSD/SBC PLC
LDFLAGS += -lm

CORE += \
	btstack_memory.c            \
	btstack_linked_list.c	    \
	btstack_memory_pool.c       \
	btstack_run_loop.c		    \
	btstack_util.c 	            \

COMMON += \
	ad_parser.c                 \
	hci.c			            \
	hci_cmd.c		            \
	hci_dump.c		            \
	hci_event.c                 \
	l2cap.c			            \
	l2cap_signaling.c	        \
	btstack_audio.c             \
	btstack_tlv.c               \
	btstack_crypto.c            \
	uECC.c                      \
	sm.c                        \

CLASSIC ?= \
	sdp_util.c	                \
	gatt_sdp.c                  \
	spp_server.c  				\
	rfcomm.c	                \
	bnep.c	                    \
	sdp_server.c			            \
	device_id_server.c          \

SDP_CLIENT += \
	sdp_client.o		        \
	sdp_client_rfcomm.o		    \

ATT	+= \
	att_dispatch.c       	    \

GATT_SERVER += \
	att_db.c 				 	    \
	att_server.c        	    \

GATT_CLIENT += \
	gatt_client.c        	    			\
	battery_service_client.c 				\
	device_information_service_client.c 	\
	scan_parameters_service_client.c 	    \
	hids_client.c 	    					\

PAN += \
	pan.c \

MBEDTLS = 					\
	bignum.c 				\
	ecp.c 					\
	ecp_curves.c 			\
	sm_mbedtls_allocator.c  \
	memory_buffer_alloc.c   \
	platform.c 				\


LWIP_CORE_SRC  = init.c mem.c memp.c netif.c udp.c ip.c pbuf.c inet_chksum.c def.c tcp.c tcp_in.c tcp_out.c timeouts.c sys_arch.c
LWIP_IPV4_SRC  = acd.c dhcp.c etharp.c icmp.c ip4.c ip4_frag.c ip4_addr.c
LWIP_NETIF_SRC = ethernet.c
LWIP_HTTPD = altcp_proxyconnect.c fs.c httpd.c
LWIP_SRC = ${LWIP_CORE_SRC} ${LWIP_IPV4_SRC} ${LWIP_NETIF_SRC} ${LWIP_HTTPD} dhserver.c

# List of files for Bluedroid SBC codec
include ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/Makefile.inc

SBC_DECODER += \
	btstack_sbc_plc.c \
	btstack_sbc_decoder_bluedroid.c \

SBC_ENCODER += \
	btstack_sbc_encoder_bluedroid.c \
	hfp_msbc.c \
	hfp_codec.c

CVSD_PLC = \
	btstack_cvsd_plc.c \

AVDTP += \
	avdtp_util.c           \
	avdtp.c                \
	avdtp_initiator.c      \
	avdtp_acceptor.c       \
	avdtp_source.c 	       \
	avdtp_sink.c           \
	a2dp.c                 \
	a2dp_source.c          \
	a2dp_sink.c            \
	btstack_ring_buffer.c \

AVRCP += \
	avrcp.c							\
	avrcp_controller.c				\
	avrcp_target.c					\
	avrcp_browsing.c				\
	avrcp_browsing_controller.c		\
	avrcp_browsing_target.c			\
	avrcp_media_item_iterator.c		\

HXCMOD_PLAYER = \
	hxcmod.c                    \
	nao-deceased_by_disease.c 	\

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

# List of General Examples without Bluetooth

EXAMPLES_GENERAL =          \
	audio_duplex            \
	led_counter             \
	mod_player	            \
	sine_player             \

# List of Examples that only use Bluetooth BR/EDR = Classic

EXAMPLES_CLASSIC_ONLY =     \
	a2dp_sink_demo          \
	a2dp_source_demo        \
	avrcp_browsing_client   \
	dut_mode_classic        \
	gap_dedicated_bonding   \
	gap_inquiry             \
	gap_link_keys           \
	hfp_ag_demo             \
	hfp_hf_demo             \
	hid_host_demo           \
	hid_keyboard_demo       \
	hid_mouse_demo          \
	hsp_ag_demo             \
	hsp_hs_demo             \
	pbap_client_demo        \
	sdp_bnep_query          \
	sdp_general_query       \
	sdp_rfcomm_query        \
	spp_counter             \
	spp_streamer            \
	spp_streamer_client     \
	ublox_spp_le_counter    \

# List of Examples that only use Bluetooth LE

EXAMPLES_LE_ONLY=           		\
	ancs_client_demo        		\
	att_delayed_response    		\
	gap_le_advertisements   		\
	gatt_battery_query      		\
	gatt_browser            		\
	gatt_counter            		\
	gatt_device_information_query	\
	gatt_heart_rate_client  		\
	gatt_streamer_server    		\
	hog_boot_host_demo      		\
	hog_host_demo      		        \
	hog_keyboard_demo       		\
	hog_mouse_demo          		\
	le_credit_based_flow_control_mode_client  		\
	le_credit_based_flow_control_mode_server  		\
	le_mitm                 		\
	le_streamer_client      		\
	mesh_node_demo          		\
	nordic_spp_le_counter   		\
	nordic_spp_le_streamer  		\
	sm_pairing_central      		\
	sm_pairing_peripheral   		\
	ublox_spp_le_counter    		\

# List of Examples that use Bluetooth BR/EDR/LE = Dual Mode

EXAMPLES_DUAL_MODE=         \
	gatt_counter            \
	gatt_streamer_server    \
	spp_and_gatt_counter    \
	spp_and_gatt_streamer   \

# List of GATT files used by either LE_ONLY or DUAL_MODE examples

EXAMPLES_GATT_FILES =           		\
	ancs_client_demo.gatt       		\
	att_delayed_response.gatt   		\
	gatt_battery_query.gatt     		\
	gatt_browser.gatt           		\
	gatt_counter.gatt           		\
	gatt_device_information_query.gatt 	\
	gatt_streamer_server.gatt   		\
	hog_keyboard_demo.gatt      		\
	hog_mouse_demo.gatt         		\
	le_credit_based_flow_control_mode_server.gatt 		\
	nordic_spp_le_counter.gatt  		\
	nordic_spp_le_streamer.gatt 		\
	sm_pairing_central.gatt     		\
	sm_pairing_peripheral.gatt  		\
	spp_and_gatt_counter.gatt   		\
	spp_and_gatt_streamer.gatt  		\
	ublox_spp_le_counter.gatt  			\

# .h for .gatt
EXAMPLES_GATT_H_FILES = $(EXAMPLES_GATT_FILES:.gatt=.h)

# .o for .c
CORE_OBJ    = $(CORE:.c=.o)
COMMON_OBJ  = $(COMMON:.c=.o)
CLASSIC_OBJ  = $(CLASSIC:.c=.o)
ATT_OBJ     = $(ATT:.c=.o)
GATT_CLIENT_OBJ = $(GATT_CLIENT:.c=.o)
GATT_SERVER_OBJ = $(GATT_SERVER:.c=.o)
PAN_OBJ = $(PAN:.c=.o)
SBC_DECODER_OBJ  = $(SBC_DECODER:.c=.o)
SBC_ENCODER_OBJ  = $(SBC_ENCODER:.c=.o)
CVSD_PLC_OBJ = $(CVSD_PLC:.c=.o)
AVDTP_OBJ  = $(AVDTP:.c=.o)
AVRCP_OBJ  = $(AVRCP:.c=.o)
HXCMOD_PLAYER_OBJ = $(HXCMOD_PLAYER:.c=.o)
LWIP_OBJ = $(LWIP_SRC:.c=.o)
MESH_OBJ = $(MESH:.c=.o)

default_target: all

# compile .gatt descriptions
%.h: %.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

# examples

ant_test: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ant_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sdp_rfcomm_query: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${PAN_OBJ} ${SDP_CLIENT} sdp_rfcomm_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

pbap_client_demo: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} md5.o obex_iterator.o obex_parser.o obex_message_builder.o goep_client.o yxml.o pbap_client.o pbap_client_demo.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sdp_general_query: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} sdp_general_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sdp_bnep_query: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} sdp_bnep_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

spp_counter: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} spp_counter.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@


att_delayed_response.o: att_delayed_response.h att_delayed_response.c
	${CC} ${CFLAGS} -c $(filter-out att_delayed_response.h,$^) -o $@

att_delayed_response: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} att_delayed_response.o
	${CC} $^ ${LDFLAGS} -o $@


hog_keyboard_demo.o: hog_keyboard_demo.h hog_keyboard_demo.c
	${CC} ${CFLAGS} -c $(filter-out hog_keyboard_demo.h,$^) -o $@

hog_keyboard_demo: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} battery_service_server.o device_information_service_server.o hids_device.o btstack_ring_buffer.o hog_keyboard_demo.o
	${CC} $^ ${LDFLAGS} -o $@


hog_mouse_demo.o: hog_mouse_demo.h hog_mouse_demo.c
	${CC} ${CFLAGS} -c $(filter-out hog_mouse_demo.h,$^) -o $@

hog_mouse_demo: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} battery_service_server.o device_information_service_server.o hids_device.o hog_mouse_demo.o
	${CC} $^ ${LDFLAGS} -o $@


hog_boot_host_demo: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} hog_boot_host_demo.o
	${CC} $^ ${LDFLAGS} -o $@



hog_host_demo: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} btstack_hid_parser.o btstack_hid.o hog_host_demo.o
	${CC} $^ ${LDFLAGS} -o $@


sm_pairing_peripheral.o: sm_pairing_peripheral.h sm_pairing_peripheral.c
	${CC} ${CFLAGS} -c $(filter-out sm_pairing_peripheral.h,$^) -o $@

sm_pairing_peripheral: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} sm_pairing_peripheral.o
	${CC} $^ ${LDFLAGS} -o $@


sm_pairing_central.o: sm_pairing_central.h sm_pairing_central.c
	${CC} ${CFLAGS} -c $(filter-out sm_pairing_central.h,$^) -o $@

sm_pairing_central: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} sm_pairing_central.o
	${CC} $^ ${LDFLAGS} -o $@


gatt_counter.o: gatt_counter.h gatt_counter.c
	${CC} ${CFLAGS} -c $(filter-out gatt_counter.h,$^) -o $@

gatt_counter: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${CLASSIC_OBJ} battery_service_server.o gatt_counter.o
	${CC} $^ ${LDFLAGS} -o $@


gatt_streamer_server.o: gatt_streamer_server.h gatt_streamer_server.c
	${CC} ${CFLAGS} -c $(filter-out gatt_streamer_server.h,$^) -o $@

gatt_streamer_server: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${CLASSIC_OBJ} gatt_streamer_server.o
	${CC} $^ ${LDFLAGS} -o $@


le_streamer_client: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} le_streamer_client.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

le_mitm: ${CORE_OBJ} ${COMMON_OBJ} le_mitm.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gatt_heart_rate_client: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} gatt_heart_rate_client.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@


spp_and_gatt_counter.o: spp_and_gatt_counter.h spp_and_gatt_counter.c
	${CC} ${CFLAGS} -c $(filter-out spp_and_gatt_counter.h,$^) -o $@

spp_and_gatt_counter: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} spp_and_gatt_counter.o
	${CC} $^ ${LDFLAGS} -o $@


spp_and_gatt_streamer.o: spp_and_gatt_streamer.h spp_and_gatt_streamer.c
	${CC} ${CFLAGS} -c $(filter-out spp_and_gatt_streamer.h,$^) -o $@

spp_and_gatt_streamer: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} spp_and_gatt_streamer.o
	${CC} $^ ${LDFLAGS} -o $@


spp_streamer: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} spp_streamer.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

spp_flowcontrol: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} spp_flowcontrol.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

spp_streamer_client: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} spp_streamer_client.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_dedicated_bonding: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} gap_dedicated_bonding.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_inquiry: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} gap_inquiry.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_link_keys: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} gap_link_keys.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

panu_demo: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${PAN_OBJ} panu_demo.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

pan_lwip_http_server: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${PAN_OBJ} ${LWIP_SRC} btstack_ring_buffer.o bnep_lwip.o pan_lwip_http_server.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@


gatt_browser.o: gatt_browser.h gatt_browser.c
	${CC} ${CFLAGS} -c $(filter-out gatt_browser.h,$^) -o $@

gatt_browser: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${GATT_SERVER_OBJ} gatt_browser.o
	${CC} $^ ${LDFLAGS} -o $@


gatt_battery_query.o: gatt_battery_query.h gatt_battery_query.c
	${CC} ${CFLAGS} -c $(filter-out gatt_battery_query.h,$^) -o $@

gatt_battery_query: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${GATT_SERVER_OBJ} gatt_battery_query.o
	${CC} $^ ${LDFLAGS} -o $@


gatt_device_information_query.o: gatt_device_information_query.h gatt_device_information_query.c
	${CC} ${CFLAGS} -c $(filter-out gatt_device_information_query.h,$^) -o $@

gatt_device_information_query: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${GATT_SERVER_OBJ} gatt_device_information_query.o
	${CC} $^ ${LDFLAGS} -o $@


ancs_client_demo.o: ancs_client_demo.h ancs_client_demo.c
	${CC} ${CFLAGS} -c $(filter-out ancs_client_demo.h,$^) -o $@

ancs_client_demo: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} ancs_client.o ancs_client_demo.o
	${CC} $^ ${LDFLAGS} -o $@


led_counter: ${CORE_OBJ} ${COMMON_OBJ} led_counter.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_le_advertisements: ${CORE_OBJ} ${COMMON_OBJ}  gap_le_advertisements.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hsp_hs_demo: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${CVSD_PLC_OBJ} wav_util.o sco_demo_util.o btstack_ring_buffer.o hsp_hs.o hsp_hs_demo.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hsp_ag_demo: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${CVSD_PLC_OBJ} wav_util.o sco_demo_util.o btstack_ring_buffer.o hsp_ag.o hsp_ag_demo.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_ag_demo: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${CVSD_PLC_OBJ} wav_util.o sco_demo_util.o btstack_ring_buffer.o hfp.o hfp_gsm_model.o hfp_ag.o hfp_ag_demo.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_hf_demo: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${CVSD_PLC_OBJ} wav_util.o sco_demo_util.o btstack_ring_buffer.o hfp.o hfp_hf.o hfp_hf_demo.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hid_host_demo: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} btstack_hid_parser.o hid_host.o hid_host_demo.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hid_keyboard_demo: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} btstack_ring_buffer.o hid_device.o btstack_hid_parser.o hid_keyboard_demo.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hid_mouse_demo: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} btstack_ring_buffer.o hid_device.o btstack_hid_parser.o hid_mouse_demo.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

a2dp_source_demo: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${SBC_ENCODER_OBJ} ${AVDTP_OBJ} ${HXCMOD_PLAYER_OBJ} avrcp.o avrcp_controller.o avrcp_target.o a2dp_source_demo.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

a2dp_sink_demo: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${SBC_DECODER_OBJ} ${AVDTP_OBJ} avrcp.o avrcp_controller.o avrcp_target.o avrcp_cover_art_client.o goep_client.o obex_parser.o obex_message_builder.o btstack_resample.o btstack_sample_rate_compensation.o a2dp_sink_demo.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

avrcp_browsing_client: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${AVRCP_OBJ} ${AVDTP_OBJ} avrcp_browsing_client.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

dut_mode_classic: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} dut_mode_classic.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

le_credit_based_flow_control_mode_client: ${CORE_OBJ} ${COMMON_OBJ} le_credit_based_flow_control_mode_client.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@


le_credit_based_flow_control_mode_server.o: le_credit_based_flow_control_mode_server.h le_credit_based_flow_control_mode_server.c
	${CC} ${CFLAGS} -c $(filter-out le_credit_based_flow_control_mode_server.h,$^) -o $@

le_credit_based_flow_control_mode_server: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} le_credit_based_flow_control_mode_server.o
	${CC} $^ ${LDFLAGS} -o $@


mod_player: ${CORE_OBJ} ${COMMON_OBJ} ${HXCMOD_PLAYER_OBJ} btstack_audio.o mod_player.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sine_player: ${CORE_OBJ} ${COMMON_OBJ} btstack_audio.o sine_player.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

audio_duplex: ${CORE_OBJ} ${COMMON_OBJ} btstack_audio.o btstack_ring_buffer.o audio_duplex.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@


nordic_spp_le_counter.o: nordic_spp_le_counter.h nordic_spp_le_counter.c
	${CC} ${CFLAGS} -c $(filter-out nordic_spp_le_counter.h,$^) -o $@

nordic_spp_le_counter: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} nordic_spp_service_server.o nordic_spp_le_counter.o
	${CC} $^ ${LDFLAGS} -o $@


nordic_spp_le_streamer.o: nordic_spp_le_streamer.h nordic_spp_le_streamer.c
	${CC} ${CFLAGS} -c $(filter-out nordic_spp_le_streamer.h,$^) -o $@

nordic_spp_le_streamer: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} nordic_spp_service_server.o nordic_spp_le_streamer.o
	${CC} $^ ${LDFLAGS} -o $@


ublox_spp_le_counter.o: ublox_spp_le_counter.h ublox_spp_le_counter.c
	${CC} ${CFLAGS} -c $(filter-out ublox_spp_le_counter.h,$^) -o $@

ublox_spp_le_counter: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} device_information_service_server.o ublox_spp_service_server.o ublox_spp_le_counter.o
	${CC} $^ ${LDFLAGS} -o $@


mesh_node_demo.o: mesh_node_demo.h mesh_node_demo.c
	${CC} ${CFLAGS} -c $(filter-out mesh_node_demo.h,$^) -o $@

mesh_node_demo:${CORE_OBJ} ${COMMON_OBJ} ${MESH_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} mesh_node_demo.o
	${CC} $^ ${LDFLAGS} -o $@

clean:
	rm -f ${EXAMPLES} ${EXAMPLES_GATT_H_FILES}
	rm -f  *.o *.out *.hex *.exe *.wav *.sbc 
	rm -rf *.dSYM
	rm -rf ${BTSTACK_ROOT}/src/*.o
	rm -rf ${BTSTACK_ROOT}/src/ble/*.o
	rm -rf ${BTSTACK_ROOT}/src/ble/gatt-service/*.o
	rm -rf ${BTSTACK_ROOT}/src/classic/*.o
	rm -rf ${BTSTACK_ROOT}/example/*.o
