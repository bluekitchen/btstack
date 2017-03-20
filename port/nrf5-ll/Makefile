

INCLUDES = \
	-I. \
	-I../../src \
	-I../../platform/embedded \
	-I../../../ble/ll \
	-I../../../util \
	-I../../../ticker \
	-I../../../hal \
	-I../../../external/arm/cmsis/include \
	-I../../../external/nordic/include \
	-I../../../arch/arm/cortex_m \
	-I../../../soc/nrf5/ \
	-lm

LDSCRIPT = ${PHOENIX_ROOT}/arch/arm/cortex_m/link.lds

BTSTACK_ROOT =../..
PHOENIX_ROOT =../../..

VPATH += \
	$(BTSTACK_ROOT)/example \
	${BTSTACK_ROOT}/src \
	${BTSTACK_ROOT}/src/ble \
	${BTSTACK_ROOT}/src/ble/gatt-service \
	${BTSTACK_ROOT}/src/classic \
	${PHOENIX_ROOT}/arch/arm/cortex_m \
	${PHOENIX_ROOT}/ble/ll \
	${PHOENIX_ROOT}/hal/nrf5 \
	${PHOENIX_ROOT}/soc/nrf5 \
	${PHOENIX_ROOT}/ticker \
	${PHOENIX_ROOT}/util \

TARGETS += \
	le_counter.elf \
	le_streamer.elf \
	sm_pairing_peripheral.elf \
	revogi_mitm.elf \
	gatt_browser.elf \
	gap_le_advertisements.elf \
	sbc_test.elf \
#   ancs_client_demo.elf \
#	sm_pairing_central.elf \
#	gatt_battery_query.elf \

all : $(SUBDIRS) $(TARGETS)

# compile .gatt descriptions
profile.h: ${BTSTACK_ROOT}/example/profile.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
ancs_client_demo.h: ${BTSTACK_ROOT}/example/ancs_client_demo.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
le_counter.h: ${BTSTACK_ROOT}/example/le_counter.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
le_streamer.h: ${BTSTACK_ROOT}/example/le_streamer.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
sm_pairing_peripheral.h: ${BTSTACK_ROOT}/example/sm_pairing_peripheral.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
revogi_mitm.h: ${BTSTACK_ROOT}/example/revogi_mitm.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 


SRCS_SOC_NRF5 = \
	soc_c.c \

SRCS_HAL_NRF5 = \
	uart.c \
	clock.c \
	cntr.c \
	rand.c \
	ecb.c \
	radio.c \

SRCS_UTIL = \
	util.c \
	string.c \
	mem.c \
	memq.c \
	mayfly.c \

ASMS_COMMON = \
	startup.s \

ASMS_SOC_NRF5 = \
	soc.s \

ASMS_APP_BTSTACK = \
	$(ASMS_COMMON) \
	$(ASMS_SOC_NRF5) \
	soc_isr.s \

SRCS_BTSTACK = \
	hci-ll.c \
	retarget_printf.c \
	port.c \
	btstack_linked_list.c \
	btstack_memory.c \
	btstack_memory_pool.c \
	btstack_run_loop.c \
	btstack_util.c \
	hci.c \
	hci_cmd.c \
	hci_dump.c \
	l2cap.c \
	l2cap_signaling.c \
	att_db.c \
	att_dispatch.c \
	att_server.c \
	sm.c \
	le_device_db_memory.c \
	battery_service_server.c \

SRCS_APP_BTSTACK = \
	$(SRCS_SOC_NRF5) \
	$(SRCS_HAL_NRF5) \
	$(SRCS_UTIL) \
	$(SRCS_BTSTACK) \
	ticker.c \
	ctrl.c \
	ll.c \
	app_btstack.c \

SRCS_ANCS_CLIENT_DEMO = \
	$(SRCS_APP_BTSTACK) \
	ancs_client_demo.c

SRCS_GAP_LE_ADVERTISEMENTS = \
	$(SRCS_APP_BTSTACK) \
	$(BTSTACK_ROOT)/src/ad_parser.c \
	gap_le_advertisements.c

SRCS_GATT_BATTERY_QUERY = \
	$(SRCS_APP_BTSTACK) \
	gatt_battery_query.c \
	$(BTSTACK_ROOT)/src/ble/gatt_client.c \

