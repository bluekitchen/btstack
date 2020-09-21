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

CFLAGS += -I.
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
CFLAGS  += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined -DCOVERAGE
LDFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined

# use pkg-config for libusb
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)


CORE_OBJ    = $(CORE:.c=.o)
COMMON_OBJ  = $(COMMON:.c=.o) 
ATT_OBJ     = $(ATT:.c=.o)
SM_OBJ = $(SM:.c=.o) $(MICROECC:.c=.o)
GATT_CLIENT_OBJ = $(GATT_CLIENT:.c=.o)
GATT_SERVER_OBJ = $(GATT_SERVER:.c=.o)

# compile .gatt descriptions
%.h: %.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

all: sm_test

sm_test: sm_test.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${GATT_CLIENT_OBJ}  ${SM_OBJ} sm_test.o
	${CC} $(filter-out sm_test.h,$^) ${LDFLAGS} -o $@

myclean:
	rm -rf sm_test.h sm_test
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda
	
clean: myclean

test: sm_test
	./sm_test

coverage: sm_test
	# delete trace data
	# find . -name "*.gcda" -type f -delete
	# run tests
	#./sm_test
	# collect traces
	lcov --capture --rc lcov_branch_coverage=1 --directory . --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/test/*" --output-file coverage-sm-sc.info
	# strip path prefix such that paths start with 'btstack'
	sed -i.bak -e 's|/Users/mringwal/buildbot-worker/test-develop/build|btstack|' coverage-sm-sc.info
	# generate html output
	genhtml coverage-sm-sc.info --branch-coverage --output-directory coverage-sm-sc
