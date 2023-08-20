# Makefile for libusb based PTS tests

BTSTACK_ROOT = ../..
POSIX_ROOT= ${BTSTACK_ROOT}/platform/posix

# CppuTest from pkg-config
CFLAGS  += ${shell pkg-config --cflags CppuTest}
LDFLAGS += ${shell pkg-config --libs   CppuTest}

include ${BTSTACK_ROOT}/example/Makefile.inc

COMMON = \
	ad_parser.c 				 \
	sdp_server.c			     \
	sdp_client_rfcomm.c		     \
    btstack_link_key_db_memory.c \
    btstack_linked_list.c	     \
    btstack_memory.c             \
    btstack_memory_pool.c        \
    btstack_run_loop.c		     \
    btstack_run_loop_posix.c     \
    btstack_util.c			     \
    hci.c			             \
    hci_cmd.c		             \
    hci_dump.c		             \
    l2cap.c			             \
    l2cap_signaling.c 			 \
    rfcomm.c			         \
    sdp_client.c		         \
    sdp_util.c	                 \


MOCK = \
	mock.c 						\
	test_sequences.c            \
    btstack_link_key_db_memory.c \
    btstack_linked_list.c	    \
    btstack_memory.c            \
    btstack_memory_pool.c       \
    btstack_util.c			    \
    hci_cmd.c					\
    hci_dump.c     				\
    sdp_util.c	                \

CFLAGS  += -DUNIT_TEST -g -Wall -Wnarrowing -Wconversion-null
CFLAGS  += -I. -I../
CFLAGS  += -I${BTSTACK_ROOT}/src
CFLAGS  += -I${BTSTACK_ROOT}/src/classic
CFLAGS  += -I${BTSTACK_ROOT}/platform/posix
# CFLAGS  += -D OCTAVE_OUTPUT

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))
MOCK_OBJ_COVERAGE   = $(addprefix build-coverage/,$(MOCK:.c=.o))
MOCK_OBJ_ASAN       = $(addprefix build-asan/,    $(MOCK:.c=.o))


# CC = gcc-fsf-4.9
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

EXAMPLES = hfp_at_parser_test hfp_ag_client_test hfp_hf_client_test cvsd_plc_test hfp_link_settings_test

all:  $(addprefix build-coverage/,${EXAMPLES}) $(addprefix build-asan/,${EXAMPLES}) build-asan/pklg_cvsd_test

build-%:
	mkdir -p $@


build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-coverage/%.o: %.cpp | build-coverage
	${CXX} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-asan/%.o: %.cpp | build-asan
	${CXX} -c $(CFLAGS_ASAN) $< -o $@

build-coverage/hfp_at_parser_test: ${COMMON_OBJ_COVERAGE} build-coverage/hfp_gsm_model.o build-coverage/hfp_ag.o build-coverage/hfp.o build-coverage/hfp_at_parser_test.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/hfp_hf_client_test: ${MOCK_OBJ_COVERAGE} build-coverage/hfp_hf.o build-coverage/hfp.o build-coverage/hfp_hf_client_test.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/hfp_ag_client_test: ${MOCK_OBJ_COVERAGE} build-coverage/hfp_gsm_model.o build-coverage/hfp_ag.o build-coverage/hfp.o build-coverage/hfp_ag_client_test.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/cvsd_plc_test: ${COMMON_OBJ_COVERAGE} build-coverage/btstack_cvsd_plc.o build-coverage/wav_util.o build-coverage/cvsd_plc_test.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/hfp_link_settings_test: ${MOCK_OBJ_COVERAGE} build-coverage/hfp_hf.o build-coverage/hfp.o build-coverage/hfp_link_settings_test.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@


build-asan/hfp_at_parser_test: ${COMMON_OBJ_ASAN} build-asan/hfp_gsm_model.o build-asan/hfp_ag.o build-asan/hfp.o build-asan/hfp_at_parser_test.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-asan/hfp_hf_client_test: ${MOCK_OBJ_ASAN} build-asan/hfp_hf.o build-asan/hfp.o build-asan/hfp_hf_client_test.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-asan/hfp_ag_client_test: ${MOCK_OBJ_ASAN} build-asan/hfp_gsm_model.o build-asan/hfp_ag.o build-asan/hfp.o build-asan/hfp_ag_client_test.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-asan/cvsd_plc_test: ${COMMON_OBJ_ASAN} build-asan/btstack_cvsd_plc.o build-asan/wav_util.o build-asan/cvsd_plc_test.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-asan/hfp_link_settings_test: ${MOCK_OBJ_ASAN} build-asan/hfp_hf.o build-asan/hfp.o build-asan/hfp_link_settings_test.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-asan/pklg_cvsd_test: build-asan/hci_dump.o build-asan/btstack_util.o build-asan/btstack_cvsd_plc.o build-asan/wav_util.o build-asan/pklg_cvsd_test.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

test: all
	mkdir -p results
	build-asan/hfp_at_parser_test
	build-asan/hfp_ag_client_test
	build-asan/hfp_hf_client_test
	build-asan/cvsd_plc_test
	build-asan/hfp_link_settings_test

coverage: all
	mkdir -p results
	build-coverage/hfp_at_parser_test
	build-coverage/hfp_ag_client_test
	build-coverage/hfp_hf_client_test
	build-coverage/cvsd_plc_test
	build-coverage/hfp_link_settings_test

pklg-test: build-asan/pklg_cvsd_test
	build-asan/pklg_cvsd_test pklg/test1
	build-asan/pklg_cvsd_test pklg/test2
	build-asan/pklg_cvsd_test pklg/test3
	build-asan/pklg_cvsd_test pklg/test4
	build-asan/pklg_cvsd_test pklg/test5

clean:
	rm -rf build-coverage build-asan
	rm -rf *.wav results/* pklg/*.wav
