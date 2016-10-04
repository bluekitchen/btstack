CC=g++

# Makefile for libusb based PTS tests
BTSTACK_ROOT = ../..
POSIX_ROOT= ${BTSTACK_ROOT}/platform/posix
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

include ${BTSTACK_ROOT}/example/Makefile.inc

COMMON = \
	sdp_server.c			            \
	sdp_client_rfcomm.c		    \
    btstack_link_key_db_memory.c \
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
    rfcomm.c			        \
    sdp_client.c		        \
    sdp_util.c	                \


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
CFLAGS  = -g -Wall -Wmissing-prototype -Wnarrowing \
		  -I. \
		  -I.. \
		  -I${BTSTACK_ROOT}/src
# CFLAGS += -Werror

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/src/classic -I${POSIX_ROOT} -I${BTSTACK_ROOT}/include -I${BTSTACK_ROOT}/ble
LDFLAGS += -lCppUTest -lCppUTestExt

EXAMPLES = hfp_ag_parser_test hfp_ag_client_test hfp_hf_parser_test hfp_hf_client_test cvsd_plc_test

all: ${EXAMPLES}

clean:
	rm -rf *.o $(EXAMPLES) $(CLIENT_EXAMPLES) *.dSYM *.wav 

hfp_ag_parser_test: ${COMMON_OBJ} hfp_gsm_model.o hfp_ag.o hfp.o hfp_ag_parser_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_hf_parser_test: ${COMMON_OBJ} hfp_hf.o hfp.o hfp_hf_parser_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_hf_client_test: ${MOCK_OBJ} hfp_hf.o hfp.o hfp_hf_client_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

hfp_ag_client_test: ${MOCK_OBJ} hfp_gsm_model.o hfp_ag.o hfp.o hfp_ag_client_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

cvsd_plc_test: ${COMMON_OBJ} btstack_cvsd_plc.o wav_util.o cvsd_plc_test.c  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./hfp_ag_parser_test
	./hfp_ag_client_test
	./hfp_hf_parser_test
	./hfp_hf_client_test
	./cvsd_plc_test
