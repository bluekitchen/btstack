# Makefile for libusb based PTS tests
BTSTACK_ROOT = ../..

CORE += \
	btstack_memory.c            \
	btstack_linked_list.c	    \
	btstack_memory_pool.c       \
	btstack_run_loop.c		    \
	btstack_util.c 	            \
	main.c 						\
	btstack_stdin_posix.c       \
	btstack_tlv.c 		\
	btstack_tlv_posix.c 		\
	rijndael.c \

COMMON += \
	hci.c			            \
	ad_parser.c                 \
	hci_cmd.c		            \
	hci_dump.c		            \
	hci_dump_posix_fs.c         \
	l2cap.c			            \
	l2cap_signaling.c	        \
	hci_transport_h2_libusb.c 	\
	btstack_crypto.c            \
	btstack_run_loop_posix.c 	\
	le_device_db_tlv.c 			\
	sm.c 						\
	uECC.c 					    \
	btstack_chipset_zephyr.c    \

ATT	+= \
	att_dispatch.c       	    \

GATT_SERVER += \
	att_db.c 				 	    \
	att_server.c        	    \

GATT_CLIENT += \
	gatt_client.c        	    \

SM += \
	sm.c 				 	    \

CFLAGS += -g -std=c99 -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wunused-parameter -Wredundant-decls -Wsign-compare
CFLAGS += -D ENABLE_TESTING_SUPPORT
CFLAGS += -DCOVERAGE
CFLAGS += -I.
CFLAGS += -Ibuild-coverage
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/tinydir
CFLAGS += -I${BTSTACK_ROOT}/chipset/zephyr
CFLAGS += -I${BTSTACK_ROOT}/platform/posix 
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/libusb
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/chipset/zephyr

# coverage
CFLAGS  += -fprofile-arcs -ftest-coverage
LDFLAGS += -fprofile-arcs -ftest-coverage

# use pkg-config for libusb
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)

CORE_OBJ = $(addprefix build-coverage/,$(CORE:.c=.o))
COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))
ATT_OBJ = $(addprefix build-coverage/,$(ATT:.c=.o))
SM_OBJ = $(addprefix build-coverage/,$(SM:.c=.o))
GATT_CLIENT_OBJ = $(addprefix build-coverage/,$(GATT_CLIENT:.c=.o))
GATT_SERVER_OBJ = $(addprefix build-coverage/,$(GATT_SERVER:.c=.o))

# compile .gatt descriptions
build-coverage/%.h: %.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

.PHONY: all coverage test

all: build-coverage/sm_test

build-%:
	mkdir -p $@

build-coverage/sm_test.h: sm_test.gatt | build-coverage
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) ${CPPFLAGS} $< -o $@

build-coverage/sm_test: build-coverage/sm_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ}  ${SM_OBJ} build-coverage/sm_test.o | build-coverage
	${CC} $(filter-out build-coverage/sm_test.h,$^) ${LDFLAGS} -o $@

myclean:
	rm -rf build-coverage
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda
	rm -rf SM_*

clean: myclean

test: build-coverage/sm_test
	rm -rf SM_*
	python3 ./sm_test.py

coverage: build-coverage/sm_test
	# delete trace data
	rm -f build-coverage/*.gcda
	# run tests
	make test
	# collect traces
	lcov --capture --rc lcov_branch_coverage=1 --directory . --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/test/*" --output-file coverage-sm-sc.info
	# demangle
	python3 ../coverage_demangle.py coverage-sm-sc.info
