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

# Object Directory
OBJECTDIR=build/${CND_CONF}/${IMAGE_TYPE}

# Distribution Directory
DISTDIR=dist/${CND_CONF}/${IMAGE_TYPE}

# Source Files Quoted if spaced
SOURCEFILES_QUOTED_IF_SPACED=../src/system_config/bt_audio_dk/system_init.c ../src/system_config/bt_audio_dk/system_tasks.c ../src/btstack_port.c ../src/app_debug.c ../src/app.c ../src/main.c ../../../example/spp_and_le_counter.c ../../../src/ad_parser.c ../../../src/ble/att_db.c ../../../src/ble/att_dispatch.c ../../../src/ble/att_server.c ../../../src/ble/le_device_db_memory.c ../../../src/ble/sm.c ../../../chipset/csr/btstack_chipset_csr.c ../../../platform/embedded/btstack_run_loop_embedded.c ../../../platform/embedded/btstack_uart_block_embedded.c ../../../src/btstack_memory.c ../../../src/hci.c ../../../src/hci_cmd.c ../../../src/hci_dump.c ../../../src/l2cap.c ../../../src/l2cap_signaling.c ../../../src/btstack_linked_list.c ../../../src/btstack_memory_pool.c ../../../src/classic/btstack_link_key_db_memory.c ../../../src/classic/rfcomm.c ../../../src/btstack_run_loop.c ../../../src/classic/sdp_server.c ../../../src/classic/sdp_client.c ../../../src/classic/sdp_client_rfcomm.c ../../../src/classic/sdp_util.c ../../../src/btstack_util.c ../../../src/classic/spp_server.c ../../../src/hci_transport_h4.c ../../../src/hci_transport_h5.c ../../../src/btstack_slip.c ../../../../driver/tmr/src/dynamic/drv_tmr.c ../../../../system/clk/src/sys_clk.c ../../../../system/clk/src/sys_clk_pic32mx.c ../../../../system/devcon/src/sys_devcon.c ../../../../system/devcon/src/sys_devcon_pic32mx.c ../../../../system/int/src/sys_int_pic32.c ../../../../system/ports/src/sys_ports.c

