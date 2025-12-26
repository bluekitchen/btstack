# Requirements: cpputest.github.io


include ../common.make

INCLUDES := -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/platform/posix
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/mbedtls/include
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/rijndael
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES}
CXXFLAGS += ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

all: build-coverage/aes_ccm_test build-coverage/aestest build-coverage/ecc_micro_ecc build-coverage/aes_cmac_test build-coverage/aes_cmac_test2 \
	 build-asan/aes_ccm_test build-asan/aestest build-asan/ecc_micro_ecc build-asan/aes_cmac_test build-asan/aes_cmac_test2

build-coverage/aes_ccm_test: build-coverage/aes_ccm.o build-coverage/aes_ccm_test.o build-coverage/btstack_crypto.o build-coverage/btstack_linked_list.o build-coverage/hci_cmd.o build-coverage/btstack_util.o build-coverage/hci_dump.o build-coverage/aes_cmac.o build-coverage/rijndael.o build-coverage/uECC.o build-coverage/mock.o

build-coverage/aestest: build-coverage/rijndael.o build-coverage/hci_dump.o build-coverage/btstack_util.o

build-coverage/ecc_micro_ecc: build-coverage/uECC.o  build-coverage/hci_dump.o build-coverage/btstack_util.o

build-coverage/aes_cmac_test: build-coverage/aes_cmac.o build-coverage/rijndael.o  build-coverage/hci_dump.o build-coverage/btstack_util.o

build-coverage/aes_cmac_test2: build-coverage/btstack_crypto.o  build-coverage/btstack_linked_list.o  build-coverage/hci_cmd.o  build-coverage/btstack_util.o  build-coverage/hci_dump.o  build-coverage/rijndael.o build-coverage/uECC.o

build-asan/aes_ccm_test: build-asan/aes_ccm.o build-asan/btstack_crypto.o build-asan/btstack_linked_list.o build-asan/hci_cmd.o build-asan/btstack_util.o build-asan/hci_dump.o build-asan/aes_cmac.o build-asan/rijndael.o build-asan/uECC.o build-asan/mock.o

build-asan/aestest: build-asan/rijndael.o build-asan/hci_dump.o build-asan/btstack_util.o

build-asan/ecc_micro_ecc: build-asan/uECC.o build-asan/hci_dump.o build-asan/btstack_util.o

build-asan/aes_cmac_test: build-asan/aes_cmac.o build-asan/rijndael.o

build-asan/aes_cmac_test2: build-asan/btstack_crypto.o  build-asan/btstack_linked_list.o  build-asan/hci_cmd.o  build-asan/btstack_util.o  build-asan/hci_dump.o build-asan/uECC.o build-asan/rijndael.o

test: all
	build-asan/aes_cmac_test
	build-asan/aes_cmac_test2
	build-asan/aes_ccm_test
	build-asan/aestest
	build-asan/ecc_micro_ecc

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/aes_cmac_test
	build-coverage/aes_cmac_test2
	build-coverage/aes_ccm_test
	build-coverage/aestest
	build-coverage/ecc_micro_ecc

clean:
	rm -rf build-coverage build-asan

