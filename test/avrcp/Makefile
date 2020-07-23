# Makefile for libusb based PTS tests
BTSTACK_ROOT = ../..

include ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/Makefile.inc

CORE += \
	btstack_memory.c            \
	btstack_linked_list.c	    \
	btstack_memory_pool.c       \
	btstack_run_loop.c		    \
	btstack_util.c 	            \
	main.c 	\
	btstack_stdin_posix.c \
	btstack_tlv.c \
	btstack_link_key_db_tlv.c \
	btstack_tlv_posix.c \
	le_device_db_tlv.c \

COMMON += \
	ad_parser.c 				\
	btstack_audio.c             \
	btstack_audio_portaudio.c   \
	btstack_link_key_db_fs.c    \
	btstack_run_loop_posix.c    \
	hci.c			            \
	hci_cmd.c		            \
	hci_dump.c		            \
	hci_transport_h2_libusb.c   \
	l2cap.c			            \
	l2cap_signaling.c	        \
	sdp_server.c			    \
	sdp_client.c 				\
	sdp_util.c	                \
	btstack_chipset_zephyr.c    \
	wav_util.c 					\
	sm.c 						\
	btstack_crypto.c            \
	uECC.c                      \

CFLAGS += -g -Wall -Wmissing-prototypes -Wshadow -Wunused-variable -Wunused-parameter -Werror 
#  -Wstrict-prototypes disabled because of portaudio headers
CFLAGS += -I${BTSTACK_ROOT}/chipset/zephyr
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/src/classic
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include -D OI_DEBUG
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/tinydir
CFLAGS += -I${BTSTACK_ROOT}/platform/posix 
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded 
CFLAGS += -I..

VPATH += ${BTSTACK_ROOT}/chipset/zephyr
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/src/ble
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

SBC_DECODER += \
	${BTSTACK_ROOT}/src/classic/btstack_sbc_plc.c \
	${BTSTACK_ROOT}/src/classic/btstack_sbc_decoder_bluedroid.c \

SBC_ENCODER += \
	${BTSTACK_ROOT}/src/classic/btstack_sbc_encoder_bluedroid.c \
	${BTSTACK_ROOT}/src/classic/hfp_msbc.c \

AVRCP += \
	avrcp.c 			\
	
AVRCP_TESTS = avrcp_controller_test

CORE_OBJ    = $(CORE:.c=.o)
COMMON_OBJ  = $(COMMON:.c=.o) 
SBC_DECODER_OBJ  = $(SBC_DECODER:.c=.o) 
SBC_ENCODER_OBJ  = $(SBC_ENCODER:.c=.o)
AVRCP_OBJ  = $(AVRCP:.c=.o)

all: ${AVRCP_TESTS}

avrcp_controller_test: ${CORE_OBJ} ${COMMON_OBJ} ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${AVRCP_OBJ} avrcp_controller.o avrcp_controller_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all

clean:
	rm -rf *.pyc *.o $(AVDTP_TESTS) *.dSYM *_test *.wav *.sbc 
	rm -f *.gcno *.gcda
