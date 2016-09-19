CC=gcc

BTSTACK_ROOT = ../..
SBC_DECODER_ROOT = ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder
SBC_ENCODER_ROOT = ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder

include ${SBC_DECODER_ROOT}/Makefile.inc
include ${SBC_ENCODER_ROOT}/Makefile.inc

SBC_DECODER += \
	${BTSTACK_ROOT}/src/classic/btstack_sbc_plc.c \
	${BTSTACK_ROOT}/src/classic/btstack_sbc_bludroid.c \

SBC_ENCODER += \
	${BTSTACK_ROOT}/src/classic/btstack_sbc_bludroid.c \
	${BTSTACK_ROOT}/src/classic/hfp_msbc.c \

SBC_DECODER_OBJ  = $(SBC_DECODER:.c=.o) 
SBC_ENCODER_OBJ  = $(SBC_ENCODER:.c=.o)

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/src/classic -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${SBC_DECODER_ROOT}/include 
CFLAGS += -I${SBC_ENCODER_ROOT}/include 
CFLAGS += -D PRINT_SAMPLES -D PRINT_SCALEFACTORS -D OI_DEBUG -D SBC_NO_PCM_CPY_OPTION
# -D TRACE_EXECUTION -D CODEC_DEBUG 
LDFLAGS += -lCppUTest -lCppUTestExt
VPATH += ${SBC_DECODER_ROOT}/srce 
VPATH += ${SBC_ENCODER_ROOT}/srce
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON += \
	hci_dump.c		            \
	btstack_util.c 				\
	wav_util.c 					\

COMMON_OBJ  = $(COMMON:.c=.o) 

SBC_TESTS = sbc_decoder_test sbc_encoder_test 

all: ${SBC_TESTS}

sbc_decoder_test: ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${COMMON_OBJ} sbc_decoder_test.o  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sbc_encoder_test: ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${COMMON_OBJ} sbc_encoder_test.o  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./sbc_decoder_test data/sine-4sb-mono.sbc data/sine-4sb-decoded-mono.wav
	./sbc_encoder_test data/sine-mono.wav data/sine-4sb-mono.sbc

pytest-sine:
	./sbc_decoder_test.py data/sine-4sb-mono.sbc data/sine-4sb-decoded-mono.wav
	./sbc_decoder_test.py data/sine-8sb-mono.sbc data/sine-8sb-decoded-mono.wav
	./sbc_decoder_test.py data/sine-4sb-stereo.sbc data/sine-4sb-decoded-stereo.wav
	./sbc_decoder_test.py data/sine-8sb-stereo.sbc data/sine-8sb-decoded-stereo.wav

	./sbc_encoder_test.py data/sine-mono.wav 16 4 31 0 data/sine-4sb-mono.sbc
	./sbc_encoder_test.py data/sine-mono.wav 16 8 64 0 data/sine-8sb-mono.sbc
	./sbc_encoder_test.py data/sine-stereo.wav 16 4 30 1 2 data/sine-4sb-stereo.sbc
	./sbc_encoder_test.py data/sine-stereo.wav 16 8 64 2 data/sine-8sb-stereo.sbc

pytest-short:
	./sbc_decoder_test.py data/fanfare-short-4sb-mono.sbc 0 SIG data/fanfare-short-4sb-decoded-mono.wav
	./sbc_decoder_test.py data/fanfare-short-8sb-mono.sbc 0 V1 data/fanfare-short-8sb-decoded-mono.wav
	./sbc_decoder_test.py data/fanfare-short-4sb-stereo.sbc 2 SIG data/fanfare-short-4sb-decoded-stereo.wav
	./sbc_decoder_test.py data/fanfare-short-8sb-stereo.sbc 2 SIG data/fanfare-short-8sb-decoded-stereo.wav
	
	./sbc_encoder_test.py data/fanfare-short-mono.wav 16 4 31 1 0 data/fanfare-short-4sb-mono.sbc
	./sbc_encoder_test.py data/fanfare-short-mono.wav 16 8 64 1 0 data/fanfare-short-8sb-mono.sbc
	./sbc_encoder_test.py data/fanfare-short-stereo.wav 16 4 30 1 2 data/fanfare-short-4sb-stereo.sbc
	./sbc_encoder_test.py data/fanfare-short-stereo.wav 16 8 62 1 2 data/fanfare-short-8sb-stereo.sbc
	
pytest:
	./sbc_decoder_test.py data/fanfare-4sb-mono.sbc data/fanfare-4sb-decoded-mono.wav
	./sbc_decoder_test.py data/fanfare-8sb-mono.sbc data/fanfare-8sb-decoded-mono.wav
	./sbc_decoder_test.py data/fanfare-4sb-stereo.sbc data/fanfare-4sb-decoded-stereo.wav
	./sbc_decoder_test.py data/fanfare-8sb-stereo.sbc data/fanfare-8sb-decoded-stereo.wav

	./sbc_encoder_test.py data/fanfare-mono.wav 16 4 31 0 data/fanfare-4sb-mono.sbc
	./sbc_encoder_test.py data/fanfare-mono.wav 16 8 64 0 data/fanfare-8sb-mono.sbc
	./sbc_encoder_test.py data/fanfare-stereo.wav 16 4 31 2 data/fanfare-4sb-stereo.sbc
	./sbc_encoder_test.py data/fanfare-stereo.wav 16 8 64 2 data/fanfare-8sb-stereo.sbc

clean:
	rm -f *.pyc *.wav *.sbc data/*-decoded.wav data/*-encoded.sbc *.o $(SBC_TESTS) *.dSYM *_test