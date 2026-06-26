#
# CMake file with PatchRAM metadata for Infineon AIROC CYW55310
# This uses explicit commit hashes as a new version will have a different filename which we cannot predict

set(BTSTACK_CYW55310_PATCHRAM_NAME "CYW55500A1_001.002.032.0211.0125")
set(BTSTACK_CYW55310_PATCHRAM_FILE "${BTSTACK_CYW55310_PATCHRAM_NAME}.hcd")
set(BTSTACK_CYW55310_PATCHRAM_BASE_URL "https://github.com/Ezurio/ifx_flasher/raw/d36466a5a6dbcaee0c175af932cf88344edde8a2/files/if310/firmware")
set(BTSTACK_CYW55310_PATCHRAM_URL "${BTSTACK_CYW55310_PATCHRAM_BASE_URL}/${BTSTACK_CYW55310_PATCHRAM_FILE}")
