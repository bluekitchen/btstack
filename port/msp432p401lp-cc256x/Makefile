#
# Defines the part type that this project uses.
#
PART=__MSP432P401R__

#
# The base directory for MSPWare.
#
ROOT=ti/devices/msp432p4xx/

#
# Include the common make definitions.
#
include Makefile.defs

#
# Where to find header files that do not live in the source directory.
#
IPATH+=CMSIS
IPATH+=${ROOT}/driverlib

# BTstack
BTSTACK_ROOT=../..
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods
VPATH += ${BTSTACK_ROOT}/3rd-party/md5
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/segger-rtt
VPATH += ${BTSTACK_ROOT}/3rd-party/yxml
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
IPATH += ${BTSTACK_ROOT}/3rd-party/md5
IPATH += ${BTSTACK_ROOT}/3rd-party/yxml
IPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include
IPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
IPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/include
IPATH += ${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server
IPATH += ${BTSTACK_ROOT}/platform/lwip/
IPATH += ${BTSTACK_ROOT}/platform/lwip/port


IPATH += .
IPATH += ${COMPILER}

BTSTACK_SOURCES = \
${BTSTACK_ROOT}/3rd-party/hxcmod-player/hxcmod.c \
${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods/nao-deceased_by_disease.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/def.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/inet_chksum.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/init.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ip.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/mem.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/memp.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/netif.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/pbuf.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/tcp.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/tcp_in.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/tcp_out.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/timeouts.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/udp.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/acd.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/dhcp.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/etharp.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/icmp.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/ip4.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/ip4_addr.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/ip4_frag.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/netif/ethernet.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/apps/http/altcp_proxyconnect.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/apps/http/fs.c \
${BTSTACK_ROOT}/3rd-party/lwip/core/src/apps/http/httpd.c \
${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server/dhserver.c \
${BTSTACK_ROOT}/3rd-party/md5/md5.c \
${BTSTACK_ROOT}/3rd-party/micro-ecc/uECC.c \
${BTSTACK_ROOT}/3rd-party/segger-rtt/SEGGER_RTT.c \
${BTSTACK_ROOT}/3rd-party/segger-rtt/SEGGER_RTT_printf.c \
${BTSTACK_ROOT}/3rd-party/segger-rtt/SEGGER_RTT_Syscalls_GCC.c \
${BTSTACK_ROOT}/3rd-party/yxml/yxml.c \
${BTSTACK_ROOT}/example/sco_demo_util.c \
${BTSTACK_ROOT}/src/ad_parser.c \
${BTSTACK_ROOT}/src/ble/att_db.c \
${BTSTACK_ROOT}/src/ble/att_dispatch.c \
${BTSTACK_ROOT}/src/ble/att_server.c \
${BTSTACK_ROOT}/src/ble/gatt-service/ancs_client.c \
${BTSTACK_ROOT}/src/ble/gatt-service/battery_service_server.c \
${BTSTACK_ROOT}/src/ble/gatt-service/device_information_service_server.c \
${BTSTACK_ROOT}/src/ble/gatt-service/hids_device.c \
${BTSTACK_ROOT}/src/ble/gatt_client.c \
${BTSTACK_ROOT}/src/ble/le_device_db_memory.c \
${BTSTACK_ROOT}/src/ble/le_device_db_tlv.c \
${BTSTACK_ROOT}/src/ble/sm.c \
${BTSTACK_ROOT}/src/btstack_crypto.c \
${BTSTACK_ROOT}/src/btstack_hid_parser.c \
${BTSTACK_ROOT}/src/btstack_linked_list.c \
${BTSTACK_ROOT}/src/btstack_memory.c \
${BTSTACK_ROOT}/src/btstack_memory_pool.c \
${BTSTACK_ROOT}/src/btstack_ring_buffer.c \
${BTSTACK_ROOT}/src/btstack_run_loop.c \
${BTSTACK_ROOT}/src/btstack_tlv.c \
${BTSTACK_ROOT}/src/btstack_util.c \
${BTSTACK_ROOT}/src/classic/a2dp.c \
${BTSTACK_ROOT}/src/classic/a2dp_sink.c \
${BTSTACK_ROOT}/src/classic/a2dp_source.c \
${BTSTACK_ROOT}/src/classic/avdtp.c \
${BTSTACK_ROOT}/src/classic/avdtp_acceptor.c \
${BTSTACK_ROOT}/src/classic/avdtp_initiator.c \
${BTSTACK_ROOT}/src/classic/avdtp_sink.c \
${BTSTACK_ROOT}/src/classic/avdtp_source.c \
${BTSTACK_ROOT}/src/classic/avdtp_util.c \
${BTSTACK_ROOT}/src/classic/avrcp.c \
${BTSTACK_ROOT}/src/classic/avrcp_browsing_controller.c \
${BTSTACK_ROOT}/src/classic/avrcp_controller.c \
${BTSTACK_ROOT}/src/classic/avrcp_media_item_iterator.c \
${BTSTACK_ROOT}/src/classic/avrcp_target.c \
${BTSTACK_ROOT}/src/classic/bnep.c \
${BTSTACK_ROOT}/src/classic/btstack_cvsd_plc.c \
${BTSTACK_ROOT}/src/classic/btstack_link_key_db_tlv.c \
${BTSTACK_ROOT}/src/classic/btstack_sbc_decoder_bluedroid.c \
${BTSTACK_ROOT}/src/classic/btstack_sbc_encoder_bluedroid.c \
${BTSTACK_ROOT}/src/classic/btstack_sbc_plc.c \
${BTSTACK_ROOT}/src/classic/device_id_server.c \
${BTSTACK_ROOT}/src/classic/goep_client.c \
${BTSTACK_ROOT}/src/classic/hfp.c \
${BTSTACK_ROOT}/src/classic/hfp_ag.c \
${BTSTACK_ROOT}/src/classic/hfp_gsm_model.c \
${BTSTACK_ROOT}/src/classic/hfp_hf.c \
${BTSTACK_ROOT}/src/classic/hfp_msbc.c \
${BTSTACK_ROOT}/src/classic/hid_device.c \
${BTSTACK_ROOT}/src/classic/hid_host.c \
${BTSTACK_ROOT}/src/classic/hsp_ag.c \
${BTSTACK_ROOT}/src/classic/hsp_hs.c \
${BTSTACK_ROOT}/src/classic/obex_parser.c \
${BTSTACK_ROOT}/src/classic/obex_message_builder.c \
${BTSTACK_ROOT}/src/classic/pan.c \
${BTSTACK_ROOT}/src/classic/pbap_client.c \
${BTSTACK_ROOT}/src/classic/rfcomm.c \
${BTSTACK_ROOT}/src/classic/sdp_client.c \
${BTSTACK_ROOT}/src/classic/sdp_client_rfcomm.c \
${BTSTACK_ROOT}/src/classic/sdp_server.c \
${BTSTACK_ROOT}/src/classic/sdp_util.c \
${BTSTACK_ROOT}/src/classic/spp_server.c \
${BTSTACK_ROOT}/src/hci.c \
${BTSTACK_ROOT}/src/hci_cmd.c \
${BTSTACK_ROOT}/src/hci_dump.c \
${BTSTACK_ROOT}/src/hci_transport_h4.c \
${BTSTACK_ROOT}/src/l2cap.c \
${BTSTACK_ROOT}/src/l2cap_signaling.c \
${BTSTACK_ROOT}/platform/embedded/btstack_run_loop_embedded.c \
${BTSTACK_ROOT}/platform/embedded/btstack_stdin_embedded.c \
${BTSTACK_ROOT}/platform/embedded/btstack_tlv_flash_bank.c \
${BTSTACK_ROOT}/platform/embedded/btstack_uart_block_embedded.c \
${BTSTACK_ROOT}/platform/embedded/hci_dump_embedded_stdout.c \
${BTSTACK_ROOT}/platform/embedded/hci_dump_segger_rtt_stdout.c \
${BTSTACK_ROOT}/platform/lwip/port/sys_arch.c \
${BTSTACK_ROOT}/platform/lwip/bnep_lwip.c \

# CC2564B
# - BOOST-CC2564MODA
# - EM Wireless BoosterPacket with CC2564B module
# INIT_SCRIPT = bluetooth_init_cc2564B_1.8_BT_Spec_4.1

# CC2564C
# - EM Wireless BoosterPacket with CC2564C module
INIT_SCRIPT = bluetooth_init_cc2564C_1.4

PORT_SOURCES = \
	${BTSTACK_ROOT}/chipset/cc256x/${INIT_SCRIPT}.o \
 	${BTSTACK_ROOT}/chipset/cc256x/btstack_chipset_cc256x.o \
	hal_flash_bank_msp432.o \
	main.o \
	startup_msp432p401r_${COMPILER}.o \
	system_msp432p401r.o \

include ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/Makefile.inc

BTSTACK_SOURCES += ${SBC_ENCODER}
BTSTACK_SOURCES += ${SBC_DECODER}

EXAMPLES =                      \
        a2dp_source_demo        \
        ancs_client_demo        \
        dut_mode_classic        \
        gap_dedicated_bonding   \
        gap_inquiry             \
        gap_le_advertisements   \
        gatt_battery_query      \
        gatt_browser            \
        gatt_counter            \
        gatt_streamer_server    \
        hid_host_demo           \
        hid_keyboard_demo       \
        hid_mouse_demo          \
        hog_keyboard_demo       \
        hog_mouse_demo          \
        le_streamer_client      \
        led_counter             \
        sdp_bnep_query          \
        sdp_general_query       \
        sdp_rfcomm_query        \
        sm_pairing_central      \
        sm_pairing_peripheral   \
        spp_and_gatt_counter    \
        spp_and_gatt_streamer   \
        spp_counter             \
        spp_streamer            \
        spp_streamer_client     \
        pbap_client_demo        \
        pan_lwip_http_server    \
#        audio_duplex            \
#        a2dp_sink_demo          \
#        hfp_ag_demo             \
#        hfp_hf_demo             \
#        hsp_ag_demo             \
#        hsp_hs_demo             \
#        mod_player              \
#        sine_player             \

GATT_FILES = \
        ancs_client_demo.gatt \
        gatt_counter.gatt \
        gatt_streamer_server.gatt \
        gatt_browser.gatt \
        gatt_battery_query.gatt \
        hog_keyboard_demo.gatt \
        hog_mouse_demo.gatt \
        sm_pairing_peripheral.gatt \
        sm_pairing_central.gatt \
        spp_and_gatt_counter.gatt \
        spp_and_gatt_streamer.gatt \


#
# The default rule, which causes the le_counter example to be built.
#
all: ${COMPILER}
all: \
    linker_script_gcc.ld \
	$(addprefix $(COMPILER)/,$(GATT_FILES:.gatt=.h)) \
	$(addprefix $(COMPILER)/,$(EXAMPLES:=.elf)) \
	$(addprefix $(COMPILER)/,$(EXAMPLES:=.jdebug)) \

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

$(COMPILER)/%.jdebug: ozone.jdebug | $(COMPILER)
	sed -e "s|EXAMPLE|$(basename $(notdir $@))|" $<  > $@

##

# pick up sources
C_SOURCES = $(BTSTACK_SOURCES) $(PORT_SOURCES)
OBJECTS = $(addprefix $(COMPILER)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

LDFLAGS2 = ${CPU} ${FPU} -specs=nano.specs  -lc -lm -lnosys -Wl,--gc-sections

# #
# # The rule for linking the application.
# #
${COMPILER}${SUFFIX}/%.elf: ${OBJECTS} ${COMPILER}/%.o ti/devices/msp432p4xx/driverlib/${COMPILER}/msp432p4xx_driverlib.a
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
