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
	
COMMON_OBJ  = $(COMMON:.c=.o) 
MOCK_OBJ  = $(MOCK:.c=.o)

# CC = gcc-fsf-4.9
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

CFLAGS   = -g -Wall -Wmissing-prototypes -Wnarrowing 
# CFLAGS += -Werror
CFLAGS  += -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/src/classic -I${POSIX_ROOT} -I${BTSTACK_ROOT}/include
# CFLAGS  += -D OCTAVE_OUTPUT
CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address
LDFLAGS_CPPUTEST += -lCppUTest -lCppUTestExt

EXAMPLES = hfp_ag_parser_test hfp_ag_client_test hfp_hf_parser_test hfp_hf_client_test cvsd_plc_test pklg_cvsd_test

all: ${EXAMPLES}

clean:
	rm -rf *.o $(EXAMPLES) $(CLIENT_EXAMPLES) *.dSYM *.wav results/* pklg/*.wav
	rm -f *.gcno *.gcda

hfp_ag_parser_test: ${COMMON_OBJ} hfp_gsm_model.o hfp_ag.o hfp.o hfp_ag_parser_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

hfp_hf_parser_test: ${COMMON_OBJ} hfp_hf.o hfp.o hfp_hf_parser_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

hfp_hf_client_test: ${MOCK_OBJ} hfp_hf.o hfp.o hfp_hf_client_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

hfp_ag_client_test: ${MOCK_OBJ} hfp_gsm_model.o hfp_ag.o hfp.o hfp_ag_client_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

cvsd_plc_test: ${COMMON_OBJ} btstack_cvsd_plc.o wav_util.o cvsd_plc_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

pklg_cvsd_test: hci_dump.o btstack_util.o btstack_cvsd_plc.o wav_util.o pklg_cvsd_test.o
	${CC} $^ ${CFLAGS} -o $@

test: all
	mkdir -p results
	./hfp_ag_parser_test
	./hfp_ag_client_test
	./hfp_hf_parser_test
	./hfp_hf_client_test
	./cvsd_plc_test

pklg-test: pklg_cvsd_test
	./pklg_cvsd_test pklg/test1
	./pklg_cvsd_test pklg/test2
	./pklg_cvsd_test pklg/test3
	./pklg_cvsd_test pklg/test4
	./pklg_cvsd_test pklg/test5
