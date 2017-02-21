

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
	-I../../../soc/nrf5/

LDSCRIPT = ${PHOENIX_ROOT}/arch/arm/cortex_m/link.lds

BTSTACK_ROOT =../..
PHOENIX_ROOT =../../..

VPATH += \
	$(BTSTACK_ROOT)/example \
	${BTSTACK_ROOT}/src \
	${BTSTACK_ROOT}/src/ble \
	${BTSTACK_ROOT}/src/ble/gatt-service \
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
#   ancs_client_demo.elf \
#	sm_pairing_central.elf \
#	gap_le_advertisements.elf \
#	gatt_battery_query.elf \
#	gatt_browser.elf \

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

ancs_client_demo.elf : $(ASMS_APP_BTSTACK:.s=.o)
ancs_client_demo.elf : ancs_client_demo.h $(SRCS_ANCS_CLIENT_DEMO:.c=.o)

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

SRCS = ${SRCS_BTSTACK} ${SRCS_UTIL} ${SRCS_APP_BTSTACK} ${TARGETS:.elf=.c}
ASMS = ${ASMS_COMMON}  ${ASMS_SOC_NRF5} ${ASMS_APP_BTSTACK}

include ../../../makefile.inc
