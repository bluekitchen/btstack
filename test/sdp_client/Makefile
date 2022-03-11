# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -DUNIT_TEST -g -Wall -Wnarrowing -Wconversion-null
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I..

LDFLAGS += -lCppUTest -lCppUTestExt

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

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all:  $(addprefix build-coverage/, sdp_rfcomm_query general_sdp_query service_attribute_search_query service_search_query) \
	  $(addprefix build-asan/,     sdp_rfcomm_query general_sdp_query service_attribute_search_query service_search_query)

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

build-coverage/sdp_rfcomm_query: ${COMMON_OBJ_COVERAGE} build-coverage/sdp_client_rfcomm.o build-coverage/sdp_rfcomm_query.o build-coverage/btstack_linked_list.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/general_sdp_query: ${COMMON_OBJ_COVERAGE} build-coverage/general_sdp_query.o build-coverage/btstack_linked_list.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/service_attribute_search_query: ${COMMON_OBJ_COVERAGE} build-coverage/service_attribute_search_query.o build-coverage/btstack_linked_list.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/service_search_query: ${COMMON_OBJ_COVERAGE} build-coverage/service_search_query.o build-coverage/btstack_linked_list.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@


build-asan/sdp_rfcomm_query: ${COMMON_OBJ_ASAN} build-asan/sdp_client_rfcomm.o build-asan/sdp_rfcomm_query.o build-asan/btstack_linked_list.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-asan/general_sdp_query: ${COMMON_OBJ_ASAN} build-asan/general_sdp_query.o build-asan/btstack_linked_list.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-asan/service_attribute_search_query: ${COMMON_OBJ_ASAN} build-asan/service_attribute_search_query.o build-asan/btstack_linked_list.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-asan/service_search_query: ${COMMON_OBJ_ASAN} build-asan/service_search_query.o build-asan/btstack_linked_list.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@


test: all
	ASAN_OPTIONS=detect_leaks=0 build-asan/sdp_rfcomm_query
	build-asan/general_sdp_query
	build-asan/service_attribute_search_query
	build-asan/service_search_query

coverage: all
	rm -f build-coverage/*.gcda
	ASAN_OPTIONS=detect_leaks=0 build-coverage/sdp_rfcomm_query
	build-coverage/general_sdp_query
	build-coverage/service_attribute_search_query
	build-coverage/service_search_query
	
clean:
	rm -rf build-coverage build-asan
