VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/ble
VPATH += ${BTSTACK_ROOT}/example/embedded


CFLAGS += -I.
CFLAGS += -I${BTSTACK_ROOT}/ble
CFLAGS += -I${BTSTACK_ROOT}/include 
CFLAGS += -I${BTSTACK_ROOT}/src

CORE += \
    btstack_memory.c            \
    linked_list.c	            \
    memory_pool.c               \
    run_loop.c		            \

COMMON += \
    hci.c			            \
    hci_cmds.c		            \
    hci_dump.c		            \
    l2cap.c			            \
    l2cap_signaling.c	        \
	remote_device_db_memory.c   \
    sdp_util.c	                \
    utils.c			            \
    rfcomm.c	                \
    bnep.c	                    \
    sdp.c			            \

SDP_CLIENT += \
    sdp_client.o		        \
    sdp_parser.o		        \
    sdp_query_util.o		    \
	sdp_query_rfcomm.o		    \

ATT	+= \
    att_dispatch.c       	    \

GATT_SERVER += \
    att.c 				 	    \
    att_server.c        	    \

GATT_CLIENT += \
    ad_parser.c                 \
    gatt_client.c        	    \

SM_REAL += \
	sm.c 				 	    \
    le_device_db_memory.c       \

SM_MINIMAL += \
	sm_minimal.c  		 	    \
    le_device_db_dummy.c        \

PAN += \
	pan.c \

EXAMPLES = 					\
	ancs_client 			\
	gap_dedicated_bonding	\
	gap_inquiry 			\
	gap_inquiry_and_bond 	\
	gatt_battery_query		\
	gatt_browser            \
	le_counter              \
	le_streamer				\
	led_counter				\
	sdp_bnep_query 			\
	sdp_general_query		\
	sdp_rfcomm_query		\
	spp_and_le_counter 		\
	spp_counter 			\
	spp_streamer			\
    gap_le_advertisements   \
    hsp_hs_test             \

EXAMPLES_USING_LE =			\
	ancs_client 			\
	gatt_battery_query      \
	gatt_browser			\
	le_counter              \
	le_streamer				\
	spp_and_le_counter	    \
    gap_le_advertisements   \

# .o for .c
CORE_OBJ    = $(CORE:.c=.o)
COMMON_OBJ  = $(COMMON:.c=.o) 
SM_REAL_OBJ = $(SM_REAL:.c=.o)
SM_MINIMAL_OBJ = $(SM_MINIMAL:.c=.o)
ATT_OBJ     = $(ATT:.c=.o)
GATT_CLIENT_OBJ = $(GATT_CLIENT:.c=.o)
GATT_SERVER_OBJ = $(GATT_SERVER:.c=.o)
PAN_OBJ = $(PAN:.c=.o)

default_target: all

${BTSTACK_ROOT}/include/btstack/version.h:
	${BTSTACK_ROOT}/tools/get_version.sh

# compile .gatt descriptions
profile.h: profile.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 
ancs_client.h: ancs_client.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 
spp_and_le_counter.h: spp_and_le_counter.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 
le_counter.h: le_counter.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 
le_streamer.h: le_streamer.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 

# examples
sdp_rfcomm_query: ${CORE_OBJ} ${COMMON_OBJ} ${PAN_OBJ} ${SDP_CLIENT} sdp_rfcomm_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sdp_general_query: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} sdp_general_query.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sdp_bnep_query: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} sdp_bnep_query.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

spp_counter: ${CORE_OBJ} ${COMMON_OBJ} spp_counter.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

le_counter: le_counter.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_REAL_OBJ} le_counter.c 
	${CC} $(filter-out le_counter.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

le_streamer: le_streamer.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_REAL_OBJ} le_streamer.c 
	${CC} $(filter-out le_streamer.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

spp_and_le_counter: spp_and_le_counter.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_REAL_OBJ} spp_and_le_counter.c 
	${CC} $(filter-out spp_and_le_counter.h,$^)  ${CFLAGS} ${LDFLAGS} -o $@

spp_counter_ssp: ${CORE_OBJ} ${COMMON_OBJ} spp_counter_ssp.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

spp_streamer: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} spp_streamer.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_dedicated_bonding: ${CORE_OBJ} ${COMMON_OBJ} gap_dedicated_bonding.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_inquiry: ${CORE_OBJ} ${COMMON_OBJ} gap_inquiry.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_inquiry_and_bond: ${CORE_OBJ} ${COMMON_OBJ} gap_inquiry_and_bond.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

panu_demo: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} panu_demo.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gatt_browser: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} gatt_browser.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gatt_battery_query: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} gatt_battery_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

ancs_client: ancs_client.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} ${BTSTACK_ROOT}/ble/ancs_client_lib.c ancs_client.c
	${CC} $(filter-out ancs_client.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

led_counter: ${CORE_OBJ} ${COMMON_OBJ} led_counter.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

ble_client: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL} ble_client.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_le_advertisements: ${CORE_OBJ} ${COMMON_OBJ} ad_parser.c gap_le_advertisements.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hsp_hs_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} hsp_hs.o hsp_hs_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f ${EXAMPLES}
	rm -f  *.o *.out *.hex 
	rm -f  ${BTSTACK_ROOT}/include/btstack/version.h
	rm -f  ancs_client.h profile.h spp_and_le_counter.h
	rm -rf *.dSYM
	rm -rf ${BTSTACK_ROOT}/ble/*.o
	rm -rf ${BTSTACK_ROOT}/src/*.o
	rm -rf ${BTSTACK_ROOT}/platforms/posix/src/*.o
	
	
