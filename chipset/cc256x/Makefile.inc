# 
# Makefile to download and convert TI's CC256x Service Pack .bts files from BlueKitchen mirror
# https://e2e.ti.com/support/wireless_connectivity/bluetooth_cc256x/f/660/p/560523/2056007#2056007
#
# Original sources:
# TI Processor Wiki https://web.archive.org/web/20210114141711/https://processors.wiki.ti.com/index.php/CC256x_Downloads
# TI Git Repo:      https://git.ti.com/cgit/ti-bt/service-packs/blobs/raw/a027ae390d8790e56e1c78136c78fe6537470e91

BLUEKITCHEN_URL = https://bluekitchen-gmbh.com/files/ti/service-packs
CONVERSION_SCRIPT=$(BTSTACK_ROOT)/chipset/cc256x/convert_bts_init_scripts.py

# First generation CC2560 - AKA TIInit_6.2.31.bts 

# v2.44 - file part of .zip archive

CC2560_BT_SP_BTS.zip:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/CC2560_BT_SP_BTS.zip

bluetooth_init_cc2560_2.44.bts: CC2560_BT_SP_BTS.zip
	@echo "Extracting $@"
	@unzip -q -j -o CC2560_BT_SP_BTS.zip CC2560_BT_SP_BTS/bluetooth_init_cc2560_2.44.bts

# Second generation CC2560A and CC2564 - AKA TIInit_6.6.15.bts

# v2.14 - files part of .zip archive

CC2560A_BT_SP_BTS.zip:
	@echo "Downloading $@"
	@curl -sS -O  $(BLUEKITCHEN_URL)/CC2560A_BT_SP_BTS.zip

CC2564_BT_BLE_SP_BTS.zip:
	@echo "Downloading $@"
	@curl -sS -O  $(BLUEKITCHEN_URL)/CC2564_BT_BLE_SP_BTS.zip

bluetooth_init_cc2560A_2.14.bts: CC2560A_BT_SP_BTS.zip
	@echo "Extracting $@"
	@unzip -q -j -o CC2560A_BT_SP_BTS.zip CC2560A_BT_SP_BTS/bluetooth_init_cc2560A_2.14.bts

bluetooth_init_cc2564_2.14.bts: CC2564_BT_BLE_SP_BTS.zip
	@echo "Extracting $@"
	@unzip -q -j -o CC2564_BT_BLE_SP_BTS.zip CC2564_BT_BLE_SP_BTS/bluetooth_init_cc2564_2.14.bts

# Third generation CC256xB - AKA TIInit_6.7.16.bts

# v1.2 - versioned files TI Git Repo
bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts

bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts: 
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts

BLE_init_cc2564B_1.2.bts:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/BLE_init_cc2564B_1.2.bts

# v1.4 - versioned files TI Git Repo
TIInit_6.7.16_bt_spec_4.1.bts:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/TIInit_6.7.16_bt_spec_4.1.bts

TIInit_6.7.16_ble_add-on.bts:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/TIInit_6.7.16_ble_add-on.bts

# v1.5 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xb-bt-sp
cc256x_bt_sp_v1.5.zip:
	echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/cc256x_bt_sp_v1.5.zip

cc256x_bt_sp_v1.5/initscripts-TIInit_6.7.16_bt_spec_4.1.bts: cc256x_bt_sp_v1.5.zip
	@echo "Extracting $@"
	@mkdir -p cc256x_bt_sp_v1.5
	@unzip -q -j -o cc256x_bt_sp_v1.5.zip -d cc256x_bt_sp_v1.5 CC256X_BT_SP/v1.5/initscripts-TIInit_6.7.16_bt_spec_4.1.bts

cc256x_bt_sp_v1.5/initscripts-TIInit_6.7.16_ble_add-on.bts: cc256x_bt_sp_v1.5.zip
	@echo "Extracting $@"
	@mkdir -p cc256x_bt_sp_v1.5
	@unzip -q -j -o cc256x_bt_sp_v1.5.zip -d cc256x_bt_sp_v1.5 CC256X_BT_SP/v1.5/initscripts-TIInit_6.7.16_ble_add-on.bts

