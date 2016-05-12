test-sine:
	./sbc_decoder_test.py sine-4sb-mono.sbc sine-4sb-decoded-mono.wav
	./sbc_decoder_test.py sine-8sb-mono.sbc sine-8sb-decoded-mono.wav
	./sbc_decoder_test.py sine-4sb-stereo.sbc sine-4sb-decoded-stereo.wav
	./sbc_decoder_test.py sine-8sb-stereo.sbc sine-8sb-decoded-stereo.wav

	./sbc_encoder_test.py sine-mono.wav 16 4 31 sine-4sb-mono.sbc
	./sbc_encoder_test.py sine-mono.wav 16 8 31 sine-8sb-mono.sbc
	./sbc_encoder_test.py sine-stereo.wav 16 4 31 sine-4sb-stereo.sbc
	./sbc_encoder_test.py sine-stereo.wav 16 8 31 sine-8sb-stereo.sbc

test-short:
	./sbc_decoder_test.py fanfare-short-4sb-mono.sbc fanfare-short-4sb-decoded-mono.wav
	./sbc_decoder_test.py fanfare-short-8sb-mono.sbc fanfare-short-8sb-decoded-mono.wav
	./sbc_decoder_test.py fanfare-short-4sb-stereo.sbc fanfare-short-4sb-decoded-stereo.wav
	./sbc_decoder_test.py fanfare-short-8sb-stereo.sbc fanfare-short-8sb-decoded-stereo.wav

	./sbc_encoder_test.py fanfare-short-mono.wav 16 4 31 fanfare-short-4sb-mono.sbc
	./sbc_encoder_test.py fanfare-short-mono.wav 16 8 31 fanfare-short-8sb-mono.sbc
	./sbc_encoder_test.py fanfare-short-stereo.wav 16 4 31 fanfare-short-4sb-stereo.sbc
	./sbc_encoder_test.py fanfare-short-stereo.wav 16 8 31 fanfare-short-8sb-stereo.sbc

test:
	./sbc_decoder_test.py fanfare-4sb-mono.sbc fanfare-4sb-decoded-mono.wav
	./sbc_decoder_test.py fanfare-8sb-mono.sbc fanfare-8sb-decoded-mono.wav
	./sbc_decoder_test.py fanfare-4sb-stereo.sbc fanfare-4sb-decoded-stereo.wav
	./sbc_decoder_test.py fanfare-8sb-stereo.sbc fanfare-8sb-decoded-stereo.wav

	./sbc_encoder_test.py fanfare-mono.wav 16 4 31 fanfare-4sb-mono.sbc
	./sbc_encoder_test.py fanfare-mono.wav 16 8 31 fanfare-8sb-mono.sbc
	./sbc_encoder_test.py fanfare-stereo.wav 16 4 31 fanfare-4sb-stereo.sbc
	./sbc_encoder_test.py fanfare-stereo.wav 16 8 31 fanfare-8sb-stereo.sbc

clean:
	rm -f *.pyc