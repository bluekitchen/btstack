CC = g++

# Requirements: http://www.cpputest.org/ should be placed in btstack/test

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I${BTSTACK_ROOT}/example/libusb -I${BTSTACK_ROOT}/include -I$(CPPUTEST_HOME)/include
LDFLAGS += -L$(CPPUTEST_HOME) -lCppUTest -lCppUTestExt

COMMON = \
	${BTSTACK_ROOT}/example/libusb/ad_parser.c              \
	
COMMON_OBJ = $(COMMON:.c=.o)

all: ad_parser 

ad_parser: ${CORE_OBJ} ${COMMON_OBJ} advertising_data_parser.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} advertising_data_parser.c ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f  ad_parser
	rm -f  *.o ${BTSTACK_ROOT}/src/*.o
	rm -rf *.dSYM
	