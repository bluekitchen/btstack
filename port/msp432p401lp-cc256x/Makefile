#
# Defines the part type that this project uses.
#
PART=__MSP432P401R__

#
# The base directory for MSPWare.
#
ROOT=MSP432_DriverLib_3_10_00_09

#
# Include the common make definitions.
#
include Makefile.defs

#
# Where to find header files that do not live in the source directory.
#
#IPATH=../../../..
IPATH+=${ROOT}/inc/
IPATH+=${ROOT}/inc/CMSIS/
IPATH+=${ROOT}/driverlib/MSP432P4xx

#
# The default rule, which causes the uart_loopback_24mhz_brclk example to be built.
#
all: ${COMPILER}
all: ${COMPILER}/uart_loopback_24mhz_brclk.axf

#
# The rule to clean out all the build products.
#
clean:
	@rm -rf ${COMPILER} ${wildcard *~}

#
# The rule to create the target directory.
#
${COMPILER}:
	@mkdir -p ${COMPILER}

# BTstack
BTSTACK_ROOT=../..
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/segger-rtt
VPATH += ${BTSTACK_ROOT}/example
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/chipset/cc256x
IPATH += ${BTSTACK_ROOT}/3rd-party/segger-rtt
IPATH += ${BTSTACK_ROOT}/src
IPATH += ${BTSTACK_ROOT}/platform/embedded
IPATH += ${BTSTACK_ROOT}/chipset/cc256x
IPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include
IPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
IPATH += .
IPATH += ${COMPILER}

BTSTACK_SOURCES = \
ad_parser.c \
ancs_client.c \
att_db.c \
att_dispatch.c \
att_server.c \
battery_service_server.c \
btstack_crypto.c \
btstack_linked_list.c \
btstack_memory.c \
btstack_memory_pool.c \
btstack_ring_buffer.c \
btstack_run_loop.c \
btstack_run_loop_embedded.c \
btstack_tlv.c \
btstack_uart_block_embedded.c \
btstack_util.c \
device_information_service_server.c \
hids_device.c \
gatt_client.c \
hci.c \
hci_cmd.c \
hci_dump.c \
hci_transport_h4.c \
l2cap.c \
l2cap_signaling.c \
le_device_db_memory.c \
sm.c \
uECC.c \
btstack_tlv_flash_bank.c \
le_device_db_tlv.c \
btstack_link_key_db_tlv.c \
a2dp_sink.c \
a2dp_source.c \
avdtp.c \
avdtp_acceptor.c \
avdtp_initiator.c \
avdtp_sink.c \
avdtp_source.c \
avdtp_util.c \
avrcp.c \
avrcp_browsing_controller.c \
avrcp_controller.c \
avrcp_media_item_iterator.c \
avrcp_target.c \
sdp_util.c \
sdp_server.c \
sdp_client.c \
sdp_client_rfcomm.c \
spp_server.c \
btstack_sbc_decoder_bluedroid.c \
btstack_sbc_encoder_bluedroid.c \
btstack_sbc_plc.c \
hxcmod.c \
nao-deceased_by_disease.c \
hfp_ag.c \
hfp_hf.c \
hfp.c \
hfp_gsm_model.c \
hfp_msbc.c \
hsp_hs.c \
hsp_ag.c \
hid_device.c \
rfcomm.c \
sco_demo_util.c \
btstack_hid_parser.c \
device_id_server.c \
obex_iterator.c \
pbap_client.c \

PORT_SOURCES = \
	btstack_chipset_cc256x.o \
	bluetooth_init_cc2564B_1.6_BT_Spec_4.1.o \
	hal_flash_bank_msp432.o \
	main.o \
	startup_msp432p401r_${COMPILER}.o \
	system_msp432p401r.o \
	SEGGER_RTT.o \
	SEGGER_RTT_Syscalls_GCC.o \
	SEGGER_RTT_printf.o \
	le_counter.o \

include ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/Makefile.inc

BTSTACK_SOURCES += ${SBC_ENCODER}
BTSTACK_SOURCES += ${SBC_DECODER}

# compile .gatt descriptions
${COMPILER}/%.h: %.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

OBJECTS = $(BTSTACK_SOURCES:%.c=$(COMPILER)/%.o) $(PORT_SOURCES:%.o=$(COMPILER)/%.o)

# ${COMPILER}/startup_msp432p401r_${COMPILER}.o \
# ${COMPILER}/system_msp432p401r.o \
# ${COMPILER}/main.o \
# ${COMPILER}/SEGGER_RTT.o \
# ${COMPILER}/SEGGER_RTT_Syscalls_GCC.o \
# ${COMPILER}/SEGGER_RTT_printf.o \
# ${COMPILER}/hci_dump.o \
# ${COMPILER}/btstack_linked_list.o \
# ${COMPILER}/btstack_memory.o \
# ${COMPILER}/btstack_memory_pool.o \
# ${COMPILER}/btstack_run_loop.o \
# ${COMPILER}/btstack_run_loop_embedded.o \
# ${COMPILER}/btstack_uart_block_embedded.o \
# ${COMPILER}/btstack_chipset_cc256x.o \
# ${COMPILER}/hci_transport_h4.o \
# ${COMPILER}/btstack_util.o \
# ${COMPILER}/hci.o \
# ${COMPILER}/hci_cmd.o \
# ${COMPILER}/ad_parser.o \
# ${COMPILER}/bluetooth_init_cc2564B_1.6_BT_Spec_4.1.o \

#
# Rules for building the uart_loopback_24mhz_brclk example.
#
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/le_counter.h
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${OBJECTS}
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${ROOT}/driverlib/MSP432P4xx/${COMPILER}/msp432p4xx_driverlib.a
${COMPILER}/uart_loopback_24mhz_brclk.axf: linker_script_gcc.ld
SCATTERgcc_uart_loopback_24mhz_brclk=linker_script_gcc.ld
ENTRY_uart_loopback_24mhz_brclk=resetISR
CFLAGSgcc=-DTARGET_IS_MSP432P4XX

#
# Include the automatically generated dependency files.
#
ifneq (${MAKECMDGOALS},clean)
-include ${wildcard ${COMPILER}/*.d} __dummy__
endif
