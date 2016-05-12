test:
	./sbc_decoder_test.py fanfare-4sb.sbc fanfare-4sb-decoded.wav
	./sbc_decoder_test.py fanfare-8sb.sbc fanfare-8sb-decoded.wav
	./sbc_decoder_test.py fanfare-stereo-4sb.sbc fanfare-stereo-4sb-decoded.wav
	./sbc_decoder_test.py fanfare-stereo-8sb.sbc fanfare-stereo-8sb-decoded.wav

	./sbc_encoder_test.py fanfare-mono.wav 16 4 31 fanfare-4sb.sbc
	./sbc_encoder_test.py fanfare-mono.wav 16 8 31 fanfare-8sb.sbc
	./sbc_encoder_test.py fanfare-stereo.wav 16 4 31 fanfare-stereo-4sb.sbc
	./sbc_encoder_test.py fanfare-stereo.wav 16 8 31 fanfare-stereo-8sb.sbc

clean:
	rm -f *.pyc