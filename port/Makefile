
# Makefile to build example code for all platforms but 
# - iOS (iOS requires double Xcode installation)
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
			deamon \
			ez430-rf2560 \
			libusb \
			msp-exp430f5438-cc2564b \
			msp430f5229lp-cc2564b \
			mtk \
			posix-h4 \
			posix-h5 \
			stm32-f103rb-nucleo \
			max32630-fthr \

EXCLUDED = \
			pic32-harmony/app.X \
			ios \
			wiced \
			windows-h4 \
			windows-winusb \

subdirs:
	echo Building all ports
	for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir; \
	done
	echo Building daemon
	cd daemon ; ./bootstrap.sh ; make

clean:
	echo Cleaning all ports
	for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir clean; \
	done
	echo Cleaning daemon
	cd daemon ; make clean
