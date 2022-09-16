#
# CMake file to download and convert TI's CC256x Service Pack .bts files from BlueKitchen mirror
# https://e2e.ti.com/support/wireless_connectivity/bluetooth_cc256x/f/660/p/560523/2056007#2056007
#
# Original sources:
# TI Processor Wiki https://web.archive.org/web/20210114141711/https://processors.wiki.ti.com/index.php/CC256x_Downloads
# TI Git Repo:      https://git.ti.com/cgit/ti-bt/service-packs/blobs/raw/a027ae390d8790e56e1c78136c78fe6537470e91

set(BLUEKITCHEN_URL https://bluekitchen-gmbh.com/files/ti/service-packs)
set(CONVERSION_SCRIPT ${BTSTACK_ROOT}/chipset/cc256x/convert_bts_init_scripts.py)

#
# Service Pack / Init Script / .bts Conversion function
#
# only download and unzip if requested by main target via CC256X_INIT_SCRIPT
#
function(cc256x_init_script output_file archive main_script optional_script)
    if (DEFINED CC256X_INIT_SCRIPT)
        if ("${CC256X_INIT_SCRIPT}" STREQUAL ${output_file})

            # Download init scripts
            message("CC256x: Download ${archive}")
            file(DOWNLOAD ${BLUEKITCHEN_URL}/${archive} ${CMAKE_CURRENT_BINARY_DIR}/${archive})

            # unpack zip
            if (${archive} MATCHES "zip$")
                message("CC256x: Extract ${archive}")
                file(ARCHIVE_EXTRACT INPUT ${CMAKE_CURRENT_BINARY_DIR}/${archive})
            endif()

            # add custom command to convert bts file(s) into C array
            message("CC256x: ${output_file} <- ${main_script} ${optional_script}")
            add_custom_command(
                    OUTPUT ${output_file}
                    DEPENDS ${main_script} ${optional_script}
                    COMMAND python
                    ARGS ${CONVERSION_SCRIPT} ${main_script} ${optional_script} ${output_file}
            )

            # Add CC256x chipset support
            include_directories(${BTSTACK_ROOT}/chipset/cc256x)
            target_sources(btstack PRIVATE ${BTSTACK_ROOT}/chipset/cc256x/btstack_chipset_cc256x.c)

            # add init script to target
            target_sources(btstack PRIVATE ${CC256X_INIT_SCRIPT})

        endif()
    endif()
endfunction()

#
#                   output_file                                  archives                        main_bts                                                     add_on
#

# First generation CC2560 - AKA TIInit_6.2.31.bts 
# - v2.44 - file part of .zip archive
cc256x_init_script(bluetooth_init_cc2560_2.44.c                  CC2560_BT_SP_BTS.zip            CC2560_BT_SP_BTS/bluetooth_init_cc2560_2.44.bts              "")

# Second generation CC2560A and CC2564 - AKA TIInit_6.6.15.bts
# - v2.14 - files part of .zip archive
cc256x_init_script(bluetooth_init_cc2560A_2.14.c                 CC2560A_BT_SP_BTS.zip           CC2560A_BT_SP_BTS/bluetooth_init_cc2560A_2.14.bts            "")
cc256x_init_script(bluetooth_init_cc2564_2.14.c                  CC2564_BT_BLE_SP_BTS.zip        CC2564_BT_BLE_SP_BTS/bluetooth_init_cc2564_2.14.bts          "")

# Third generation CC256xB - AKA TIInit_6.7.16.bts
# - v1.2 - versioned files TI Git Repo
cc256x_init_script(bluetooth_init_cc2560B_1.2_BT_Spec_4.1.c      cc256xb_bt_sp_v1.2.zip          bluetooth_init_cc2560B_1.2_BT_Spec_4.1.bts                   "")
cc256x_init_script(bluetooth_init_cc2564B_1.2_BT_Spec_4.1.c      cc256xb_bt_sp_v1.2.zip          bluetooth_init_cc2564B_1.2_BT_Spec_4.1.bts                   BLE_init_cc2564B_1.2.bts)

# - v1.3 - not available, please use v1.8 or newer

# - v1.4 - versioned files TI Git Repo
cc256x_init_script(bluetooth_init_cc2560B_1.4_BT_Spec_4.1.c      cc256xb_bt_sp_v1.4.zip          TIInit_6.7.16_bt_spec_4.1.bts                                "")
cc256x_init_script(bluetooth_init_cc2564B_1.4_BT_Spec_4.1.c      cc256xb_bt_sp_v1.4.zip          TIInit_6.7.16_bt_spec_4.1.bts                                TIInit_6.7.16_ble_add-on.bts)

# - v1.5 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xb-bt-sp
cc256x_init_script(bluetooth_init_cc2560B_1.5_BT_Spec_4.1.c      cc256xb_bt_sp_v1.5.zip          cc256x_bt_sp_v1.5/initscripts-TIInit_6.7.16_bt_spec_4.1.bts  "")
cc256x_init_script(bluetooth_init_cc2564B_1.5_BT_Spec_4.1.c      cc256xb_bt_sp_v1.5.zip          cc256x_bt_sp_v1.5/initscripts-TIInit_6.7.16_bt_spec_4.1.bts  cc256x_bt_sp_v1.5/initscripts-TIInit_6.7.16_ble_add-on.bts)

# - v1.6 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xb-bt-sp
cc256x_init_script(bluetooth_init_cc2560B_1.6_BT_Spec_4.1.c      cc256xb_bt_sp_v1.6.zip          cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_bt_spec_4.1.bts "")
cc256x_init_script(bluetooth_init_cc2564B_1.6_BT_Spec_4.1.c      cc256xb_bt_sp_v1.6.zip          cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_bt_spec_4.1.bts cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_ble_add-on.bts)
cc256x_init_script(bluetooth_init_cc2560B_1.8_BT_Spec_4.1.c      cc256xb_bt_sp_v1.6.zip          cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts "")

# - v1.7 - not available, please use v1.8 or newer

# - v1.8 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xb-bt-sp
cc256x_init_script(bluetooth_init_cc2560B_avpr_1.8_BT_Spec_4.1.c cc256xb_bt_sp_v1.8.zip          cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_avpr_add-on.bts)
cc256x_init_script(bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c      cc256xb_bt_sp_v1.8.zip          cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_ble_add-on.bts)

# Fourth generation CC256xC - TIInit_6.12.26.bts

# - v1.0 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256x_init_script(bluetooth_init_cc2560C_1.0.c                  cc256xc_bt_sp_v1.0.zip          CC256XC_BT_SP/v1.0/initscripts-TIInit_6.12.26.bts            "")
cc256x_init_script(bluetooth_init_cc2564C_1.0.c                  cc256xc_bt_sp_v1.0.zip          CC256XC_BT_SP/v1.0/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.0/initscripts-TIInit_6.12.26_ble_add-on.bts)

# - v1.1 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256x_init_script(bluetooth_init_cc2560C_1.1.c                  cc256xc_bt_sp_v1.1.zip          CC256XC_BT_SP/v1.1/initscripts-TIInit_6.12.26.bts            "")
cc256x_init_script(bluetooth_init_cc2564C_1.1.c                  cc256xc_bt_sp_v1.1.zip          CC256XC_BT_SP/v1.1/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.1/initscripts-TIInit_6.12.26_ble_add-on.bts)

# - v1.2 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256x_init_script(bluetooth_init_cc2560C_1.2.c                  cc256xc_bt_sp_v1.2.zip          CC256XC_BT_SP/v1.2/initscripts-TIInit_6.12.26.bts            "")
cc256x_init_script(bluetooth_init_cc2564C_1.2.c                  cc256xc_bt_sp_v1.2.zip          CC256XC_BT_SP/v1.2/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.2/initscripts-TIInit_6.12.26_ble_add-on.bts)

# - v1.3 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256x_init_script(bluetooth_init_cc2560C_1.3.c                  cc256xc_bt_sp_v1.3.zip          CC256XC_BT_SP/v1.3/initscripts-TIInit_6.12.26.bts            "")
cc256x_init_script(bluetooth_init_cc2564C_1.3.c                  cc256xc_bt_sp_v1.3.zip          CC256XC_BT_SP/v1.3/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.3/initscripts-TIInit_6.12.26_ble_add-on.bts)

# - v1.4 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256x_init_script(bluetooth_init_cc2560C_1.4.c                  cc256xc_bt_sp_v1.4.zip          CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26.bts             "")
cc256x_init_script(bluetooth_init_cc2560C_avpr_1.4.c             cc256xc_bt_sp_v1.4.zip          CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26_avpr_add-on.bts)
cc256x_init_script(bluetooth_init_cc2564C_1.4.c                  cc256xc_bt_sp_v1.4.zip          CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26.bts            CC256XC_BT_SP/v1.4/initscripts-TIInit_6.12.26_ble_add-on.bts)

# - v1.5 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xc-bt-sp
cc256x_init_script(bluetooth_init_cc2560C_1.5.c                  cc256xc_bt_sp_v1.5.zip          CC256XC_v1.5/v1.5/initscripts-TIInit_6.12.26.bts             "")
cc256x_init_script(bluetooth_init_cc2560C_avpr_1.5.c             cc256xc_bt_sp_v1.5.zip          CC256XC_v1.5/v1.5/initscripts-TIInit_6.12.26.bts             CC256XC_v1.5/v1.5i/nitscripts-TIInit_6.12.26_avpr_add-on.bts)
cc256x_init_script(bluetooth_init_cc2564C_1.5.c                  cc256xc_bt_sp_v1.5.zip          CC256XC_v1.5/v1.5/initscripts-TIInit_6.12.26.bts             CC256XC_v1.5/v1.5/initscripts-TIInit_6.12.26_ble_add-on.bts)

# Various scripts for WL chipsets from http://www.ti.com/tool/wl18xx-bt-sp
cc256x_init_script(TIInit_11.8.32_4.2.c                          TIInit_11.8.32_4.2.bts          TIInit_11.8.32_4.2.bts                                       "")
cc256x_init_script(TIInit_11.8.32_4.6.c                          TIInit_11.8.32_4.6.bts          TIInit_11.8.32_4.6.bts                                       "")
cc256x_init_script(TIInit_11.8.32_4.7.c                          TIInit_11.8.32_4.7.bts          TIInit_11.8.32_4.7.bts                                       "")
cc256x_init_script(TIInit_12.10.28.c                             TIInit_12.10.28.bts             TIInit_12.10.28.bts                                          "")
cc256x_init_script(TIInit_12.8.32.c                              TIInit_12.8.32.bts              TIInit_12.8.32.bts                                           "")

# ANT Service Pack required
# cc256x_init_script(bluetooth_init_cc2567_2.4.c                 ""                              CC2567_BT_ANT_Service_Pack_2.4.bts                           "")
# cc256x_init_script(bluetooth_init_cc2567_2.8.c                 ""                              CC256x_BT_Service_Pack_2.8_ANT_1.16.bts                      "")