# v1.6 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xb-bt-sp
cc256xb_bt_sp_v1.6.zip:
	echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/cc256xb_bt_sp_v1.6.zip

cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_bt_spec_4.1.bts: cc256xb_bt_sp_v1.6.zip
	@echo "Extracting $@"
	@mkdir -p cc256xb_bt_sp_v1.6
	@unzip -q -j -o cc256xb_bt_sp_v1.6.zip -d cc256xb_bt_sp_v1.6 CC256XB_BT_SP/v1.6/initscripts-TIInit_6.7.16_bt_spec_4.1.bts

cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_ble_add-on.bts: cc256xb_bt_sp_v1.6.zip
	@echo "Extracting $@"
	@mkdir -p cc256xb_bt_sp_v1.6
	@unzip -q -j -o cc256xb_bt_sp_v1.6.zip -d cc256xb_bt_sp_v1.6 CC256XB_BT_SP/v1.6/initscripts-TIInit_6.7.16_ble_add-on.bts

# v1.7 - not available, please use v1.8 or newer

# v1.8 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xb-bt-sp
cc256xb_bt_sp_v1.8.zip:
	echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/cc256xb_bt_sp_v1.8.zip

cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts: cc256xb_bt_sp_v1.8.zip
	@echo "Extracting $@"
	@mkdir -p cc256xb_bt_sp_v1.8
	@unzip -q -j -o cc256xb_bt_sp_v1.8.zip -d cc256xb_bt_sp_v1.8 CC256XB_BT_SP/v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts

cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_ble_add-on.bts: cc256xb_bt_sp_v1.8.zip
	@echo "Extracting $@"
	@mkdir -p cc256xb_bt_sp_v1.8
	@unzip -q -j -o cc256xb_bt_sp_v1.8.zip -d cc256xb_bt_sp_v1.8 CC256XB_BT_SP/v1.8/initscripts-TIInit_6.7.16_ble_add-on.bts

cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_avpr_add-on.bts: cc256xb_bt_sp_v1.8.zip
	@echo "Extracting $@"
	@mkdir -p cc256xb_bt_sp_v1.8
	@unzip -q -j -o cc256xb_bt_sp_v1.8.zip -d cc256xb_bt_sp_v1.8 CC256XB_BT_SP/v1.8/initscripts-TIInit_6.7.16_avpr_add-on.bts

# Fourth generation CC256xC - TIInit_6.12.26.bts

# v1.0 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256xc_bt_sp_v1.0.zip:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/cc256xc_bt_sp_v1.0.zip

initscripts-TIInit_6.12.26_v1.0.bts: cc256xc_bt_sp_v1.0.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.0.zip CC256XC_BT_SP/v1.0/initscripts-TIInit_6.12.26.bts > $@

initscripts-TIInit_6.12.26_ble_add-on_v1.0.bts: cc256xc_bt_sp_v1.0.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.0.zip CC256XC_BT_SP/v1.0/initscripts-TIInit_6.12.26_ble_add-on.bts > $@

# v1.1 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256xc_bt_sp_v1.1.zip:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/cc256xc_bt_sp_v1.1.zip

initscripts-TIInit_6.12.26_v1.1.bts: cc256xc_bt_sp_v1.1.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.1.zip CC256XC_BT_SP/v1.1/initscripts-TIInit_6.12.26.bts > $@

initscripts-TIInit_6.12.26_ble_add-on_v1.1.bts: cc256xc_bt_sp_v1.1.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.1.zip CC256XC_BT_SP/v1.1/initscripts-TIInit_6.12.26_ble_add-on.bts > $@

# v1.2 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256xc_bt_sp_v1.2.zip:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/cc256xc_bt_sp_v1.2.zip

initscripts-TIInit_6.12.26_v1.2.bts: cc256xc_bt_sp_v1.2.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.2.zip CC256XC_BT_SP/v1.2/initscripts-TIInit_6.12.26.bts > $@

