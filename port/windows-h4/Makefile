# Makefile for windows-h4 examples
BTSTACK_ROOT ?= ../..

CORE += main.c btstack_stdin_windows.c btstack_tlv_posix.c hci_dump_posix_fs.c

COMMON  += \
			btstack_run_loop_windows.c   \
			hci_transport_h4.c           \
			btstack_uart_block_windows.c \
			le_device_db_tlv.c           \
			btstack_link_key_db_tlv.c    \
			bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c \
			btstack_chipset_cc256x.c \
			btstack_chipset_csr.c \
			btstack_chipset_em9301.c \
			btstack_chipset_stlc2500d.c \
			btstack_chipset_tc3566x.c \
			btstack_chipset_bcm.c \
            rijndael.c \
            wav_util.c 					\

# examples
include ${BTSTACK_ROOT}/example/Makefile.inc

# fetch and convert TI init scripts
include ${BTSTACK_ROOT}/chipset/cc256x/Makefile.inc

# fetch Broadcom init scripts
# include ${BTSTACK_ROOT}/chipset/bcm/Makefile.inc

# CC = gcc-fsf-4.9
CFLAGS  += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Werror
# CFLAGS += -Werror

CFLAGS += -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/platform/windows \
		  -I${BTSTACK_ROOT}/platform/embedded \
		  -I$(BTSTACK_ROOT)/chipset/bcm \
		  -I$(BTSTACK_ROOT)/chipset/cc256x \
		  -I$(BTSTACK_ROOT)/chipset/csr \
		  -I$(BTSTACK_ROOT)/chipset/em9301 \
		  -I$(BTSTACK_ROOT)/chipset/stlc2500d \
		  -I$(BTSTACK_ROOT)/chipset/tc3566x \
          -I${BTSTACK_ROOT}/3rd-party/rijndael \
		  -I${BTSTACK_ROOT}/3rd-party/tinydir

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

VPATH += ${BTSTACK_ROOT}/platform/windows
VPATH += ${BTSTACK_ROOT}/platform/posix

VPATH += ${BTSTACK_ROOT}/chipset/bcm
VPATH += ${BTSTACK_ROOT}/chipset/cc256x
VPATH += ${BTSTACK_ROOT}/chipset/csr
VPATH += ${BTSTACK_ROOT}/chipset/em9301
VPATH += ${BTSTACK_ROOT}/chipset/stlc2500d
VPATH += ${BTSTACK_ROOT}/chipset/tc3566x

EXAMPLES = ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY} ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE}
EXAMPLES += pan_lwip_http_server

# assume portaudio is installed in /usr/local
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio

all: ${EXAMPLES}
