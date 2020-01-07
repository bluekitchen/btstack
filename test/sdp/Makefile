CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall \
		  -I.. \
		  -I${BTSTACK_ROOT}/src \

CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined

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
	
COMMON_OBJ = $(COMMON:.c=.o)

all: sdp_record_builder

sdp_record_builder: ${COMMON_OBJ} sdp_record_builder.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./sdp_record_builder

clean:
	rm -f  sdp_record_builder
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda
	
