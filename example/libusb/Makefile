BTSTACK_ROOT = ../..


CFLAGS  = -g -Wall -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include -I${BTSTACK_ROOT}/chipset-cc256x
CFLAGS += -I/sw/include -I../.. 
LDFLAGS = -L/sw/lib -lusb-1.0

CORE   = \
    ${BTSTACK_ROOT}/src/btstack_memory.c          \
    ${BTSTACK_ROOT}/src/linked_list.c	          \
    ${BTSTACK_ROOT}/src/memory_pool.c             \
    ${BTSTACK_ROOT}/src/run_loop.c		          \
    ${BTSTACK_ROOT}/src/run_loop_posix.c          \

COMMON = \
    ${BTSTACK_ROOT}/src/hci.c			          \
    ${BTSTACK_ROOT}/src/hci_cmds.c		          \
    ${BTSTACK_ROOT}/src/hci_dump.c		          \
    ${BTSTACK_ROOT}/src/hci_transport_h4.c        \
    ${BTSTACK_ROOT}/src/hci_transport_usb.c       \
    ${BTSTACK_ROOT}/src/l2cap.c			          \
    ${BTSTACK_ROOT}/src/l2cap_signaling.c	      \
	${BTSTACK_ROOT}/src/remote_device_db_memory.c \
    ${BTSTACK_ROOT}/src/sdp_util.c	              \
    ${BTSTACK_ROOT}/src/utils.c			          \
    ${BTSTACK_ROOT}/src/sdp_parser.c              \
    ${BTSTACK_ROOT}/src/sdp_client.c              \
    ${BTSTACK_ROOT}/src/rfcomm.c	              \
    ${BTSTACK_ROOT}/src/sdp.c			          \
 #    ${BTSTACK_ROOT}/chipset-cc256x/bt_control_cc256x.c   \
 #	 ${BTSTACK_ROOT}/chipset-cc256x/bluetooth_init_cc2560A_2.1.c \

CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ = $(COMMON:.c=.o)


# create firmware image from common objects and example source file

all: sdp_rfcomm_query general_sdp_query spp_counter
#spp-usb l2cap-server-usb l2cap-client-usb l2cap-server-uart l2cap-client-uart

sdp_rfcomm_query: ${CORE_OBJ} ${COMMON_OBJ} ${BTSTACK_ROOT}/src/sdp_query_rfcomm.c sdp_rfcomm_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

general_sdp_query: ${CORE_OBJ} ${COMMON_OBJ} general_sdp_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

spp_counter: ${CORE_OBJ} ${COMMON_OBJ} spp_counter.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

l2cap_server: ${CORE_OBJ} ${COMMON_OBJ} l2cap-server.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

l2cap_client: ${CORE_OBJ} ${COMMON_OBJ} l2cap-client.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f  spp_counter sdp_rfcomm_query general_sdp_query l2cap_server l2cap_client spp *.o *.out *.hex ../driver/*.o ${BTSTACK_ROOT}/src/*.o
	rm -f  ${BTSTACK_ROOT}/chipset-cc256x/*.o
	rm -rf *.dSYM
	