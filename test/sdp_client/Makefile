CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I.. -I${BTSTACK_ROOT}/src
CFLAGS += -fprofile-arcs -ftest-coverage
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

COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))

all:  $(addprefix build-coverage/, sdp_rfcomm_query general_sdp_query service_attribute_search_query service_search_query)

build-coverage:
	mkdir -p build-coverage

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) ${CPPFLAGS} $< -o $@

build-coverage/sdp_rfcomm_query: ${COMMON_OBJ} build-coverage/sdp_client_rfcomm.o build-coverage/sdp_rfcomm_query.o build-coverage/btstack_linked_list.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

build-coverage/general_sdp_query: ${COMMON_OBJ} build-coverage/general_sdp_query.o build-coverage/btstack_linked_list.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

build-coverage/service_attribute_search_query: ${COMMON_OBJ} build-coverage/service_attribute_search_query.o build-coverage/btstack_linked_list.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

build-coverage/service_search_query: ${COMMON_OBJ} build-coverage/service_search_query.o build-coverage/btstack_linked_list.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	ASAN_OPTIONS=detect_leaks=0 build-coverage/sdp_rfcomm_query
	build-coverage/general_sdp_query
	build-coverage/service_attribute_search_query
	build-coverage/service_search_query

coverage: all
	ASAN_OPTIONS=detect_leaks=0 build-coverage//sdp_rfcomm_query
	build-coverage//general_sdp_query
	build-coverage//service_attribute_search_query
	build-coverage//service_search_query

clean:
	rm -f sdp_rfcomm_query general_sdp_query service_attribute_search_query service_search_query *.o *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda
	
