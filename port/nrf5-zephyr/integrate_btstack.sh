#/bin/sh

ZEPHYR_BASE=../../..

echo "Adding BTstack sources as subsys/btstack"


# add btstack folder to subsys/Makefile
MAKEFILE_ADD_ON='obj-$(CONFIG_BTSTACK) += btstack/'
NET_MAKEFILE=${ZEPHYR_BASE}/subsys/Makefile
grep -q -F btstack ${NET_MAKEFILE} || echo ${MAKEFILE_ADD_ON} >> ${NET_MAKEFILE}

# add BTstack KConfig to net/Kconfig
SUBSYS_KCONFIG=${ZEPHYR_BASE}/subsys/Kconfig
grep -q -F btstack ${SUBSYS_KCONFIG} || echo 'source "subsys/btstack/Kconfig"' >> ${SUBSYS_KCONFIG}

# set Nordic Semiconductor as manufacturer
CTRL_H=${ZEPHYR_BASE}/subsys/bluetooth/controller/ll/ctrl.h
sed -i "s|#define RADIO_BLE_COMPANY_ID.*0xFFFF.|#define RADIO_BLE_COMPANY_ID (0x0059) // Nordic Semiconductor ASA|g" ${CTRL_H}


# create subsys/btstack
mkdir -p ${ZEPHYR_BASE}/subsys/btstack

# copy sources
rsync -a ../../src/ ${ZEPHYR_BASE}/subsys/btstack

# copy embedded run loop
rsync -a ../../platform/embedded/hal_cpu.h ${ZEPHYR_BASE}/subsys/btstack
rsync -a ../../platform/embedded/hal_time_ms.h ${ZEPHYR_BASE}/subsys/btstack
rsync -a ../../platform/embedded/hal_tick.h ${ZEPHYR_BASE}/subsys/btstack
rsync -a ../../platform/embedded/btstack_run_loop_embedded.h ${ZEPHYR_BASE}/subsys/btstack
rsync -a ../../platform/embedded/btstack_run_loop_embedded.c ${ZEPHYR_BASE}/subsys/btstack

# copy bludroid
rsync -a ../../3rd-party/bluedroid ${ZEPHYR_BASE}/subsys/btstack

# copy btstack_config.h
rsync -a btstack_config.h ${ZEPHYR_BASE}/subsys/btstack

# copy Kconfig
rsync -a Kconfig ${ZEPHYR_BASE}/subsys/btstack

# copy Makefiles
rsync -a Makefile.src 	        ${ZEPHYR_BASE}/subsys/btstack/Makefile
rsync -a Makefile.classic 	    ${ZEPHYR_BASE}/subsys/btstack/classic/Makefile
rsync -a Makefile.ble 	        ${ZEPHYR_BASE}/subsys/btstack/ble/Makefile
rsync -a Makefile.gatt-service  ${ZEPHYR_BASE}/subsys/btstack/ble/gatt-service/Makefile
rsync -a Makefile.bluedroid 	${ZEPHYR_BASE}/subsys/btstack/bluedroid/Makefile
rsync -a Makefile.bluedroid-encoder ${ZEPHYR_BASE}/subsys/btstack/bluedroid/encoder/srce/Makefile
rsync -a Makefile.bluedroid-decoder ${ZEPHYR_BASE}/subsys/btstack/bluedroid/decoder/srce/Makefile

# create samples/btstack
./create_examples.py


## Additonal changes for HCI Controller firmware in samples/bluetooth/hci-uart
HCI_UART=${ZEPHYR_BASE}/samples/bluetooth/hci_uart

# add flash scripts to hci-uart
rsync -a flash_nrf51_pca10028.sh ${HCI_UART}/flash_nrf51_pca10028.sh
rsync -a flash_nrf52_pca10040.sh ${HCI_UART}/flash_nrf52_pca10040.sh

# use 115200 baud
sed -i 's|CONFIG_UART_NRF5_BAUD_RATE=1000000|CONFIG_UART_NRF5_BAUD_RATE=115200|g' ${HCI_UART}/nrf5.conf

# provide static random address to host via hci read bd addr
grep -q -F ll_address_set ${HCI_UART}/src/main.c || cat hci_uart.patch | patch -d ${ZEPHYR_BASE} -p1
