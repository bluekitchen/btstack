#
# Makefile to setup BTstack library in Arduino folder (OS X/Linux only) and create .zip archive
#

DIR=.
BTSTACK_ROOT=${realpath ../..}
VERSION=$(shell ${BTSTACK_ROOT}/tool/get_git_version.sh)
ARCHIVE=$(BTSTACK_ROOT)/btstack-arduino-${VERSION}.zip

SRC_C_FILES  = btstack_memory.c btstack_linked_list.c btstack_memory_pool.c btstack_run_loop.c btstack_crypto.c
SRC_C_FILES += hci_dump.c hci.c hci_cmd.c  btstack_util.c l2cap.c l2cap_signaling.c ad_parser.c hci_transport_h4.c btstack_tlv.c
BLE_C_FILES  = att_db.c att_server.c att_dispatch.c att_db_util.c le_device_db_memory.c gatt_client.c
BLE_C_FILES += sm.c att_db_util.c
BLE_GATT_C_FILES = ancs_client.c
PORT_C_FILES = bsp_arduino_em9301.cpp BTstack.cpp
EMBEDDED_C_FILES = btstack_run_loop_embedded.c btstack_uart_block_embedded.c hci_dump_embedded_stdout.c
EM9301_C_FILES = btstack_chipset_em9301.c

PATHS  = $(addprefix ${BTSTACK_ROOT}/src/,     ${SRC_C_FILES})
PATHS += $(addprefix ${BTSTACK_ROOT}/src/ble/, ${BLE_C_FILES})
PATHS += $(addprefix ${BTSTACK_ROOT}/src/ble/gatt-service/, ${BLE_GATT_C_FILES})
PATHS += $(addprefix ${BTSTACK_ROOT}/platform/embedded/,    ${EMBEDDED_C_FILES})
PATHS += $(addprefix ${BTSTACK_ROOT}/chipset/em9301/,       ${EM9301_C_FILES})
PATHS += $(addprefix ${DIR}/, ${PORT_FILES})
PATHS += ${BTSTACK_ROOT}/port/arduino/examples

ARDUINO_LIBS=~/Documents/arduino/libraries/BTstack

all: release

update_version:
	${BTSTACK_ROOT}/tool/get_version.sh 

install: update_version
	rm -rf ${ARDUINO_LIBS}
	mkdir ${ARDUINO_LIBS}
	cd ${ARDUINO_LIBS} && unzip ${ARCHIVE}

release: update_version
	rm -f ${ARCHIVE}
	cd ${BTSTACK_ROOT}/chipset/em9301    && zip    $(ARCHIVE) *.h
	cd ${BTSTACK_ROOT}/platform/embedded && zip    $(ARCHIVE) *.h
	cd ${BTSTACK_ROOT}/src 				 && zip    $(ARCHIVE) *.h ble/*.h ble/gatt-service/*.h
	# remove the duplicate BTstack.h
	zip -d ${ARCHIVE} btstack.h
	# port specific files
	zip ${ARCHIVE} *.h *.cpp
	cd ${BTSTACK_ROOT}/port/arduino      && zip -r $(ARCHIVE) examples
	# all c files
	zip --junk-paths ${ARCHIVE} ${PATHS}
	cp ${ARCHIVE} btstack-arduino-latest.zip
