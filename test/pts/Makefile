# Makefile for libusb based PTS tests
BTSTACK_ROOT = ../..

CORE += \
	btstack_memory.c            \
	btstack_linked_list.c	    \
	btstack_memory_pool.c       \
	btstack_run_loop.c		    \
	btstack_util.c 	            \
	btstack_audio.c             \
	btstack_audio_portaudio.c   \
	main.c 						\
	btstack_stdin_posix.c       \
	btstack_signal.c            \
	btstack_tlv.c 		\
	btstack_tlv_posix.c 		\
	rijndael.c \

COMMON += \
	hci.c			            \
	device_id_server.c          \
	ad_parser.c                 \
	hci_cmd.c		            \
	hci_dump.c		            \
	hci_dump_posix_fs.c         \
	l2cap.c			            \
	l2cap_signaling.c	        \
	rfcomm.c                    \
	hci_transport_h2_libusb.c 	\
	btstack_run_loop_posix.c 	\
	btstack_link_key_db_tlv.c 	\
	le_device_db_tlv.c 			\
	wav_util.c 					\
	sdp_util.c	                \
	sdp_server.c		        \
	sdp_client.c 				\
	sdp_client_rfcomm.c 		\
	sm.c 						\
	uECC.c 					    \
	spp_server.c 				\
	btstack_chipset_zephyr.c    \
	btstack_chipset_realtek.c   \

ATT	+= \
	att_dispatch.c       	    \

GATT_SERVER += \
	att_db.c 				 	    \
	att_server.c        	    \

GATT_CLIENT += \
	gatt_client.c        	    \

SM += \
	sm.c 				 	    \

SBC_DECODER += \
	${BTSTACK_ROOT}/src/classic/btstack_sbc_plc.c \
	${BTSTACK_ROOT}/src/classic/btstack_sbc_decoder_bluedroid.c \

SBC_ENCODER += \
	${BTSTACK_ROOT}/src/classic/btstack_sbc_encoder_bluedroid.c \
	${BTSTACK_ROOT}/src/classic/hfp_msbc.c \

AVDTP += \
	avdtp_util.c  		\
	avdtp.c  			\
	avdtp_initiator.c 	\
	avdtp_acceptor.c  	\
	avdtp_source.c 		\
	avdtp_sink.c  		\
	a2dp_source.c 		\
	a2dp_sink.c  		\
	btstack_ring_buffer.c \
	btstack_resample.c \

AVRCP += \
	avrcp_media_item_iterator.c \
	avrcp.c  \
	avrcp_target.c  \
	avrcp_controller.c  \
	avrcp_browsing.c \
	avrcp_browsing_target.c \
	avrcp_browsing_controller.c \

SRC_BLE_GATT_SERVICE_FILES += \
	audio_input_control_service_server.c \
	microphone_control_service_server.c \
	volume_control_service_server.c \
	
# include ${BTSTACK_ROOT}/example/Makefile.inc

HXCMOD_PLAYER = \
	${BTSTACK_ROOT}/3rd-party/hxcmod-player/hxcmod.c 						\
	${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods/nao-deceased_by_disease.c 	\

include ${BTSTACK_ROOT}/example/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/Makefile.inc
include ${BTSTACK_ROOT}/src/ble/gatt-service/Makefile.inc

CFLAGS += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wunused-variable -Wunused-parameter -Wswitch-default
CFLAGS += -D ENABLE_TESTING_SUPPORT
CFLAGS += -D ENABLE_LE_SIGNED_WRITE
CFLAGS += -fsanitize=address -fno-omit-frame-pointer

CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/example
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include -D OI_DEBUG
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/eCC
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/tinydir
CFLAGS += -I${BTSTACK_ROOT}/chipset/realtek
CFLAGS += -I${BTSTACK_ROOT}/chipset/zephyr
CFLAGS += -I${BTSTACK_ROOT}/platform/posix 
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded 
CFLAGS += -I${BTSTACK_ROOT}/port/libusb 

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/src/ble/le_audio
VPATH += ${BTSTACK_ROOT}/src/ble/mesh
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/example
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/libusb
VPATH += ${BTSTACK_ROOT}/port/libusb
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce 
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce 
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/chipset/realtek
VPATH += ${BTSTACK_ROOT}/chipset/zephyr

# use pkg-config for libusb
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)
# use pkg-config for portaudio
CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO 
LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# use pkg-config for fdk-aac
CFLAGS  += $(shell pkg-config fdk-aac --cflags) -DHAVE_AAC_FDK
LDFLAGS += $(shell pkg-config fdk-aac --libs)