SRCS_GATT_BROWSER = \
	$(SRCS_APP_BTSTACK) \
	gatt_browser.c \
	$(BTSTACK_ROOT)/src/ble/gatt_client.c \

SRCS_REVOGI_MITM = \
	$(SRCS_APP_BTSTACK) \
	revogi_mitm.c \
	$(BTSTACK_ROOT)/src/ble/gatt_client.c \

SRCS_LE_COUNTER = \
	$(SRCS_APP_BTSTACK) \
	le_counter.c

SRCS_LE_STREAMER = \
	$(SRCS_APP_BTSTACK) \
	le_streamer.c

SRCS_SM_PAIRING_CENTRAL = \
	$(SRCS_APP_BTSTACK) \
	$(BTSTACK_ROOT)/src/ad_parser.c \
	sm_pairing_central.c

SRCS_SM_PAIRING_PERIPHERAL = \
	$(SRCS_APP_BTSTACK) \
	sm_pairing_peripheral.c

SRCS_SBC_TEST = \
	$(SRCS_APP_BTSTACK) \
	btstack_sbc_bludroid.c \
	btstack_sbc_plc.c \
	sbc_test.c \
	${SBC_ENCODER} \
	${SBC_DECODER} \

include $(BTSTACK_ROOT)/3rd-party/bluedroid/encoder/Makefile.inc
include $(BTSTACK_ROOT)/3rd-party/bluedroid/decoder/Makefile.inc
INCLUDES += -I$(BTSTACK_ROOT)/3rd-party/bluedroid/encoder/include
INCLUDES += -I$(BTSTACK_ROOT)/3rd-party/bluedroid/decoder/include
VPATH += $(BTSTACK_ROOT)/3rd-party/bluedroid/encoder/srce
VPATH += $(BTSTACK_ROOT)/3rd-party/bluedroid/decoder/srce

ancs_client_demo.elf : $(ASMS_APP_BTSTACK:.s=.o)
ancs_client_demo.elf : ancs_client_demo.h $(SRCS_ANCS_CLIENT_DEMO:.c=.o)

revogi_mitm.elf : $(ASMS_APP_BTSTACK:.s=.o)
revogi_mitm.elf : revogi_mitm.h $(SRCS_REVOGI_MITM:.c=.o)

gap_le_advertisements.elf : $(ASMS_APP_BTSTACK:.s=.o)
gap_le_advertisements.elf : $(SRCS_GAP_LE_ADVERTISEMENTS:.c=.o)

le_counter.elf : $(ASMS_APP_BTSTACK:.s=.o)
le_counter.elf : le_counter.h $(SRCS_LE_COUNTER:.c=.o)

gatt_battery_query.elf : $(ASMS_APP_BTSTACK:.s=.o)
gatt_battery_query.elf : $(SRCS_GATT_BATTERY_QUERY:.c=.o)

gatt_browser.elf : $(ASMS_APP_BTSTACK:.s=.o)
gatt_browser.elf : $(SRCS_GATT_BROWSER:.c=.o)

le_streamer.elf : $(ASMS_APP_BTSTACK:.s=.o)
le_streamer.elf : le_streamer.h $(SRCS_LE_STREAMER:.c=.o)

sm_pairing_central.elf : $(ASMS_APP_BTSTACK:.s=.o)
sm_pairing_central.elf : $(SRCS_SM_PAIRING_CENTRAL:.c=.o)

sm_pairing_peripheral.elf : $(ASMS_APP_BTSTACK:.s=.o)
sm_pairing_peripheral.elf : sm_pairing_peripheral.h $(SRCS_SM_PAIRING_PERIPHERAL:.c=.o)

sbc_test.elf : $(ASMS_APP_BTSTACK:.s=.o)
sbc_test.elf : $(SRCS_SBC_TEST:.c=.o)

SRCS = ${SRCS_BTSTACK} ${SRCS_UTIL} ${SRCS_APP_BTSTACK} ${TARGETS:.elf=.c}
ASMS = ${ASMS_COMMON}  ${ASMS_SOC_NRF5} ${ASMS_APP_BTSTACK}

include ../../../makefile.inc
