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
	

COMMON += \
	ad_parser.c 				\
	btstack_link_key_db_fs.c    \
	btstack_run_loop_posix.c    \
	hci.c			            \
	hci_cmd.c		            \
	hci_dump.c		            \
	hci_transport_h2_libusb.c   \
	l2cap.c			            \
	l2cap_signaling.c	        \
	le_device_db_fs.c           \
	sdp_server.c			    \
	sdp_util.c	                \
	wav_util.c 					\
	sdp_server.c			    \
	sdp_client.c 				\

CFLAGS += -g -Wall  -Wenum-compare -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wunused-variable -Wunused-parameter
CFLAGS += -I.
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/src/classic
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include -D OI_DEBUG
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/hxcmod-player
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/tinydir
CFLAGS += -I${BTSTACK_ROOT}/platform/posix 
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded 
CFLAGS += -I${BTSTACK_ROOT}/port/libusb 

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/libusb
VPATH += ${BTSTACK_ROOT}/port/libusb
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce 
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce 
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods

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

HXCMOD_PLAYER = \
	${BTSTACK_ROOT}/3rd-party/hxcmod-player/hxcmod.c 						\
	${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods/nao-deceased_by_disease.c 	\
 
AVDTP_TESTS = portaudio_test
#sine_encode_decode_ring_buffer_test sine_encode_decode_test sine_encode_decode_performance_test

CORE_OBJ    = $(CORE:.c=.o)
COMMON_OBJ  = $(COMMON:.c=.o) 
SBC_DECODER_OBJ  = $(SBC_DECODER:.c=.o) 
SBC_ENCODER_OBJ  = $(SBC_ENCODER:.c=.o)
AVDTP_OBJ  = $(AVDTP:.c=.o)
HXCMOD_PLAYER_OBJ = $(HXCMOD_PLAYER:.c=.o)

# For more warnings & errors, use C++
# CC=g++

all: ${AVDTP_TESTS}

portaudio_test: btstack_util.o hci_dump.o wav_util.o btstack_ring_buffer.o portaudio_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@


sine_encode_decode_test: ${CORE_OBJ} ${COMMON_OBJ} ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${AVDTP_OBJ} sine_encode_decode_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sine_encode_decode_ring_buffer_test: ${CORE_OBJ} ${COMMON_OBJ} ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${AVDTP_OBJ} sine_encode_decode_ring_buffer_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sine_encode_decode_performance_test: ${CORE_OBJ} ${COMMON_OBJ} ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${AVDTP_OBJ} sine_encode_decode_performance_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

	
test: all
coverage: all

clean:
	rm -rf *.pyc *.o $(AVDTP_TESTS) *.dSYM *_test *.wav *.sbc ${BTSTACK_ROOT}/port/libusb/*.o
	rm -f *.gcno *.gcda
