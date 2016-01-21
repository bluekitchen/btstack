CC=g++

# Makefile for libusb based PTS tests
BTSTACK_ROOT = ../..
POSIX_ROOT= ${BTSTACK_ROOT}/platform/posix
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

include ${BTSTACK_ROOT}/example/embedded/Makefile.inc

COMMON = \
	sdp.c			            \
	sdp_query_rfcomm.c		    \
    btstack_linked_list.c	    \
    btstack_memory.c            \
    btstack_memory_pool.c       \
    btstack_run_loop.c		    \
    btstack_run_loop_posix.c    \
    btstack_util.c			    \
    hci.c			            \
    hci_cmd.c		            \
    hci_dump.c		            \
    l2cap.c			            \
    l2cap_signaling.c 			\
    remote_device_db_memory.c   \
    rfcomm.c			        \
    sdp_client.c		        \
    sdp_parser.c		        \
    sdp_query_util.c		    \
    sdp_util.c	                \


MOCK = \
	mock.c 						\
	test_sequences.c            \
    btstack_linked_list.c	    \
    btstack_memory.c            \
    hci_cmd.c					\
    hci_dump.c     				\
    btstack_memory_pool.c       \
    remote_device_db_memory.c   \
    sdp_util.c	                \
    btstack_util.c			    \
	
COMMON_OBJ  = $(COMMON:.c=.o) 
MOCK_OBJ  = $(MOCK:.c=.o)

# CC = gcc-fsf-4.9
CFLAGS  = -g -Wall \
		  -I. \
		  -I.. \
		  -I${BTSTACK_ROOT}/src
# CFLAGS += -Werror

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include -I${BTSTACK_ROOT}/ble
LDFLAGS += -lCppUTest -lCppUTestExt

EXAMPLES = hfp_ag_parser_test hfp_ag_client_test hfp_hf_parser_test hfp_hf_client_test 

all: ${BTSTACK_ROOT}/src/btstack_version.h ${EXAMPLES}

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
	
