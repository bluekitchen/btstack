# Makefile for basic test
BTSTACK_ROOT = ../..

CORE += \
    ad_parser.c                 \
    btstack_audio.c             \
    btstack_audio_portaudio.c   \
    btstack_link_key_db_tlv.c   \
    btstack_linked_list.c       \
    btstack_memory.c            \
    btstack_memory_pool.c       \
    btstack_run_loop.c          \
    btstack_run_loop_posix.c    \
    btstack_stdin_posix.c       \
    btstack_tlv.c               \
    btstack_tlv_posix.c         \
    btstack_util.c              \
    device_id_server.c          \
    hci.c                       \
    hci_cmd.c                   \
    hci_dump.c                  \
    hci_transport_h2_libusb.c   \
    l2cap.c                     \
    l2cap_signaling.c           \
    main.c                      \
    rfcomm.c                    \
    sdp_client.c                \
    sdp_client_rfcomm.c         \
    sdp_server.c                \
    sdp_util.c                  \
    spp_server.c                \

SBC_DECODER += \
    ${BTSTACK_ROOT}/src/classic/btstack_sbc_plc.c \
    ${BTSTACK_ROOT}/src/classic/btstack_sbc_decoder_bluedroid.c \

SBC_ENCODER += \
    ${BTSTACK_ROOT}/src/classic/btstack_sbc_encoder_bluedroid.c \
    ${BTSTACK_ROOT}/src/classic/hfp_msbc.c \

AVDTP += \
    avdtp_util.c        \
    avdtp.c             \
    avdtp_initiator.c   \
    avdtp_acceptor.c    \
    avdtp_source.c      \
    avdtp_sink.c        \
    a2dp_source.c       \
    a2dp_sink.c         \
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

HXCMOD_PLAYER = \
    ${BTSTACK_ROOT}/3rd-party/hxcmod-player/hxcmod.c                        \
    ${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods/nao-deceased_by_disease.c  \

include ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/Makefile.inc

CFLAGS += -g -Wall -Werror -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wunused-variable -Wunused-parameter -Wswitch-default
CFLAGS += -D ENABLE_TESTING_SUPPORT
CFLAGS += -D ENABLE_LE_SIGNED_WRITE
CFLAGS += -fsanitize=address -fno-omit-frame-pointer

CFLAGS += -I.
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/example
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include -D OI_DEBUG
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/eCC
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/tinydir
CFLAGS += -I${BTSTACK_ROOT}/platform/posix 

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble/mesh
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/example
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/libusb
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce 
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce 
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

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
SBC_DECODER_OBJ  = $(SBC_DECODER:.c=.o) 
SBC_ENCODER_OBJ  = $(SBC_ENCODER:.c=.o)
AVDTP_OBJ  = $(AVDTP:.c=.o)
AVRCP_OBJ  = $(AVRCP:.c=.o)
HXCMOD_PLAYER_OBJ = ${HXCMOD_PLAYER:.c=.o}

EXAMPLES  = spp_counter

all: ${EXAMPLES}

spp_counter: spp_counter.o ${CORE_OBJ}
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

myclean:
	rm -rf *.o *.dSYM *.wav *.sbc ${EXAMPLES}

clean: myclean

test: ${EXAMPLES}
	echo "No unit tests here, but examples build"

coverage: all
	echo "No coverage tests here"
