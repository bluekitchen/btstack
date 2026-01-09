
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

all: coverage test

build-coverage/sdp_rfcomm_query: ${COMMON_OBJ_COVERAGE} build-coverage/sdp_client_rfcomm.o

build-coverage/general_sdp_query: ${COMMON_OBJ_COVERAGE}

build-coverage/service_attribute_search_query: ${COMMON_OBJ_COVERAGE}

build-coverage/service_search_query: ${COMMON_OBJ_COVERAGE}

build-asan/sdp_rfcomm_query: ${COMMON_OBJ_ASAN} build-asan/sdp_client_rfcomm.o

build-asan/general_sdp_query: ${COMMON_OBJ_ASAN}

build-asan/service_attribute_search_query: ${COMMON_OBJ_ASAN}

build-asan/service_search_query: ${COMMON_OBJ_ASAN}


test: build-asan/sdp_rfcomm_query \
	  build-asan/general_sdp_query \
	  build-asan/service_attribute_search_query \
	  build-asan/service_search_query
	ASAN_OPTIONS=detect_leaks=0 build-asan/sdp_rfcomm_query
	build-asan/general_sdp_query
	build-asan/service_attribute_search_query
	build-asan/service_search_query

coverage: build-coverage/sdp_rfcomm_query.info \
		  build-coverage/general_sdp_query.info \
		  build-coverage/service_attribute_search_query.info \
		  build-coverage/service_search_query.info
	
clean:
	rm -rf build-coverage build-asan