CORE_OBJ    = $(CORE:.c=.o)
COMMON_OBJ  = $(COMMON:.c=.o) 
ATT_OBJ     = $(ATT:.c=.o)
SM_OBJ = $(SM:.c=.o) $(MICROECC:.c=.o)
GATT_CLIENT_OBJ = $(GATT_CLIENT:.c=.o)
GATT_SERVER_OBJ = $(GATT_SERVER:.c=.o)
SBC_DECODER_OBJ  = $(SBC_DECODER:.c=.o) 
SBC_ENCODER_OBJ  = $(SBC_ENCODER:.c=.o)
AVDTP_OBJ  = $(AVDTP:.c=.o)
AVRCP_OBJ  = $(AVRCP:.c=.o)
HXCMOD_PLAYER_OBJ = ${HXCMOD_PLAYER:.c=.o}
SRC_BLE_GATT_SERVICE_FILES_OBJ =  ${SRC_BLE_GATT_SERVICE_FILES:.c=.o}

EXAMPLES  = iopt gatt_server_test gatt_client_test l2cap_test classic_test bnep_test l2cap_cbm_ecbm
EXAMPLES += hsp_ag_test hsp_hs_test sco_loopback hfp_ag_test hfp_hf_test
EXAMPLES += avdtp_source_test avdtp_sink_test avrcp_test gatt_profiles hrp_col_test hrp_server_test
EXAMPLES += hog_device_test hog_host_test hid_device_test hid_host_test 
EXAMPLES += csc_server_test csc_client_test cycling_power_server_test
EXAMPLES += gap_peripheral_test gap_central_test
EXAMPLES += microphone_control_service_client_test

all: ${EXAMPLES}

cycling_power_server_test.h: cycling_power_server_test.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

cycling_power_server_test: cycling_power_server_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} ${SRC_BLE_GATT_SERVICE_FILES_OBJ} gatt_client.o cycling_power_server_test.c
	${CC} $(filter-out cycling_power_server_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

csc_server_test.h: csc_server_test.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

csc_server_test: csc_server_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} ${SRC_BLE_GATT_SERVICE_FILES_OBJ} gatt_client.o csc_server_test.c
	${CC} $(filter-out csc_server_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

csc_client_test: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_OBJ} csc_client_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hid_host_test: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} btstack_hid_parser.o hid_host.o hid_host_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hid_device_test: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} btstack_hid_parser.o btstack_ring_buffer.o hid_device.o hid_device_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hog_device_test: hog_device_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} ${SRC_BLE_GATT_SERVICE_FILES_OBJ} gatt_client.o btstack_ring_buffer.o hog_device_test.c
	${CC} $(filter-out hog_device_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

hog_host_test: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_OBJ} btstack_hid_parser.o btstack_hid.o hog_host_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hrp_server_test.h: hrp_server_test.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
	
hrp_server_test: hrp_server_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} heart_rate_service_server.o hrp_server_test.c
	${CC} $(filter-out hrp_server_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

hrp_col_test: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_OBJ} hrp_col_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

avrcp_test: ${CORE_OBJ} ${COMMON_OBJ} ${SBC_DECODER_OBJ} ${SM_OBJ} ${SBC_ENCODER_OBJ} ${AVRCP_OBJ} ${AVDTP_OBJ} sdp_client.o btstack_stdin_pts.o avrcp_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

