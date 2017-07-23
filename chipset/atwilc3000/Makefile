# 
# Makefile to convert .bin files from ATWILC3000 HCI Firmware release
#

all: wilc3000_bt_firmware.bin wilc3000_bt_firmware_no_rtc.bin
	./convert_firmware_bin.py

wilc3000_bt_firmware.bin:
	curl -O https://raw.githubusercontent.com/atwilc3000/firmware/master/wilc3000_bt_firmware.bin

wilc3000_bt_firmware_no_rtc.bin:
	curl -O https://raw.githubusercontent.com/atwilc3000/firmware/master/wilc3000_bt_firmware_no_rtc.bin
clean:
	rm -f *.h *.c

clean-firmware:
	rm -f *.bin
