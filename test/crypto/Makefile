# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -g
CPPFLAGS = -Wall -Wno-unused -fno-exceptions
CFLAGS += -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/mbedtls/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

all: build-coverage/aes_ccm_test build-coverage/aestest build-coverage/ecc_micro_ecc build-coverage/aes_cmac_test build-coverage/aes_cmac_test2 \
	 build-asan/aes_ccm_test build-asan/aestest build-asan/ecc_micro_ecc build-asan/aes_cmac_test build-asan/aes_cmac_test2

build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c ${CFLAGS_COVERAGE} $< -o $@

build-coverage/%.o: %.cpp | build-coverage
	${CXX} -c ${CFLAGS_COVERAGE} $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c ${CFLAGS_ASAN} $< -o $@

build-asan/%.o: %.cpp | build-asan
	${CXX} -c ${CFLAGS_ASAN} $< -o $@


build-coverage/aes_ccm_test: build-coverage/aes_ccm.o build-coverage/aes_ccm_test.o build-coverage/btstack_crypto.o build-coverage/btstack_linked_list.o build-coverage/hci_cmd.o build-coverage/btstack_util.o build-coverage/hci_dump.o build-coverage/aes_cmac.o build-coverage/rijndael.o build-coverage/mock.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/aestest: build-coverage/aestest.o build-coverage/rijndael.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/ecc_micro_ecc: build-coverage/ecc_micro_ecc.o build-coverage/uECC.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/aes_cmac_test: build-coverage/aes_cmac_test.o build-coverage/aes_cmac.o build-coverage/rijndael.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/aes_cmac_test2: build-coverage/aes_cmac_test2.o build-coverage/btstack_crypto.o  build-coverage/btstack_linked_list.o  build-coverage/hci_cmd.o  build-coverage/btstack_util.o  build-coverage/hci_dump.o  build-coverage/rijndael.o | build-asan
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@


build-asan/aes_ccm_test: build-asan/aes_ccm.o build-asan/aes_ccm_test.o build-asan/btstack_crypto.o build-asan/btstack_linked_list.o build-asan/hci_cmd.o build-asan/btstack_util.o build-asan/hci_dump.o build-asan/aes_cmac.o build-asan/rijndael.o build-asan/mock.o | build-asan
	${CXX} $^  ${LDFLAGS_ASAN} -o $@

build-asan/aestest: build-asan/aestest.o build-asan/rijndael.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-asan/ecc_micro_ecc: build-asan/ecc_micro_ecc.o build-asan/uECC.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-asan/aes_cmac_test: build-asan/aes_cmac_test.o build-asan/aes_cmac.o build-asan/rijndael.o | build-asan
	${CXX} $^  ${LDFLAGS_ASAN} -o $@

build-asan/aes_cmac_test2: build-asan/aes_cmac_test2.o build-asan/btstack_crypto.o  build-asan/btstack_linked_list.o  build-asan/hci_cmd.o  build-asan/btstack_util.o  build-asan/hci_dump.o  build-asan/rijndael.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

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

