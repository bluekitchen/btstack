# Makefile for libusb based examples
BTSTACK_ROOT ?= ../..

CORE += \
	bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c \
	btstack_chipset_bcm.c \
	btstack_chipset_cc256x.c \
	btstack_chipset_csr.c \
	btstack_chipset_em9301.c \
	btstack_chipset_stlc2500d.c \
	btstack_chipset_tc3566x.c \
	btstack_link_key_db_tlv.c \
	btstack_run_loop_posix.c \
	btstack_audio.c \
    btstack_audio_portaudio.c \
	btstack_tlv_posix.c \
	btstack_uart_posix.c \
	hci_dump_posix_fs.c \
	hci_transport_h4.c \
	le_device_db_tlv.c \
	rijndael.c \
	main.c \
	btstack_stdin_posix.c \
	wav_util.c 					\
	0000000_META_hci_patches_v7.c \
	# bluetooth_init_cc2564_2.14.c \
	# bluetooth_init_cc2564C_1.4.c \
# TI-WL183x requires TIInit_11.8.32.c

# examples
include ${BTSTACK_ROOT}/example/Makefile.inc

# fetch and convert TI init scripts
include ${BTSTACK_ROOT}/chipset/cc256x/Makefile.inc

# fetch Broadcom init scripts
include ${BTSTACK_ROOT}/chipset/bcm/Makefile.inc

CFLAGS  += -g -Wall -Werror \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/chipset/bcm \
	-I$(BTSTACK_ROOT)/chipset/cc256x \
	-I$(BTSTACK_ROOT)/chipset/csr \
	-I$(BTSTACK_ROOT)/chipset/em9301 \
	-I$(BTSTACK_ROOT)/chipset/stlc2500d \
	-I$(BTSTACK_ROOT)/chipset/tc3566x \
    -I${BTSTACK_ROOT}/3rd-party/rijndael \
    -I${BTSTACK_ROOT}/3rd-party/tinydir

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded

VPATH += ${BTSTACK_ROOT}/chipset/bcm	
VPATH += ${BTSTACK_ROOT}/chipset/cc256x
VPATH += ${BTSTACK_ROOT}/chipset/csr
VPATH += ${BTSTACK_ROOT}/chipset/em9301
VPATH += ${BTSTACK_ROOT}/chipset/stlc2500d
VPATH += ${BTSTACK_ROOT}/chipset/tc3566x

EXAMPLES = ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY} ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE}
EXAMPLES += pan_lwip_http_server

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

all: BCM43430A1.hcd ${EXAMPLES}

