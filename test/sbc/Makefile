test-sine:
	./sbc_decoder_test.py data/sine-4sb-mono.sbc data/sine-4sb-decoded-mono.wav
	./sbc_decoder_test.py data/sine-8sb-mono.sbc data/sine-8sb-decoded-mono.wav
	./sbc_decoder_test.py data/sine-4sb-stereo.sbc data/sine-4sb-decoded-stereo.wav
	./sbc_decoder_test.py data/sine-8sb-stereo.sbc data/sine-8sb-decoded-stereo.wav

	./sbc_encoder_test.py data/sine-mono.wav 16 4 31 0 data/sine-4sb-mono.sbc
	./sbc_encoder_test.py data/sine-mono.wav 16 8 64 0 data/sine-8sb-mono.sbc
	./sbc_encoder_test.py data/sine-stereo.wav 16 4 30 1 2 data/sine-4sb-stereo.sbc
	./sbc_encoder_test.py data/sine-stereo.wav 16 8 64 2 data/sine-8sb-stereo.sbc

test-short:
	./sbc_decoder_test.py data/fanfare-short-4sb-mono.sbc data/fanfare-short-4sb-decoded-mono.wav
	./sbc_decoder_test.py data/fanfare-short-8sb-mono.sbc data/fanfare-short-8sb-decoded-mono.wav
	./sbc_decoder_test.py data/fanfare-short-4sb-stereo.sbc data/fanfare-short-4sb-decoded-stereo.wav
	./sbc_decoder_test.py data/fanfare-short-8sb-stereo.sbc data/fanfare-short-8sb-decoded-stereo.wav
	
	./sbc_encoder_test.py data/fanfare-short-mono.wav 16 4 31 1 0 data/fanfare-short-4sb-mono.sbc
	./sbc_encoder_test.py data/fanfare-short-mono.wav 16 8 64 1 0 data/fanfare-short-8sb-mono.sbc
	./sbc_encoder_test.py data/fanfare-short-stereo.wav 16 4 30 1 2 data/fanfare-short-4sb-stereo.sbc
	./sbc_encoder_test.py data/fanfare-short-stereo.wav 16 8 62 1 2 data/fanfare-short-8sb-stereo.sbc
	
test:
	./sbc_decoder_test.py data/fanfare-4sb-mono.sbc data/fanfare-4sb-decoded-mono.wav
	./sbc_decoder_test.py data/fanfare-8sb-mono.sbc data/fanfare-8sb-decoded-mono.wav
	./sbc_decoder_test.py data/fanfare-4sb-stereo.sbc data/fanfare-4sb-decoded-stereo.wav
	./sbc_decoder_test.py data/fanfare-8sb-stereo.sbc data/fanfare-8sb-decoded-stereo.wav

	./sbc_encoder_test.py data/fanfare-mono.wav 16 4 31 0 data/fanfare-4sb-mono.sbc
	./sbc_encoder_test.py data/fanfare-mono.wav 16 8 64 0 data/fanfare-8sb-mono.sbc
	./sbc_encoder_test.py data/fanfare-stereo.wav 16 4 31 2 data/fanfare-4sb-stereo.sbc
	./sbc_encoder_test.py data/fanfare-stereo.wav 16 8 64 2 data/fanfare-8sb-stereo.sbc

clean:
	rm -f *.pyc *.wav *.sbc data/*-decoded.wav data/*-encoded.sbc 