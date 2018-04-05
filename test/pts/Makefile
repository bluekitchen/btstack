# Makefile for libusb based PTS tests
BTSTACK_ROOT = ../..


CLASSIC += \
	btstack_link_key_db_memory.c   \
	sdp_util.c	                \
	spp_server.c  				\
	rfcomm.c	                \
	bnep.c	                    \
	sdp_server.c			    \
	device_id_server.c          \


CORE += \
	btstack_memory.c            \
	btstack_linked_list.c	    \
	btstack_memory_pool.c       \
	btstack_run_loop.c		    \
	btstack_util.c 	            \
	main.c 						\
	btstack_stdin_posix.c       \
	btstack_tlv.c 		\
	btstack_tlv_posix.c 		\
	
COMMON += \
	hci.c			            \
	ad_parser.c                 \
	hci_cmd.c		            \
	hci_dump.c		            \
	l2cap.c			            \
	l2cap_signaling.c	        \
	rfcomm.c                    \
	hci_transport_h2_libusb.c 	\
	btstack_run_loop_posix.c 	\
	btstack_link_key_db_fs.c 	\
	le_device_db_fs.c 			\
	wav_util.c 					\
	sdp_util.c	                \
	sdp_server.c		        \
	sdp_client.c 				\
	sdp_client_rfcomm.c 		\
	sm.c 						\
	uECC.c 					    \
	spp_server.c 				\

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

# include ${BTSTACK_ROOT}/example/Makefile.inc

HXCMOD_PLAYER = \
	${BTSTACK_ROOT}/3rd-party/hxcmod-player/hxcmod.c 						\
	${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods/nao-deceased_by_disease.c 	\

include ${BTSTACK_ROOT}/example/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/Makefile.inc

CFLAGS += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wunused-variable -Wunused-parameter
# -Werror 
CFLAGS += -D ENABLE_LE_DATA_CHANNELS
CFLAGS += -D ENABLE_LE_SECURE_CONNECTIONS
CFLAGS += -D ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS
CFLAGS += -D ENABLE_TESTING_SUPPORT
CFLAGS += -I..
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/src/classic
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include -D OI_DEBUG
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/eCC
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/tinydir
CFLAGS += -I${BTSTACK_ROOT}/platform/posix 
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded 
CFLAGS += -I${BTSTACK_ROOT}/port/libusb 
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/tinydir

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src/ble/mesh
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/libusb
VPATH += ${BTSTACK_ROOT}/port/libusb
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce 
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce 
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc

# use pkg-config for libusb
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)
# use pkg-config for portaudio
CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO 
LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)

CORE_OBJ    = $(CORE:.c=.o)
COMMON_OBJ  = $(COMMON:.c=.o) 
ATT_OBJ     = $(ATT:.c=.o)
SM_OBJ = $(SM:.c=.o) $(MICROECC:.c=.o)
GATT_CLIENT_OBJ = $(GATT_CLIENT:.c=.o)
GATT_SERVER_OBJ = $(GATT_SERVER:.c=.o)
CLASSIC_OBJ  = $(CLASSIC:.c=.o) 
SBC_DECODER_OBJ  = $(SBC_DECODER:.c=.o) 
SBC_ENCODER_OBJ  = $(SBC_ENCODER:.c=.o)
AVDTP_OBJ  = $(AVDTP:.c=.o)
HXCMOD_PLAYER_OBJ = ${HXCMOD_PLAYER:.c=.o}

EXAMPLES  = iopt ble_peripheral_test ble_central_test l2cap_test classic_test bnep_test hsp_ag_test hsp_hs_test sco_loopback le_data_channel
EXAMPLES += avdtp_source_test avdtp_sink_test le_data_channel avrcp_controller_test sm_test
# avrcp_target_test

all: ${EXAMPLES}

# compile .gatt descriptions
# avrcp_target_test: ${CORE_OBJ} ${COMMON_OBJ} ${SBC_ENCODER_OBJ} ${SM_OBJ} ${AVDTP_OBJ} ${HXCMOD_PLAYER_OBJ} avrcp.o avrcp_target.o avrcp_target_test.c
# 	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

avrcp_controller_test: ${CORE_OBJ} ${COMMON_OBJ} ${SBC_DECODER_OBJ} ${SM_OBJ} ${SBC_ENCODER_OBJ} ${AVDTP_OBJ} sdp_client.o avrcp.o avrcp_media_item_iterator.o avrcp_controller.o avrcp_browsing_controller.o avrcp_controller_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

avdtp_sink_test: ${CORE_OBJ} ${COMMON_OBJ} ${SBC_DECODER_OBJ} ${SM_OBJ} ${SBC_ENCODER_OBJ} ${AVDTP_OBJ} avdtp_sink_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

avdtp_source_test: ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} ${SBC_ENCODER_OBJ} ${AVDTP_OBJ} avdtp_source_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

ble_peripheral_test.h: ble_peripheral_test.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

ble_central_test.h: ble_central_test.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
	
ble_peripheral_test: ble_peripheral_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} ble_peripheral_test.o
	${CC} $(filter-out ble_peripheral_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

ble_central_test: ble_central_test.h ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} ad_parser.o ble_central_test.c
	${CC} $(filter-out ble_central_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

le_data_channel: ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} ${ATT_OBJ} ad_parser.o le_data_channel.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hsp_ag_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} hsp_ag.o hsp_ag_test.c 
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hsp_hs_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} hsp_hs.o hsp_hs_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

l2cap_test: ${CORE_OBJ} ${COMMON_OBJ} l2cap_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

classic_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} classic_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

bnep_test: ${CORE_OBJ} ${COMMON_OBJ} bnep.o pan.o bnep_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sco_loopback: ${CORE_OBJ} ${COMMON_OBJ} sco_loopback.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

iopt: ${CORE_OBJ} ${COMMON_OBJ} pan.o hsp_ag.o hsp_hs.o hfp_ag.o hfp_hf.o hfp_gsm_model.o iopt.c hfp.o ${SDP_CLIENT}
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sm_test: sm_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ}  ${SM_OBJ} sm_test.o
	${CC} $(filter-out sm_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

myclean:
	rm -rf *.pyc *.o $(AVDTP_TESTS) *.dSYM *_test *.wav *.sbc ${BTSTACK_ROOT}/port/libusb/*.o ${BTSTACK_ROOT}/src/*.o ${BTSTACK_ROOT}/src/classic/*.o ${BTSTACK_ROOT}/src/ble/*.o

clean: myclean
