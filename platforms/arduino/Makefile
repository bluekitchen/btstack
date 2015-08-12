#
# Makefile to setup BTstack library in Arduino folder (OS X/Linux only) and create .zip archive
#

DIR=.
BTSTACK_ROOT=${DIR}/../..
DUMMY=$(shell )
VERSION=`sed -n -e 's/^.*BTSTACK_VERSION \"\(.*\)\"/\1/p' ${BTSTACK_ROOT}/include/btstack/version.h`
BTSTACK_PACKAGE=/tmp/btstack
ARCHIVE=btstack-arduino-${VERSION}.zip

SRC_FILES  =btstack_memory.c linked_list.c memory_pool.c run_loop.c run_loop_embedded.c
SRC_FILES +=hci_dump.c hci.c hci_cmds.c hci_transport_h4_dma.c sdp_util.c utils.c
BLE_FILES  =  ad_parser.c att.c att_server.c att_dispatch.c att_db_util.c le_device_db_memory.c gatt_client.c
BLE_FILES  += sm.c l2cap_le.c ancs_client_lib.h ancs_client_lib.c
PORT_FILES = btstack-config.h bsp_arduino_em9301.cpp BTstack.cpp BTstack.h

PATHS  = $(addprefix ${BTSTACK_ROOT}/src/, ${SRC_FILES})
PATHS += $(wildcard  ${BTSTACK_ROOT}/src/*.h)
PATHS += $(addprefix ${BTSTACK_ROOT}/ble/, ${BLE_FILES})
PATHS += $(wildcard  ${BTSTACK_ROOT}/ble/*.h)
PATHS += $(wildcard  ${BTSTACK_ROOT}/chipset-em9301/*)
PATHS += ${BTSTACK_ROOT}/include/btstack
PATHS += ${BTSTACK_ROOT}/platforms/arduino/examples
PATHS += $(addprefix ${DIR}/, ${PORT_FILES})

ARDUINO_LIBS=~/Documents/arduino/libraries/BTstack

all: release

clean:
	rm -rf ${BTSTACK_PACKAGE}

update_version:
	${BTSTACK_ROOT}/tools/get_version.sh 

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
