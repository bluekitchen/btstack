BTSTACK_ROOT =  ../..
CORE += main.c btstack_stdin_posix.c btstack_tlv_posix.c

COMMON  += hci_transport_h2_libusb.c btstack_run_loop_posix.c btstack_chipset_zephyr.c btstack_link_key_db_tlv.c le_device_db_tlv.c 

include ${BTSTACK_ROOT}/example/Makefile.inc

CFLAGS  += -g -std=c99 -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wunused-parameter -Wredundant-decls -Wsign-compare
# CFLAGS += -Werror
# CFLAGS += -pedantic

# only LLVM
# CFLAGS += -Wnewline-eof
# CFLAGS += -Wc11-extensions
# CFLAGS += -Wgnu-empty-initializer

CFLAGS += -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/platform/embedded \
		  -I${BTSTACK_ROOT}/3rd-party/tinydir \
		  -I${BTSTACK_ROOT}/chipset/zephyr

VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/libusb
VPATH += ${BTSTACK_ROOT}/port/libusb
VPATH += ${BTSTACK_ROOT}/chipset/zephyr

# use pkg-config
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)

MAP = \
	obex_iterator.c \
	obex_message_builder.c \
	goep_client.c \
	yxml.c \
	btstack_util.c \
	hci_dump.c \
	map_util.c \
	map_client.c \
	map_server.c \
    
COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))
MAP_OBJ = $(addprefix build-coverage/,$(MAP:.c=.o))
CORE_OBJ = $(addprefix build-coverage/,$(CORE:.c=.o))

all: map_client_test

build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) ${CPPFLAGS} $< -o $@

map_client_test: ${CORE_OBJ} ${COMMON_OBJ} ${CLASSIC_OBJ} ${SDP_CLIENT} ${MAP_OBJ} map_client_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./map_client_test

clean:
	rm -f  *.o
	rm -rf *.dSYM build-coverage
	rm -f *.gcno *.gcda
	
