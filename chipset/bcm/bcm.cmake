#
# CMake file to download and convert .hcd init files for Broadcom Bluetooth chipsets
#

# used with AMPAK AP6121 and BCM43438A1
file(DOWNLOAD https://github.com/OpenELEC/misc-firmware/raw/master/firmware/brcm/BCM43430A1.hcd ${CMAKE_CURRENT_BINARY_DIR}/BCM43430A1.hcd)