initscripts-TIInit_6.12.26_ble_add-on_v1.2.bts: cc256xc_bt_sp_v1.2.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.2.zip CC256XC_BT_SP/v1.2/initscripts-TIInit_6.12.26_ble_add-on.bts > $@

# v1.3 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256xc_bt_sp_v1.3.zip:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/cc256xc_bt_sp_v1.3.zip

initscripts-TIInit_6.12.26_v1.3.bts: cc256xc_bt_sp_v1.3.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.3.zip CC256XC_BT_SP/v1.3/initscripts-TIInit_6.12.26.bts > $@

initscripts-TIInit_6.12.26_ble_add-on_v1.3.bts: cc256xc_bt_sp_v1.3.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.3.zip CC256XC_BT_SP/v1.3/initscripts-TIInit_6.12.26_ble_add-on.bts > $@

# v1.4 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256xc_bt_sp_v1.4.zip:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/cc256xc_bt_sp_v1.4.zip

initscripts-TIInit_6.12.26_v1.4.bts: cc256xc_bt_sp_v1.4.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.4.zip CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26.bts > $@

initscripts-TIInit_6.12.26_ble_add-on_v1.4.bts: cc256xc_bt_sp_v1.4.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.4.zip CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26_ble_add-on.bts > $@

initscripts-TIInit_6.12.26_avpr_add-on_v1.4.bts: cc256xc_bt_sp_v1.4.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.4.zip CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26_avpr_add-on.bts > $@

# v1.5 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256xc_bt_sp_v1.5.zip:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/cc256xc_bt_sp_v1.5.zip

initscripts-TIInit_6.12.26_v1.5.bts: cc256xc_bt_sp_v1.5.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.5.zip CC256XC_v1.5/v1.5/initscripts-TIInit_6.12.26.bts > $@

initscripts-TIInit_6.12.26_ble_add-on_v1.5.bts: cc256xc_bt_sp_v1.5.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.5.zip CC256XC_v1.5/v1.5/initscripts-TIInit_6.12.26_ble_add-on.bts > $@

initscripts-TIInit_6.12.26_avpr_add-on_v1.5.bts: cc256xc_bt_sp_v1.5.zip
	@echo "Extracting $@"
	@unzip -q -p cc256xc_bt_sp_v1.5.zip CC256XC_v1.5/v1.5/initscripts-TIInit_6.12.26_avpr_add-on.bts > $@

# Various scripts for WL chipsets from http://www.ti.com/tool/wl18xx-bt-sp

TIInit_11.8.32_4.2.bts:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/TIInit_11.8.32_4.2.bts

TIInit_11.8.32_4.6.bts:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/TIInit_11.8.32_4.6.bts

TIInit_11.8.32_4.7.bts:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/TIInit_11.8.32_4.7.bts


TIInit_12.10.28.bts:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/TIInit_12.10.28.bts

TIInit_12.8.32.bts:
	@echo "Downloading $@"
	@curl -sS -O $(BLUEKITCHEN_URL)/TIInit_12.8.32.bts



# convert to .c files based on main file and option add-on

