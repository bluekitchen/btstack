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
IPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player
IPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods
IPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include
IPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
IPATH += .
IPATH += ${COMPILER}

BTSTACK_SOURCES = \
a2dp_sink.c \
a2dp_source.c \
ad_parser.c \
ancs_client.c \
att_db.c \
att_dispatch.c \
att_server.c \
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
battery_service_server.c \
btstack_crypto.c \
btstack_hid_parser.c \
btstack_link_key_db_tlv.c \
btstack_linked_list.c \
btstack_memory.c \
btstack_memory_pool.c \
btstack_ring_buffer.c \
btstack_run_loop.c \
btstack_run_loop_embedded.c \
btstack_sbc_decoder_bluedroid.c \
btstack_sbc_encoder_bluedroid.c \
btstack_sbc_plc.c \
btstack_tlv.c \
btstack_tlv_flash_bank.c \
btstack_uart_block_embedded.c \
btstack_util.c \
device_id_server.c \
device_information_service_server.c \
goep_client.c \
gatt_client.c \
hci.c \
hci_cmd.c \
hci_dump.c \
hci_transport_h4.c \
hfp.c \
hfp_ag.c \
hfp_gsm_model.c \
hfp_hf.c \
hfp_msbc.c \
hid_device.c \
hids_device.c \
hsp_ag.c \
hsp_hs.c \
hxcmod.c \
l2cap.c \
l2cap_signaling.c \
le_device_db_memory.c \
le_device_db_tlv.c \
nao-deceased_by_disease.c \
obex_iterator.c \
pbap_client.c \
rfcomm.c \
sco_demo_util.c \
sdp_client.c \
sdp_client_rfcomm.c \
sdp_server.c \
sdp_util.c \
sm.c \
spp_server.c \
uECC.c \

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

include ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/Makefile.inc

BTSTACK_SOURCES += ${SBC_ENCODER}
BTSTACK_SOURCES += ${SBC_DECODER}

EXAMPLES = 					\
	a2dp_sink_demo			\
	a2dp_source_demo       \
	ancs_client_demo		\
	dut_mode_classic        \
	gap_dedicated_bonding	\
	gap_inquiry 			\
	gap_le_advertisements   \
	gatt_battery_query		\
	gatt_browser            \
	hfp_ag_demo             \
	hfp_hf_demo             \
	hid_host_demo           \
	hid_keyboard_demo 	    \
	hid_mouse_demo          \
	hog_keyboard_demo       \
	hog_mouse_demo          \
	hsp_ag_demo             \
	hsp_hs_demo             \
	le_counter              \
	le_streamer				\
	le_streamer_client      \
	pbap_client_demo		\
	sdp_bnep_query 			\
	sdp_general_query		\
	sdp_rfcomm_query		\
	sm_pairing_central      \
	sm_pairing_peripheral   \
	spp_and_le_counter 		\
	spp_and_le_streamer     \
	spp_counter 			\
	spp_streamer			\
	spp_streamer_client     \

GATT_FILES = \
	ancs_client_demo.gatt \
	le_counter.gatt \
	le_streamer.gatt \
	gatt_browser.gatt \
	gatt_battery_query.gatt \
	hog_keyboard_demo.gatt \
	hog_mouse_demo.gatt \
	sm_pairing_peripheral.gatt \
	sm_pairing_central.gatt \
	spp_and_le_counter.gatt \
	spp_and_le_streamer.gatt \

#
# The default rule, which causes the le_counter example to be built.
#
all: ${COMPILER}
all: \
    linker_script_gcc.ld \
	$(addprefix $(COMPILER)/,$(GATT_FILES:.gatt=.h)) \
	$(addprefix $(COMPILER)/,$(EXAMPLES:=.elf)) \

include $(BTSTACK_ROOT)/chipset/cc256x/Makefile.inc

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

# compile .gatt descriptions
${COMPILER}/%.h: %.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

##

# pick up sources
OBJECTS = $(BTSTACK_SOURCES:%.c=$(COMPILER)/%.o) $(PORT_SOURCES:%.o=$(COMPILER)/%.o)

CFLAGSgcc=-DTARGET_IS_MSP432P4XX

LDFLAGS2 = ${CPU} ${FPU} -specs=nano.specs  -lc -lm -lnosys -Wl,--gc-sections

# #
# # The rule for linking the application.
# #
${COMPILER}${SUFFIX}/%.elf: ${OBJECTS} ${COMPILER}/%.o ${ROOT}/driverlib/MSP432P4xx/${COMPILER}/msp432p4xx_driverlib.a
	@ \
	 ldname="linker_script_gcc.ld";                                       \
	 if [ 'x${VERBOSE}' = x ];                                            \
	 then                                                                 \
	     echo "  LD          ${@} ${LNK_SCP}";                            \
	 else                                                                 \
	     echo ${CC} -T linker_script_gcc.ld ${LDFLAGS2} -o ${@} $(filter %.o %.a, ${^}); \
	 fi;                                                                  \
 	${CC} -T linker_script_gcc.ld ${LDFLAGS2} -o ${@} $(filter %.o %.a, ${^})
	@${OBJCOPY} -O binary ${@} ${@:.elf=.bin}

#
# Include the automatically generated dependency files.
#
ifneq (${MAKECMDGOALS},clean)
-include ${wildcard ${COMPILER}/*.d} __dummy__
endif
