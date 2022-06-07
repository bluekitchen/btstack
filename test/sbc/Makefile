BTSTACK_ROOT = ../..
SBC_DECODER_ROOT = ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder
SBC_ENCODER_ROOT = ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder

include ${SBC_DECODER_ROOT}/Makefile.inc
include ${SBC_ENCODER_ROOT}/Makefile.inc

SBC_DECODER += btstack_sbc_plc.c               btstack_sbc_decoder_bluedroid.c
SBC_ENCODER += btstack_sbc_encoder_bluedroid.c hfp_msbc.c \

SBC_DECODER_OBJ  = $(SBC_DECODER:.c=.o) 
SBC_ENCODER_OBJ  = $(SBC_ENCODER:.c=.o)

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/src/classic -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${SBC_DECODER_ROOT}/include 
CFLAGS += -I${SBC_ENCODER_ROOT}/include 
CFLAGS += -Werror=unused-parameter
# CFLAGS += -D OCTAVE_OUTPUT 
#CFLAGS += -D PRINT_SAMPLES -D PRINT_SCALEFACTORS -D OI_DEBUG -D TRACE_EXECUTION 
VPATH += ${SBC_DECODER_ROOT}/srce
VPATH += ${SBC_ENCODER_ROOT}/srce
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/port/libusb

COMMON += \
	hci_dump.c		            \
	btstack_util.c 				\
	wav_util.c 					\

COMMON_OBJ  = $(COMMON:.c=.o) 

SBC_TESTS = sbc_decoder_test msbc_encoder_test pklg_msbc_test
# sco_cvsd_test
#sbc_decoder_sine

all: ${SBC_TESTS}

sbc_decoder_test: ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${COMMON_OBJ} sbc_decoder_test.o  
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

msbc_encoder_test: ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${COMMON_OBJ} msbc_encoder_test.o  
	${CC} $^ ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

pklg_msbc_test: ${SBC_DECODER_OBJ} hci_dump.o btstack_util.o wav_util.o pklg_msbc_test.o  
	${CC} $^ ${CFLAGS} -o $@

sbc_decoder_sine: ${SBC_DECODER_OBJ} ${SBC_ENCODER_OBJ} ${COMMON_OBJ} sbc_decoder_sine.o data_sine_stereo_sbc.h
	${CC} $(filter-out data_sine_stereo_sbc.h,$^) ${CFLAGS} ${LDFLAGS_CPPUTEST} -o $@

data_sine_stereo_sbc.h: data/sine-stereo.sbc
	xxd -i -l 14800 $^ > $@

data_fanfare_8sb_stereo_sbc.h: data/fanfare-8sb-stereo.sbc
	xxd -i $^ > $@


test: all
	./sbc_decoder_test data/avdtp_sink sbc 0 0
	
	#./sbc_decoder_test data/sine-4sb-mono msbc 1 100
	#./sbc_encoder_test data/sine-mono.wav data/sine-4sb-mono.sbc

pytest-sine:
	./sbc_decoder_test.py data/sine-4sb-mono.sbc data/sine-4sb-decoded-mono.wav
	./sbc_decoder_test.py data/sine-8sb-mono.sbc data/sine-8sb-decoded-mono.wav
	./sbc_decoder_test.py data/sine-4sb-stereo.sbc data/sine-4sb-decoded-stereo.wav
	./sbc_decoder_test.py data/sine-8sb-stereo.sbc data/sine-8sb-decoded-stereo.wav

	./sbc_encoder_test.py data/sine-mono.wav 16 4 31 0 data/sine-4sb-mono.sbc
	./sbc_encoder_test.py data/sine-mono.wav 16 8 64 0 data/sine-8sb-mono.sbc
	./sbc_encoder_test.py data/sine-stereo.wav 16 4 30 1 2 data/sine-4sb-stereo.sbc
	./sbc_encoder_test.py data/sine-stereo.wav 16 8 64 2 data/sine-8sb-stereo.sbc

pytest:
	./sbc_decoder_test.py data/fanfare-4sb-mono.sbc data/fanfare-4sb-decoded-mono.wav
	./sbc_decoder_test.py data/fanfare-8sb-mono.sbc data/fanfare-8sb-decoded-mono.wav
	./sbc_decoder_test.py data/fanfare-4sb-stereo.sbc data/fanfare-4sb-decoded-stereo.wav
	./sbc_decoder_test.py data/fanfare-8sb-stereo.sbc data/fanfare-8sb-decoded-stereo.wav

	./sbc_encoder_test.py data/fanfare-mono.wav 16 4 31 0 data/fanfare-4sb-mono.sbc
	./sbc_encoder_test.py data/fanfare-mono.wav 16 8 64 0 data/fanfare-8sb-mono.sbc
	./sbc_encoder_test.py data/fanfare-stereo.wav 16 4 31 2 data/fanfare-4sb-stereo.sbc
	./sbc_encoder_test.py data/fanfare-stereo.wav 16 8 64 2 data/fanfare-8sb-stereo.sbc

pklg-test: pklg_msbc_test
	./pklg_msbc_test pklg/test1
	./pklg_msbc_test pklg/test2
	./pklg_msbc_test pklg/test3
	./pklg_msbc_test pklg/test4
	./pklg_msbc_test pklg/test5

clean:
	rm -f *.pyc *.wav *.sbc data/*-decoded.wav data/*-encoded.sbc *.o $(SBC_TESTS) *.dSYM *_test data_*.h pklg/*.wav pklg/*.m pklg/*.jpg