bluetooth_init_cc2560_2.44.c: bluetooth_init_cc2560_2.44.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560A_2.14.c: bluetooth_init_cc2560A_2.14.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2567_2.4.c: CC2567_BT_ANT_Service_Pack_2.4.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2567_2.8.c: CC256x_BT_Service_Pack_2.8_ANT_1.16.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564_2.14.c: bluetooth_init_cc2564_2.14.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560B_1.2_BT_Spec_4.1.c: bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564B_1.2_BT_Spec_4.1.c: bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts BLE_init_cc2564B_1.2.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560B_1.4_BT_Spec_4.1.c: TIInit_6.7.16_bt_spec_4.1.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564B_1.4_BT_Spec_4.1.c: TIInit_6.7.16_bt_spec_4.1.bts TIInit_6.7.16_ble_add-on.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560B_1.5_BT_Spec_4.1.c: cc256x_bt_sp_v1.5/initscripts-TIInit_6.7.16_bt_spec_4.1.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564B_1.5_BT_Spec_4.1.c: cc256x_bt_sp_v1.5/initscripts-TIInit_6.7.16_bt_spec_4.1.bts cc256x_bt_sp_v1.5/initscripts-TIInit_6.7.16_ble_add-on.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560B_1.6_BT_Spec_4.1.c: cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_bt_spec_4.1.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564B_1.6_BT_Spec_4.1.c: cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_bt_spec_4.1.bts cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_ble_add-on.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560B_1.8_BT_Spec_4.1.c: cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560B_avpr_1.8_BT_Spec_4.1.c: cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_avpr_add-on.bts

	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c: cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_ble_add-on.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560C_1.0.c: initscripts-TIInit_6.12.26_v1.0.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564C_1.0.c: initscripts-TIInit_6.12.26_v1.0.bts initscripts-TIInit_6.12.26_ble_add-on_v1.0.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560C_1.1.c: initscripts-TIInit_6.12.26_v1.1.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564C_1.1.c: initscripts-TIInit_6.12.26_v1.1.bts initscripts-TIInit_6.12.26_ble_add-on_v1.1.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560C_1.2.c: initscripts-TIInit_6.12.26_v1.2.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564C_1.2.c: initscripts-TIInit_6.12.26_v1.2.bts initscripts-TIInit_6.12.26_ble_add-on_v1.2.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560C_1.3.c: initscripts-TIInit_6.12.26_v1.3.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564C_1.3.c: initscripts-TIInit_6.12.26_v1.3.bts initscripts-TIInit_6.12.26_ble_add-on_v1.3.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560C_1.4.c: initscripts-TIInit_6.12.26_v1.4.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560C_avpr_1.4.c: initscripts-TIInit_6.12.26_v1.4.bts initscripts-TIInit_6.12.26_avpr_add-on_v1.4.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564C_1.4.c: initscripts-TIInit_6.12.26_v1.4.bts initscripts-TIInit_6.12.26_ble_add-on_v1.4.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560C_1.5.c: initscripts-TIInit_6.12.26_v1.5.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2560C_avpr_1.5.c: initscripts-TIInit_6.12.26_v1.5.bts initscripts-TIInit_6.12.26_avpr_add-on_v1.5.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

bluetooth_init_cc2564C_1.5.c: initscripts-TIInit_6.12.26_v1.5.bts initscripts-TIInit_6.12.26_ble_add-on_v1.5.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@


TIInit_11.8.32_4.2.c: TIInit_11.8.32_4.2.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

TIInit_11.8.32_4.6.c: TIInit_11.8.32_4.6.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

TIInit_11.8.32_4.7.c: TIInit_11.8.32_4.7.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

TIInit_12.10.28.c: TIInit_12.10.28.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

TIInit_12.8.32.c: TIInit_12.8.32.bts
	@echo "Creating $@"
	@$(CONVERSION_SCRIPT) $^ $@

all-scripts: \
	bluetooth_init_cc2560_2.44.c \
	bluetooth_init_cc2560A_2.14.c \
	bluetooth_init_cc2564_2.14.bts \
	bluetooth_init_cc2560B_1.8_BT_Spec_4.1.c \
	bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c \
	bluetooth_init_cc2560C_1.4.c \
	bluetooth_init_cc2560C_avpr_1.4.c \
	bluetooth_init_cc2564C_1.4.c \
    bluetooth_init_cc2560C_1.5.c \
	bluetooth_init_cc2560C_avpr_1.5.c \
    bluetooth_init_cc2564C_1.5.c \
    TIInit_11.8.32_4.2.c \
	TIInit_11.8.32_4.6.c \
	TIInit_11.8.32_4.7.c \
	TIInit_12.10.28.c \
	TIInit_12.8.32.c \

clean-scripts:
	rm -fr cc256x*.zip CC256*.zip bluetooth_init_cc256*.c TIInit*.c *.bts BLE_init_cc256*.c cc256xb_bt_sp_v1.8


