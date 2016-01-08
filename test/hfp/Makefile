CC=g++

# Makefile for libusb based PTS tests
BTSTACK_ROOT = ../..
POSIX_ROOT= ${BTSTACK_ROOT}/platforms/posix
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

include ${BTSTACK_ROOT}/example/embedded/Makefile.inc

COMMON = \
    btstack_memory.c            \
    linked_list.c	            \
    memory_pool.c               \
    run_loop.c		            \
    run_loop_posix.c            \
    hci.c			            \
    hci_cmds.c		            \
    hci_dump.c		            \
    l2cap.c			            \
    l2cap_signaling.c 			\
    rfcomm.c			        \
    remote_device_db_memory.c   \
    sdp_util.c	                \
    utils.c			            \
    sdp_client.c		        \
    sdp_parser.c		        \
    sdp_query_util.c		    \
	sdp_query_rfcomm.c		    \
	sdp.c			            \


MOCK = \
	btstack_memory.c            \
    linked_list.c	            \
    memory_pool.c               \
    remote_device_db_memory.c   \
    hci_cmds.c					\
    hci_dump.c     				\
    sdp_util.c	                \
    utils.c			            \
	mock.c 						\
	test_sequences.c            \
	
COMMON_OBJ  = $(COMMON:.c=.o) 
MOCK_OBJ  = $(MOCK:.c=.o)

# CC = gcc-fsf-4.9
CFLAGS  = -g -Wall -Wmissing-prototype -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
CFLAGS += -Werror

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/ble
VPATH += ${BTSTACK_ROOT}/platforms/posix/src

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include -I${BTSTACK_ROOT}/ble
LDFLAGS += -lCppUTest -lCppUTestExt

EXAMPLES = hfp_ag_parser_test hfp_ag_client_test hfp_hf_parser_test hfp_hf_client_test 

all: ${BTSTACK_ROOT}/include/btstack/version.h ${EXAMPLES}

clean:
	rm -rf *.o $(EXAMPLES) $(CLIENT_EXAMPLES) *.dSYM

hfp_ag_parser_test: ${COMMON_OBJ} hfp_gsm_model.o hfp_ag.o hfp.o hfp_ag_parser_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_hf_parser_test: ${COMMON_OBJ} hfp_hf.o hfp.o hfp_hf_parser_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_hf_client_test: ${MOCK_OBJ} hfp_hf.o hfp.o hfp_hf_client_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_ag_client_test: ${MOCK_OBJ} hfp_gsm_model.o hfp_ag.o hfp.o hfp_ag_client_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./hfp_ag_parser_test
	./hfp_ag_client_test
	./hfp_hf_parser_test
	./hfp_hf_client_test
	