avdtp_sink_test: ${CORE_OBJ} ${COMMON_OBJ} ${SBC_DECODER_OBJ} ${SM_OBJ} ${SBC_ENCODER_OBJ} ${AVDTP_OBJ} avdtp_sink_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

avdtp_source_test: ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} ${SBC_ENCODER_OBJ} ${AVDTP_OBJ} avdtp_source_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gatt_server_test.h: gatt_server_test.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

gatt_client_test.h: gatt_client_test.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
	
gatt_server_test: gatt_server_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} att_db_util.o gatt_sdp.o gatt_server_test.o
	${CC} $(filter-out gatt_server_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

gatt_client_test: gatt_client_test.h ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} ad_parser.o gatt_client_test.c
	${CC} $(filter-out gatt_client_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

gap_peripheral_test.h: gap_peripheral_test.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

gap_central_test.h: gap_central_test.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
	
gap_peripheral_test: gap_peripheral_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} att_db_util.o gatt_sdp.o gap_peripheral_test.o
	${CC} $(filter-out gap_peripheral_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

gap_central_test: gap_central_test.h ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} att_db_util.o ad_parser.o gap_central_test.c
	${CC} $(filter-out gap_central_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

l2cap_cbm_ecbm: ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} ${ATT_OBJ} ad_parser.o l2cap_cbm_ecbm.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hsp_ag_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} hsp_ag.o hsp_ag_test.c 
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hsp_hs_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} hsp_hs.o hsp_hs_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_ag_test: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${CVSD_PLC_OBJ} wav_util.o sco_demo_util.o btstack_ring_buffer.o hfp.o hfp_gsm_model.o hfp_ag.o hfp_ag_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_hf_test: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${CVSD_PLC_OBJ} wav_util.o sco_demo_util.o btstack_ring_buffer.o hfp.o hfp_hf.o hfp_hf_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

l2cap_test: ${CORE_OBJ} ${COMMON_OBJ} l2cap_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

classic_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} classic_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

bnep_test: ${CORE_OBJ} ${COMMON_OBJ} bnep.o pan.o bnep_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sco_loopback: ${CORE_OBJ} ${COMMON_OBJ} sco_loopback.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

iopt: ${CORE_OBJ} ${COMMON_OBJ} pan.o hsp_ag.o hsp_hs.o hfp_ag.o hfp_hf.o hfp_gsm_model.o iopt.c hfp.o a2dp_sink.o a2dp_source.o ${AVDTP_OBJ} avrcp_controller.o avrcp_target.o avrcp.o ${SDP_CLIENT}
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gatt_profiles.h: gatt_profiles.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

gatt_profiles: gatt_profiles.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} ${SRC_BLE_GATT_SERVICE_FILES_OBJ} gatt_client.o gatt_sdp.o gatt_profiles.o
	${CC} $(filter-out gatt_profiles.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

microphone_control_service_client_test.h: microphone_control_service_client_test.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

microphone_control_service_client_test: microphone_control_service_client_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} microphone_control_service_client_test.c
	${CC} $(filter-out microphone_control_service_client_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

myclean:
	rm -rf *.pyc *.o $(AVDTP_TESTS) *.dSYM *_test *.wav *.sbc ${BTSTACK_ROOT}/port/libusb/*.o ${BTSTACK_ROOT}/src/*.o ${BTSTACK_ROOT}/src/classic/*.o ${BTSTACK_ROOT}/src/ble/*.o 
	rm -f csc_server_test.h
	rm -f cycling_power_server_test.h
	rm -f gap_central_test.h
	rm -f gap_peripheral_test.h
	rm -f gatt_client_test.h
	rm -f gatt_profiles.h
	rm -f gatt_server_test.h
	rm -f hog_device_test.h
	rm -f hrp_server_test.h
	rm -f microphone_control_service_client_test.h

clean: myclean

test:
	echo "No unit tests in PTS"
coverage:
	echo "No coverage tests in PTS"
