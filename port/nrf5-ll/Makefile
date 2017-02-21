# ASMS = $(wildcard *.s)
SRCS = \
	../../src/ble/att_db.c \
	../../src/ble/att_dispatch.c \
	../../src/ble/att_server.c \
	../../src/ble/sm.c \
	../../src/ble/le_device_db_memory.c \
	../../src/ble/gatt-service/battery_service_server.c \
	../../src/btstack_linked_list.c \
	../../src/btstack_memory.c \
	../../src/btstack_memory_pool.c \
	../../src/btstack_run_loop.c \
	../../src/btstack_util.c \
	../../src/hci.c \
	../../src/hci_cmd.c \
	../../src/hci_dump.c \
	../../src/hci_dump.c \
	../../src/l2cap.c \
	../../src/l2cap_signaling.c \
	hci-ll.c \
	port.c \
	retarget_printf.c \

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
BTSTACK_ROOT=../..
${BTSTACK_ROOT}/example/profile.h: ${BTSTACK_ROOT}/example/profile.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
${BTSTACK_ROOT}/example/ancs_client_demo.h: ${BTSTACK_ROOT}/example/ancs_client_demo.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
${BTSTACK_ROOT}/example/le_counter.h: ${BTSTACK_ROOT}/example/le_counter.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
${BTSTACK_ROOT}/example/le_streamer.h: ${BTSTACK_ROOT}/example/le_streamer.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
${BTSTACK_ROOT}/example/sm_pairing_peripheral.h: ${BTSTACK_ROOT}/example/sm_pairing_peripheral.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

PHOENIX_ROOT =../../..

SRCS_SOC_NRF5 = \
	${PHOENIX_ROOT}/soc/nrf5/soc_c.c \

SRCS_HAL_NRF5 = \
	${PHOENIX_ROOT}/hal/nrf5/uart.c \
	${PHOENIX_ROOT}/hal/nrf5/clock.c \
	${PHOENIX_ROOT}/hal/nrf5/cntr.c \
	${PHOENIX_ROOT}/hal/nrf5/rand.c \
	${PHOENIX_ROOT}/hal/nrf5/ecb.c \
	${PHOENIX_ROOT}/hal/nrf5/radio.c \

SRCS_UTIL = \
	${PHOENIX_ROOT}/util/util.c \
	${PHOENIX_ROOT}/util/string.c \
	${PHOENIX_ROOT}/util/mem.c \
	${PHOENIX_ROOT}/util/memq.c \
	${PHOENIX_ROOT}/util/mayfly.c \

ASMS_COMMON = \
	${PHOENIX_ROOT}/arch/arm/cortex_m/startup.s \

ASMS_SOC_NRF5 = \
	${PHOENIX_ROOT}/soc/nrf5/soc.s \

ASMS_APP_BTSTACK = \
	$(ASMS_COMMON) \
	$(ASMS_SOC_NRF5) \
	${PHOENIX_ROOT}/soc/nrf5/soc_isr.s \

SRCS_BTSTACK = \
	hci-ll.c \
	retarget_printf.c \
	port.c \
	${BTSTACK_ROOT}/src/btstack_linked_list.c \
	${BTSTACK_ROOT}/src/btstack_memory.c \
	${BTSTACK_ROOT}/src/btstack_memory_pool.c \
	${BTSTACK_ROOT}/src/btstack_run_loop.c \
	${BTSTACK_ROOT}/src/btstack_util.c \
	${BTSTACK_ROOT}/src/hci.c \
	${BTSTACK_ROOT}/src/hci_cmd.c \
	${BTSTACK_ROOT}/src/hci_dump.c \
	${BTSTACK_ROOT}/src/l2cap.c \
	${BTSTACK_ROOT}/src/l2cap_signaling.c \
	${BTSTACK_ROOT}/src/ble/att_db.c \
	${BTSTACK_ROOT}/src/ble/att_dispatch.c \
	${BTSTACK_ROOT}/src/ble/att_server.c \
	${BTSTACK_ROOT}/src/ble/sm.c \
	${BTSTACK_ROOT}/src/ble/le_device_db_memory.c \
	${BTSTACK_ROOT}/src/ble/gatt-service/battery_service_server.c \

