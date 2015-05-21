# 
# Makefile to download and convert .bts files from 
# http://processors.wiki.ti.com/index.php/CC256x_Downloads
#
BASE_URL = https://git.ti.com/ti-bt/service-packs/blobs/raw/a027ae390d8790e56e1c78136c78fe6537470e91

bluetooth_init_cc2560B_1.2_BT_Spec_4.0.bts:
	curl -O $(BASE_URL)/bluetooth_init_cc2560B_1.2_BT_Spec_4.0.bts

bluetooth_init_cc2564B_1.2_BT_Spec_4.0.bts: 
	curl -O $(BASE_URL)/bluetooth_init_cc2564B_1.2_BT_Spec_4.0.bts

bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts:
	curl -O $(BASE_URL)/bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts

bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts: 
	curl -O $(BASE_URL)/bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts

BLE_init_cc2564B_1.2.bts:
	curl -O $(BASE_URL)/BLE_init_cc2564B_1.2.bts

TIInit_11.8.32.bts:
	curl -O $(BASE_URL)/TIInit_11.8.32.bts

TIInit_12.10.28.bts:
	curl -O $(BASE_URL)/TIInit_12.10.28.bts

TIInit_12.8.32.bts:
	curl -O $(BASE_URL)/TIInit_12.8.32.bts

# convert to .c files
CONVERSION_SCRIPT=$(BTSTACK_ROOT)/chipset-cc256x/convert_bts_init_scripts.py

bluetooth_init_cc2560B_1.2_BT_Spec_4.0.c: bluetooth_init_cc2560B_1.2_BT_Spec_4.0.bts
	$(CONVERSION_SCRIPT)

bluetooth_init_cc2564B_1.2_BT_Spec_4.0.c: bluetooth_init_cc2564B_1.2_BT_Spec_4.0.bts BLE_init_cc2564B_1.2.bts
	$(CONVERSION_SCRIPT)

bluetooth_init_cc2560B_1.2_BT_Spec_4.1.c: bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts
	$(CONVERSION_SCRIPT)

bluetooth_init_cc2564B_1.2_BT_Spec_4.1.c: bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts BLE_init_cc2564B_1.2.bts
	$(CONVERSION_SCRIPT)

TIInit_11.8.32.c: TIInit_11.8.32.bts
	$(CONVERSION_SCRIPT)

TIInit_12.10.28.c: TIInit_12.10.28.bts
	$(CONVERSION_SCRIPT)

TIInit_12.8.32.c: /TIInit_12.8.32.bts
	$(CONVERSION_SCRIPT)
