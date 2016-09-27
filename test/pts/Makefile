# Makefile for libusb based PTS tests
BTSTACK_ROOT = ../..

CORE += main.c stdin_support.c

COMMON += hci_transport_h2_libusb.c btstack_run_loop_posix.c btstack_link_key_db_fs.c le_device_db_fs.c

include ${BTSTACK_ROOT}/example/Makefile.inc

CFLAGS += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Werror \
          -I${BTSTACK_ROOT}/platform/posix \
          -I${BTSTACK_ROOT}/platform/embedded \
	      -I${BTSTACK_ROOT}/port/libusb

VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/port/libusb

# use pkg-config for libusb
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)
# hard coded flags for libusb in /usr/local/lib
# CFLAGS  += -I/usr/local/include
# LDFLAGS += -L/usr/local/lib -lusb-1.0

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags)
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

EXAMPLES = iopt ble_peripheral_test ble_central_test l2cap_test classic_test bnep_test hsp_ag_test hsp_hs_test sco_loopback

all: ${EXAMPLES}

# compile .gatt descriptions
ble_peripheral_test.h: ble_peripheral_test.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
ble_central_test.h: ble_central_test.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
	
ble_peripheral_test: ble_peripheral_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_REAL_OBJ} ble_peripheral_test.o
	${CC} $(filter-out ble_peripheral_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

ble_central_test: ble_central_test.h ${CORE_OBJ} ${COMMON_OBJ} ${SM_REAL_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} ad_parser.o ble_central_test.c
	${CC} $(filter-out ble_central_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

hsp_ag_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} hsp_ag.o hsp_ag_test.c 
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hsp_hs_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} hsp_hs.o hsp_hs_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

l2cap_test: ${CORE_OBJ} ${COMMON_OBJ} l2cap_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

classic_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} classic_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

bnep_test: ${CORE_OBJ} ${COMMON_OBJ} pan.o bnep_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sco_loopback: ${CORE_OBJ} ${COMMON_OBJ} sco_loopback.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

iopt: ${CORE_OBJ} ${COMMON_OBJ} pan.o hsp_ag.o hsp_hs.o hfp_ag.o hfp_hf.o hfp_gsm_model.o iopt.c hfp.o ${SDP_CLIENT}
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@
	