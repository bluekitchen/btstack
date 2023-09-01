# CMake file to download and UART firmware files for NXP Bluetooth/Wifi Controllers

# 88W8997
set(NXP_8997_PATH https://github.com/nxp-imx/imx-firmware/raw/lf-6.1.22_2.0.0/nxp/FwImage_8997)
set(NXP_8997_FILE uartuart8997_bt_v4.bin)
message("NXP 88W8997: Download ${NXP_8997_FILE}")
file(DOWNLOAD ${NXP_8997_PATH}/${NXP_8997_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${NXP_8997_FILE})

# IW416
set(NXP_IW416_PATH https://github.com/nxp-imx/imx-firmware/raw/lf-6.1.22_2.0.0/nxp/FwImage_IW416_SD)
set(NXP_IW416_FILE uartiw416_bt_v0.bin)
message("NXP IW416:   Download ${NXP_IW416_FILE}")
file(DOWNLOAD ${NXP_IW416_PATH}/${NXP_IW416_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${NXP_IW416_FILE})

# IW612
set(NXP_IW612_PATH https://github.com/nxp-imx/imx-firmware/raw/lf-6.1.22_2.0.0/nxp/FwImage_IW612_SD)
set(NXP_IW612_FILE uartspi_n61x_v1.bin.se)
message("NXP IW612:   Download ${NXP_IW612_FILE}")
file(DOWNLOAD ${NXP_IW612_PATH}/${NXP_IW612_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${NXP_IW612_FILE})
