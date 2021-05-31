#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Include project Makefile
ifeq "${IGNORE_LOCAL}" "TRUE"
# do not include local makefile. User is passing all local related variables already
else
include Makefile
# Include makefile containing local settings
ifeq "$(wildcard nbproject/Makefile-local-default.mk)" "nbproject/Makefile-local-default.mk"
include nbproject/Makefile-local-default.mk
endif
endif

# Environment
MKDIR=mkdir -p
RM=rm -f 
MV=mv 
CP=cp 

# Macros
CND_CONF=default
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
IMAGE_TYPE=debug
OUTPUT_SUFFIX=elf
DEBUGGABLE_SUFFIX=elf
FINAL_IMAGE=dist/${CND_CONF}/${IMAGE_TYPE}/app.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
else
IMAGE_TYPE=production
OUTPUT_SUFFIX=hex
DEBUGGABLE_SUFFIX=elf
FINAL_IMAGE=dist/${CND_CONF}/${IMAGE_TYPE}/app.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
endif

ifeq ($(COMPARE_BUILD), true)
COMPARISON_BUILD=-mafrlcsj
else
COMPARISON_BUILD=
endif

ifdef SUB_IMAGE_ADDRESS

else
SUB_IMAGE_ADDRESS_COMMAND=
endif

# Object Directory
OBJECTDIR=build/${CND_CONF}/${IMAGE_TYPE}

# Distribution Directory
DISTDIR=dist/${CND_CONF}/${IMAGE_TYPE}

# Source Files Quoted if spaced
SOURCEFILES_QUOTED_IF_SPACED=../src/system_config/bt_audio_dk/system_init.c ../src/system_config/bt_audio_dk/system_tasks.c ../src/btstack_port.c ../src/app_debug.c ../src/app.c ../src/main.c ../../../example/spp_counter.c ../../../3rd-party/bluedroid/decoder/srce/alloc.c ../../../3rd-party/bluedroid/decoder/srce/bitalloc-sbc.c ../../../3rd-party/bluedroid/decoder/srce/bitalloc.c ../../../3rd-party/bluedroid/decoder/srce/bitstream-decode.c ../../../3rd-party/bluedroid/decoder/srce/decoder-oina.c ../../../3rd-party/bluedroid/decoder/srce/decoder-private.c ../../../3rd-party/bluedroid/decoder/srce/decoder-sbc.c ../../../3rd-party/bluedroid/decoder/srce/dequant.c ../../../3rd-party/bluedroid/decoder/srce/framing-sbc.c ../../../3rd-party/bluedroid/decoder/srce/framing.c ../../../3rd-party/bluedroid/decoder/srce/oi_codec_version.c ../../../3rd-party/bluedroid/decoder/srce/synthesis-8-generated.c ../../../3rd-party/bluedroid/decoder/srce/synthesis-dct8.c ../../../3rd-party/bluedroid/decoder/srce/synthesis-sbc.c ../../../3rd-party/bluedroid/encoder/srce/sbc_analysis.c ../../../3rd-party/bluedroid/encoder/srce/sbc_dct.c ../../../3rd-party/bluedroid/encoder/srce/sbc_dct_coeffs.c ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_mono.c ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_ste.c ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_coeffs.c ../../../3rd-party/bluedroid/encoder/srce/sbc_encoder.c ../../../3rd-party/bluedroid/encoder/srce/sbc_packing.c ../../../3rd-party/hxcmod-player/mods/nao-deceased_by_disease.c ../../../3rd-party/hxcmod-player/hxcmod.c ../../../3rd-party/md5/md5.c ../../../3rd-party/micro-ecc/uECC.c ../../../3rd-party/yxml/yxml.c ../../../chipset/csr/btstack_chipset_csr.c ../../../platform/embedded/btstack_run_loop_embedded.c ../../../platform/embedded/btstack_uart_block_embedded.c ../../../src/ble/gatt-service/battery_service_server.c ../../../src/ble/gatt-service/device_information_service_server.c ../../../src/ble/gatt-service/hids_device.c ../../../src/ble/att_db.c ../../../src/ble/att_dispatch.c ../../../src/ble/att_server.c ../../../src/ble/le_device_db_memory.c ../../../src/ble/sm.c ../../../src/ble/ancs_client.c ../../../src/ble/gatt_client.c ../../../src/classic/btstack_link_key_db_memory.c ../../../src/classic/sdp_client.c ../../../src/classic/sdp_client_rfcomm.c ../../../src/classic/sdp_server.c ../../../src/classic/sdp_util.c ../../../src/classic/spp_server.c ../../../src/classic/a2dp_sink.c ../../../src/classic/a2dp_source.c ../../../src/classic/avdtp.c ../../../src/classic/avdtp_acceptor.c ../../../src/classic/avdtp_initiator.c ../../../src/classic/avdtp_sink.c ../../../src/classic/avdtp_source.c ../../../src/classic/avdtp_util.c ../../../src/classic/avrcp.c ../../../src/classic/avrcp_browsing_controller.c ../../../src/classic/avrcp_controller.c ../../../src/classic/avrcp_media_item_iterator.c ../../../src/classic/avrcp_target.c ../../../src/classic/bnep.c ../../../src/classic/btstack_cvsd_plc.c ../../../src/classic/btstack_sbc_decoder_bluedroid.c ../../../src/classic/btstack_sbc_encoder_bluedroid.c ../../../src/classic/btstack_sbc_plc.c ../../../src/classic/device_id_server.c ../../../src/classic/goep_client.c ../../../src/classic/hfp.c ../../../src/classic/hfp_ag.c ../../../src/classic/hfp_gsm_model.c ../../../src/classic/hfp_hf.c ../../../src/classic/hfp_msbc.c ../../../src/classic/hid_device.c ../../../src/classic/hsp_ag.c ../../../src/classic/hsp_hs.c ../../../src/classic/obex_iterator.c ../../../src/classic/pan.c ../../../src/classic/pbap_client.c ../../../src/btstack_memory.c ../../../src/hci.c ../../../src/hci_cmd.c ../../../src/hci_dump.c ../../../src/l2cap.c ../../../src/l2cap_signaling.c ../../../src/btstack_linked_list.c ../../../src/btstack_memory_pool.c ../../../src/classic/rfcomm.c ../../../src/btstack_run_loop.c ../../../src/btstack_util.c ../../../src/hci_transport_h4.c ../../../src/hci_transport_h5.c ../../../src/btstack_slip.c ../../../src/ad_parser.c ../../../src/btstack_tlv.c ../../../src/btstack_crypto.c ../../../src/btstack_hid_parser.c ../../../../driver/tmr/src/dynamic/drv_tmr.c ../../../../system/clk/src/sys_clk.c ../../../../system/clk/src/sys_clk_pic32mx.c ../../../../system/devcon/src/sys_devcon.c ../../../../system/devcon/src/sys_devcon_pic32mx.c ../../../../system/int/src/sys_int_pic32.c ../../../../system/ports/src/sys_ports.c ../../../src/btstack_uart_slip_wrapper.c

