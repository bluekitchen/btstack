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
VPATH += ${BTSTACK_ROOT}/3rd-party/segger-rtt
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/chipset/cc256x
IPATH += ${BTSTACK_ROOT}/3rd-party/segger-rtt
IPATH += ${BTSTACK_ROOT}/src
IPATH += ${BTSTACK_ROOT}/platform/embedded
IPATH += ${BTSTACK_ROOT}/chipset/cc256x
IPATH += .

#
# Rules for building the uart_loopback_24mhz_brclk example.
#
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/startup_msp432p401r_${COMPILER}.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/system_msp432p401r.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/main.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/SEGGER_RTT.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/SEGGER_RTT_Syscalls_GCC.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/SEGGER_RTT_printf.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/hci_dump.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/btstack_linked_list.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/btstack_memory.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/btstack_memory_pool.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/btstack_run_loop.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/btstack_run_loop_embedded.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/btstack_uart_block_embedded.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/btstack_chipset_cc256x.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/hci_transport_h4.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/btstack_util.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/hci.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/hci_cmd.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/ad_parser.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${COMPILER}/bluetooth_init_cc2564B_1.6_BT_Spec_4.1.o
${COMPILER}/uart_loopback_24mhz_brclk.axf: ${ROOT}/driverlib/MSP432P4xx/${COMPILER}/msp432p4xx_driverlib.a
${COMPILER}/uart_loopback_24mhz_brclk.axf: uart_loopback_24mhz_brclk_gcc.ld
SCATTERgcc_uart_loopback_24mhz_brclk=linker_script_gcc.ld
ENTRY_uart_loopback_24mhz_brclk=resetISR
CFLAGSgcc=-DTARGET_IS_MSP432P4XX

#
# Include the automatically generated dependency files.
#
ifneq (${MAKECMDGOALS},clean)
-include ${wildcard ${COMPILER}/*.d} __dummy__
endif
