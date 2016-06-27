# Makefile for libusb based examples
BTSTACK_ROOT = ../..

CORE += \
	bluetooth_init_cc2564B_1.4_BT_Spec_4.1.c \
	btstack_chipset_cc256x.c \
	btstack_chipset_csr.c \
	btstack_chipset_em9301.c \
	btstack_chipset_stlc2500d.c \
	btstack_chipset_tc3566x.c \
	btstack_link_key_db_fs.c \
	btstack_run_loop_posix.c \
	btstack_uart_block_posix.c \
	btstack_slip.c \
	hci_transport_h5.c \
	le_device_db_fs.c \
	main.c \
	stdin_support.c \
#	btstack_chipset_bcm.c \

# TI-WL183x requires TIInit_11.8.32.c

# examples
include ${BTSTACK_ROOT}/example/Makefile.inc

# fetch and convert TI init scripts
include ${BTSTACK_ROOT}/chipset/cc256x/Makefile.inc

CFLAGS  += -g -Wall -Werror \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/chipset/bcm \
	-I$(BTSTACK_ROOT)/chipset/cc256x \
	-I$(BTSTACK_ROOT)/chipset/csr \
	-I$(BTSTACK_ROOT)/chipset/em9301 \
	-I$(BTSTACK_ROOT)/chipset/stlc2500d \
	-I$(BTSTACK_ROOT)/chipset/tc3566x \

VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded

VPATH += ${BTSTACK_ROOT}/chipset/bcm	
VPATH += ${BTSTACK_ROOT}/chipset/cc256x
VPATH += ${BTSTACK_ROOT}/chipset/csr
VPATH += ${BTSTACK_ROOT}/chipset/em9301
VPATH += ${BTSTACK_ROOT}/chipset/stlc2500d
VPATH += ${BTSTACK_ROOT}/chipset/tc3566x

ifeq ($(OS),Windows_NT)
LDFLAGS += -lws2_32
endif

# Command Line examples require porting to win32, so only build on other unix-ish hosts
ifneq ($(OS),Windows_NT)
EXAMPLES += ${EXAMPLES_CLI}
endif

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

all: ${EXAMPLES}
