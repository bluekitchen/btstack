# Makefile for windows WinUSB based examples
BTSTACK_ROOT = ../..

CORE += main.c btstack_stdin_windows.c

COMMON  += hci_transport_h2_winusb.c btstack_run_loop_windows.c le_device_db_fs.c btstack_link_key_db_fs.c wav_util.c

include ${BTSTACK_ROOT}/example/Makefile.inc

# CC = gcc-fsf-4.9
CFLAGS  += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Werror
# CFLAGS += -Werror

CFLAGS += -I${BTSTACK_ROOT}/platform/windows \
		  -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/platform/embedded

VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/windows

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

LDFLAGS += -lsetupapi -lwinusb

EXAMPLES=le_counter hfp_hf_demo

all: ${EXAMPLES}
