#/bin/sh

ZEPHYR_BASE=../../..

echo "Integrating BTstack into Zephyr"

echo "Adding subsys/btstack"

# add btstack folder to subsys/Makefile
MAKEFILE_ADD_ON='obj-$(CONFIG_BTSTACK) += btstack/'
NET_MAKEFILE=${ZEPHYR_BASE}/subsys/Makefile
grep -q -F btstack ${NET_MAKEFILE} || echo ${MAKEFILE_ADD_ON} >> ${NET_MAKEFILE}

# add BTstack KConfig to subsys/Kconfig
SUBSYS_KCONFIG=${ZEPHYR_BASE}/subsys/Kconfig
grep -q -F btstack ${SUBSYS_KCONFIG} || echo 'source "subsys/btstack/Kconfig"' >> ${SUBSYS_KCONFIG}

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
rsync -a ../../platform/embedded/hci_dump_embedded_stdout.h  ${ZEPHYR_BASE}/subsys/btstack
rsync -a ../../platform/embedded/hci_dump_embedded_stdout.c  ${ZEPHYR_BASE}/subsys/btstack

# copy bludroid
rsync -a ../../3rd-party/bluedroid ${ZEPHYR_BASE}/subsys/btstack

# copy btstack_config.h
rsync -a btstack_config.h ${ZEPHYR_BASE}/subsys/btstack

# copy Kconfig
rsync -a Kconfig ${ZEPHYR_BASE}/subsys/btstack

# copy Makefiles
rsync -a Makefile.src 	        ${ZEPHYR_BASE}/subsys/btstack/Makefile
rsync -a Makefile.ble 	        ${ZEPHYR_BASE}/subsys/btstack/ble/Makefile
rsync -a Makefile.gatt-service  ${ZEPHYR_BASE}/subsys/btstack/ble/gatt-service/Makefile
rsync -a Makefile.bluedroid 	${ZEPHYR_BASE}/subsys/btstack/bluedroid/Makefile
rsync -a Makefile.bluedroid-encoder ${ZEPHYR_BASE}/subsys/btstack/bluedroid/encoder/srce/Makefile
rsync -a Makefile.bluedroid-decoder ${ZEPHYR_BASE}/subsys/btstack/bluedroid/decoder/srce/Makefile

# create samples/btstack
./create_examples.py
