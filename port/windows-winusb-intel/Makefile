# Makefile for windows WinUSB based examples
BTSTACK_ROOT ?= ../..

CORE += main.c btstack_stdin_windows.c

COMMON  += hci_transport_h2_winusb.c btstack_run_loop_windows.c le_device_db_fs.c btstack_link_key_db_fs.c wav_util.c
COMMON  += btstack_chipset_intel_firmware.c

include ${BTSTACK_ROOT}/example/Makefile.inc
include ${BTSTACK_ROOT}/chipset/intel/Makefile.inc

# CC = gcc-fsf-4.9
CFLAGS  += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wpedantic
# CFLAGS += -Werror

CFLAGS += -I${BTSTACK_ROOT}/platform/windows \
		  -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/chipset/intel \
		  -I${BTSTACK_ROOT}/platform/embedded \
		  -I${BTSTACK_ROOT}/3rd-party/tinydir

VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/windows
VPATH += ${BTSTACK_ROOT}/chipset/csr
VPATH += ${BTSTACK_ROOT}/chipset/intel

EXAMPLES += pan_lwip_http_server

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

LDFLAGS += -lsetupapi -lwinusb

clean: clean-intel

all: all-intel ${EXAMPLES}
