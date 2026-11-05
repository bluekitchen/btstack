# NXP UART firmware metadata and build-time download support.

include_guard(GLOBAL)

include(${BTSTACK_ROOT}/tool/cmake/BTstackFirmware.cmake)

# Pin the former lf-6.1.22_2.0.0 branch to an immutable commit.
set(NXP_FIRMWARE_COMMIT "f775d53ca3a478c85e8c8a880e44cc269bd14db0")
set(NXP_FIRMWARE_BASE_URL "https://raw.githubusercontent.com/nxp-imx/imx-firmware/${NXP_FIRMWARE_COMMIT}/nxp")

# 88W8997
set(NXP_8997_FILE "uartuart8997_bt_v4.bin")
set(NXP_8997_URL "${NXP_FIRMWARE_BASE_URL}/FwImage_8997/${NXP_8997_FILE}")
set(NXP_8997_SHA256 "1204e38d01fa3f7d861b5f6457a93498ea9be57c88382900f53203d28fcb6371")

# IW416
set(NXP_IW416_FILE "uartiw416_bt_v0.bin")
set(NXP_IW416_URL "${NXP_FIRMWARE_BASE_URL}/FwImage_IW416_SD/${NXP_IW416_FILE}")
set(NXP_IW416_SHA256 "1b629409f5c9b2b2d2f7ff331c3e07647082ed71e9e8701dc5214dff1cadb496")

# IW612
set(NXP_IW612_FILE "uartspi_n61x_v1.bin.se")
set(NXP_IW612_URL "${NXP_FIRMWARE_BASE_URL}/FwImage_IW612_SD/${NXP_IW612_FILE}")
set(NXP_IW612_SHA256 "9cec635eb1bf3485f3c985929a17aca39185c549476a57bc70656b7d1f2ba415")

# Declare build-time downloads for all firmware supported by a desktop NXP
# controller port. The caller receives explicit target and file lists instead
# of relying on configuration-time side effects in its build directory.
function(btstack_add_nxp_firmware_downloads)
    cmake_parse_arguments(ARG "" "OUTPUT_DIRECTORY;TARGETS_VARIABLE;FILES_VARIABLE" "" ${ARGN})

    foreach(required OUTPUT_DIRECTORY TARGETS_VARIABLE FILES_VARIABLE)
        if(NOT ARG_${required})
            message(FATAL_ERROR "btstack_add_nxp_firmware_downloads requires ${required}")
        endif()
    endforeach()

    set(firmware_targets)
    set(firmware_files)
    foreach(firmware IN ITEMS 8997 IW416 IW612)
        set(target "nxp_${firmware}_firmware")
        set(file "${NXP_${firmware}_FILE}")
        btstack_add_firmware_download(
            TARGET "${target}"
            OUTPUT "${ARG_OUTPUT_DIRECTORY}/${file}"
            URL "${NXP_${firmware}_URL}"
            SHA256 "${NXP_${firmware}_SHA256}"
        )
        list(APPEND firmware_targets "${target}")
        list(APPEND firmware_files "${ARG_OUTPUT_DIRECTORY}/${file}")
    endforeach()

    set(${ARG_TARGETS_VARIABLE} "${firmware_targets}" PARENT_SCOPE)
    set(${ARG_FILES_VARIABLE} "${firmware_files}" PARENT_SCOPE)
endfunction()
