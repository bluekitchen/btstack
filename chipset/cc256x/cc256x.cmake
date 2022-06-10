#
# CMake file to download and convert TI's CC256x Service Pack .bts files from BlueKitchen mirror
# https://e2e.ti.com/support/wireless_connectivity/bluetooth_cc256x/f/660/p/560523/2056007#2056007
#
# Original sources:
# TI Processor Wiki https://web.archive.org/web/20210114141711/https://processors.wiki.ti.com/index.php/CC256x_Downloads
# TI Git Repo:      https://git.ti.com/cgit/ti-bt/service-packs/blobs/raw/a027ae390d8790e56e1c78136c78fe6537470e91

set(BLUEKITCHEN_URL https://bluekitchen-gmbh.com/files/ti/service-packs)
set(CONVERSION_SCRIPT ${BTSTACK_ROOT}/chipset/cc256x/convert_bts_init_scripts.py)

set (BK_FILES
        CC2560_BT_SP_BTS.zip
        CC2560A_BT_SP_BTS.zip
        CC2564_BT_BLE_SP_BTS.zip
        bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts
        bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts
        BLE_init_cc2564B_1.2.bts
        cc256xb_bt_sp_v1.6.zip
        cc256xb_bt_sp_v1.8.zip
        cc256xc_bt_sp_v1.0.zip
        cc256xc_bt_sp_v1.1.zip
        cc256xc_bt_sp_v1.2.zip
        cc256xc_bt_sp_v1.3.zip
        cc256xc_bt_sp_v1.4.zip
        cc256xc_bt_sp_v1.5.zip
        TIInit_11.8.32_4.2.bts
        TIInit_11.8.32_4.6.bts
        TIInit_11.8.32_4.7.bts
        TIInit_12.10.28.bts
        TIInit_12.8.32.bts
)

# Conversion function
function(cc256x_init_script output_file main_script optional_script)
    message("CC256x: ${output_file} <- ${main_script} ${optional_script}")
    add_custom_command(
            OUTPUT ${output_file}
            DEPENDS ${main_script} ${optional_script}
            COMMAND ${CONVERSION_SCRIPT}
            ARGS  ${main_script} ${optional_script} ${output_file}
    )
endfunction()

# ZIP and bts from BlueKitchen mirror
foreach(FILE ${BK_FILES})
    message("CC256x: Download ${FILE}")
    file(DOWNLOAD ${BLUEKITCHEN_URL}/${FILE} ${CMAKE_CURRENT_BINARY_DIR}/${FILE})
endforeach()

# Other bts
file(DOWNLOAD https://git.ti.com/ti-bt/service-packs/blobs/raw/54f5c151dacc608b19ab2ce4c30e27a3983048b2/initscripts/TIInit_6.7.16_bt_spec_4.1.bts TIInit_6.7.16_bt_spec_4.1.bts)
file(DOWNLOAD https://git.ti.com/ti-bt/service-packs/blobs/raw/89c8db14929f10d75627b132690432cd71f5f54f/initscripts/TIInit_6.7.16_ble_add-on.bts  TIInit_6.7.16_ble_add-on.bts)

# Extract archives
foreach(FILE ${BK_FILES})
    # filter for .zip
    if (${FILE} MATCHES "zip$")
        message("CC256x: Extract ${FILE}")
        file(ARCHIVE_EXTRACT INPUT ${FILE})
    endif()
endforeach()

# Define all init script
#
#                   output_file                                   main_bts                                                     add_on
#
cc256x_init_script(bluetooth_init_cc2560_2.44.c                   CC2560_BT_SP_BTS/bluetooth_init_cc2560_2.44.bts              "")
cc256x_init_script(bluetooth_init_cc2560A_2.14.c                  CC2560A_BT_SP_BTS/bluetooth_init_cc2560A_2.14.bts            "")
cc256x_init_script(bluetooth_init_cc2564_2.14.c                   CC2564_BT_BLE_SP_BTS/bluetooth_init_cc2564_2.14.bts          "")
cc256x_init_script(bluetooth_init_cc2560B_1.2_BT_Spec_4.1.c       bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts                   "")
cc256x_init_script(bluetooth_init_cc2564B_1.2_BT_Spec_4.1.c       bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts                   BLE_init_cc2564B_1.2.bts)
cc256x_init_script(bluetooth_init_cc2560B_1.4_BT_Spec_4.1.c       TIInit_6.7.16_bt_spec_4.1.bts                                "")
cc256x_init_script(bluetooth_init_cc2564B_1.4_BT_Spec_4.1.c       TIInit_6.7.16_bt_spec_4.1.bts                                TIInit_6.7.16_ble_add-on.bts)
cc256x_init_script(bluetooth_init_cc2560B_1.5_BT_Spec_4.1.c       cc256x_bt_sp_v1.5/initscripts-TIInit_6.7.16_bt_spec_4.1.bts  "")
cc256x_init_script(bluetooth_init_cc2564B_1.5_BT_Spec_4.1.c       cc256x_bt_sp_v1.5/initscripts-TIInit_6.7.16_bt_spec_4.1.bts  cc256x_bt_sp_v1.5/initscripts-TIInit_6.7.16_ble_add-on.bts)
cc256x_init_script(bluetooth_init_cc2560B_1.6_BT_Spec_4.1.c       cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_bt_spec_4.1.bts "")
cc256x_init_script(bluetooth_init_cc2564B_1.6_BT_Spec_4.1.c       cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_bt_spec_4.1.bts cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_ble_add-on.bts)
cc256x_init_script(bluetooth_init_cc2560B_1.8_BT_Spec_4.1.c       cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts "")
cc256x_init_script(bluetooth_init_cc2560B_avpr_1.8_BT_Spec_4.1.c  cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_avpr_add-on.bts)
cc256x_init_script(bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c       cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_ble_add-on.bts)
cc256x_init_script(bluetooth_init_cc2560C_1.0.c                   CC256XC_BT_SP/v1.0/initscripts-TIInit_6.12.26.bts            "")
cc256x_init_script(bluetooth_init_cc2564C_1.0.c                   CC256XC_BT_SP/v1.0/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.0/initscripts-TIInit_6.12.26_ble_add-on.bts)
cc256x_init_script(bluetooth_init_cc2560C_1.1.c                   CC256XC_BT_SP/v1.1/initscripts-TIInit_6.12.26.bts            "")
cc256x_init_script(bluetooth_init_cc2564C_1.1.c                   CC256XC_BT_SP/v1.1/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.1/initscripts-TIInit_6.12.26_ble_add-on.bts)
cc256x_init_script(bluetooth_init_cc2560C_1.2.c                   CC256XC_BT_SP/v1.2/initscripts-TIInit_6.12.26.bts            "")
cc256x_init_script(bluetooth_init_cc2564C_1.2.c                   CC256XC_BT_SP/v1.2/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.2/initscripts-TIInit_6.12.26_ble_add-on.bts)
cc256x_init_script(bluetooth_init_cc2560C_1.3.c                   CC256XC_BT_SP/v1.3/initscripts-TIInit_6.12.26.bts            "")
cc256x_init_script(bluetooth_init_cc2564C_1.3.c                   CC256XC_BT_SP/v1.3/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.3/initscripts-TIInit_6.12.26_ble_add-on.bts)
cc256x_init_script(bluetooth_init_cc2560C_1.4.c                   CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26.bts             "")
cc256x_init_script(bluetooth_init_cc2560C_avpr_1.4.c              CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26_avpr_add-on.bts)
cc256x_init_script(bluetooth_init_cc2564C_1.4.c                   CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26_ble_add-on.bts)
cc256x_init_script(bluetooth_init_cc2560C_1.5.c                   CC256XC_v1.5/v1.5/initscripts-TIInit_6.12.26.bts             "")
cc256x_init_script(bluetooth_init_cc2560C_avpr_1.5.c              CC256XC_v1.5/v1.5/initscripts-TIInit_6.12.26.bts             CC256XC_v1.5/v1.5i/nitscripts-TIInit_6.12.26_avpr_add-on.bts)
cc256x_init_script(bluetooth_init_cc2564C_1.5.c                   CC256XC_v1.5/v1.5/initscripts-TIInit_6.12.26.bts             CC256XC_v1.5/v1.5/initscripts-TIInit_6.12.26_ble_add-on.bts)
cc256x_init_script(TIInit_11.8.32_4.2.c                           TIInit_11.8.32_4.2.bts                                       "")
cc256x_init_script(TIInit_11.8.32_4.6.c                           TIInit_11.8.32_4.6.bts                                       "")
cc256x_init_script(TIInit_11.8.32_4.7.c                           TIInit_11.8.32_4.7.bts                                       "")
cc256x_init_script(TIInit_12.10.28.c                              TIInit_12.10.28.bts                                          "")
cc256x_init_script(TIInit_12.8.32.c                               TIInit_12.8.32.bts                                           "")

# ANT Service Pack required
# cc256x_init_script(bluetooth_init_cc2567_2.4.c                    CC2567_BT_ANT_Service_Pack_2.4.bts                           "")
# cc256x_init_script(bluetooth_init_cc2567_2.8.c                    CC256x_BT_Service_Pack_2.8_ANT_1.16.bts                      "")
