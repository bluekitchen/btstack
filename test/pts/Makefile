# Makefile for libusb based PTS tests
BTSTACK_ROOT = ../..
POSIX_ROOT= ${BTSTACK_ROOT}/platforms/posix

CORE += main.c stdin_support.c

COMMON += hci_transport_h2_libusb.c run_loop_posix.c remote_device_db_fs.c


include ${BTSTACK_ROOT}/example/embedded/Makefile.inc

CFLAGS += -I${POSIX_ROOT}/src -I${BTSTACK_ROOT}/platforms/libusb
CFLAGS += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow
CFLAGS += -Werror

VPATH += ${BTSTACK_ROOT}/platforms/posix/src
VPATH += ${BTSTACK_ROOT}/platforms/libusb

# use pkg-config for libusb
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)
# hard coded flags for libusb in /usr/local/lib
# CFLAGS  += -I/usr/local/include
# LDFLAGS += -L/usr/local/lib -lusb-1.0

# use pkg-config for portaudio
CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags)
LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

EXAMPLES = hfp_hf_test hfp_ag_test ble_peripheral_test ble_central_test l2cap_test classic_test bnep_test hsp_ag_test hsp_hs_test sco_loopback

all: ${BTSTACK_ROOT}/include/btstack/version.h ${EXAMPLES}

# compile .gatt descriptions
ble_peripheral_test.h: ble_peripheral_test.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 
ble_central_test.h: ble_central_test.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 
	
ble_peripheral_test: ble_peripheral_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_REAL_OBJ} ble_peripheral_test.o
	${CC} $(filter-out ble_peripheral_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

ble_central_test: ble_central_test.h ${CORE_OBJ} ${COMMON_OBJ} ${SM_REAL_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ} ad_parser.o ble_central_test.c
	${CC} $(filter-out ble_central_test.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

hsp_ag_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} hsp_ag.o hsp_ag_test.c 
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hsp_hs_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} hsp_hs.o hsp_hs_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_hf_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} hfp.o hfp_hf.o hfp_hf_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_ag_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} hfp.o hfp_gsm_model.o hfp_ag.o hfp_ag_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

l2cap_test: ${CORE_OBJ} ${COMMON_OBJ} l2cap_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

classic_test: ${CORE_OBJ} ${COMMON_OBJ} ${SDP_CLIENT} classic_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

bnep_test: ${CORE_OBJ} ${COMMON_OBJ} pan.o bnep_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sco_loopback: ${CORE_OBJ} ${COMMON_OBJ} sco_loopback.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@
