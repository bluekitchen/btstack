CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null -I. -I../ -I${BTSTACK_ROOT}/src
CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	ad_parser.c                 \
	ancs_client.c               \
	att_db.c                    \
	att_dispatch.c              \
	btstack_linked_list.c       \
	btstack_memory.c            \
	btstack_memory_pool.c       \
	btstack_util.c              \
	gatt_client.c               \
	hci_cmd.c                   \
	hci_dump.c                  \
	le_device_db_memory.c       \
	mock.c                      \

COMMON_OBJ = $(COMMON:.c=.o)

all: gatt_client_test le_central

# compile .ble description
profile.h: profile.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

gatt_client_test: profile.h ${COMMON_OBJ} gatt_client_test.o expected_results.h
	${CC} ${COMMON_OBJ} gatt_client_test.o ${CFLAGS} ${LDFLAGS} -o $@

le_central: ${COMMON_OBJ} le_central.o
	${CC} ${COMMON_OBJ} le_central.o ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./gatt_client_test
	./le_central
		
clean:
	rm -f  gatt_client_test le_central
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda

