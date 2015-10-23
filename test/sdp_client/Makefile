CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I.. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include -I${BTSTACK_ROOT}/ble
LDFLAGS += -lCppUTest -lCppUTestExt
# -L$(CPPUTEST_HOME) 

VPATH += ${BTSTACK_ROOT}/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platforms/posix/src

COMMON = \
    sdp_util.c	              \
	sdp_parser.c		      \
	hci_dump.c                \
    utils.c			          \
 
COMMON_OBJ = $(COMMON:.c=.o)

all: sdp_rfcomm_query general_sdp_query service_attribute_search_query service_search_query

sdp_rfcomm_query: ${COMMON_OBJ} sdp_query_rfcomm.c sdp_rfcomm_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

general_sdp_query: ${COMMON_OBJ} general_sdp_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

service_attribute_search_query: ${COMMON_OBJ} service_attribute_search_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

service_search_query: ${COMMON_OBJ} service_search_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./sdp_rfcomm_query
	./general_sdp_query
	./service_attribute_search_query
	./service_search_query
	
clean:
	rm -f sdp_rfcomm_query general_sdp_query service_attribute_search_query service_search_query *.o *.o
	rm -rf *.dSYM
	