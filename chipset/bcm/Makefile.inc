#
# Makefile to download and convert .hcd init files for Broadcom Bluetooth chipsets
#

all: BCM43430A1.hcd CYW55500A1_001.002.032.0253.0000_Generic_UART_37_4MHz_wlbga_iPA_sLNA_ANT0.hcd CYW55560A1_001.002.087.0415.0000_Generic_UART_37_4MHz_fcbga_iPA_sLNA_ANT0.hcd

# used with Ampak AP6121 and BCM43438A1
BCM43430A1.hcd:
	curl -O -L https://github.com/OpenELEC/misc-firmware/raw/master/firmware/brcm/BCM43430A1.hcd

# Generic Infineon firmware for CYW55513 / CYW55500A1.
CYW55500A1_001.002.032.0253.0000_Generic_UART_37_4MHz_wlbga_iPA_sLNA_ANT0.hcd:
	curl -fLO https://raw.githubusercontent.com/Infineon/ifx-linux-bluetooth/e5f0a31b95d51bbeb7ea5ef152ab0ff563961ff8/fw/CYW55500A1_001.002.032.0253.0000_Generic_UART_37_4MHz_wlbga_iPA_sLNA_ANT0.hcd

# Generic Infineon firmware for CYW55573 / CYW55560A1.
CYW55560A1_001.002.087.0415.0000_Generic_UART_37_4MHz_fcbga_iPA_sLNA_ANT0.hcd:
	curl -fLO https://raw.githubusercontent.com/Infineon/ifx-linux-bluetooth/e5f0a31b95d51bbeb7ea5ef152ab0ff563961ff8/fw/CYW55560A1_001.002.087.0415.0000_Generic_UART_37_4MHz_fcbga_iPA_sLNA_ANT0.hcd
