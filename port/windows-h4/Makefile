# Makefile for windows-h4 examples
BTSTACK_ROOT = ../..

CORE += main.c stdin_support.c

COMMON  += \
			btstack_run_loop_windows.c   \
			hci_transport_h4.c           \
			btstack_uart_block_windows.c \
			le_device_db_fs.c            \
			btstack_link_key_db_fs.c     \

include ${BTSTACK_ROOT}/example/Makefile.inc

# CC = gcc-fsf-4.9
CFLAGS  += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Werror
# CFLAGS += -Werror

CFLAGS += -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/platform/embedded

VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/libusb

# assume portaudio is installed in /usr/local
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio

all: ${EXAMPLES}
