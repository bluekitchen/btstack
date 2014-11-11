BTSTACK_ROOT = ../..
POSIX_ROOT= ${BTSTACK_ROOT}/platforms/posix

CFLAGS  = -g -Wall -I.
CFLAGS += -I${BTSTACK_ROOT}/ble
CFLAGS += -I${BTSTACK_ROOT}/include 
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}

CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)
# CFLAGS += -I/usr/local/include/libusb-1.0
# LDFLAGS += -L/usr/local/lib -lusb-1.0
# LDFLAGS += -lws2_32

CORE   = \
    ${BTSTACK_ROOT}/src/btstack_memory.c            \
    ${BTSTACK_ROOT}/src/linked_list.c	            \
    ${BTSTACK_ROOT}/src/memory_pool.c               \
    ${BTSTACK_ROOT}/src/run_loop.c		            \
    ${POSIX_ROOT}/src/run_loop_posix.c              \

COMMON = \
    ${BTSTACK_ROOT}/src/hci.c			            \
    ${BTSTACK_ROOT}/src/hci_cmds.c		            \
    ${BTSTACK_ROOT}/src/hci_dump.c		            \
    ${BTSTACK_ROOT}/src/l2cap.c			            \
    ${BTSTACK_ROOT}/src/l2cap_signaling.c	        \
	${BTSTACK_ROOT}/src/remote_device_db_memory.c   \
    ${BTSTACK_ROOT}/src/sdp_util.c	                \
    ${BTSTACK_ROOT}/src/utils.c			            \
    ${BTSTACK_ROOT}/src/rfcomm.c	                \
    ${BTSTACK_ROOT}/src/bnep.c	                    \
    ${BTSTACK_ROOT}/src/sdp.c			            \
    ${BTSTACK_ROOT}/src/sdp_client.c		        \
    ${BTSTACK_ROOT}/src/sdp_parser.c		        \
    ${BTSTACK_ROOT}/src/sdp_query_util.c		    \
	${BTSTACK_ROOT}/src/sdp_query_rfcomm.c		    \
    ${POSIX_ROOT}/src/hci_transport_h4.c            \
    ${POSIX_ROOT}/src/hci_transport_h2_libusb.c     \

ATT	= \
    ${BTSTACK_ROOT}/ble/att_dispatch.c       	    \

GATT_SERVER = \
    ${BTSTACK_ROOT}/ble/att.c 				 	    \
    ${BTSTACK_ROOT}/ble/att_server.c        	    \

GATT_CLIENT = \
    ${BTSTACK_ROOT}/ble/ad_parser.c                 \
    ${BTSTACK_ROOT}/ble/gatt_client.c        	    \

SM_REAL = \
	${BTSTACK_ROOT}/ble/sm.c 				 	    \
    ${BTSTACK_ROOT}/ble/central_device_db_memory.c  \

SM_MINIMAL = \
	${BTSTACK_ROOT}/ble/sm_minimal.c  		 	    \
    ${BTSTACK_ROOT}/ble/central_device_db_dummy.c   \

SDP_DES = \
	${BTSTACK_ROOT}/test/des_iterator/des_iterator.c  \

PAN = \
	${BTSTACK_ROOT}/src/pan.c \

CORE_OBJ    = $(CORE:.c=.o)
COMMON_OBJ  = $(COMMON:.c=.o) 
SM_REAL_OBJ = $(SM_REAL:.c=.o)
SM_MINIMAL_OBJ = $(SM_MINIMAL:.c=.o)
ATT_OBJ     = $(ATT:.c=.o)
GATT_CLIENT_OBJ = $(GATT_CLIENT:.c=.o)
GATT_SERVER_OBJ = $(GATT_SERVER:.c=.o)
PAN_OBJ = $(PAN:.c=.o)

# create firmware image from common objects and example source file

all: ${BTSTACK_ROOT}/include/btstack/version.h gatt_browser gatt_battery_query sdp_rfcomm_query sdp_general_query spp_counter ble_peripheral \
		ble_peripheral_sm_minimal gap_inquiry gap_dedicated_bonding gap_inquiry_and_bond l2cap_test spp_streamer \
		classic_test ancs_client spp_and_le_counter sdp_bnep_query