SRCS_APP_BTSTACK = \
	$(SRCS_SOC_NRF5) \
	$(SRCS_HAL_NRF5) \
	$(SRCS_UTIL) \
	$(SRCS_BTSTACK) \
	${PHOENIX_ROOT}/ticker/ticker.c \
	${PHOENIX_ROOT}/ble/ll/ctrl.c \
	${PHOENIX_ROOT}/ble/ll/ll.c \
	app_btstack.c \

SRCS_ANCS_CLIENT_DEMO = \
	$(SRCS_APP_BTSTACK) \
	$(BTSTACK_ROOT)/example/ancs_client_demo.c

SRCS_GAP_LE_ADVERTISEMENTS = \
	$(SRCS_APP_BTSTACK) \
	$(BTSTACK_ROOT)/src/ad_parser.c \
	$(BTSTACK_ROOT)/example/gap_le_advertisements.c

SRCS_GATT_BATTERY_QUERY = \
	$(SRCS_APP_BTSTACK) \
	$(BTSTACK_ROOT)/example/gatt_battery_query.c \
	$(BTSTACK_ROOT)/src/ble/gatt_client.c \

SRCS_GATT_BROWSER = \
	$(SRCS_APP_BTSTACK) \
	$(BTSTACK_ROOT)/example/gatt_browser.c \
	$(BTSTACK_ROOT)/src/ble/gatt_client.c \

SRCS_LE_COUNTER = \
	$(SRCS_APP_BTSTACK) \
	$(BTSTACK_ROOT)/example/le_counter.c

SRCS_LE_STREAMER = \
	$(SRCS_APP_BTSTACK) \
	$(BTSTACK_ROOT)/example/le_streamer.c

SRCS_SM_PAIRING_CENTRAL = \
	$(SRCS_APP_BTSTACK) \
	$(BTSTACK_ROOT)/src/ad_parser.c \
	$(BTSTACK_ROOT)/example/sm_pairing_central.c

SRCS_SM_PAIRING_PERIPHERAL = \
	$(SRCS_APP_BTSTACK) \
	$(BTSTACK_ROOT)/example/sm_pairing_peripheral.c

ancs_client_demo.elf : $(ASMS_APP_BTSTACK:.s=.o)
ancs_client_demo.elf : ${BTSTACK_ROOT}/example/ancs_client_demo.h $(SRCS_ANCS_CLIENT_DEMO:.c=.o)

ancs_client_demo.elf : $(ASMS_APP_BTSTACK:.s=.o)
ancs_client_demo.elf : ${BTSTACK_ROOT}/example/ancs_client_demo.h $(SRCS_ANCS_CLIENT_DEMO:.c=.o)

gap_le_advertisements.elf : $(ASMS_APP_BTSTACK:.s=.o)
gap_le_advertisements.elf : $(SRCS_GAP_LE_ADVERTISEMENTS:.c=.o)

le_counter.elf : $(ASMS_APP_BTSTACK:.s=.o)
le_counter.elf : ${BTSTACK_ROOT}/example/le_counter.h $(SRCS_LE_COUNTER:.c=.o)

gatt_battery_query.elf : $(ASMS_APP_BTSTACK:.s=.o)
gatt_battery_query.elf : $(SRCS_GATT_BATTERY_QUERY:.c=.o)

gatt_browser.elf : $(ASMS_APP_BTSTACK:.s=.o)
gatt_browser.elf : $(SRCS_GATT_BROWSER:.c=.o)

le_streamer.elf : $(ASMS_APP_BTSTACK:.s=.o)
le_streamer.elf : ${BTSTACK_ROOT}/example/le_streamer.h $(SRCS_LE_STREAMER:.c=.o)

sm_pairing_central.elf : $(ASMS_APP_BTSTACK:.s=.o)
sm_pairing_central.elf : $(SRCS_SM_PAIRING_CENTRAL:.c=.o)

sm_pairing_peripheral.elf : $(ASMS_APP_BTSTACK:.s=.o)
sm_pairing_peripheral.elf : ${BTSTACK_ROOT}/example/sm_pairing_peripheral.h $(SRCS_SM_PAIRING_PERIPHERAL:.c=.o)


include ../../../makefile.inc
