#
# CMake file to download and convert TI's CC256x Service Pack .bts files from BlueKitchen mirror
# https://e2e.ti.com/support/wireless_connectivity/bluetooth_cc256x/f/660/p/560523/2056007#2056007
#
# Original sources:
# TI Processor Wiki https://web.archive.org/web/20210114141711/https://processors.wiki.ti.com/index.php/CC256x_Downloads
# TI Git Repo:      https://git.ti.com/cgit/ti-bt/service-packs/blobs/raw/a027ae390d8790e56e1c78136c78fe6537470e91

set(BLUEKITCHEN_URL https://bluekitchen-gmbh.com/files/ti/service-packs)
set(CONVERSION_SCRIPT ${BTSTACK_ROOT}/chipset/cc256x/convert_bts_init_scripts.py)
include(${BTSTACK_ROOT}/tool/cmake/BTstackFirmware.cmake)
set(CC256X_GENERATOR_SCRIPT ${BTSTACK_ROOT}/tool/cmake/BTstackGenerateCc256xInitScript.cmake)

# SHA-256 digests for every supported init-script download. The variable suffix
# is the upper-case CMake identifier of the downloaded filename.
set(CC256X_ARCHIVE_SHA256_CC2560_BT_SP_BTS_ZIP "031e489699c667067701fe67d761ee436d2e83893acf0b30d92d21df529c0695")
set(CC256X_ARCHIVE_SHA256_CC2560A_BT_SP_BTS_ZIP "3df6ce360da2b0148bbf56427e70f065ae411127e4ba6680c32f5c2bc7e13225")
set(CC256X_ARCHIVE_SHA256_CC2564_BT_BLE_SP_BTS_ZIP "61a407a60a82f68f650225bf9cd960d98cbde2e5ecb2aa52c1c26ef0312726b0")
set(CC256X_ARCHIVE_SHA256_CC256XB_BT_SP_V1_6_ZIP "a2063e4b597be828e009aba73b1433bb4684a29f6c0356c156fd21e69e3e741b")
set(CC256X_ARCHIVE_SHA256_CC256XB_BT_SP_V1_8_ZIP "d6c2a1dd255d34d09315bc971317653a6eb779fd7d91f7fc6bc8c3f82081151a")
set(CC256X_ARCHIVE_SHA256_CC256XC_BT_SP_V1_0_ZIP "21298e7df55905eb7107514614dbaec8ad658a30c5e6ae1570b80c68e1befa13")
set(CC256X_ARCHIVE_SHA256_CC256XC_BT_SP_V1_1_ZIP "fa8a4ba9126641f4b492685c07262cd19b74d8fc22bd2d5af23b3f6e1f570aec")
set(CC256X_ARCHIVE_SHA256_CC256XC_BT_SP_V1_2_ZIP "3e3f2ce38a1ef2b0e65d01f64f509ba0ade11a9240eb142691886fc0a4206ba7")
set(CC256X_ARCHIVE_SHA256_CC256XC_BT_SP_V1_3_ZIP "d6bc3109041d57233cd0168dc4c41c835406339f2b842e812eb50cb304befcec")
set(CC256X_ARCHIVE_SHA256_CC256XC_BT_SP_V1_4_ZIP "fb27f622d374e248f22032497a269c606c8ac10a2a5ad7d84d5c7411024ec160")
set(CC256X_ARCHIVE_SHA256_CC256XC_BT_SP_V1_5_ZIP "bb4263528b16a3ebadfca500ac5d5cc4f3e6da3990192030c75c6fbbf95d0f4d")
set(CC256X_ARCHIVE_SHA256_TIINIT_11_8_32_4_2_BTS "962322c05857ad6b1fb81467bdfc59c125e04a6a8eaabf7f24b742ddd68c3bfa")
set(CC256X_ARCHIVE_SHA256_TIINIT_11_8_32_4_6_BTS "303cba175ad4962ccdc25bff7f9448b66d237ea0b4401eab3f14751633a29f67")
set(CC256X_ARCHIVE_SHA256_TIINIT_11_8_32_4_7_BTS "3e5fd8e12f2665914b9da8d70e4cad3dcd8a9cf09eb130218405dc5c6bbbc563")
set(CC256X_ARCHIVE_SHA256_TIINIT_11_8_32_4_8_BTS "09c4495be75a4bff029c7258b3dc2f6e25691f9c23a3ebe11a1fa68082ad4400")
set(CC256X_ARCHIVE_SHA256_TIINIT_12_10_28_BTS "f72ec5bfa58522b822a7ac8744a5ab3578378437b9b0cf814a9f91783593c45e")
set(CC256X_ARCHIVE_SHA256_TIINIT_12_8_32_BTS "f6873846c09082ed1082d8f95e5cb38c9e867c49208850e0ee8729cffc903147")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
#
# Service Pack / Init Script / .bts Conversion function
#
# only download and unzip if requested by main target via CC256X_INIT_SCRIPT
#
function(cc256x_init_script output_file archive main_script optional_script)
    if (DEFINED CC256X_INIT_SCRIPT)
        if ("${CC256X_INIT_SCRIPT}" STREQUAL ${output_file})
            set(archive_sha256 "${ARGV4}")
            if("${archive_sha256}" STREQUAL "")
                string(MAKE_C_IDENTIFIER "${archive}" archive_identifier)
                string(TOUPPER "${archive_identifier}" archive_identifier)
                set(archive_sha256 "${CC256X_ARCHIVE_SHA256_${archive_identifier}}")
            endif()
            if("${archive_sha256}" STREQUAL "")
                message(FATAL_ERROR "No SHA-256 digest recorded for CC256x init script ${archive}")
            endif()

            # A checksum-pinned artifact uses only build-time actions.
            set(archive_file "${CMAKE_CURRENT_BINARY_DIR}/${archive}")
            get_filename_component(output_name "${output_file}" NAME_WE)
            string(MAKE_C_IDENTIFIER "cc256x_${output_name}" target_suffix)
            set(download_target "${target_suffix}_download")
            set(generate_target "${target_suffix}_generate")
            set(generated_source "${CMAKE_CURRENT_BINARY_DIR}/${output_file}")
            set(work_directory "${CMAKE_CURRENT_BINARY_DIR}/${target_suffix}_work")

            btstack_add_firmware_download(
                TARGET "${download_target}"
                OUTPUT "${archive_file}"
                URL "${BLUEKITCHEN_URL}/${archive}"
                SHA256 "${archive_sha256}"
            )
            add_custom_command(
                OUTPUT "${generated_source}"
                COMMAND "${CMAKE_COMMAND}"
                    "-DARCHIVE=${archive_file}"
                    "-DWORK_DIRECTORY=${work_directory}"
                    "-DMAIN_SCRIPT=${main_script}"
                    "-DOPTIONAL_SCRIPT=${optional_script}"
                    "-DOUTPUT=${generated_source}"
                    "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
                    "-DCONVERSION_SCRIPT=${CONVERSION_SCRIPT}"
                    -P "${CC256X_GENERATOR_SCRIPT}"
                DEPENDS "${archive_file}" "${CONVERSION_SCRIPT}" "${CC256X_GENERATOR_SCRIPT}"
                COMMENT "Generate ${output_file} from ${archive}"
                VERBATIM
            )
            add_custom_target("${generate_target}" DEPENDS "${generated_source}")
            add_dependencies("${generate_target}" "${download_target}")

            set(CC256X_INIT_SCRIPT_SOURCES
                ${CC256X_INIT_SCRIPT_SOURCES}
                "${generated_source}"
                PARENT_SCOPE)
            set(CC256X_INIT_SCRIPT_TARGETS
                ${CC256X_INIT_SCRIPT_TARGETS}
                "${generate_target}"
                PARENT_SCOPE)
            return()

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
# - v1.6 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xb-bt-sp
cc256x_init_script(bluetooth_init_cc2560B_1.6_BT_Spec_4.1.c      cc256xb_bt_sp_v1.6.zip          cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_bt_spec_4.1.bts "")
cc256x_init_script(bluetooth_init_cc2564B_1.6_BT_Spec_4.1.c      cc256xb_bt_sp_v1.6.zip          cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_bt_spec_4.1.bts cc256xb_bt_sp_v1.6/initscripts-TIInit_6.7.16_ble_add-on.bts)

# - v1.7 - not available, please use v1.8 or newer

# - v1.8 - unversioned files from BlueKitchen website, original: http://www.ti.com/tool/cc256xb-bt-sp
cc256x_init_script(bluetooth_init_cc2560B_1.8_BT_Spec_4.1.c      cc256xb_bt_sp_v1.8.zip          CC256XB_BT_SP/v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts "")
cc256x_init_script(bluetooth_init_cc2560B_avpr_1.8_BT_Spec_4.1.c cc256xb_bt_sp_v1.8.zip          CC256XB_BT_SP/v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts CC256XB_BT_SP/v1.8/initscripts-TIInit_6.7.16_avpr_add-on.bts)
cc256x_init_script(bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c      cc256xb_bt_sp_v1.8.zip          CC256XB_BT_SP/v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts CC256XB_BT_SP/v1.8/initscripts-TIInit_6.7.16_ble_add-on.bts)

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
cc256x_init_script(TIInit_11.8.32_4.8.c                          TIInit_11.8.32_4.8.bts          TIInit_11.8.32_4.8.bts                                       "")
cc256x_init_script(TIInit_12.10.28.c                             TIInit_12.10.28.bts             TIInit_12.10.28.bts                                          "")
cc256x_init_script(TIInit_12.8.32.c                              TIInit_12.8.32.bts              TIInit_12.8.32.bts                                           "")

# ANT Service Pack required
# cc256x_init_script(bluetooth_init_cc2567_2.4.c                 ""                              CC2567_BT_ANT_Service_Pack_2.4.bts                           "")
# cc256x_init_script(bluetooth_init_cc2567_2.8.c                 ""                              CC256x_BT_Service_Pack_2.8_ANT_1.16.bts                      "")
