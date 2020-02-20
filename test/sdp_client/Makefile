CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I.. -I${BTSTACK_ROOT}/src
CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined
LDFLAGS += -lCppUTest -lCppUTestExt
# -L$(CPPUTEST_HOME) 

VPATH += ${BTSTACK_ROOT}/src/classic 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    sdp_util.c	              \
	sdp_client.c		      \
	spp_server.c		      \
	mock.c 					  \
	hci_dump.c                \
    btstack_util.c			          \
 
COMMON_OBJ = $(COMMON:.c=.o)

all: sdp_rfcomm_query general_sdp_query service_attribute_search_query service_search_query

sdp_rfcomm_query: ${COMMON_OBJ} sdp_client_rfcomm.c sdp_rfcomm_query.c
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
	rm -f *.gcno *.gcda
	
