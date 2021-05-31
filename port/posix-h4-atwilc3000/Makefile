# Makefile for libusb based examples
BTSTACK_ROOT ?= ../..

CORE += \
	btstack_chipset_atwilc3000.c \
	btstack_run_loop_posix.c \
	btstack_tlv_posix.c \
	btstack_uart_posix.c \
	hci_dump_posix_fs.c \
	hci_transport_h4.c \
	le_device_db_tlv.c \
	main.c \
	wav_util.c 					\
	btstack_stdin_posix.c \

# examples
CLASSIC =
include ${BTSTACK_ROOT}/example/Makefile.inc

# firmware
include ${BTSTACK_ROOT}/chipset/atwilc3000/Makefile.inc

all: wilc3000_ble_firmware.h

CFLAGS  += -g -Wall -Werror \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/chipset/atwilc3000 \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I${BTSTACK_ROOT}/3rd-party/tinydir

VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/chipset/atwilc3000

EXAMPLES = ${EXAMPLES_GENERAL} ${EXAMPLES_LE_ONLY}

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

all: ${EXAMPLES}