# Object Files Quoted if spaced
OBJECTFILES_QUOTED_IF_SPACED=${OBJECTDIR}/_ext/101891878/system_init.o ${OBJECTDIR}/_ext/101891878/system_tasks.o ${OBJECTDIR}/_ext/1360937237/btstack_port.o ${OBJECTDIR}/_ext/1360937237/app_debug.o ${OBJECTDIR}/_ext/1360937237/app.o ${OBJECTDIR}/_ext/1360937237/main.o ${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o ${OBJECTDIR}/_ext/534563071/ad_parser.o ${OBJECTDIR}/_ext/534563071/att_db.o ${OBJECTDIR}/_ext/534563071/att_dispatch.o ${OBJECTDIR}/_ext/534563071/att_server.o ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o ${OBJECTDIR}/_ext/534563071/sm.o ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o ${OBJECTDIR}/_ext/1386528437/btstack_memory.o ${OBJECTDIR}/_ext/1386528437/hci.o ${OBJECTDIR}/_ext/1386528437/hci_cmd.o ${OBJECTDIR}/_ext/1386528437/hci_dump.o ${OBJECTDIR}/_ext/1386528437/l2cap.o ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o ${OBJECTDIR}/_ext/1386327864/rfcomm.o ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o ${OBJECTDIR}/_ext/1386327864/sdp_server.o ${OBJECTDIR}/_ext/1386327864/sdp_client.o ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o ${OBJECTDIR}/_ext/1386327864/sdp_util.o ${OBJECTDIR}/_ext/1386528437/btstack_util.o ${OBJECTDIR}/_ext/1386327864/spp_server.o ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o ${OBJECTDIR}/_ext/1386528437/btstack_slip.o ${OBJECTDIR}/_ext/1880736137/drv_tmr.o ${OBJECTDIR}/_ext/1112166103/sys_clk.o ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o ${OBJECTDIR}/_ext/1510368962/sys_devcon.o ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o ${OBJECTDIR}/_ext/2147153351/sys_ports.o
POSSIBLE_DEPFILES=${OBJECTDIR}/_ext/101891878/system_init.o.d ${OBJECTDIR}/_ext/101891878/system_tasks.o.d ${OBJECTDIR}/_ext/1360937237/btstack_port.o.d ${OBJECTDIR}/_ext/1360937237/app_debug.o.d ${OBJECTDIR}/_ext/1360937237/app.o.d ${OBJECTDIR}/_ext/1360937237/main.o.d ${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o.d ${OBJECTDIR}/_ext/534563071/ad_parser.o.d ${OBJECTDIR}/_ext/534563071/att_db.o.d ${OBJECTDIR}/_ext/534563071/att_dispatch.o.d ${OBJECTDIR}/_ext/534563071/att_server.o.d ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d ${OBJECTDIR}/_ext/534563071/sm.o.d ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d ${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d ${OBJECTDIR}/_ext/1386528437/hci.o.d ${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d ${OBJECTDIR}/_ext/1386528437/hci_dump.o.d ${OBJECTDIR}/_ext/1386528437/l2cap.o.d ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d ${OBJECTDIR}/_ext/1386327864/rfcomm.o.d ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d ${OBJECTDIR}/_ext/1386327864/sdp_server.o.d ${OBJECTDIR}/_ext/1386327864/sdp_client.o.d ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d ${OBJECTDIR}/_ext/1386327864/sdp_util.o.d ${OBJECTDIR}/_ext/1386528437/btstack_util.o.d ${OBJECTDIR}/_ext/1386327864/spp_server.o.d ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d ${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d ${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d ${OBJECTDIR}/_ext/1112166103/sys_clk.o.d ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d ${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d ${OBJECTDIR}/_ext/2147153351/sys_ports.o.d

# Object Files
OBJECTFILES=${OBJECTDIR}/_ext/101891878/system_init.o ${OBJECTDIR}/_ext/101891878/system_tasks.o ${OBJECTDIR}/_ext/1360937237/btstack_port.o ${OBJECTDIR}/_ext/1360937237/app_debug.o ${OBJECTDIR}/_ext/1360937237/app.o ${OBJECTDIR}/_ext/1360937237/main.o ${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o ${OBJECTDIR}/_ext/534563071/ad_parser.o ${OBJECTDIR}/_ext/534563071/att_db.o ${OBJECTDIR}/_ext/534563071/att_dispatch.o ${OBJECTDIR}/_ext/534563071/att_server.o ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o ${OBJECTDIR}/_ext/534563071/sm.o ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o ${OBJECTDIR}/_ext/1386528437/btstack_memory.o ${OBJECTDIR}/_ext/1386528437/hci.o ${OBJECTDIR}/_ext/1386528437/hci_cmd.o ${OBJECTDIR}/_ext/1386528437/hci_dump.o ${OBJECTDIR}/_ext/1386528437/l2cap.o ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o ${OBJECTDIR}/_ext/1386327864/rfcomm.o ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o ${OBJECTDIR}/_ext/1386327864/sdp_server.o ${OBJECTDIR}/_ext/1386327864/sdp_client.o ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o ${OBJECTDIR}/_ext/1386327864/sdp_util.o ${OBJECTDIR}/_ext/1386528437/btstack_util.o ${OBJECTDIR}/_ext/1386327864/spp_server.o ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o ${OBJECTDIR}/_ext/1386528437/btstack_slip.o ${OBJECTDIR}/_ext/1880736137/drv_tmr.o ${OBJECTDIR}/_ext/1112166103/sys_clk.o ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o ${OBJECTDIR}/_ext/1510368962/sys_devcon.o ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o ${OBJECTDIR}/_ext/2147153351/sys_ports.o

# Source Files
SOURCEFILES=../src/system_config/bt_audio_dk/system_init.c ../src/system_config/bt_audio_dk/system_tasks.c ../src/btstack_port.c ../src/app_debug.c ../src/app.c ../src/main.c ../../../example/spp_and_le_counter.c ../../../src/ad_parser.c ../../../src/ble/att_db.c ../../../src/ble/att_dispatch.c ../../../src/ble/att_server.c ../../../src/ble/le_device_db_memory.c ../../../src/ble/sm.c ../../../chipset/csr/btstack_chipset_csr.c ../../../platform/embedded/btstack_run_loop_embedded.c ../../../platform/embedded/btstack_uart_block_embedded.c ../../../src/btstack_memory.c ../../../src/hci.c ../../../src/hci_cmd.c ../../../src/hci_dump.c ../../../src/l2cap.c ../../../src/l2cap_signaling.c ../../../src/btstack_linked_list.c ../../../src/btstack_memory_pool.c ../../../src/classic/btstack_link_key_db_memory.c ../../../src/classic/rfcomm.c ../../../src/btstack_run_loop.c ../../../src/classic/sdp_server.c ../../../src/classic/sdp_client.c ../../../src/classic/sdp_client_rfcomm.c ../../../src/classic/sdp_util.c ../../../src/btstack_util.c ../../../src/classic/spp_server.c ../../../src/hci_transport_h4.c ../../../src/hci_transport_h5.c ../../../src/btstack_slip.c ../../../../driver/tmr/src/dynamic/drv_tmr.c ../../../../system/clk/src/sys_clk.c ../../../../system/clk/src/sys_clk_pic32mx.c ../../../../system/devcon/src/sys_devcon.c ../../../../system/devcon/src/sys_devcon_pic32mx.c ../../../../system/int/src/sys_int_pic32.c ../../../../system/ports/src/sys_ports.c


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
	@${FIXDEPS} "${OBJECTDIR}/_ext/101891878/system_init.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/101891878/system_init.o.d" -o ${OBJECTDIR}/_ext/101891878/system_init.o ../src/system_config/bt_audio_dk/system_init.c     
	
${OBJECTDIR}/_ext/101891878/system_tasks.o: ../src/system_config/bt_audio_dk/system_tasks.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/101891878" 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_tasks.o.d 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_tasks.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/101891878/system_tasks.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/101891878/system_tasks.o.d" -o ${OBJECTDIR}/_ext/101891878/system_tasks.o ../src/system_config/bt_audio_dk/system_tasks.c     
	
${OBJECTDIR}/_ext/1360937237/btstack_port.o: ../src/btstack_port.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/btstack_port.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/btstack_port.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/btstack_port.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1360937237/btstack_port.o.d" -o ${OBJECTDIR}/_ext/1360937237/btstack_port.o ../src/btstack_port.c     
	
${OBJECTDIR}/_ext/1360937237/app_debug.o: ../src/app_debug.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app_debug.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app_debug.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/app_debug.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1360937237/app_debug.o.d" -o ${OBJECTDIR}/_ext/1360937237/app_debug.o ../src/app_debug.c     
	
${OBJECTDIR}/_ext/1360937237/app.o: ../src/app.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/app.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1360937237/app.o.d" -o ${OBJECTDIR}/_ext/1360937237/app.o ../src/app.c     
	
${OBJECTDIR}/_ext/1360937237/main.o: ../src/main.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/main.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1360937237/main.o.d" -o ${OBJECTDIR}/_ext/1360937237/main.o ../src/main.c     
	
${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o: ../../../example/spp_and_le_counter.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/97075643" 
	@${RM} ${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o.d 
	@${RM} ${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o.d" -o ${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o ../../../example/spp_and_le_counter.c     
	
${OBJECTDIR}/_ext/534563071/ad_parser.o: ../../../src/ad_parser.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/ad_parser.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/ad_parser.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/ad_parser.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/ad_parser.o.d" -o ${OBJECTDIR}/_ext/534563071/ad_parser.o ../../../src/ad_parser.c     
	
${OBJECTDIR}/_ext/534563071/att_db.o: ../../../src/ble/att_db.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_db.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_db.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_db.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_db.o.d" -o ${OBJECTDIR}/_ext/534563071/att_db.o ../../../src/ble/att_db.c     
	
${OBJECTDIR}/_ext/534563071/att_dispatch.o: ../../../src/ble/att_dispatch.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_dispatch.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_dispatch.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_dispatch.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_dispatch.o.d" -o ${OBJECTDIR}/_ext/534563071/att_dispatch.o ../../../src/ble/att_dispatch.c     
	
${OBJECTDIR}/_ext/534563071/att_server.o: ../../../src/ble/att_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_server.o.d" -o ${OBJECTDIR}/_ext/534563071/att_server.o ../../../src/ble/att_server.c     
	
${OBJECTDIR}/_ext/534563071/le_device_db_memory.o: ../../../src/ble/le_device_db_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d" -o ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o ../../../src/ble/le_device_db_memory.c     
	
${OBJECTDIR}/_ext/534563071/sm.o: ../../../src/ble/sm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/sm.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/sm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/sm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/sm.o.d" -o ${OBJECTDIR}/_ext/534563071/sm.o ../../../src/ble/sm.c     
	
${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o: ../../../chipset/csr/btstack_chipset_csr.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1768064806" 
	@${RM} ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d 
	@${RM} ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d" -o ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o ../../../chipset/csr/btstack_chipset_csr.c     
	
${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o: ../../../platform/embedded/btstack_run_loop_embedded.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/993942601" 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d" -o ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o ../../../platform/embedded/btstack_run_loop_embedded.c     
	
${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o: ../../../platform/embedded/btstack_uart_block_embedded.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/993942601" 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d" -o ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o ../../../platform/embedded/btstack_uart_block_embedded.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_memory.o: ../../../src/btstack_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_memory.o ../../../src/btstack_memory.c     
	
${OBJECTDIR}/_ext/1386528437/hci.o: ../../../src/hci.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci.o ../../../src/hci.c     
	
${OBJECTDIR}/_ext/1386528437/hci_cmd.o: ../../../src/hci_cmd.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_cmd.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_cmd.o ../../../src/hci_cmd.c     
	
${OBJECTDIR}/_ext/1386528437/hci_dump.o: ../../../src/hci_dump.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_dump.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_dump.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_dump.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_dump.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_dump.o ../../../src/hci_dump.c     
	
${OBJECTDIR}/_ext/1386528437/l2cap.o: ../../../src/l2cap.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/l2cap.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/l2cap.o.d" -o ${OBJECTDIR}/_ext/1386528437/l2cap.o ../../../src/l2cap.c     
	
${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o: ../../../src/l2cap_signaling.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d" -o ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o ../../../src/l2cap_signaling.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o: ../../../src/btstack_linked_list.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o ../../../src/btstack_linked_list.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o: ../../../src/btstack_memory_pool.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o ../../../src/btstack_memory_pool.c     
	
${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o: ../../../src/classic/btstack_link_key_db_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o ../../../src/classic/btstack_link_key_db_memory.c     
	
${OBJECTDIR}/_ext/1386327864/rfcomm.o: ../../../src/classic/rfcomm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/rfcomm.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/rfcomm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/rfcomm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/rfcomm.o.d" -o ${OBJECTDIR}/_ext/1386327864/rfcomm.o ../../../src/classic/rfcomm.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o: ../../../src/btstack_run_loop.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o ../../../src/btstack_run_loop.c     
	
${OBJECTDIR}/_ext/1386327864/sdp_server.o: ../../../src/classic/sdp_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_server.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_server.o ../../../src/classic/sdp_server.c     
	
${OBJECTDIR}/_ext/1386327864/sdp_client.o: ../../../src/classic/sdp_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_client.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_client.o ../../../src/classic/sdp_client.c     
	
${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o: ../../../src/classic/sdp_client_rfcomm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o ../../../src/classic/sdp_client_rfcomm.c     
	
${OBJECTDIR}/_ext/1386327864/sdp_util.o: ../../../src/classic/sdp_util.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_util.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_util.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_util.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_util.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_util.o ../../../src/classic/sdp_util.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_util.o: ../../../src/btstack_util.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_util.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_util.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_util.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_util.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_util.o ../../../src/btstack_util.c     
	
${OBJECTDIR}/_ext/1386327864/spp_server.o: ../../../src/classic/spp_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/spp_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/spp_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/spp_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/spp_server.o.d" -o ${OBJECTDIR}/_ext/1386327864/spp_server.o ../../../src/classic/spp_server.c     
	
${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o: ../../../src/hci_transport_h4.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o ../../../src/hci_transport_h4.c     
	
${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o: ../../../src/hci_transport_h5.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o ../../../src/hci_transport_h5.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_slip.o: ../../../src/btstack_slip.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_slip.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_slip.o ../../../src/btstack_slip.c     
	
${OBJECTDIR}/_ext/1880736137/drv_tmr.o: ../../../../driver/tmr/src/dynamic/drv_tmr.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1880736137" 
	@${RM} ${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d 
	@${RM} ${OBJECTDIR}/_ext/1880736137/drv_tmr.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d" -o ${OBJECTDIR}/_ext/1880736137/drv_tmr.o ../../../../driver/tmr/src/dynamic/drv_tmr.c     
	
${OBJECTDIR}/_ext/1112166103/sys_clk.o: ../../../../system/clk/src/sys_clk.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1112166103" 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk.o.d 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1112166103/sys_clk.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1112166103/sys_clk.o.d" -o ${OBJECTDIR}/_ext/1112166103/sys_clk.o ../../../../system/clk/src/sys_clk.c     
	
${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o: ../../../../system/clk/src/sys_clk_pic32mx.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1112166103" 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d" -o ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o ../../../../system/clk/src/sys_clk_pic32mx.c     
	
${OBJECTDIR}/_ext/1510368962/sys_devcon.o: ../../../../system/devcon/src/sys_devcon.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1510368962" 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d" -o ${OBJECTDIR}/_ext/1510368962/sys_devcon.o ../../../../system/devcon/src/sys_devcon.c     
	
${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o: ../../../../system/devcon/src/sys_devcon_pic32mx.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1510368962" 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d" -o ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o ../../../../system/devcon/src/sys_devcon_pic32mx.c     
	
${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o: ../../../../system/int/src/sys_int_pic32.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/2087176412" 
	@${RM} ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d 
	@${RM} ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d" -o ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o ../../../../system/int/src/sys_int_pic32.c     
	
${OBJECTDIR}/_ext/2147153351/sys_ports.o: ../../../../system/ports/src/sys_ports.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/2147153351" 
	@${RM} ${OBJECTDIR}/_ext/2147153351/sys_ports.o.d 
	@${RM} ${OBJECTDIR}/_ext/2147153351/sys_ports.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/2147153351/sys_ports.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -fframe-base-loclist  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/2147153351/sys_ports.o.d" -o ${OBJECTDIR}/_ext/2147153351/sys_ports.o ../../../../system/ports/src/sys_ports.c     
	
else
${OBJECTDIR}/_ext/101891878/system_init.o: ../src/system_config/bt_audio_dk/system_init.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/101891878" 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_init.o.d 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_init.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/101891878/system_init.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/101891878/system_init.o.d" -o ${OBJECTDIR}/_ext/101891878/system_init.o ../src/system_config/bt_audio_dk/system_init.c     
	
${OBJECTDIR}/_ext/101891878/system_tasks.o: ../src/system_config/bt_audio_dk/system_tasks.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/101891878" 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_tasks.o.d 
	@${RM} ${OBJECTDIR}/_ext/101891878/system_tasks.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/101891878/system_tasks.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/101891878/system_tasks.o.d" -o ${OBJECTDIR}/_ext/101891878/system_tasks.o ../src/system_config/bt_audio_dk/system_tasks.c     
	
${OBJECTDIR}/_ext/1360937237/btstack_port.o: ../src/btstack_port.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/btstack_port.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/btstack_port.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/btstack_port.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1360937237/btstack_port.o.d" -o ${OBJECTDIR}/_ext/1360937237/btstack_port.o ../src/btstack_port.c     
	
${OBJECTDIR}/_ext/1360937237/app_debug.o: ../src/app_debug.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app_debug.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app_debug.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/app_debug.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1360937237/app_debug.o.d" -o ${OBJECTDIR}/_ext/1360937237/app_debug.o ../src/app_debug.c     
	
${OBJECTDIR}/_ext/1360937237/app.o: ../src/app.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/app.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/app.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1360937237/app.o.d" -o ${OBJECTDIR}/_ext/1360937237/app.o ../src/app.c     
	
${OBJECTDIR}/_ext/1360937237/main.o: ../src/main.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1360937237/main.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1360937237/main.o.d" -o ${OBJECTDIR}/_ext/1360937237/main.o ../src/main.c     
	
${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o: ../../../example/spp_and_le_counter.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/97075643" 
	@${RM} ${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o.d 
	@${RM} ${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o.d" -o ${OBJECTDIR}/_ext/97075643/spp_and_le_counter.o ../../../example/spp_and_le_counter.c     
	
${OBJECTDIR}/_ext/534563071/ad_parser.o: ../../../src/ad_parser.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/ad_parser.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/ad_parser.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/ad_parser.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/ad_parser.o.d" -o ${OBJECTDIR}/_ext/534563071/ad_parser.o ../../../src/ad_parser.c     
	
${OBJECTDIR}/_ext/534563071/att_db.o: ../../../src/ble/att_db.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_db.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_db.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_db.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_db.o.d" -o ${OBJECTDIR}/_ext/534563071/att_db.o ../../../src/ble/att_db.c     
	
${OBJECTDIR}/_ext/534563071/att_dispatch.o: ../../../src/ble/att_dispatch.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_dispatch.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_dispatch.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_dispatch.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_dispatch.o.d" -o ${OBJECTDIR}/_ext/534563071/att_dispatch.o ../../../src/ble/att_dispatch.c     
	
${OBJECTDIR}/_ext/534563071/att_server.o: ../../../src/ble/att_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/att_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/att_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/att_server.o.d" -o ${OBJECTDIR}/_ext/534563071/att_server.o ../../../src/ble/att_server.c     
	
${OBJECTDIR}/_ext/534563071/le_device_db_memory.o: ../../../src/ble/le_device_db_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/le_device_db_memory.o.d" -o ${OBJECTDIR}/_ext/534563071/le_device_db_memory.o ../../../src/ble/le_device_db_memory.c     
	
${OBJECTDIR}/_ext/534563071/sm.o: ../../../src/ble/sm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/534563071" 
	@${RM} ${OBJECTDIR}/_ext/534563071/sm.o.d 
	@${RM} ${OBJECTDIR}/_ext/534563071/sm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/534563071/sm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/534563071/sm.o.d" -o ${OBJECTDIR}/_ext/534563071/sm.o ../../../src/ble/sm.c     
	
${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o: ../../../chipset/csr/btstack_chipset_csr.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1768064806" 
	@${RM} ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d 
	@${RM} ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o.d" -o ${OBJECTDIR}/_ext/1768064806/btstack_chipset_csr.o ../../../chipset/csr/btstack_chipset_csr.c     
	
${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o: ../../../platform/embedded/btstack_run_loop_embedded.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/993942601" 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o.d" -o ${OBJECTDIR}/_ext/993942601/btstack_run_loop_embedded.o ../../../platform/embedded/btstack_run_loop_embedded.c     
	
${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o: ../../../platform/embedded/btstack_uart_block_embedded.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/993942601" 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d 
	@${RM} ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o.d" -o ${OBJECTDIR}/_ext/993942601/btstack_uart_block_embedded.o ../../../platform/embedded/btstack_uart_block_embedded.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_memory.o: ../../../src/btstack_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_memory.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_memory.o ../../../src/btstack_memory.c     
	
${OBJECTDIR}/_ext/1386528437/hci.o: ../../../src/hci.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci.o ../../../src/hci.c     
	
${OBJECTDIR}/_ext/1386528437/hci_cmd.o: ../../../src/hci_cmd.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_cmd.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_cmd.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_cmd.o ../../../src/hci_cmd.c     
	
${OBJECTDIR}/_ext/1386528437/hci_dump.o: ../../../src/hci_dump.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_dump.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_dump.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_dump.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_dump.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_dump.o ../../../src/hci_dump.c     
	
${OBJECTDIR}/_ext/1386528437/l2cap.o: ../../../src/l2cap.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/l2cap.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/l2cap.o.d" -o ${OBJECTDIR}/_ext/1386528437/l2cap.o ../../../src/l2cap.c     
	
${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o: ../../../src/l2cap_signaling.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o.d" -o ${OBJECTDIR}/_ext/1386528437/l2cap_signaling.o ../../../src/l2cap_signaling.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o: ../../../src/btstack_linked_list.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_linked_list.o ../../../src/btstack_linked_list.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o: ../../../src/btstack_memory_pool.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_memory_pool.o ../../../src/btstack_memory_pool.c     
	
${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o: ../../../src/classic/btstack_link_key_db_memory.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o.d" -o ${OBJECTDIR}/_ext/1386327864/btstack_link_key_db_memory.o ../../../src/classic/btstack_link_key_db_memory.c     
	
${OBJECTDIR}/_ext/1386327864/rfcomm.o: ../../../src/classic/rfcomm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/rfcomm.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/rfcomm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/rfcomm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/rfcomm.o.d" -o ${OBJECTDIR}/_ext/1386327864/rfcomm.o ../../../src/classic/rfcomm.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o: ../../../src/btstack_run_loop.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_run_loop.o ../../../src/btstack_run_loop.c     
	
${OBJECTDIR}/_ext/1386327864/sdp_server.o: ../../../src/classic/sdp_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_server.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_server.o ../../../src/classic/sdp_server.c     
	
${OBJECTDIR}/_ext/1386327864/sdp_client.o: ../../../src/classic/sdp_client.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_client.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_client.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_client.o ../../../src/classic/sdp_client.c     
	
${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o: ../../../src/classic/sdp_client_rfcomm.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_client_rfcomm.o ../../../src/classic/sdp_client_rfcomm.c     
	
${OBJECTDIR}/_ext/1386327864/sdp_util.o: ../../../src/classic/sdp_util.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_util.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/sdp_util.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/sdp_util.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/sdp_util.o.d" -o ${OBJECTDIR}/_ext/1386327864/sdp_util.o ../../../src/classic/sdp_util.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_util.o: ../../../src/btstack_util.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_util.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_util.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_util.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_util.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_util.o ../../../src/btstack_util.c     
	
${OBJECTDIR}/_ext/1386327864/spp_server.o: ../../../src/classic/spp_server.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386327864" 
	@${RM} ${OBJECTDIR}/_ext/1386327864/spp_server.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386327864/spp_server.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386327864/spp_server.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386327864/spp_server.o.d" -o ${OBJECTDIR}/_ext/1386327864/spp_server.o ../../../src/classic/spp_server.c     
	
${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o: ../../../src/hci_transport_h4.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_transport_h4.o ../../../src/hci_transport_h4.c     
	
${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o: ../../../src/hci_transport_h5.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o.d" -o ${OBJECTDIR}/_ext/1386528437/hci_transport_h5.o ../../../src/hci_transport_h5.c     
	
${OBJECTDIR}/_ext/1386528437/btstack_slip.o: ../../../src/btstack_slip.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1386528437" 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d 
	@${RM} ${OBJECTDIR}/_ext/1386528437/btstack_slip.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1386528437/btstack_slip.o.d" -o ${OBJECTDIR}/_ext/1386528437/btstack_slip.o ../../../src/btstack_slip.c     
	
${OBJECTDIR}/_ext/1880736137/drv_tmr.o: ../../../../driver/tmr/src/dynamic/drv_tmr.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1880736137" 
	@${RM} ${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d 
	@${RM} ${OBJECTDIR}/_ext/1880736137/drv_tmr.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1880736137/drv_tmr.o.d" -o ${OBJECTDIR}/_ext/1880736137/drv_tmr.o ../../../../driver/tmr/src/dynamic/drv_tmr.c     
	
${OBJECTDIR}/_ext/1112166103/sys_clk.o: ../../../../system/clk/src/sys_clk.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1112166103" 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk.o.d 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1112166103/sys_clk.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1112166103/sys_clk.o.d" -o ${OBJECTDIR}/_ext/1112166103/sys_clk.o ../../../../system/clk/src/sys_clk.c     
	
${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o: ../../../../system/clk/src/sys_clk_pic32mx.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1112166103" 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d 
	@${RM} ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o.d" -o ${OBJECTDIR}/_ext/1112166103/sys_clk_pic32mx.o ../../../../system/clk/src/sys_clk_pic32mx.c     
	
${OBJECTDIR}/_ext/1510368962/sys_devcon.o: ../../../../system/devcon/src/sys_devcon.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1510368962" 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1510368962/sys_devcon.o.d" -o ${OBJECTDIR}/_ext/1510368962/sys_devcon.o ../../../../system/devcon/src/sys_devcon.c     
	
${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o: ../../../../system/devcon/src/sys_devcon_pic32mx.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/1510368962" 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d 
	@${RM} ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o.d" -o ${OBJECTDIR}/_ext/1510368962/sys_devcon_pic32mx.o ../../../../system/devcon/src/sys_devcon_pic32mx.c     
	
${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o: ../../../../system/int/src/sys_int_pic32.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/2087176412" 
	@${RM} ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d 
	@${RM} ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o.d" -o ${OBJECTDIR}/_ext/2087176412/sys_int_pic32.o ../../../../system/int/src/sys_int_pic32.c     
	
${OBJECTDIR}/_ext/2147153351/sys_ports.o: ../../../../system/ports/src/sys_ports.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}/_ext/2147153351" 
	@${RM} ${OBJECTDIR}/_ext/2147153351/sys_ports.o.d 
	@${RM} ${OBJECTDIR}/_ext/2147153351/sys_ports.o 
	@${FIXDEPS} "${OBJECTDIR}/_ext/2147153351/sys_ports.o.d" $(SILENT) -rsi ${MP_CC_DIR}../  -c ${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -Os -I"." -I"../../../.." -I"../src" -I"../src/system_config/bt_audio_dk" -I"../../../src" -I"../../../chipset/csr" -I"../../../platform/embedded" -MMD -MF "${OBJECTDIR}/_ext/2147153351/sys_ports.o.d" -o ${OBJECTDIR}/_ext/2147153351/sys_ports.o ../../../../system/ports/src/sys_ports.c     
	
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
	${MP_CC} $(MP_EXTRA_LD_PRE)  -mdebugger -D__MPLAB_DEBUGGER_PK3=1 -mprocessor=$(MP_PROCESSOR_OPTION)  -o dist/${CND_CONF}/${IMAGE_TYPE}/app.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}    ../../../../../bin/framework/peripheral/PIC32MX450F256L_peripherals.a          -mreserve=boot@0x1FC02000:0x1FC02FEF -mreserve=boot@0x1FC02000:0x1FC0275F  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),--defsym=__MPLAB_DEBUG=1,--defsym=__DEBUG=1,--defsym=__MPLAB_DEBUGGER_PK3=1,--defsym=_min_heap_size=16,-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,dist/${CND_CONF}/${IMAGE_TYPE}/memoryfile.xml
	
else
dist/${CND_CONF}/${IMAGE_TYPE}/app.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk  ../../../../../bin/framework/peripheral/PIC32MX450F256L_peripherals.a 
	@${MKDIR} dist/${CND_CONF}/${IMAGE_TYPE} 
	${MP_CC} $(MP_EXTRA_LD_PRE)  -mprocessor=$(MP_PROCESSOR_OPTION)  -o dist/${CND_CONF}/${IMAGE_TYPE}/app.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}    ../../../../../bin/framework/peripheral/PIC32MX450F256L_peripherals.a        -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),--defsym=_min_heap_size=16,-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,dist/${CND_CONF}/${IMAGE_TYPE}/memoryfile.xml
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
