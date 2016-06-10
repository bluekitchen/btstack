VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/example
VPATH += ${BTSTACK_ROOT}/3rd-party/mbedtls/library

CFLAGS += -I.
CFLAGS += -I${BTSTACK_ROOT}/src/ble
CFLAGS += -I${BTSTACK_ROOT}/src/classic
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/mbedtls/include
CFLAGS += -I${BTSTACK_ROOT}/test/security_manager

CORE += \
    btstack_memory.c            \
    btstack_linked_list.c	    \
    btstack_memory_pool.c       \
    btstack_run_loop.c		    \
    btstack_util.c 	            \

COMMON += \
    hci.c			            \
    hci_cmd.c		            \
    hci_dump.c		            \
    l2cap.c			            \
    l2cap_signaling.c	        \
	btstack_link_key_db_memory.c   \
    sdp_util.c	                \
    spp_server.c  				\
    rfcomm.c	                \
    bnep.c	                    \
    sdp_server.c			            \

SDP_CLIENT += \
    sdp_client.o		        \
	sdp_client_rfcomm.o		    \

ATT	+= \
    att_dispatch.c       	    \

GATT_SERVER += \
    att_db.c 				 	    \
    att_server.c        	    \

GATT_CLIENT += \
    ad_parser.c                 \
    gatt_client.c        	    \

SM_REAL += \
	sm.c 				 	    \
    le_device_db_memory.c       \

PAN += \
	pan.c \

MBEDTLS = 					\
	bignum.c 				\
	ecp.c 					\
	ecp_curves.c 			\
	sm_mbedtls_allocator.c  \
	memory_buffer_alloc.c   \
	platform.c 				\

EXAMPLES = 					\
	ancs_client_demo		\
	gap_dedicated_bonding	\
	gap_inquiry 			\
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
    hfp_ag_demo             \
    hfp_hf_demo             \
    hsp_ag_demo             \
    hsp_hs_demo             \
    sm_pairing_peripheral   \
    sm_pairing_central      \

EXAMPLES_USING_LE =			\
	ancs_client_demo		\
	gatt_battery_query      \
	gatt_browser			\
	le_counter              \
	le_streamer				\
	spp_and_le_counter	    \
    gap_le_advertisements   \
    sm_pairing_peripheral   \
    sm_pairing_central      \

# .o for .c
CORE_OBJ    = $(CORE:.c=.o)
COMMON_OBJ  = $(COMMON:.c=.o) 
SM_REAL_OBJ = $(SM_REAL:.c=.o) $(MBEDTLS:.c=.o)
ATT_OBJ     = $(ATT:.c=.o)
GATT_CLIENT_OBJ = $(GATT_CLIENT:.c=.o)
GATT_SERVER_OBJ = $(GATT_SERVER:.c=.o)
PAN_OBJ = $(PAN:.c=.o)

default_target: all

# compile .gatt descriptions
profile.h: profile.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
ancs_client_demo.h: ancs_client_demo.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
spp_and_le_counter.h: spp_and_le_counter.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
le_counter.h: le_counter.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
le_streamer.h: le_streamer.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
sm_pairing_peripheral.h: sm_pairing_peripheral.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

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

sm_pairing_peripheral: sm_pairing_peripheral.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_REAL_OBJ} sm_pairing_peripheral.c 
	${CC} $(filter-out sm_pairing_peripheral.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

sm_pairing_central: ${CORE_OBJ} ${COMMON_OBJ} ${SM_REAL_OBJ} ad_parser.o sm_pairing_central.o 
	${CC} ${CFLAGS} ${LDFLAGS} $^ -o $@

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

panu_demo: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} panu_demo.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gatt_browser: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} gatt_browser.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gatt_battery_query: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} gatt_battery_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

ancs_client_demo: ancs_client_demo.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} ${SM_REAL_OBJ} ancs_client.c ancs_client_demo.c
	${CC} $(filter-out ancs_client_demo.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

led_counter: ${CORE_OBJ} ${COMMON_OBJ} led_counter.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

gap_le_advertisements: ${CORE_OBJ} ${COMMON_OBJ} ad_parser.c gap_le_advertisements.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hsp_hs_demo: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} sco_demo_util.o hsp_hs.o hsp_hs_demo.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hsp_ag_demo: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} sco_demo_util.o hsp_ag.o hsp_ag_demo.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_ag_demo: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} sco_demo_util.o hfp.o hfp_gsm_model.o hfp_ag.o hfp_ag_demo.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_hf_demo: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} sco_demo_util.o hfp.o hfp_hf.o hfp_hf_demo.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f ${EXAMPLES}
	rm -f  *.o *.out *.hex *.exe
	rm -f  ancs_client_demo.h profile.h spp_and_le_counter.h
	rm -rf *.dSYM
	rm -rf ${BTSTACK_ROOT}/ble/*.o
	rm -rf ${BTSTACK_ROOT}/src/*.o
	
	
