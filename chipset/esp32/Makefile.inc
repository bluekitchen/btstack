#
# Makefile to download and convert .hcd init files for Broadcom Bluetooth chipsets
#

all: BCM43430A1.hcd

# used with Ampak AP6121 and BCM43438A1
BCM43430A1.hcd:
	curl -O -L https://github.com/OpenELEC/misc-firmware/raw/master/firmware/brcm/BCM43430A1.hcd