# Object Files Quoted if spaced
OBJECTFILES_QUOTED_IF_SPACED=${OBJECTDIR}/_ext/101891878/system_init.o ${OBJECTDIR}/_ext/101891878/system_tasks.o ${OBJECTDIR}/_ext/1360937237/btstack_port.o ${OBJECTDIR}/_ext/1360937237/app_debug.o ${OBJECTDIR}/_ext/1360937237/app.o ${OBJECTDIR}/_ext/1360937237/main.o ${OBJECTDIR}/_ext/97075643/spp_counter.o ${OBJECTDIR}/_ext/770672057/alloc.o ${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o ${OBJECTDIR}/_ext/770672057/bitalloc.o ${OBJECTDIR}/_ext/770672057/bitstream-decode.o ${OBJECTDIR}/_ext/770672057/decoder-oina.o ${OBJECTDIR}/_ext/770672057/decoder-private.o ${OBJECTDIR}/_ext/770672057/decoder-sbc.o ${OBJECTDIR}/_ext/770672057/dequant.o ${OBJECTDIR}/_ext/770672057/framing-sbc.o ${OBJECTDIR}/_ext/770672057/framing.o ${OBJECTDIR}/_ext/770672057/oi_codec_version.o ${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o ${OBJECTDIR}/_ext/770672057/synthesis-dct8.o ${OBJECTDIR}/_ext/770672057/synthesis-sbc.o ${OBJECTDIR}/_ext/1907061729/sbc_analysis.o ${OBJECTDIR}/_ext/1907061729/sbc_dct.o ${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o ${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o ${OBJECTDIR}/_ext/1907061729/sbc_encoder.o ${OBJECTDIR}/_ext/1907061729/sbc_packing.o ${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o ${OBJECTDIR}/_ext/835724193/hxcmod.o ${OBJECTDIR}/_ext/762785730/md5.o ${OBJECTDIR}/_ext/34712644/uECC.o ${OBJECTDIR}/_ext/2123824702/yxml.o ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o ${OBJECTDIR}/_ext/524132624/battery_service_server.o ${OBJECTDIR}/_ext/524132624/device_information_service_server.o ${OBJECTDIR}/_ext/524132624/hids_device.o ${OBJECTDIR}/_ext/534563071/att_db.o ${OBJECTDIR}/_ext/534563071/att_dispatch.o ${OBJECTDIR}/_ext/534563071/att_server.o ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o ${OBJECTDIR}/_ext/534563071/sm.o ${OBJECTDIR}/_ext/534563071/ancs_client.o ${OBJECTDIR}/_ext/534563071/gatt_client.o ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o ${OBJECTDIR}/_ext/1386327864/sdp_client.o ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o ${OBJECTDIR}/_ext/1386327864/sdp_server.o ${OBJECTDIR}/_ext/1386327864/sdp_util.o ${OBJECTDIR}/_ext/1386327864/spp_server.o ${OBJECTDIR}/_ext/1386327864/a2dp_sink.o ${OBJECTDIR}/_ext/1386327864/a2dp_source.o ${OBJECTDIR}/_ext/1386327864/avdtp.o ${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o ${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o ${OBJECTDIR}/_ext/1386327864/avdtp_sink.o ${OBJECTDIR}/_ext/1386327864/avdtp_source.o ${OBJECTDIR}/_ext/1386327864/avdtp_util.o ${OBJECTDIR}/_ext/1386327864/avrcp.o ${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o ${OBJECTDIR}/_ext/1386327864/avrcp_controller.o ${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o ${OBJECTDIR}/_ext/1386327864/avrcp_target.o ${OBJECTDIR}/_ext/1386327864/bnep.o ${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o ${OBJECTDIR}/_ext/1386327864/device_id_server.o ${OBJECTDIR}/_ext/1386327864/goep_client.o ${OBJECTDIR}/_ext/1386327864/hfp.o ${OBJECTDIR}/_ext/1386327864/hfp_ag.o ${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o ${OBJECTDIR}/_ext/1386327864/hfp_hf.o ${OBJECTDIR}/_ext/1386327864/hfp_msbc.o ${OBJECTDIR}/_ext/1386327864/hid_device.o ${OBJECTDIR}/_ext/1386327864/hsp_ag.o ${OBJECTDIR}/_ext/1386327864/hsp_hs.o ${OBJECTDIR}/_ext/1386327864/obex_iterator.o ${OBJECTDIR}/_ext/1386327864/pan.o ${OBJECTDIR}/_ext/1386327864/pbap_client.o ${OBJECTDIR}/_ext/1386528437/btstack_memory.o ${OBJECTDIR}/_ext/1386528437/hci.o ${OBJECTDIR}/_ext/1386528437/hci_cmd.o ${OBJECTDIR}/_ext/1386528437/hci_dump.o ${OBJECTDIR}/_ext/1386528437/l2cap.o ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o ${OBJECTDIR}/_ext/1386327864/rfcomm.o ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o ${OBJECTDIR}/_ext/1386528437/btstack_util.o ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o ${OBJECTDIR}/_ext/1386528437/btstack_slip.o ${OBJECTDIR}/_ext/1386528437/ad_parser.o ${OBJECTDIR}/_ext/1386528437/btstack_tlv.o ${OBJECTDIR}/_ext/1386528437/btstack_crypto.o ${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o ${OBJECTDIR}/_ext/1880736137/drv_tmr.o ${OBJECTDIR}/_ext/1112166103/sys_clk.o ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o ${OBJECTDIR}/_ext/1510368962/sys_devcon.o ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o ${OBJECTDIR}/_ext/2147153351/sys_ports.o ${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o
POSSIBLE_DEPFILES=${OBJECTDIR}/_ext/101891878/system_init.o.d ${OBJECTDIR}/_ext/101891878/system_tasks.o.d ${OBJECTDIR}/_ext/1360937237/btstack_port.o.d ${OBJECTDIR}/_ext/1360937237/app_debug.o.d ${OBJECTDIR}/_ext/1360937237/app.o.d ${OBJECTDIR}/_ext/1360937237/main.o.d ${OBJECTDIR}/_ext/97075643/spp_counter.o.d ${OBJECTDIR}/_ext/770672057/alloc.o.d ${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o.d ${OBJECTDIR}/_ext/770672057/bitalloc.o.d ${OBJECTDIR}/_ext/770672057/bitstream-decode.o.d ${OBJECTDIR}/_ext/770672057/decoder-oina.o.d ${OBJECTDIR}/_ext/770672057/decoder-private.o.d ${OBJECTDIR}/_ext/770672057/decoder-sbc.o.d ${OBJECTDIR}/_ext/770672057/dequant.o.d ${OBJECTDIR}/_ext/770672057/framing-sbc.o.d ${OBJECTDIR}/_ext/770672057/framing.o.d ${OBJECTDIR}/_ext/770672057/oi_codec_version.o.d ${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o.d ${OBJECTDIR}/_ext/770672057/synthesis-dct8.o.d ${OBJECTDIR}/_ext/770672057/synthesis-sbc.o.d ${OBJECTDIR}/_ext/1907061729/sbc_analysis.o.d ${OBJECTDIR}/_ext/1907061729/sbc_dct.o.d ${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o.d ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o.d ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o.d ${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o.d ${OBJECTDIR}/_ext/1907061729/sbc_encoder.o.d ${OBJECTDIR}/_ext/1907061729/sbc_packing.o.d ${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o.d ${OBJECTDIR}/_ext/835724193/hxcmod.o.d ${OBJECTDIR}/_ext/762785730/md5.o.d ${OBJECTDIR}/_ext/34712644/uECC.o.d ${OBJECTDIR}/_ext/2123824702/yxml.o.d ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d ${OBJECTDIR}/_ext/524132624/battery_service_server.o.d ${OBJECTDIR}/_ext/524132624/device_information_service_server.o.d ${OBJECTDIR}/_ext/524132624/hids_device.o.d ${OBJECTDIR}/_ext/534563071/att_db.o.d ${OBJECTDIR}/_ext/534563071/att_dispatch.o.d ${OBJECTDIR}/_ext/534563071/att_server.o.d ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d ${OBJECTDIR}/_ext/534563071/sm.o.d ${OBJECTDIR}/_ext/534563071/ancs_client.o.d ${OBJECTDIR}/_ext/534563071/gatt_client.o.d ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d ${OBJECTDIR}/_ext/1386327864/sdp_client.o.d ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d ${OBJECTDIR}/_ext/1386327864/sdp_server.o.d ${OBJECTDIR}/_ext/1386327864/sdp_util.o.d ${OBJECTDIR}/_ext/1386327864/spp_server.o.d ${OBJECTDIR}/_ext/1386327864/a2dp_sink.o.d ${OBJECTDIR}/_ext/1386327864/a2dp_source.o.d ${OBJECTDIR}/_ext/1386327864/avdtp.o.d ${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o.d ${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o.d ${OBJECTDIR}/_ext/1386327864/avdtp_sink.o.d ${OBJECTDIR}/_ext/1386327864/avdtp_source.o.d ${OBJECTDIR}/_ext/1386327864/avdtp_util.o.d ${OBJECTDIR}/_ext/1386327864/avrcp.o.d ${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o.d ${OBJECTDIR}/_ext/1386327864/avrcp_controller.o.d ${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o.d ${OBJECTDIR}/_ext/1386327864/avrcp_target.o.d ${OBJECTDIR}/_ext/1386327864/bnep.o.d ${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o.d ${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o.d ${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o.d ${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o.d ${OBJECTDIR}/_ext/1386327864/device_id_server.o.d ${OBJECTDIR}/_ext/1386327864/goep_client.o.d ${OBJECTDIR}/_ext/1386327864/hfp.o.d ${OBJECTDIR}/_ext/1386327864/hfp_ag.o.d ${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o.d ${OBJECTDIR}/_ext/1386327864/hfp_hf.o.d ${OBJECTDIR}/_ext/1386327864/hfp_msbc.o.d ${OBJECTDIR}/_ext/1386327864/hid_device.o.d ${OBJECTDIR}/_ext/1386327864/hsp_ag.o.d ${OBJECTDIR}/_ext/1386327864/hsp_hs.o.d ${OBJECTDIR}/_ext/1386327864/obex_iterator.o.d ${OBJECTDIR}/_ext/1386327864/pan.o.d ${OBJECTDIR}/_ext/1386327864/pbap_client.o.d ${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d ${OBJECTDIR}/_ext/1386528437/hci.o.d ${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d ${OBJECTDIR}/_ext/1386528437/hci_dump.o.d ${OBJECTDIR}/_ext/1386528437/l2cap.o.d ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d ${OBJECTDIR}/_ext/1386327864/rfcomm.o.d ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d ${OBJECTDIR}/_ext/1386528437/btstack_util.o.d ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d ${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d ${OBJECTDIR}/_ext/1386528437/ad_parser.o.d ${OBJECTDIR}/_ext/1386528437/btstack_tlv.o.d ${OBJECTDIR}/_ext/1386528437/btstack_crypto.o.d ${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o.d ${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d ${OBJECTDIR}/_ext/1112166103/sys_clk.o.d ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d ${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d ${OBJECTDIR}/_ext/2147153351/sys_ports.o.d ${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o.d

# Object Files
OBJECTFILES=${OBJECTDIR}/_ext/101891878/system_init.o ${OBJECTDIR}/_ext/101891878/system_tasks.o ${OBJECTDIR}/_ext/1360937237/btstack_port.o ${OBJECTDIR}/_ext/1360937237/app_debug.o ${OBJECTDIR}/_ext/1360937237/app.o ${OBJECTDIR}/_ext/1360937237/main.o ${OBJECTDIR}/_ext/97075643/spp_counter.o ${OBJECTDIR}/_ext/770672057/alloc.o ${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o ${OBJECTDIR}/_ext/770672057/bitalloc.o ${OBJECTDIR}/_ext/770672057/bitstream-decode.o ${OBJECTDIR}/_ext/770672057/decoder-oina.o ${OBJECTDIR}/_ext/770672057/decoder-private.o ${OBJECTDIR}/_ext/770672057/decoder-sbc.o ${OBJECTDIR}/_ext/770672057/dequant.o ${OBJECTDIR}/_ext/770672057/framing-sbc.o ${OBJECTDIR}/_ext/770672057/framing.o ${OBJECTDIR}/_ext/770672057/oi_codec_version.o ${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o ${OBJECTDIR}/_ext/770672057/synthesis-dct8.o ${OBJECTDIR}/_ext/770672057/synthesis-sbc.o ${OBJECTDIR}/_ext/1907061729/sbc_analysis.o ${OBJECTDIR}/_ext/1907061729/sbc_dct.o ${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o ${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o ${OBJECTDIR}/_ext/1907061729/sbc_encoder.o ${OBJECTDIR}/_ext/1907061729/sbc_packing.o ${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o ${OBJECTDIR}/_ext/835724193/hxcmod.o ${OBJECTDIR}/_ext/762785730/md5.o ${OBJECTDIR}/_ext/34712644/uECC.o ${OBJECTDIR}/_ext/2123824702/yxml.o ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o ${OBJECTDIR}/_ext/524132624/battery_service_server.o ${OBJECTDIR}/_ext/524132624/device_information_service_server.o ${OBJECTDIR}/_ext/524132624/hids_device.o ${OBJECTDIR}/_ext/534563071/att_db.o ${OBJECTDIR}/_ext/534563071/att_dispatch.o ${OBJECTDIR}/_ext/534563071/att_server.o ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o ${OBJECTDIR}/_ext/534563071/sm.o ${OBJECTDIR}/_ext/534563071/ancs_client.o ${OBJECTDIR}/_ext/534563071/gatt_client.o ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o ${OBJECTDIR}/_ext/1386327864/sdp_client.o ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o ${OBJECTDIR}/_ext/1386327864/sdp_server.o ${OBJECTDIR}/_ext/1386327864/sdp_util.o ${OBJECTDIR}/_ext/1386327864/spp_server.o ${OBJECTDIR}/_ext/1386327864/a2dp_sink.o ${OBJECTDIR}/_ext/1386327864/a2dp_source.o ${OBJECTDIR}/_ext/1386327864/avdtp.o ${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o ${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o ${OBJECTDIR}/_ext/1386327864/avdtp_sink.o ${OBJECTDIR}/_ext/1386327864/avdtp_source.o ${OBJECTDIR}/_ext/1386327864/avdtp_util.o ${OBJECTDIR}/_ext/1386327864/avrcp.o ${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o ${OBJECTDIR}/_ext/1386327864/avrcp_controller.o ${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o ${OBJECTDIR}/_ext/1386327864/avrcp_target.o ${OBJECTDIR}/_ext/1386327864/bnep.o ${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o ${OBJECTDIR}/_ext/1386327864/device_id_server.o ${OBJECTDIR}/_ext/1386327864/goep_client.o ${OBJECTDIR}/_ext/1386327864/hfp.o ${OBJECTDIR}/_ext/1386327864/hfp_ag.o ${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o ${OBJECTDIR}/_ext/1386327864/hfp_hf.o ${OBJECTDIR}/_ext/1386327864/hfp_msbc.o ${OBJECTDIR}/_ext/1386327864/hid_device.o ${OBJECTDIR}/_ext/1386327864/hsp_ag.o ${OBJECTDIR}/_ext/1386327864/hsp_hs.o ${OBJECTDIR}/_ext/1386327864/obex_iterator.o ${OBJECTDIR}/_ext/1386327864/pan.o ${OBJECTDIR}/_ext/1386327864/pbap_client.o ${OBJECTDIR}/_ext/1386528437/btstack_memory.o ${OBJECTDIR}/_ext/1386528437/hci.o ${OBJECTDIR}/_ext/1386528437/hci_cmd.o ${OBJECTDIR}/_ext/1386528437/hci_dump.o ${OBJECTDIR}/_ext/1386528437/l2cap.o ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o ${OBJECTDIR}/_ext/1386327864/rfcomm.o ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o ${OBJECTDIR}/_ext/1386528437/btstack_util.o ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o ${OBJECTDIR}/_ext/1386528437/btstack_slip.o ${OBJECTDIR}/_ext/1386528437/ad_parser.o ${OBJECTDIR}/_ext/1386528437/btstack_tlv.o ${OBJECTDIR}/_ext/1386528437/btstack_crypto.o ${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o ${OBJECTDIR}/_ext/1880736137/drv_tmr.o ${OBJECTDIR}/_ext/1112166103/sys_clk.o ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o ${OBJECTDIR}/_ext/1510368962/sys_devcon.o ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o ${OBJECTDIR}/_ext/2147153351/sys_ports.o ${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o

# Source Files
SOURCEFILES=../src/system_config/bt_audio_dk/system_init.c ../src/system_config/bt_audio_dk/system_tasks.c ../src/btstack_port.c ../src/app_debug.c ../src/app.c ../src/main.c ../../../example/spp_counter.c ../../../3rd-party/bluedroid/decoder/srce/alloc.c ../../../3rd-party/bluedroid/decoder/srce/bitalloc-sbc.c ../../../3rd-party/bluedroid/decoder/srce/bitalloc.c ../../../3rd-party/bluedroid/decoder/srce/bitstream-decode.c ../../../3rd-party/bluedroid/decoder/srce/decoder-oina.c ../../../3rd-party/bluedroid/decoder/srce/decoder-private.c ../../../3rd-party/bluedroid/decoder/srce/decoder-sbc.c ../../../3rd-party/bluedroid/decoder/srce/dequant.c ../../../3rd-party/bluedroid/decoder/srce/framing-sbc.c ../../../3rd-party/bluedroid/decoder/srce/framing.c ../../../3rd-party/bluedroid/decoder/srce/oi_codec_version.c ../../../3rd-party/bluedroid/decoder/srce/synthesis-8-generated.c ../../../3rd-party/bluedroid/decoder/srce/synthesis-dct8.c ../../../3rd-party/bluedroid/decoder/srce/synthesis-sbc.c ../../../3rd-party/bluedroid/encoder/srce/sbc_analysis.c ../../../3rd-party/bluedroid/encoder/srce/sbc_dct.c ../../../3rd-party/bluedroid/encoder/srce/sbc_dct_coeffs.c ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_mono.c ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_ste.c ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_coeffs.c ../../../3rd-party/bluedroid/encoder/srce/sbc_encoder.c ../../../3rd-party/bluedroid/encoder/srce/sbc_packing.c ../../../3rd-party/hxcmod-player/mods/nao-deceased_by_disease.c ../../../3rd-party/hxcmod-player/hxcmod.c ../../../3rd-party/md5/md5.c ../../../3rd-party/micro-ecc/uECC.c ../../../3rd-party/yxml/yxml.c ../../../chipset/csr/btstack_chipset_csr.c ../../../platform/embedded/btstack_run_loop_embedded.c ../../../platform/embedded/btstack_uart_block_embedded.c ../../../src/ble/gatt-service/battery_service_server.c ../../../src/ble/gatt-service/device_information_service_server.c ../../../src/ble/gatt-service/hids_device.c ../../../src/ble/att_db.c ../../../src/ble/att_dispatch.c ../../../src/ble/att_server.c ../../../src/ble/le_device_db_memory.c ../../../src/ble/sm.c ../../../src/ble/ancs_client.c ../../../src/ble/gatt_client.c ../../../src/classic/btstack_link_key_db_memory.c ../../../src/classic/sdp_client.c ../../../src/classic/sdp_client_rfcomm.c ../../../src/classic/sdp_server.c ../../../src/classic/sdp_util.c ../../../src/classic/spp_server.c ../../../src/classic/a2dp_sink.c ../../../src/classic/a2dp_source.c ../../../src/classic/avdtp.c ../../../src/classic/avdtp_acceptor.c ../../../src/classic/avdtp_initiator.c ../../../src/classic/avdtp_sink.c ../../../src/classic/avdtp_source.c ../../../src/classic/avdtp_util.c ../../../src/classic/avrcp.c ../../../src/classic/avrcp_browsing_controller.c ../../../src/classic/avrcp_controller.c ../../../src/classic/avrcp_media_item_iterator.c ../../../src/classic/avrcp_target.c ../../../src/classic/bnep.c ../../../src/classic/btstack_cvsd_plc.c ../../../src/classic/btstack_sbc_decoder_bluedroid.c ../../../src/classic/btstack_sbc_encoder_bluedroid.c ../../../src/classic/btstack_sbc_plc.c ../../../src/classic/device_id_server.c ../../../src/classic/goep_client.c ../../../src/classic/hfp.c ../../../src/classic/hfp_ag.c ../../../src/classic/hfp_gsm_model.c ../../../src/classic/hfp_hf.c ../../../src/classic/hfp_msbc.c ../../../src/classic/hid_device.c ../../../src/classic/hsp_ag.c ../../../src/classic/hsp_hs.c ../../../src/classic/obex_iterator.c ../../../src/classic/pan.c ../../../src/classic/pbap_client.c ../../../src/btstack_memory.c ../../../src/hci.c ../../../src/hci_cmd.c ../../../src/hci_dump.c ../../../src/l2cap.c ../../../src/l2cap_signaling.c ../../../src/btstack_linked_list.c ../../../src/btstack_memory_pool.c ../../../src/classic/rfcomm.c ../../../src/btstack_run_loop.c ../../../src/btstack_util.c ../../../src/hci_transport_h4.c ../../../src/hci_transport_h5.c ../../../src/btstack_slip.c ../../../src/ad_parser.c ../../../src/btstack_tlv.c ../../../src/btstack_crypto.c ../../../src/btstack_hid_parser.c ../../../../driver/tmr/src/dynamic/drv_tmr.c ../../../../system/clk/src/sys_clk.c ../../../../system/clk/src/sys_clk_pic32mx.c ../../../../system/devcon/src/sys_devcon.c ../../../../system/devcon/src/sys_devcon_pic32mx.c ../../../../system/int/src/sys_int_pic32.c ../../../../system/ports/src/sys_ports.c ../../../src/btstack_uart_slip_wrapper.c



CFLAGS=
ASFLAGS=
LDLIBSOPTIONS=

############# Tool locations ##########################################
# If you copy a project from one host to another, the path where the  #
# compiler is installed may be different.                             #
# If you open this project with MPLAB X in the new host, this         #
# makefile will be regenerated and the paths will be corrected.       #
#######################################################################
# fixDeps replaces a bunch of sed/cat/printf statements that slow down the build
FIXDEPS=fixDeps

.build-conf:  ${BUILD_SUBPROJECTS}
ifneq ($(INFORMATION_MESSAGE), )
	@echo $(INFORMATION_MESSAGE)
endif
	${MAKE}  -f nbproject/Makefile-default.mk dist/${CND_CONF}/${IMAGE_TYPE}/app.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}

MP_PROCESSOR_OPTION=32MX450F256L
MP_LINKER_FILE_OPTION=
# ------------------------------------------------------------------------------------
# Rules for buildStep: assemble
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: assembleWithPreprocess
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: compile
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
${OBJECTDIR}/_ext/101891878/system_init.o: ../src/system_config/bt_audio_dk/system_init.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/101891878" 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_init.o.d 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_init.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/101891878/system_init.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/101891878/system_init.o.d" -o ${OBJECTDIR}/_ext/101891878/system_init.o ../src/system_config/bt_audio_dk/system_init.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/101891878/system_tasks.o: ../src/system_config/bt_audio_dk/system_tasks.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/101891878" 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_tasks.o.d 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_tasks.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/101891878/system_tasks.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/101891878/system_tasks.o.d" -o ${OBJECTDIR}/_ext/101891878/system_tasks.o ../src/system_config/bt_audio_dk/system_tasks.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1360937237/btstack_port.o: ../src/btstack_port.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/btstack_port.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/btstack_port.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/btstack_port.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1360937237/btstack_port.o.d" -o ${OBJECTDIR}/_ext/1360937237/btstack_port.o ../src/btstack_port.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1360937237/app_debug.o: ../src/app_debug.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app_debug.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app_debug.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/app_debug.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1360937237/app_debug.o.d" -o ${OBJECTDIR}/_ext/1360937237/app_debug.o ../src/app_debug.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1360937237/app.o: ../src/app.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/app.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1360937237/app.o.d" -o ${OBJECTDIR}/_ext/1360937237/app.o ../src/app.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1360937237/main.o: ../src/main.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/main.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1360937237/main.o.d" -o ${OBJECTDIR}/_ext/1360937237/main.o ../src/main.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/97075643/spp_counter.o: ../../../example/spp_counter.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/97075643" 
	@${RM} ${OBJECTDIR}/_ext/97075643/spp_counter.o.d 
	@${RM} ${OBJECTDIR}/_ext/97075643/spp_counter.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/97075643/spp_counter.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/97075643/spp_counter.o.d" -o ${OBJECTDIR}/_ext/97075643/spp_counter.o ../../../example/spp_counter.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/alloc.o: ../../../3rd-party/bluedroid/decoder/srce/alloc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/alloc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/alloc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/alloc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/alloc.o.d" -o ${OBJECTDIR}/_ext/770672057/alloc.o ../../../3rd-party/bluedroid/decoder/srce/alloc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o: ../../../3rd-party/bluedroid/decoder/srce/bitalloc-sbc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o.d" -o ${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o ../../../3rd-party/bluedroid/decoder/srce/bitalloc-sbc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/bitalloc.o: ../../../3rd-party/bluedroid/decoder/srce/bitalloc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitalloc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitalloc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/bitalloc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/bitalloc.o.d" -o ${OBJECTDIR}/_ext/770672057/bitalloc.o ../../../3rd-party/bluedroid/decoder/srce/bitalloc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/bitstream-decode.o: ../../../3rd-party/bluedroid/decoder/srce/bitstream-decode.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitstream-decode.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitstream-decode.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/bitstream-decode.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/bitstream-decode.o.d" -o ${OBJECTDIR}/_ext/770672057/bitstream-decode.o ../../../3rd-party/bluedroid/decoder/srce/bitstream-decode.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/decoder-oina.o: ../../../3rd-party/bluedroid/decoder/srce/decoder-oina.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-oina.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-oina.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/decoder-oina.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/decoder-oina.o.d" -o ${OBJECTDIR}/_ext/770672057/decoder-oina.o ../../../3rd-party/bluedroid/decoder/srce/decoder-oina.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/decoder-private.o: ../../../3rd-party/bluedroid/decoder/srce/decoder-private.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-private.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-private.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/decoder-private.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/decoder-private.o.d" -o ${OBJECTDIR}/_ext/770672057/decoder-private.o ../../../3rd-party/bluedroid/decoder/srce/decoder-private.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/decoder-sbc.o: ../../../3rd-party/bluedroid/decoder/srce/decoder-sbc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-sbc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-sbc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/decoder-sbc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/decoder-sbc.o.d" -o ${OBJECTDIR}/_ext/770672057/decoder-sbc.o ../../../3rd-party/bluedroid/decoder/srce/decoder-sbc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/dequant.o: ../../../3rd-party/bluedroid/decoder/srce/dequant.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/dequant.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/dequant.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/dequant.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/dequant.o.d" -o ${OBJECTDIR}/_ext/770672057/dequant.o ../../../3rd-party/bluedroid/decoder/srce/dequant.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/framing-sbc.o: ../../../3rd-party/bluedroid/decoder/srce/framing-sbc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/framing-sbc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/framing-sbc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/framing-sbc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/framing-sbc.o.d" -o ${OBJECTDIR}/_ext/770672057/framing-sbc.o ../../../3rd-party/bluedroid/decoder/srce/framing-sbc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/framing.o: ../../../3rd-party/bluedroid/decoder/srce/framing.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/framing.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/framing.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/framing.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/framing.o.d" -o ${OBJECTDIR}/_ext/770672057/framing.o ../../../3rd-party/bluedroid/decoder/srce/framing.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/oi_codec_version.o: ../../../3rd-party/bluedroid/decoder/srce/oi_codec_version.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/oi_codec_version.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/oi_codec_version.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/oi_codec_version.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/oi_codec_version.o.d" -o ${OBJECTDIR}/_ext/770672057/oi_codec_version.o ../../../3rd-party/bluedroid/decoder/srce/oi_codec_version.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o: ../../../3rd-party/bluedroid/decoder/srce/synthesis-8-generated.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o.d" -o ${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o ../../../3rd-party/bluedroid/decoder/srce/synthesis-8-generated.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/synthesis-dct8.o: ../../../3rd-party/bluedroid/decoder/srce/synthesis-dct8.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-dct8.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-dct8.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/synthesis-dct8.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/synthesis-dct8.o.d" -o ${OBJECTDIR}/_ext/770672057/synthesis-dct8.o ../../../3rd-party/bluedroid/decoder/srce/synthesis-dct8.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/synthesis-sbc.o: ../../../3rd-party/bluedroid/decoder/srce/synthesis-sbc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-sbc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-sbc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/synthesis-sbc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/synthesis-sbc.o.d" -o ${OBJECTDIR}/_ext/770672057/synthesis-sbc.o ../../../3rd-party/bluedroid/decoder/srce/synthesis-sbc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_analysis.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_analysis.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_analysis.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_analysis.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_analysis.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_analysis.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_analysis.o ../../../3rd-party/bluedroid/encoder/srce/sbc_analysis.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_dct.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_dct.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_dct.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_dct.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_dct.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_dct.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_dct.o ../../../3rd-party/bluedroid/encoder/srce/sbc_dct.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_dct_coeffs.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o ../../../3rd-party/bluedroid/encoder/srce/sbc_dct_coeffs.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_mono.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_mono.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_ste.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_ste.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_coeffs.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_coeffs.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_encoder.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_encoder.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_encoder.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_encoder.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_encoder.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_encoder.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_encoder.o ../../../3rd-party/bluedroid/encoder/srce/sbc_encoder.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_packing.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_packing.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_packing.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_packing.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_packing.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_packing.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_packing.o ../../../3rd-party/bluedroid/encoder/srce/sbc_packing.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o: ../../../3rd-party/hxcmod-player/mods/nao-deceased_by_disease.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/968912543" 
	@${RM} ${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o.d 
	@${RM} ${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o.d" -o ${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o ../../../3rd-party/hxcmod-player/mods/nao-deceased_by_disease.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/835724193/hxcmod.o: ../../../3rd-party/hxcmod-player/hxcmod.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/835724193" 
	@${RM} ${OBJECTDIR}/_ext/835724193/hxcmod.o.d 
	@${RM} ${OBJECTDIR}/_ext/835724193/hxcmod.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/835724193/hxcmod.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/835724193/hxcmod.o.d" -o ${OBJECTDIR}/_ext/835724193/hxcmod.o ../../../3rd-party/hxcmod-player/hxcmod.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/762785730/md5.o: ../../../3rd-party/md5/md5.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/762785730" 
	@${RM} ${OBJECTDIR}/_ext/762785730/md5.o.d 
	@${RM} ${OBJECTDIR}/_ext/762785730/md5.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/762785730/md5.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/762785730/md5.o.d" -o ${OBJECTDIR}/_ext/762785730/md5.o ../../../3rd-party/md5/md5.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/34712644/uECC.o: ../../../3rd-party/micro-ecc/uECC.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/34712644" 
	@${RM} ${OBJECTDIR}/_ext/34712644/uECC.o.d 
	@${RM} ${OBJECTDIR}/_ext/34712644/uECC.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/34712644/uECC.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/34712644/uECC.o.d" -o ${OBJECTDIR}/_ext/34712644/uECC.o ../../../3rd-party/micro-ecc/uECC.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/2123824702/yxml.o: ../../../3rd-party/yxml/yxml.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/2123824702" 
	@${RM} ${OBJECTDIR}/_ext/2123824702/yxml.o.d 
	@${RM} ${OBJECTDIR}/_ext/2123824702/yxml.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/2123824702/yxml.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/2123824702/yxml.o.d" -o ${OBJECTDIR}/_ext/2123824702/yxml.o ../../../3rd-party/yxml/yxml.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o: ../../../chipset/csr/btstack_chipset_csr.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1768064806" 
	@${RM} ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d 
	@${RM} ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d" -o ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o ../../../chipset/csr/btstack_chipset_csr.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o: ../../../platform/embedded/btstack_run_loop_embedded.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/993942601" 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d" -o ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o ../../../platform/embedded/btstack_run_loop_embedded.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o: ../../../platform/embedded/btstack_uart_block_embedded.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/993942601" 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d" -o ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o ../../../platform/embedded/btstack_uart_block_embedded.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/524132624/battery_service_server.o: ../../../src/ble/gatt-service/battery_service_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/524132624" 
	@${RM} ${OBJECTDIR}/_ext/524132624/battery_service_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/524132624/battery_service_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/524132624/battery_service_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/524132624/battery_service_server.o.d" -o ${OBJECTDIR}/_ext/524132624/battery_service_server.o ../../../src/ble/gatt-service/battery_service_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/524132624/device_information_service_server.o: ../../../src/ble/gatt-service/device_information_service_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/524132624" 
	@${RM} ${OBJECTDIR}/_ext/524132624/device_information_service_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/524132624/device_information_service_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/524132624/device_information_service_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/524132624/device_information_service_server.o.d" -o ${OBJECTDIR}/_ext/524132624/device_information_service_server.o ../../../src/ble/gatt-service/device_information_service_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/524132624/hids_device.o: ../../../src/ble/gatt-service/hids_device.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/524132624" 
	@${RM} ${OBJECTDIR}/_ext/524132624/hids_device.o.d 
	@${RM} ${OBJECTDIR}/_ext/524132624/hids_device.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/524132624/hids_device.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/524132624/hids_device.o.d" -o ${OBJECTDIR}/_ext/524132624/hids_device.o ../../../src/ble/gatt-service/hids_device.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/att_db.o: ../../../src/ble/att_db.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_db.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_db.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_db.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_db.o.d" -o ${OBJECTDIR}/_ext/534563071/att_db.o ../../../src/ble/att_db.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/att_dispatch.o: ../../../src/ble/att_dispatch.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_dispatch.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_dispatch.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_dispatch.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_dispatch.o.d" -o ${OBJECTDIR}/_ext/534563071/att_dispatch.o ../../../src/ble/att_dispatch.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/att_server.o: ../../../src/ble/att_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_server.o.d" -o ${OBJECTDIR}/_ext/534563071/att_server.o ../../../src/ble/att_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/le_device_db_memory.o: ../../../src/ble/le_device_db_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d" -o ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o ../../../src/ble/le_device_db_memory.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/sm.o: ../../../src/ble/sm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/sm.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/sm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/sm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/sm.o.d" -o ${OBJECTDIR}/_ext/534563071/sm.o ../../../src/ble/sm.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/ancs_client.o: ../../../src/ble/ancs_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/ancs_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/ancs_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/ancs_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/ancs_client.o.d" -o ${OBJECTDIR}/_ext/534563071/ancs_client.o ../../../src/ble/ancs_client.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/gatt_client.o: ../../../src/ble/gatt_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/gatt_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/gatt_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/gatt_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/gatt_client.o.d" -o ${OBJECTDIR}/_ext/534563071/gatt_client.o ../../../src/ble/gatt_client.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o: ../../../src/classic/btstack_link_key_db_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o ../../../src/classic/btstack_link_key_db_memory.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/sdp_client.o: ../../../src/classic/sdp_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_client.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_client.o ../../../src/classic/sdp_client.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o: ../../../src/classic/sdp_client_rfcomm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o ../../../src/classic/sdp_client_rfcomm.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/sdp_server.o: ../../../src/classic/sdp_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_server.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_server.o ../../../src/classic/sdp_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/sdp_util.o: ../../../src/classic/sdp_util.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_util.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_util.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_util.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_util.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_util.o ../../../src/classic/sdp_util.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/spp_server.o: ../../../src/classic/spp_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/spp_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/spp_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/spp_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/spp_server.o.d" -o ${OBJECTDIR}/_ext/1386327864/spp_server.o ../../../src/classic/spp_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/a2dp_sink.o: ../../../src/classic/a2dp_sink.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/a2dp_sink.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/a2dp_sink.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/a2dp_sink.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/a2dp_sink.o.d" -o ${OBJECTDIR}/_ext/1386327864/a2dp_sink.o ../../../src/classic/a2dp_sink.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/a2dp_source.o: ../../../src/classic/a2dp_source.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/a2dp_source.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/a2dp_source.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/a2dp_source.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/a2dp_source.o.d" -o ${OBJECTDIR}/_ext/1386327864/a2dp_source.o ../../../src/classic/a2dp_source.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp.o: ../../../src/classic/avdtp.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp.o ../../../src/classic/avdtp.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o: ../../../src/classic/avdtp_acceptor.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o ../../../src/classic/avdtp_acceptor.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o: ../../../src/classic/avdtp_initiator.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o ../../../src/classic/avdtp_initiator.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp_sink.o: ../../../src/classic/avdtp_sink.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_sink.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_sink.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp_sink.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp_sink.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp_sink.o ../../../src/classic/avdtp_sink.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp_source.o: ../../../src/classic/avdtp_source.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_source.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_source.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp_source.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp_source.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp_source.o ../../../src/classic/avdtp_source.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp_util.o: ../../../src/classic/avdtp_util.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_util.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_util.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp_util.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp_util.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp_util.o ../../../src/classic/avdtp_util.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avrcp.o: ../../../src/classic/avrcp.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avrcp.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avrcp.o.d" -o ${OBJECTDIR}/_ext/1386327864/avrcp.o ../../../src/classic/avrcp.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o: ../../../src/classic/avrcp_browsing_controller.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o.d" -o ${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o ../../../src/classic/avrcp_browsing_controller.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avrcp_controller.o: ../../../src/classic/avrcp_controller.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_controller.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_controller.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avrcp_controller.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avrcp_controller.o.d" -o ${OBJECTDIR}/_ext/1386327864/avrcp_controller.o ../../../src/classic/avrcp_controller.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o: ../../../src/classic/avrcp_media_item_iterator.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o.d" -o ${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o ../../../src/classic/avrcp_media_item_iterator.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avrcp_target.o: ../../../src/classic/avrcp_target.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_target.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_target.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avrcp_target.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avrcp_target.o.d" -o ${OBJECTDIR}/_ext/1386327864/avrcp_target.o ../../../src/classic/avrcp_target.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/bnep.o: ../../../src/classic/bnep.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/bnep.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/bnep.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/bnep.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/bnep.o.d" -o ${OBJECTDIR}/_ext/1386327864/bnep.o ../../../src/classic/bnep.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o: ../../../src/classic/btstack_cvsd_plc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o ../../../src/classic/btstack_cvsd_plc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o: ../../../src/classic/btstack_sbc_decoder_bluedroid.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o ../../../src/classic/btstack_sbc_decoder_bluedroid.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o: ../../../src/classic/btstack_sbc_encoder_bluedroid.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o ../../../src/classic/btstack_sbc_encoder_bluedroid.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o: ../../../src/classic/btstack_sbc_plc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o ../../../src/classic/btstack_sbc_plc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/device_id_server.o: ../../../src/classic/device_id_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/device_id_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/device_id_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/device_id_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/device_id_server.o.d" -o ${OBJECTDIR}/_ext/1386327864/device_id_server.o ../../../src/classic/device_id_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/goep_client.o: ../../../src/classic/goep_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/goep_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/goep_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/goep_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/goep_client.o.d" -o ${OBJECTDIR}/_ext/1386327864/goep_client.o ../../../src/classic/goep_client.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hfp.o: ../../../src/classic/hfp.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hfp.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hfp.o.d" -o ${OBJECTDIR}/_ext/1386327864/hfp.o ../../../src/classic/hfp.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hfp_ag.o: ../../../src/classic/hfp_ag.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_ag.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_ag.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hfp_ag.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hfp_ag.o.d" -o ${OBJECTDIR}/_ext/1386327864/hfp_ag.o ../../../src/classic/hfp_ag.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o: ../../../src/classic/hfp_gsm_model.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o.d" -o ${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o ../../../src/classic/hfp_gsm_model.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hfp_hf.o: ../../../src/classic/hfp_hf.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_hf.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_hf.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hfp_hf.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hfp_hf.o.d" -o ${OBJECTDIR}/_ext/1386327864/hfp_hf.o ../../../src/classic/hfp_hf.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hfp_msbc.o: ../../../src/classic/hfp_msbc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_msbc.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_msbc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hfp_msbc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hfp_msbc.o.d" -o ${OBJECTDIR}/_ext/1386327864/hfp_msbc.o ../../../src/classic/hfp_msbc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hid_device.o: ../../../src/classic/hid_device.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hid_device.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hid_device.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hid_device.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hid_device.o.d" -o ${OBJECTDIR}/_ext/1386327864/hid_device.o ../../../src/classic/hid_device.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hsp_ag.o: ../../../src/classic/hsp_ag.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hsp_ag.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hsp_ag.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hsp_ag.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hsp_ag.o.d" -o ${OBJECTDIR}/_ext/1386327864/hsp_ag.o ../../../src/classic/hsp_ag.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hsp_hs.o: ../../../src/classic/hsp_hs.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hsp_hs.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hsp_hs.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hsp_hs.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hsp_hs.o.d" -o ${OBJECTDIR}/_ext/1386327864/hsp_hs.o ../../../src/classic/hsp_hs.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/obex_iterator.o: ../../../src/classic/obex_iterator.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/obex_iterator.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/obex_iterator.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/obex_iterator.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/obex_iterator.o.d" -o ${OBJECTDIR}/_ext/1386327864/obex_iterator.o ../../../src/classic/obex_iterator.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/pan.o: ../../../src/classic/pan.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/pan.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/pan.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/pan.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/pan.o.d" -o ${OBJECTDIR}/_ext/1386327864/pan.o ../../../src/classic/pan.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/pbap_client.o: ../../../src/classic/pbap_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/pbap_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/pbap_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/pbap_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/pbap_client.o.d" -o ${OBJECTDIR}/_ext/1386327864/pbap_client.o ../../../src/classic/pbap_client.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_memory.o: ../../../src/btstack_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_memory.o ../../../src/btstack_memory.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/hci.o: ../../../src/hci.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci.o ../../../src/hci.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/hci_cmd.o: ../../../src/hci_cmd.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_cmd.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_cmd.o ../../../src/hci_cmd.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/hci_dump.o: ../../../src/hci_dump.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_dump.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_dump.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_dump.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_dump.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_dump.o ../../../src/hci_dump.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/l2cap.o: ../../../src/l2cap.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/l2cap.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/l2cap.o.d" -o ${OBJECTDIR}/_ext/1386528437/l2cap.o ../../../src/l2cap.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o: ../../../src/l2cap_signaling.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d" -o ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o ../../../src/l2cap_signaling.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o: ../../../src/btstack_linked_list.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o ../../../src/btstack_linked_list.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o: ../../../src/btstack_memory_pool.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o ../../../src/btstack_memory_pool.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/rfcomm.o: ../../../src/classic/rfcomm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/rfcomm.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/rfcomm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/rfcomm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/rfcomm.o.d" -o ${OBJECTDIR}/_ext/1386327864/rfcomm.o ../../../src/classic/rfcomm.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o: ../../../src/btstack_run_loop.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o ../../../src/btstack_run_loop.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_util.o: ../../../src/btstack_util.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_util.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_util.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_util.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_util.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_util.o ../../../src/btstack_util.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o: ../../../src/hci_transport_h4.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o ../../../src/hci_transport_h4.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o: ../../../src/hci_transport_h5.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o ../../../src/hci_transport_h5.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_slip.o: ../../../src/btstack_slip.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_slip.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_slip.o ../../../src/btstack_slip.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/ad_parser.o: ../../../src/ad_parser.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/ad_parser.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/ad_parser.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/ad_parser.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/ad_parser.o.d" -o ${OBJECTDIR}/_ext/1386528437/ad_parser.o ../../../src/ad_parser.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_tlv.o: ../../../src/btstack_tlv.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_tlv.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_tlv.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_tlv.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_tlv.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_tlv.o ../../../src/btstack_tlv.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_crypto.o: ../../../src/btstack_crypto.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_crypto.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_crypto.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_crypto.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_crypto.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_crypto.o ../../../src/btstack_crypto.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o: ../../../src/btstack_hid_parser.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o ../../../src/btstack_hid_parser.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1880736137/drv_tmr.o: ../../../../driver/tmr/src/dynamic/drv_tmr.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1880736137" 
	@${RM} ${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d 
	@${RM} ${OBJECTDIR}/_ext/1880736137/drv_tmr.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d" -o ${OBJECTDIR}/_ext/1880736137/drv_tmr.o ../../../../driver/tmr/src/dynamic/drv_tmr.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1112166103/sys_clk.o: ../../../../system/clk/src/sys_clk.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1112166103" 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk.o.d 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1112166103/sys_clk.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1112166103/sys_clk.o.d" -o ${OBJECTDIR}/_ext/1112166103/sys_clk.o ../../../../system/clk/src/sys_clk.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o: ../../../../system/clk/src/sys_clk_pic32mx.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1112166103" 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d" -o ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o ../../../../system/clk/src/sys_clk_pic32mx.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1510368962/sys_devcon.o: ../../../../system/devcon/src/sys_devcon.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1510368962" 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d" -o ${OBJECTDIR}/_ext/1510368962/sys_devcon.o ../../../../system/devcon/src/sys_devcon.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o: ../../../../system/devcon/src/sys_devcon_pic32mx.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1510368962" 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d" -o ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o ../../../../system/devcon/src/sys_devcon_pic32mx.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o: ../../../../system/int/src/sys_int_pic32.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/2087176412" 
	@${RM} ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d 
	@${RM} ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d" -o ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o ../../../../system/int/src/sys_int_pic32.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/2147153351/sys_ports.o: ../../../../system/ports/src/sys_ports.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/2147153351" 
	@${RM} ${OBJECTDIR}/_ext/2147153351/sys_ports.o.d 
	@${RM} ${OBJECTDIR}/_ext/2147153351/sys_ports.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/2147153351/sys_ports.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/2147153351/sys_ports.o.d" -o ${OBJECTDIR}/_ext/2147153351/sys_ports.o ../../../../system/ports/src/sys_ports.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o: ../../../src/btstack_uart_slip_wrapper.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o ../../../src/btstack_uart_slip_wrapper.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
else
${OBJECTDIR}/_ext/101891878/system_init.o: ../src/system_config/bt_audio_dk/system_init.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/101891878" 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_init.o.d 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_init.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/101891878/system_init.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/101891878/system_init.o.d" -o ${OBJECTDIR}/_ext/101891878/system_init.o ../src/system_config/bt_audio_dk/system_init.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/101891878/system_tasks.o: ../src/system_config/bt_audio_dk/system_tasks.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/101891878" 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_tasks.o.d 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_tasks.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/101891878/system_tasks.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/101891878/system_tasks.o.d" -o ${OBJECTDIR}/_ext/101891878/system_tasks.o ../src/system_config/bt_audio_dk/system_tasks.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1360937237/btstack_port.o: ../src/btstack_port.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/btstack_port.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/btstack_port.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/btstack_port.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1360937237/btstack_port.o.d" -o ${OBJECTDIR}/_ext/1360937237/btstack_port.o ../src/btstack_port.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1360937237/app_debug.o: ../src/app_debug.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app_debug.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app_debug.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/app_debug.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1360937237/app_debug.o.d" -o ${OBJECTDIR}/_ext/1360937237/app_debug.o ../src/app_debug.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1360937237/app.o: ../src/app.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/app.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1360937237/app.o.d" -o ${OBJECTDIR}/_ext/1360937237/app.o ../src/app.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1360937237/main.o: ../src/main.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/main.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1360937237/main.o.d" -o ${OBJECTDIR}/_ext/1360937237/main.o ../src/main.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/97075643/spp_counter.o: ../../../example/spp_counter.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/97075643" 
	@${RM} ${OBJECTDIR}/_ext/97075643/spp_counter.o.d 
	@${RM} ${OBJECTDIR}/_ext/97075643/spp_counter.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/97075643/spp_counter.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/97075643/spp_counter.o.d" -o ${OBJECTDIR}/_ext/97075643/spp_counter.o ../../../example/spp_counter.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/alloc.o: ../../../3rd-party/bluedroid/decoder/srce/alloc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/alloc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/alloc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/alloc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/alloc.o.d" -o ${OBJECTDIR}/_ext/770672057/alloc.o ../../../3rd-party/bluedroid/decoder/srce/alloc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o: ../../../3rd-party/bluedroid/decoder/srce/bitalloc-sbc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o.d" -o ${OBJECTDIR}/_ext/770672057/bitalloc-sbc.o ../../../3rd-party/bluedroid/decoder/srce/bitalloc-sbc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/bitalloc.o: ../../../3rd-party/bluedroid/decoder/srce/bitalloc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitalloc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitalloc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/bitalloc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/bitalloc.o.d" -o ${OBJECTDIR}/_ext/770672057/bitalloc.o ../../../3rd-party/bluedroid/decoder/srce/bitalloc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/bitstream-decode.o: ../../../3rd-party/bluedroid/decoder/srce/bitstream-decode.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitstream-decode.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/bitstream-decode.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/bitstream-decode.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/bitstream-decode.o.d" -o ${OBJECTDIR}/_ext/770672057/bitstream-decode.o ../../../3rd-party/bluedroid/decoder/srce/bitstream-decode.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/decoder-oina.o: ../../../3rd-party/bluedroid/decoder/srce/decoder-oina.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-oina.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-oina.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/decoder-oina.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/decoder-oina.o.d" -o ${OBJECTDIR}/_ext/770672057/decoder-oina.o ../../../3rd-party/bluedroid/decoder/srce/decoder-oina.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/decoder-private.o: ../../../3rd-party/bluedroid/decoder/srce/decoder-private.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-private.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-private.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/decoder-private.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/decoder-private.o.d" -o ${OBJECTDIR}/_ext/770672057/decoder-private.o ../../../3rd-party/bluedroid/decoder/srce/decoder-private.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/decoder-sbc.o: ../../../3rd-party/bluedroid/decoder/srce/decoder-sbc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-sbc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/decoder-sbc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/decoder-sbc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/decoder-sbc.o.d" -o ${OBJECTDIR}/_ext/770672057/decoder-sbc.o ../../../3rd-party/bluedroid/decoder/srce/decoder-sbc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/dequant.o: ../../../3rd-party/bluedroid/decoder/srce/dequant.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/dequant.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/dequant.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/dequant.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/dequant.o.d" -o ${OBJECTDIR}/_ext/770672057/dequant.o ../../../3rd-party/bluedroid/decoder/srce/dequant.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/framing-sbc.o: ../../../3rd-party/bluedroid/decoder/srce/framing-sbc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/framing-sbc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/framing-sbc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/framing-sbc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/framing-sbc.o.d" -o ${OBJECTDIR}/_ext/770672057/framing-sbc.o ../../../3rd-party/bluedroid/decoder/srce/framing-sbc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/framing.o: ../../../3rd-party/bluedroid/decoder/srce/framing.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/framing.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/framing.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/framing.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/framing.o.d" -o ${OBJECTDIR}/_ext/770672057/framing.o ../../../3rd-party/bluedroid/decoder/srce/framing.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/oi_codec_version.o: ../../../3rd-party/bluedroid/decoder/srce/oi_codec_version.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/oi_codec_version.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/oi_codec_version.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/oi_codec_version.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/oi_codec_version.o.d" -o ${OBJECTDIR}/_ext/770672057/oi_codec_version.o ../../../3rd-party/bluedroid/decoder/srce/oi_codec_version.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o: ../../../3rd-party/bluedroid/decoder/srce/synthesis-8-generated.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o.d" -o ${OBJECTDIR}/_ext/770672057/synthesis-8-generated.o ../../../3rd-party/bluedroid/decoder/srce/synthesis-8-generated.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/synthesis-dct8.o: ../../../3rd-party/bluedroid/decoder/srce/synthesis-dct8.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-dct8.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-dct8.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/synthesis-dct8.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/synthesis-dct8.o.d" -o ${OBJECTDIR}/_ext/770672057/synthesis-dct8.o ../../../3rd-party/bluedroid/decoder/srce/synthesis-dct8.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/770672057/synthesis-sbc.o: ../../../3rd-party/bluedroid/decoder/srce/synthesis-sbc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/770672057" 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-sbc.o.d 
	@${RM} ${OBJECTDIR}/_ext/770672057/synthesis-sbc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/770672057/synthesis-sbc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/770672057/synthesis-sbc.o.d" -o ${OBJECTDIR}/_ext/770672057/synthesis-sbc.o ../../../3rd-party/bluedroid/decoder/srce/synthesis-sbc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_analysis.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_analysis.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_analysis.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_analysis.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_analysis.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_analysis.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_analysis.o ../../../3rd-party/bluedroid/encoder/srce/sbc_analysis.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_dct.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_dct.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_dct.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_dct.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_dct.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_dct.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_dct.o ../../../3rd-party/bluedroid/encoder/srce/sbc_dct.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_dct_coeffs.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_dct_coeffs.o ../../../3rd-party/bluedroid/encoder/srce/sbc_dct_coeffs.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_mono.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_mono.o ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_mono.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_ste.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_enc_bit_alloc_ste.o ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_ste.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_coeffs.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_enc_coeffs.o ../../../3rd-party/bluedroid/encoder/srce/sbc_enc_coeffs.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_encoder.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_encoder.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_encoder.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_encoder.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_encoder.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_encoder.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_encoder.o ../../../3rd-party/bluedroid/encoder/srce/sbc_encoder.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1907061729/sbc_packing.o: ../../../3rd-party/bluedroid/encoder/srce/sbc_packing.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1907061729" 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_packing.o.d 
	@${RM} ${OBJECTDIR}/_ext/1907061729/sbc_packing.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1907061729/sbc_packing.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1907061729/sbc_packing.o.d" -o ${OBJECTDIR}/_ext/1907061729/sbc_packing.o ../../../3rd-party/bluedroid/encoder/srce/sbc_packing.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o: ../../../3rd-party/hxcmod-player/mods/nao-deceased_by_disease.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/968912543" 
	@${RM} ${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o.d 
	@${RM} ${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o.d" -o ${OBJECTDIR}/_ext/968912543/nao-deceased_by_disease.o ../../../3rd-party/hxcmod-player/mods/nao-deceased_by_disease.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/835724193/hxcmod.o: ../../../3rd-party/hxcmod-player/hxcmod.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/835724193" 
	@${RM} ${OBJECTDIR}/_ext/835724193/hxcmod.o.d 
	@${RM} ${OBJECTDIR}/_ext/835724193/hxcmod.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/835724193/hxcmod.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/835724193/hxcmod.o.d" -o ${OBJECTDIR}/_ext/835724193/hxcmod.o ../../../3rd-party/hxcmod-player/hxcmod.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/762785730/md5.o: ../../../3rd-party/md5/md5.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/762785730" 
	@${RM} ${OBJECTDIR}/_ext/762785730/md5.o.d 
	@${RM} ${OBJECTDIR}/_ext/762785730/md5.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/762785730/md5.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/762785730/md5.o.d" -o ${OBJECTDIR}/_ext/762785730/md5.o ../../../3rd-party/md5/md5.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/34712644/uECC.o: ../../../3rd-party/micro-ecc/uECC.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/34712644" 
	@${RM} ${OBJECTDIR}/_ext/34712644/uECC.o.d 
	@${RM} ${OBJECTDIR}/_ext/34712644/uECC.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/34712644/uECC.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/34712644/uECC.o.d" -o ${OBJECTDIR}/_ext/34712644/uECC.o ../../../3rd-party/micro-ecc/uECC.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/2123824702/yxml.o: ../../../3rd-party/yxml/yxml.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/2123824702" 
	@${RM} ${OBJECTDIR}/_ext/2123824702/yxml.o.d 
	@${RM} ${OBJECTDIR}/_ext/2123824702/yxml.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/2123824702/yxml.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/2123824702/yxml.o.d" -o ${OBJECTDIR}/_ext/2123824702/yxml.o ../../../3rd-party/yxml/yxml.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o: ../../../chipset/csr/btstack_chipset_csr.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1768064806" 
	@${RM} ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d 
	@${RM} ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d" -o ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o ../../../chipset/csr/btstack_chipset_csr.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o: ../../../platform/embedded/btstack_run_loop_embedded.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/993942601" 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d" -o ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o ../../../platform/embedded/btstack_run_loop_embedded.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o: ../../../platform/embedded/btstack_uart_block_embedded.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/993942601" 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d" -o ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o ../../../platform/embedded/btstack_uart_block_embedded.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/524132624/battery_service_server.o: ../../../src/ble/gatt-service/battery_service_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/524132624" 
	@${RM} ${OBJECTDIR}/_ext/524132624/battery_service_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/524132624/battery_service_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/524132624/battery_service_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/524132624/battery_service_server.o.d" -o ${OBJECTDIR}/_ext/524132624/battery_service_server.o ../../../src/ble/gatt-service/battery_service_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/524132624/device_information_service_server.o: ../../../src/ble/gatt-service/device_information_service_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/524132624" 
	@${RM} ${OBJECTDIR}/_ext/524132624/device_information_service_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/524132624/device_information_service_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/524132624/device_information_service_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/524132624/device_information_service_server.o.d" -o ${OBJECTDIR}/_ext/524132624/device_information_service_server.o ../../../src/ble/gatt-service/device_information_service_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/524132624/hids_device.o: ../../../src/ble/gatt-service/hids_device.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/524132624" 
	@${RM} ${OBJECTDIR}/_ext/524132624/hids_device.o.d 
	@${RM} ${OBJECTDIR}/_ext/524132624/hids_device.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/524132624/hids_device.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/524132624/hids_device.o.d" -o ${OBJECTDIR}/_ext/524132624/hids_device.o ../../../src/ble/gatt-service/hids_device.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/att_db.o: ../../../src/ble/att_db.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_db.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_db.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_db.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_db.o.d" -o ${OBJECTDIR}/_ext/534563071/att_db.o ../../../src/ble/att_db.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/att_dispatch.o: ../../../src/ble/att_dispatch.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_dispatch.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_dispatch.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_dispatch.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_dispatch.o.d" -o ${OBJECTDIR}/_ext/534563071/att_dispatch.o ../../../src/ble/att_dispatch.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/att_server.o: ../../../src/ble/att_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_server.o.d" -o ${OBJECTDIR}/_ext/534563071/att_server.o ../../../src/ble/att_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/le_device_db_memory.o: ../../../src/ble/le_device_db_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d" -o ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o ../../../src/ble/le_device_db_memory.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/sm.o: ../../../src/ble/sm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/sm.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/sm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/sm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/sm.o.d" -o ${OBJECTDIR}/_ext/534563071/sm.o ../../../src/ble/sm.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/ancs_client.o: ../../../src/ble/ancs_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/ancs_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/ancs_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/ancs_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/ancs_client.o.d" -o ${OBJECTDIR}/_ext/534563071/ancs_client.o ../../../src/ble/ancs_client.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/534563071/gatt_client.o: ../../../src/ble/gatt_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/gatt_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/gatt_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/gatt_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/534563071/gatt_client.o.d" -o ${OBJECTDIR}/_ext/534563071/gatt_client.o ../../../src/ble/gatt_client.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o: ../../../src/classic/btstack_link_key_db_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o ../../../src/classic/btstack_link_key_db_memory.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/sdp_client.o: ../../../src/classic/sdp_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_client.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_client.o ../../../src/classic/sdp_client.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o: ../../../src/classic/sdp_client_rfcomm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o ../../../src/classic/sdp_client_rfcomm.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/sdp_server.o: ../../../src/classic/sdp_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_server.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_server.o ../../../src/classic/sdp_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/sdp_util.o: ../../../src/classic/sdp_util.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_util.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_util.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_util.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_util.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_util.o ../../../src/classic/sdp_util.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/spp_server.o: ../../../src/classic/spp_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/spp_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/spp_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/spp_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/spp_server.o.d" -o ${OBJECTDIR}/_ext/1386327864/spp_server.o ../../../src/classic/spp_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/a2dp_sink.o: ../../../src/classic/a2dp_sink.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/a2dp_sink.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/a2dp_sink.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/a2dp_sink.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/a2dp_sink.o.d" -o ${OBJECTDIR}/_ext/1386327864/a2dp_sink.o ../../../src/classic/a2dp_sink.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/a2dp_source.o: ../../../src/classic/a2dp_source.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/a2dp_source.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/a2dp_source.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/a2dp_source.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/a2dp_source.o.d" -o ${OBJECTDIR}/_ext/1386327864/a2dp_source.o ../../../src/classic/a2dp_source.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp.o: ../../../src/classic/avdtp.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp.o ../../../src/classic/avdtp.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o: ../../../src/classic/avdtp_acceptor.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp_acceptor.o ../../../src/classic/avdtp_acceptor.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o: ../../../src/classic/avdtp_initiator.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp_initiator.o ../../../src/classic/avdtp_initiator.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp_sink.o: ../../../src/classic/avdtp_sink.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_sink.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_sink.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp_sink.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp_sink.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp_sink.o ../../../src/classic/avdtp_sink.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp_source.o: ../../../src/classic/avdtp_source.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_source.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_source.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp_source.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp_source.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp_source.o ../../../src/classic/avdtp_source.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avdtp_util.o: ../../../src/classic/avdtp_util.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_util.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avdtp_util.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avdtp_util.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avdtp_util.o.d" -o ${OBJECTDIR}/_ext/1386327864/avdtp_util.o ../../../src/classic/avdtp_util.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avrcp.o: ../../../src/classic/avrcp.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avrcp.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avrcp.o.d" -o ${OBJECTDIR}/_ext/1386327864/avrcp.o ../../../src/classic/avrcp.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o: ../../../src/classic/avrcp_browsing_controller.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o.d" -o ${OBJECTDIR}/_ext/1386327864/avrcp_browsing_controller.o ../../../src/classic/avrcp_browsing_controller.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avrcp_controller.o: ../../../src/classic/avrcp_controller.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_controller.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_controller.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avrcp_controller.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avrcp_controller.o.d" -o ${OBJECTDIR}/_ext/1386327864/avrcp_controller.o ../../../src/classic/avrcp_controller.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o: ../../../src/classic/avrcp_media_item_iterator.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o.d" -o ${OBJECTDIR}/_ext/1386327864/avrcp_media_item_iterator.o ../../../src/classic/avrcp_media_item_iterator.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/avrcp_target.o: ../../../src/classic/avrcp_target.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_target.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/avrcp_target.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/avrcp_target.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/avrcp_target.o.d" -o ${OBJECTDIR}/_ext/1386327864/avrcp_target.o ../../../src/classic/avrcp_target.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/bnep.o: ../../../src/classic/bnep.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/bnep.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/bnep.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/bnep.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/bnep.o.d" -o ${OBJECTDIR}/_ext/1386327864/bnep.o ../../../src/classic/bnep.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o: ../../../src/classic/btstack_cvsd_plc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_cvsd_plc.o ../../../src/classic/btstack_cvsd_plc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o: ../../../src/classic/btstack_sbc_decoder_bluedroid.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_decoder_bluedroid.o ../../../src/classic/btstack_sbc_decoder_bluedroid.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o: ../../../src/classic/btstack_sbc_encoder_bluedroid.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_encoder_bluedroid.o ../../../src/classic/btstack_sbc_encoder_bluedroid.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o: ../../../src/classic/btstack_sbc_plc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_sbc_plc.o ../../../src/classic/btstack_sbc_plc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/device_id_server.o: ../../../src/classic/device_id_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/device_id_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/device_id_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/device_id_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/device_id_server.o.d" -o ${OBJECTDIR}/_ext/1386327864/device_id_server.o ../../../src/classic/device_id_server.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/goep_client.o: ../../../src/classic/goep_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/goep_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/goep_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/goep_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/goep_client.o.d" -o ${OBJECTDIR}/_ext/1386327864/goep_client.o ../../../src/classic/goep_client.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hfp.o: ../../../src/classic/hfp.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hfp.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hfp.o.d" -o ${OBJECTDIR}/_ext/1386327864/hfp.o ../../../src/classic/hfp.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hfp_ag.o: ../../../src/classic/hfp_ag.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_ag.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_ag.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hfp_ag.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hfp_ag.o.d" -o ${OBJECTDIR}/_ext/1386327864/hfp_ag.o ../../../src/classic/hfp_ag.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o: ../../../src/classic/hfp_gsm_model.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o.d" -o ${OBJECTDIR}/_ext/1386327864/hfp_gsm_model.o ../../../src/classic/hfp_gsm_model.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hfp_hf.o: ../../../src/classic/hfp_hf.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_hf.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_hf.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hfp_hf.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hfp_hf.o.d" -o ${OBJECTDIR}/_ext/1386327864/hfp_hf.o ../../../src/classic/hfp_hf.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hfp_msbc.o: ../../../src/classic/hfp_msbc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_msbc.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hfp_msbc.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hfp_msbc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hfp_msbc.o.d" -o ${OBJECTDIR}/_ext/1386327864/hfp_msbc.o ../../../src/classic/hfp_msbc.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hid_device.o: ../../../src/classic/hid_device.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hid_device.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hid_device.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hid_device.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hid_device.o.d" -o ${OBJECTDIR}/_ext/1386327864/hid_device.o ../../../src/classic/hid_device.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hsp_ag.o: ../../../src/classic/hsp_ag.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hsp_ag.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hsp_ag.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hsp_ag.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hsp_ag.o.d" -o ${OBJECTDIR}/_ext/1386327864/hsp_ag.o ../../../src/classic/hsp_ag.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/hsp_hs.o: ../../../src/classic/hsp_hs.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hsp_hs.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/hsp_hs.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/hsp_hs.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/hsp_hs.o.d" -o ${OBJECTDIR}/_ext/1386327864/hsp_hs.o ../../../src/classic/hsp_hs.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/obex_iterator.o: ../../../src/classic/obex_iterator.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/obex_iterator.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/obex_iterator.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/obex_iterator.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/obex_iterator.o.d" -o ${OBJECTDIR}/_ext/1386327864/obex_iterator.o ../../../src/classic/obex_iterator.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/pan.o: ../../../src/classic/pan.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/pan.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/pan.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/pan.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/pan.o.d" -o ${OBJECTDIR}/_ext/1386327864/pan.o ../../../src/classic/pan.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/pbap_client.o: ../../../src/classic/pbap_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/pbap_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/pbap_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/pbap_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/pbap_client.o.d" -o ${OBJECTDIR}/_ext/1386327864/pbap_client.o ../../../src/classic/pbap_client.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_memory.o: ../../../src/btstack_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_memory.o ../../../src/btstack_memory.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/hci.o: ../../../src/hci.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci.o ../../../src/hci.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/hci_cmd.o: ../../../src/hci_cmd.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_cmd.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_cmd.o ../../../src/hci_cmd.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/hci_dump.o: ../../../src/hci_dump.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_dump.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_dump.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_dump.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_dump.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_dump.o ../../../src/hci_dump.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/l2cap.o: ../../../src/l2cap.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/l2cap.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/l2cap.o.d" -o ${OBJECTDIR}/_ext/1386528437/l2cap.o ../../../src/l2cap.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o: ../../../src/l2cap_signaling.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d" -o ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o ../../../src/l2cap_signaling.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o: ../../../src/btstack_linked_list.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o ../../../src/btstack_linked_list.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o: ../../../src/btstack_memory_pool.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o ../../../src/btstack_memory_pool.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386327864/rfcomm.o: ../../../src/classic/rfcomm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/rfcomm.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/rfcomm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/rfcomm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386327864/rfcomm.o.d" -o ${OBJECTDIR}/_ext/1386327864/rfcomm.o ../../../src/classic/rfcomm.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o: ../../../src/btstack_run_loop.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o ../../../src/btstack_run_loop.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_util.o: ../../../src/btstack_util.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_util.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_util.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_util.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_util.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_util.o ../../../src/btstack_util.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o: ../../../src/hci_transport_h4.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o ../../../src/hci_transport_h4.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o: ../../../src/hci_transport_h5.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o ../../../src/hci_transport_h5.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_slip.o: ../../../src/btstack_slip.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_slip.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_slip.o ../../../src/btstack_slip.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/ad_parser.o: ../../../src/ad_parser.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/ad_parser.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/ad_parser.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/ad_parser.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/ad_parser.o.d" -o ${OBJECTDIR}/_ext/1386528437/ad_parser.o ../../../src/ad_parser.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_tlv.o: ../../../src/btstack_tlv.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_tlv.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_tlv.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_tlv.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_tlv.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_tlv.o ../../../src/btstack_tlv.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_crypto.o: ../../../src/btstack_crypto.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_crypto.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_crypto.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_crypto.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_crypto.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_crypto.o ../../../src/btstack_crypto.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o: ../../../src/btstack_hid_parser.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_hid_parser.o ../../../src/btstack_hid_parser.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1880736137/drv_tmr.o: ../../../../driver/tmr/src/dynamic/drv_tmr.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1880736137" 
	@${RM} ${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d 
	@${RM} ${OBJECTDIR}/_ext/1880736137/drv_tmr.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d" -o ${OBJECTDIR}/_ext/1880736137/drv_tmr.o ../../../../driver/tmr/src/dynamic/drv_tmr.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1112166103/sys_clk.o: ../../../../system/clk/src/sys_clk.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1112166103" 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk.o.d 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1112166103/sys_clk.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1112166103/sys_clk.o.d" -o ${OBJECTDIR}/_ext/1112166103/sys_clk.o ../../../../system/clk/src/sys_clk.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o: ../../../../system/clk/src/sys_clk_pic32mx.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1112166103" 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d" -o ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o ../../../../system/clk/src/sys_clk_pic32mx.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1510368962/sys_devcon.o: ../../../../system/devcon/src/sys_devcon.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1510368962" 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d" -o ${OBJECTDIR}/_ext/1510368962/sys_devcon.o ../../../../system/devcon/src/sys_devcon.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o: ../../../../system/devcon/src/sys_devcon_pic32mx.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1510368962" 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d" -o ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o ../../../../system/devcon/src/sys_devcon_pic32mx.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o: ../../../../system/int/src/sys_int_pic32.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/2087176412" 
	@${RM} ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d 
	@${RM} ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d" -o ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o ../../../../system/int/src/sys_int_pic32.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/2147153351/sys_ports.o: ../../../../system/ports/src/sys_ports.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/2147153351" 
	@${RM} ${OBJECTDIR}/_ext/2147153351/sys_ports.o.d 
	@${RM} ${OBJECTDIR}/_ext/2147153351/sys_ports.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/2147153351/sys_ports.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/2147153351/sys_ports.o.d" -o ${OBJECTDIR}/_ext/2147153351/sys_ports.o ../../../../system/ports/src/sys_ports.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o: ../../../src/btstack_uart_slip_wrapper.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -I"../../../3rd-party/micro-ecc" -I"../../../3rd-party/bluedroid/decoder/include" -I"../../../3rd-party/bluedroid/encoder/include" -I"../../../3rd-party/hxcmod-player" -I"../../../3rd-party/hxcmod-player/mods" -I"../../../3rd-party/md5" -I"../../../3rd-party/yxml" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_uart_slip_wrapper.o ../../../src/btstack_uart_slip_wrapper.c    -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -mdfp=${DFP_DIR}  
	
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: compileCPP
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: link
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
dist/${CND_CONF}/${IMAGE_TYPE}/app.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk  ../../../../../bin/framework/peripheral/PIC32MX450F256L_peripherals.a  
	@${MKDIR} dist/${CND_CONF}/${IMAGE_TYPE} 
	${MP_CC} $(MP_EXTRA_LD_PRE) -g   -mprocessor=$(MP_PROCESSOR_OPTION)  -o dist/${CND_CONF}/${IMAGE_TYPE}/app.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}    ../../../../../bin/framework/peripheral/PIC32MX450F256L_peripherals.a      -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)      -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),--defsym=__MPLAB_DEBUG=1,--defsym=__DEBUG=1,-D=__DEBUG_D,--defsym=_min_heap_size=16,--gc-sections,--no-code-in-dinit,--no-dinit-in-serial-mem,-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,dist/${CND_CONF}/${IMAGE_TYPE}/memoryfile.xml -mdfp=${DFP_DIR}
	
else
dist/${CND_CONF}/${IMAGE_TYPE}/app.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk  ../../../../../bin/framework/peripheral/PIC32MX450F256L_peripherals.a 
	@${MKDIR} dist/${CND_CONF}/${IMAGE_TYPE} 
	${MP_CC} $(MP_EXTRA_LD_PRE)  -mprocessor=$(MP_PROCESSOR_OPTION)  -o dist/${CND_CONF}/${IMAGE_TYPE}/app.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}    ../../../../../bin/framework/peripheral/PIC32MX450F256L_peripherals.a      -DXPRJ_default=$(CND_CONF)  -no-legacy-libc  $(COMPARISON_BUILD)  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),--defsym=_min_heap_size=16,--gc-sections,--no-code-in-dinit,--no-dinit-in-serial-mem,-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,dist/${CND_CONF}/${IMAGE_TYPE}/memoryfile.xml -mdfp=${DFP_DIR}
	${MP_CC_DIR}/xc32-bin2hex dist/${CND_CONF}/${IMAGE_TYPE}/app.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} 
endif


# Subprojects
.build-subprojects:


# Subprojects
.clean-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r build/default
	${RM} -r dist/default

# Enable dependency checking
.dep.inc: .depcheck-impl

DEPFILES=$(shell "${PATH_TO_IDE_BIN}"mplabwildcard ${POSSIBLE_DEPFILES})
ifneq (${DEPFILES},)
include ${DEPFILES}
endif
