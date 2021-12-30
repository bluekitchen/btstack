# Makefile for Raspberry Pi
BTSTACK_ROOT ?= ../..

CORE += \
	btstack_chipset_bcm.c \
	btstack_chipset_bcm_download_firmware.c \
	btstack_control_raspi.c \
	btstack_link_key_db_tlv.c \
	btstack_run_loop_posix.c \
	btstack_tlv_posix.c \
	btstack_uart_posix.c \
	btstack_slip.c \
	hci_dump_posix_fs.c \
	hci_transport_h4.c \
	hci_transport_h5.c \
	le_device_db_tlv.c \
	main.c \
	wav_util.c 					\
	btstack_stdin_posix.c \
	btstack_signal.c \
	raspi_get_model.c \
	rijndael.c

# examples
include ${BTSTACK_ROOT}/example/Makefile.inc

# use (cross)compiler for Raspi
CC = arm-linux-gnueabihf-gcc

CFLAGS  += -g -Wall -Werror \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/chipset/bcm \
	-I${BTSTACK_ROOT}/3rd-party/tinydir \
    -I${BTSTACK_ROOT}/3rd-party/rijndael



# add 'real time' lib for clock_gettime,
LDFLAGS += -lrt

# add pthread for ctrl-c signal handler
LDFLAGS += -lpthread

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded

VPATH += ${BTSTACK_ROOT}/chipset/bcm	

EXAMPLES = ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY} ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE}
EXAMPLES += pan_lwip_http_server

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

all: ${EXAMPLES}
