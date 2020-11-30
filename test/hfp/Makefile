CC=g++

# Makefile for libusb based PTS tests
BTSTACK_ROOT = ../..
POSIX_ROOT= ${BTSTACK_ROOT}/platform/posix
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

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

COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))
MOCK_OBJ   = $(addprefix build-coverage/,$(MOCK:.c=.o))

# CC = gcc-fsf-4.9
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

CFLAGS   = -g -Wall -Wmissing-prototypes -Wnarrowing 
# CFLAGS += -Werror
CFLAGS  += -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/src/classic -I${POSIX_ROOT} -I${BTSTACK_ROOT}/include
# CFLAGS  += -D OCTAVE_OUTPUT
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS_CPPUTEST += -lCppUTest -lCppUTestExt

EXAMPLES = hfp_at_parser_test hfp_ag_client_test hfp_hf_client_test cvsd_plc_test hfp_link_settings_test

all:  $(addprefix build-coverage/,${EXAMPLES}) pklg_cvsd_test

build-coverage:
	mkdir -p build-coverage

clean:
	rm -rf *.o $(EXAMPLES) $(CLIENT_EXAMPLES) *.dSYM *.wav results/* pklg/*.wav
	rm -f *.gcno *.gcda

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) $< -o $@

build-coverage/hfp_at_parser_test: ${COMMON_OBJ} build-coverage/hfp_gsm_model.o build-coverage/hfp_ag.o build-coverage/hfp.o build-coverage/hfp_at_parser_test.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

build-coverage/hfp_hf_client_test: ${MOCK_OBJ} build-coverage/hfp_hf.o build-coverage/hfp.o build-coverage/hfp_hf_client_test.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

build-coverage/hfp_ag_client_test: ${MOCK_OBJ} build-coverage/hfp_gsm_model.o build-coverage/hfp_ag.o build-coverage/hfp.o build-coverage/hfp_ag_client_test.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

build-coverage/cvsd_plc_test: ${COMMON_OBJ} build-coverage/btstack_cvsd_plc.o build-coverage/wav_util.o build-coverage/cvsd_plc_test.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

build-coverage/hfp_link_settings_test: ${MOCK_OBJ} build-coverage/hfp_hf.o build-coverage/hfp.o build-coverage/hfp_link_settings_test.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

pklg_cvsd_test: build-coverage/hci_dump.o build-coverage/btstack_util.o build-coverage/btstack_cvsd_plc.o build-coverage/wav_util.o build-coverage/pklg_cvsd_test.o | build-coverage
	${CC} $^ ${CFLAGS} -o $@

test: all
	mkdir -p results
	build-coverage/hfp_at_parser_test
	build-coverage/hfp_ag_client_test
	build-coverage/hfp_hf_client_test
	build-coverage/cvsd_plc_test
	build-coverage/hfp_link_settings_test

coverage: all
	mkdir -p results
	build-coverage/hfp_at_parser_test
	build-coverage/hfp_ag_client_test
	build-coverage/hfp_hf_client_test
	build-coverage/cvsd_plc_test
	build-coverage/hfp_link_settings_test

pklg-test: build-coverage/pklg_cvsd_test
	./pklg_cvsd_test pklg/test1
	./pklg_cvsd_test pklg/test2
	./pklg_cvsd_test pklg/test3
	./pklg_cvsd_test pklg/test4
	./pklg_cvsd_test pklg/test5
