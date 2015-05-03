
# Makefile to build example code for all platforms but iOS (iOS requires double Xcode installation)
# Intended for Continous Integration
#
# Requires:
# - gcc/clang, pkg-config, libusb
# - arm-none-eabi
# - mspgcc
# - Android Native Development Kit
# - Microchip XC32

SUBDIRS =  \
			arduino \
			daemon \
			ez430-rf2560 \
			libusb \
			msp-exp430f5438-cc2564b \
			msp430f5229lp-cc2564b \
			mtk \
			pic32-harmony/app.X \
			posix-stlc2500d \
			posix-wl183x \
			stm32-f103rb-nucleo \

EXCLUDED = \
			ios \

subdirs:
	echo Building all platforms
	for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir; \
	done
