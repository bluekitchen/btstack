include ../common.make

# Makefile for libusb based PTS tests
include ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/Makefile.inc

CORE += \
	btstack_memory.c            \
	btstack_linked_list.c	    \
	btstack_memory_pool.c       \
	btstack_run_loop.c		    \
	btstack_util.c	            \
	main.c						\
	btstack_stdin_posix.c		\
	btstack_tlv.c				\
	btstack_link_key_db_tlv.c	\
	btstack_tlv_posix.c			\
	le_device_db_tlv.c			\
	hci_dump_posix_fs.c

COMMON += \
	ad_parser.c					\
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
	sdp_client.c				\
	sdp_util.c	                \
	btstack_chipset_zephyr.c    \
	btstack_chipset_realtek.c   \
	btstack_signal.c            \
	wav_util.c					\
	sm.c						\
	btstack_crypto.c            \
	uECC.c						\
	hci_event.c

CFLAGS += -g -Wall -Wmissing-prototypes -Wshadow -Wunused-variable -Wunused-parameter -Werror 
# btstack_chipset_realtek.c uses parameter names that shadow a file-scope state
# object. Keep this legacy application target buildable with -Werror.
CFLAGS += -Wno-shadow
#  -Wstrict-prototypes disabled because of portaudio headers
CFLAGS += -I${BTSTACK_ROOT}/chipset/zephyr
CFLAGS += -I${BTSTACK_ROOT}/chipset/realtek
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/src/classic
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include -D OI_DEBUG
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/tinydir
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/yxml
CFLAGS += -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded
CFLAGS += -I..

VPATH += ${BTSTACK_ROOT}/chipset/zephyr
VPATH += ${BTSTACK_ROOT}/chipset/realtek
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

AVRCP += \
	avrcp.c

AVRCP_TESTS = avrcp_controller_test

all: test

# Link sources directly: VPATH otherwise permits make to reuse incompatible
# object files left in port/libusb by another build configuration.
avrcp_controller_test: ${CORE} ${COMMON} ${AVRCP} avrcp_controller.c

test: ${AVRCP_TESTS}

coverage:

clean: clean-common
	rm -rf *.pyc *.o $(AVDTP_TESTS) *.dSYM *_test *.wav *.sbc 
	rm -f *.gcno *.gcda
