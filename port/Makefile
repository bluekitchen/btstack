
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
			libusb \
			libusb-intel \
			max32630-fthr \
			mtk \
			posix-h4 \
			posix-h4-atwilc3000 \
			posix-h4-da14581 \
			posix-h4-da14585 \
			posix-h4-zephyr \
			posix-h5 \
			samv71-xplained-atwilc3000 \
			stm32-f4discovery-cc256x \
			stm32-l073rz-nucleo-em9304 \

EXCLUDED = \
			daemon \
			ez430-rf2560 \
			msp-exp430f5438-cc2564b \
			msp430f5229lp-cc2564b \
			pic32-harmony/app.X \
			stm32-f103rb-nucleo \
			posix-h5-bcm \
			ios \
			wiced-h5 \
			windows-h4 \
			windows-winusb \

subdirs:
	echo Building all ports
	for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir; \
	done
	echo Building daemon
	cd daemon ; ./bootstrap.sh ; ./configure ; make

clean:
	echo Cleaning all ports
	for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir clean; \
	done
	echo Cleaning daemon
	cd daemon ; ./clean.sh
