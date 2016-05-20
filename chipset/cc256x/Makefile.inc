# 
# Makefile to download and convert .bts files from 
# http://processors.wiki.ti.com/index.php/CC256x_Downloads
#
BASE_URL = https://git.ti.com/ti-bt/service-packs/blobs/raw/a027ae390d8790e56e1c78136c78fe6537470e91
CONVERSION_SCRIPT=$(BTSTACK_ROOT)/chipset/cc256x/convert_bts_init_scripts.py

# first generation CC2560 - TIInit_6.2.31.bts part of .zip archive

CC2560_BT_SP_BTS.zip:
	curl -O http://processors.wiki.ti.com/images/d/da/CC2560_BT_SP_BTS.zip

bluetooth_init_cc2560_2.44.bts: CC2560_BT_SP_BTS.zip
	unzip CC2560_BT_SP_BTS.zip
	mv CC2560_BT_SP_BTS/bluetooth_init_cc2560_2.44.bts .
	rm -rf CC2560_BT_SP_BTS


# second generation CC2560A and CC2564 - TIInit_6.6.15.bts part of .zip archive

CC2560A_BT_SP_BTS.zip:
	curl -O http://processors.wiki.ti.com/images/e/e7/CC2560A_BT_SP_BTS.zip

CC2564_BT_BLE_SP_BTS.zip:
	curl -O http://processors.wiki.ti.com/images/1/1e/CC2564_BT_BLE_SP_BTS.zip

bluetooth_init_cc2560A_2.14.bts: CC2560A_BT_SP_BTS.zip
	unzip CC2560A_BT_SP_BTS.zip
	mv CC2560A_BT_SP_BTS/bluetooth_init_cc2560a_2.14.bts .
	rm -rf CC2560A_BT_SP_BTS

bluetooth_init_cc2564_2.14.bts: CC2564_BT_BLE_SP_BTS.zip
	unzip CC2564_BT_BLE_SP_BTS.zip
	mv CC2564_BT_BLE_SP_BTS/bluetooth_init_cc2564_2.14.bts .
	rm -rf CC2564_BT_BLE_SP_BTS


# third generation

# versioned files for v1.2
bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts:
	curl -O $(BASE_URL)/bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts

bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts: 
	curl -O $(BASE_URL)/bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts

BLE_init_cc2564B_1.2.bts:
	curl -O $(BASE_URL)/BLE_init_cc2564B_1.2.bts

# unversioned files for v1.4
TIInit_6.7.16_bt_spec_4.1.bts:
	curl -O https://git.ti.com/ti-bt/service-packs/blobs/raw/54f5c151dacc608b19ab2ce4c30e27a3983048b2/initscripts/TIInit_6.7.16_bt_spec_4.1.bts

TIInit_6.7.16_ble_add-on.bts: 
	curl -O https://git.ti.com/ti-bt/service-packs/blobs/raw/89c8db14929f10d75627b132690432cd71f5f54f/initscripts/TIInit_6.7.16_ble_add-on.bts

# WL chipset
TIInit_11.8.32.bts:
	curl -O $(BASE_URL)/TIInit_11.8.32.bts

TIInit_12.10.28.bts:
	curl -O $(BASE_URL)/TIInit_12.10.28.bts

TIInit_12.8.32.bts:
	curl -O $(BASE_URL)/TIInit_12.8.32.bts

# convert to .c files
bluetooth_init_cc2560_2.44.c: bluetooth_init_cc2560_2.44.bts
	$(CONVERSION_SCRIPT)


bluetooth_init_cc2560A_2.14.c: bluetooth_init_cc2560A_2.14.bts
	$(CONVERSION_SCRIPT)

bluetooth_init_cc2564_2.14.c: bluetooth_init_cc2564_2.14.bts
	$(CONVERSION_SCRIPT)


bluetooth_init_cc2560B_1.2_BT_Spec_4.0.c: bluetooth_init_cc2560B_1.2_BT_Spec_4.0.bts
	$(CONVERSION_SCRIPT)

bluetooth_init_cc2564B_1.2_BT_Spec_4.0.c: bluetooth_init_cc2564B_1.2_BT_Spec_4.0.bts BLE_init_cc2564B_1.2.bts
	$(CONVERSION_SCRIPT)

bluetooth_init_cc2560B_1.2_BT_Spec_4.1.c: bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts
	$(CONVERSION_SCRIPT)

bluetooth_init_cc2564B_1.2_BT_Spec_4.1.c: bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts BLE_init_cc2564B_1.2.bts
	$(CONVERSION_SCRIPT)

bluetooth_init_cc2560B_1.4_BT_Spec_4.1.c: TIInit_6.7.16_bt_spec_4.1.bts
	cp TIInit_6.7.16_bt_spec_4.1.bts bluetooth_init_cc2560B_1.4_BT_Spec_4.1.bts
	$(CONVERSION_SCRIPT)

bluetooth_init_cc2564B_1.4_BT_Spec_4.1.c: TIInit_6.7.16_bt_spec_4.1.bts TIInit_6.7.16_ble_add-on.bts
	cp TIInit_6.7.16_bt_spec_4.1.bts bluetooth_init_cc2564B_1.4_BT_Spec_4.1.bts
	cp TIInit_6.7.16_ble_add-on.bts  BLE_init_cc2564B_1.4.bts
	$(CONVERSION_SCRIPT)

TIInit_11.8.32.c: TIInit_11.8.32.bts
	$(CONVERSION_SCRIPT)

TIInit_12.10.28.c: TIInit_12.10.28.bts
	$(CONVERSION_SCRIPT)

TIInit_12.8.32.c: TIInit_12.8.32.bts
	$(CONVERSION_SCRIPT)

all-scripts: bluetooth_init_cc2560_2.44.c bluetooth_init_cc2560A_2.14.c bluetooth_init_cc2564_2.14.bts bluetooth_init_cc2560B_1.2_BT_Spec_4.0.c bluetooth_init_cc2564B_1.2_BT_Spec_4.0.c bluetooth_init_cc2560B_1.2_BT_Spec_4.1.c bluetooth_init_cc2564B_1.2_BT_Spec_4.1.c TIInit_11.8.32.c TIInit_12.10.28.c TIInit_12.8.32.c

clean-scripts:
	rm -fr CC256*.zip bluetooth_init_cc256*.bts bluetooth_init_cc256*.c TIInit_*.bts TIInit_*.c BLE_init_cc256*.bts BLE_init_cc256*.c 


