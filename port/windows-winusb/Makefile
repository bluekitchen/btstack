# Makefile for windows WinUSB based examples
BTSTACK_ROOT ?= ../..

CORE += main.c btstack_stdin_windows.c btstack_tlv_windows.c hci_dump_windows_fs.c

COMMON  += hci_transport_h2_winusb.c btstack_run_loop_windows.c le_device_db_tlv.c btstack_link_key_db_tlv.c wav_util.c
COMMON  += rijndael.c

include ${BTSTACK_ROOT}/example/Makefile.inc

# CC = gcc-fsf-4.9
CFLAGS  += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Werror

CFLAGS += -I${BTSTACK_ROOT}/platform/windows \
		  -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/platform/embedded \
          -I${BTSTACK_ROOT}/3rd-party/rijndael \
          -I${BTSTACK_ROOT}/3rd-party/tinydir

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/windows
VPATH += ${BTSTACK_ROOT}/chipset/csr

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

LDFLAGS += -lsetupapi -lwinusb

EXAMPLES = ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY} ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE}
EXAMPLES += csr_set_bd_addr
EXAMPLES += pan_lwip_http_server

csr_set_bd_addr: ${CORE_OBJ} ${COMMON_OBJ} btstack_chipset_csr.o csr_set_bd_addr.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

all: ${EXAMPLES}