${BTSTACK_ROOT}/include/btstack/version.h:
	${BTSTACK_ROOT}/tools/get_version.sh

sdp_rfcomm_query: ${CORE_OBJ} ${COMMON_OBJ} ${PAN_OBJ} sdp_rfcomm_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sdp_general_query: ${CORE_OBJ} ${COMMON_OBJ} sdp_general_query.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sdp_bnep_query: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_DES} sdp_bnep_query.c  
	${CC} $^ ${CFLAGS} -I../../test/des_iterator ${LDFLAGS} -o $@

spp_counter: ${CORE_OBJ} ${COMMON_OBJ} spp_counter.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

spp_and_le_counter: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_REAL_OBJ} spp_and_le_counter.c spp_and_le_counter.h
	${CC} ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_REAL_OBJ} spp_and_le_counter.c ${CFLAGS} ${LDFLAGS} -o $@

spp_counter_ssp: ${CORE_OBJ} ${COMMON_OBJ} spp_counter_ssp.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

spp_streamer: ${CORE_OBJ} ${COMMON_OBJ} spp_streamer.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_dedicated_bonding: ${CORE_OBJ} ${COMMON_OBJ} gap_dedicated_bonding.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_inquiry: ${CORE_OBJ} ${COMMON_OBJ} gap_inquiry.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_inquiry_and_bond: ${CORE_OBJ} ${COMMON_OBJ} gap_inquiry_and_bond.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

panu_demo: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_DES} panu_demo.c  
	${CC} $^ ${CFLAGS} -I../../test/des_iterator ${LDFLAGS} -o $@

l2cap_test: ${CORE_OBJ} ${COMMON_OBJ} l2cap_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

classic_test: ${CORE_OBJ} ${COMMON_OBJ} classic_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

# compile .ble description
profile.h: profile.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 
ancs_client.h: ancs_client.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 
spp_and_le_counter.h: spp_and_le_counter.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 

ancs_client: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} ${BTSTACK_ROOT}/ble/ancs_client_lib.c ancs_client.c ancs_client.h
	${CC} ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} ${BTSTACK_ROOT}/ble/ancs_client_lib.c ancs_client.c ${CFLAGS} ${LDFLAGS} -o $@

ble_peripheral: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_REAL_OBJ} ble_peripheral.c profile.h
	${CC} ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_REAL_OBJ} ble_peripheral.c ${CFLAGS} ${LDFLAGS} -o $@

ble_peripheral_sm_minimal: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_MINIMAL_OBJ} ble_peripheral.c profile.h
	${CC} ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_MINIMAL_OBJ} ble_peripheral.c ${CFLAGS} ${LDFLAGS} -o $@

ble_client: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL} ble_client.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL} ble_client.c ${CFLAGS} ${LDFLAGS} -o $@

gatt_browser: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} gatt_browser.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} gatt_browser.c ${CFLAGS} ${LDFLAGS} -o $@

gatt_battery_query: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} gatt_battery_query.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} gatt_battery_query.c ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f  	spp_counter sdp_rfcomm_query sdp_general_query sdp_bnep_query spp_counter_ssp ble_peripheral ble_client gatt_battery_query gatt_browser \
			gatt_client ancs_client ble_peripheral_sm_minimal classic_test  \
			gap_dedicated_bonding gap_inquiry gap_inquiry_and_bond l2cap_test spp_and_le_counter spp_streamer panu_demo
	rm -f  *.o *.out *.hex 
	rm -f  ${BTSTACK_ROOT}/src/*.o
	rm -f  ${BTSTACK_ROOT}/ble/*.o
	rm -f  ${BTSTACK_ROOT}/chipset-cc256x/*.o
	rm -f  ${BTSTACK_ROOT}/platforms/posix/src/*.o
	rm -f  ${BTSTACK_ROOT}/include/btstack/version.h
	rm -f  ${BTSTACK_ROOT}/example/libusb/ancs_client.h
	rm -f  ${BTSTACK_ROOT}/example/libusb/profile.h
	rm -f  ${BTSTACK_ROOT}/example/libusb/spp_and_le_counter.h
	rm -rf *.dSYM
	
