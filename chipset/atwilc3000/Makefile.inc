#
# Makefile to convert .bin files from ATWILC3000 HCI Firmware release
#

CONVERSION_SCRIPT=$(BTSTACK_ROOT)/chipset/atwilc3000/convert_firmware_bin.py

wilc3000_ble_firmware.h:
	@echo "Downloading $@"
	@curl https://bluekitchen-gmbh.com/files/ble_firmware_3000.h > wilc3000_ble_firmware.h

wilc3000_bt_firmware.c: wilc3000_bt_firmware.bin
	@$(CONVERSION_SCRIPT) wilc3000_bt_firmware.bin

wilc3000_bt_firmware_no_rtc.c: wilc3000_bt_firmware_no_rtc.bin
	@$(CONVERSION_SCRIPT) wilc3000_bt_firmware.bin

wilc3000_bt_firmware.bin: 
	@echo "Downloading $@"
	@curl -O https://raw.githubusercontent.com/linux4wilc/firmware/master/wilc3000_bt_firmware.bin

wilc3000_bt_firmware_no_rtc.bin:
	@echo "Downloading $@"
	curl -O https://raw.githubusercontent.com/linux4wilc/firmware/master/wilc3000_bt_firmware_no_rtc.bin
