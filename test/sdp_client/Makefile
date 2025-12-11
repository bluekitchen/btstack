
include ../common.make

DEFINES := -DUNIT_TEST
INCLUDES := -I${BTSTACK_ROOT}/src
INCLUDES += -I..

CFLAGS += ${INCLUDES} ${DEFINES}
CXXFLAGS += ${INCLUDES} ${DEFINES}

VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    sdp_util.c	              \
	sdp_client.c		      \
	spp_server.c		      \
	mock.c 					  \
	hci_dump.c                \
    btstack_util.c            \
	btstack_linked_list.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all:  $(addprefix build-coverage/, sdp_rfcomm_query general_sdp_query service_attribute_search_query service_search_query) \
	  $(addprefix build-asan/,     sdp_rfcomm_query general_sdp_query service_attribute_search_query service_search_query)

build-coverage/sdp_rfcomm_query: ${COMMON_OBJ_COVERAGE} build-coverage/sdp_client_rfcomm.o

build-coverage/general_sdp_query: ${COMMON_OBJ_COVERAGE}

build-coverage/service_attribute_search_query: ${COMMON_OBJ_COVERAGE}

build-coverage/service_search_query: ${COMMON_OBJ_COVERAGE}

build-asan/sdp_rfcomm_query: ${COMMON_OBJ_ASAN} build-asan/sdp_client_rfcomm.o

build-asan/general_sdp_query: ${COMMON_OBJ_ASAN}

build-asan/service_attribute_search_query: ${COMMON_OBJ_ASAN}

build-asan/service_search_query: ${COMMON_OBJ_ASAN}


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
