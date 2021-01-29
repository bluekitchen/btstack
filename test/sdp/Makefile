CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall \
		  -I.. \
		  -I${BTSTACK_ROOT}/src \

LDFLAGS += -lCppUTest -lCppUTestExt 

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic

COMMON = \
	btstack_util.c		  \
	hci_dump.c    \
	hci.c \
	hci_cmd.c \
	btstack_run_loop.c \
	rfcomm.c \
	ad_parser.c \
	sdp_client.c \
	sdp_client_rfcomm.c \
	l2cap.c \
	l2cap_signaling.c \
	btstack_linked_list.c \
	btstack_memory.c \
	btstack_memory_pool.c \
	device_id_server.c \
	avdtp.c \
	a2dp_source.c \
	a2dp_sink.c \
	avdtp.c \
	avdtp_util.c \
	avdtp_sink.c \
	avdtp_source.c \
	avdtp_acceptor.c \
	avdtp_initiator.c \
	avrcp.c \
	avrcp_controller.c \
	avrcp_target.c \
	hfp.c \
	hfp_gsm_model.c \
	hfp_ag.c \
	hfp_hf.c \
	hsp_hs.c \
	hsp_ag.c \
	hid_device.c \
	pan.c \
	sdp_util.c \
	spp_server.c \
	btstack_hid_parser.c \
	
CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))


all: build-coverage/sdp_record_builder build-asan/sdp_record_builder

build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@


build-coverage/sdp_record_builder: ${COMMON_OBJ_COVERAGE} build-coverage/sdp_record_builder.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/sdp_record_builder: ${COMMON_OBJ_ASAN} build-asan/sdp_record_builder.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@


test: all
	build-asan/sdp_record_builder

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/sdp_record_builder

clean:
	rm -rf build-coverage build-asan
	
