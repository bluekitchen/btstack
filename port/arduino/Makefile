#
# Makefile to setup BTstack library in Arduino folder (OS X/Linux only) and create .zip archive
#

DIR=.
BTSTACK_ROOT=${DIR}/../..
DUMMY=$(shell )
VERSION=`sed -n -e 's/^.*BTSTACK_VERSION \"\(.*\)\"/\1/p' ${BTSTACK_ROOT}/platform/daemon/src/btstack_version.h`
BTSTACK_PACKAGE=/tmp/btstack
ARCHIVE=btstack-arduino-${VERSION}.zip

SRC_FILES  = btstack_memory.c btstack_linked_list.c btstack_memory_pool.c btstack_run_loop.c btstack_crypto.c
SRC_FILES += hci_dump.c hci.c hci_cmd.c  btstack_util.c l2cap.c ad_parser.c hci_transport_h4.c
BLE_FILES  = att_db.c att_server.c att_dispatch.c att_db_util.c le_device_db_memory.c gatt_client.c
BLE_FILES += sm.c ancs_client.h ancs_client.c
PORT_FILES = btstack_config.h bsp_arduino_em9301.cpp BTstack.cpp BTstack.h
EMBEDDED_FILES = btstack_run_loop_embedded.c btstack_uart_block_embedded.c

PATHS  = $(addprefix ${BTSTACK_ROOT}/src/, ${SRC_FILES})
PATHS += $(filter-out ${BTSTACK_ROOT}/src/btstack.h, $(wildcard ${BTSTACK_ROOT}/src/*.h))
PATHS += $(addprefix ${BTSTACK_ROOT}/src/ble/, ${BLE_FILES})
PATHS += $(wildcard  ${BTSTACK_ROOT}/src/ble/*.h)
PATHS += $(addprefix ${BTSTACK_ROOT}/platform/embedded/, ${EMBEDDED_FILES})
PATHS += $(wildcard  ${BTSTACK_ROOT}/platform/embedded/*.h)
PATHS += $(wildcard  ${BTSTACK_ROOT}/chipset/em9301/*)
PATHS += ${BTSTACK_ROOT}/port/arduino/examples
PATHS += $(addprefix ${DIR}/, ${PORT_FILES})

ARDUINO_LIBS=~/Documents/arduino/libraries/BTstack

all: release

clean:
	rm -rf ${BTSTACK_PACKAGE}

update_version:
	${BTSTACK_ROOT}/tool/get_version.sh 

install: update_version
	rm -rf ${ARDUINO_LIBS}
	mkdir ${ARDUINO_LIBS}
	cp -r ${PATHS} ${ARDUINO_LIBS}

release: update_version
	rm -rf ${BTSTACK_PACKAGE}
	mkdir ${BTSTACK_PACKAGE}
	cp -r ${PATHS} ${BTSTACK_PACKAGE}
	rm -f ${ARCHIVE}
	zip -r ${ARCHIVE} ${BTSTACK_PACKAGE}
	cp ${ARCHIVE} btstack-arduino-latest.zip
