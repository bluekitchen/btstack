CC = g++

# Requirements: http://www.cpputest.org/ should be placed in btstack/test

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include -I$(CPPUTEST_HOME)/include
LDFLAGS += -L$(CPPUTEST_HOME) -lCppUTest -lCppUTestExt

COMMON = \
    ${BTSTACK_ROOT}/src/sdp_util.c	              \
	${BTSTACK_ROOT}/src/sdp_parser.c		      \
    ${BTSTACK_ROOT}/src/utils.c			          \
 
COMMON_OBJ = $(COMMON:.c=.o)

all: sdp_rfcomm_query general_sdp_query

sdp_rfcomm_query: ${COMMON_OBJ} ${BTSTACK_ROOT}/src/sdp_query_rfcomm.c sdp_rfcomm_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

general_sdp_query: ${COMMON_OBJ} general_sdp_query.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f sdp_rfcomm_query general_sdp_query *.o ${BTSTACK_ROOT}/src/*.o
	rm -rf *.dSYM
	