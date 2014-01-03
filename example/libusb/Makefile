BTSTACK_ROOT = ../..

CFLAGS  = -g -Wall -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include -I${BTSTACK_ROOT}/chipset-cc256x
CFLAGS += -I/sw/include -I../../ble -I../.. -I../.. 
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
    ${BTSTACK_ROOT}/src/rfcomm.c	              \
    ${BTSTACK_ROOT}/src/sdp.c			          \
    ${BTSTACK_ROOT}/src/sdp_client.c		      \
    ${BTSTACK_ROOT}/src/sdp_parser.c		      \
    ${BTSTACK_ROOT}/src/sdp_query_util.c		  \
	${BTSTACK_ROOT}/src/sdp_query_rfcomm.c		  \
    ${BTSTACK_ROOT}/ble/att.c 				 	  \

CC2564 = \
    ${BTSTACK_ROOT}/chipset-cc256x/bt_control_cc256x.c   \
 	${BTSTACK_ROOT}/chipset-cc256x/bluetooth_init_cc2564_2.10.c \

CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ = $(COMMON:.c=.o)
CC2564_OBJ = $(CC2564:.c=.o)


# create firmware image from common objects and example source file

all: ../../include/btstack/version.h ble_client ble_client_uart sdp_rfcomm_query sdp_general_query spp_counter ble_server ble_client

#spp-usb l2cap-server-usb l2cap-client-usb l2cap-server-uart l2cap-client-uart

../../include/btstack/version.h:
	cd ..; ../src/get_version.sh

sdp_rfcomm_query: ${CORE_OBJ} ${COMMON_OBJ} sdp_rfcomm_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sdp_general_query: ${CORE_OBJ} ${COMMON_OBJ} sdp_general_query.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

spp_counter: ${CORE_OBJ} ${COMMON_OBJ} spp_counter.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

spp_counter_ssp: ${CORE_OBJ} ${COMMON_OBJ} spp_counter_ssp.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

# compile .ble description
profile.h: profile.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 

ble_server: ${CORE_OBJ} ${COMMON_OBJ} ble_server.c central_device_db_memory.c profile.h
	${CC} ${CORE_OBJ} ${COMMON_OBJ} ble_server.c central_device_db_memory.c ${CFLAGS} ${LDFLAGS} -o $@

ble_client: ${CORE_OBJ} ${COMMON_OBJ} ble_client.c ad_parser.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} ad_parser.c ble_client.c ${CFLAGS} ${LDFLAGS} -o $@

ble_client_uart: ${CORE_OBJ} ${COMMON_OBJ} ${CC2564_OBJ} ble_client.c ad_parser.c
	${CC} -DHAVE_UART_CC2564 ${CORE_OBJ} ${COMMON_OBJ} ${CC2564_OBJ} ad_parser.c ble_client.c ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f  spp_counter sdp_rfcomm_query sdp_general_query spp_counter_ssp ble_server ble_client 
	rm -f  *.o *.out *.hex
	rm -f  ${BTSTACK_ROOT}/src/*.o
	rm -f  ${BTSTACK_ROOT}/ble/*.o
	rm -f  ${BTSTACK_ROOT}/chipset-cc256x/*.o
	rm -rf *.dSYM
	