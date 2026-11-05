# PatchRAM metadata for supported Broadcom/Cypress/Infineon chipsets.
#
# Each URL is immutable and each artifact has a mandatory SHA-256 digest.

# Broadcom BCM43430A1. The source repository's master branch is recorded as
# an immutable commit so the filename, content, and checksum are reproducible.
set(BTSTACK_BCM43430A1_PATCHRAM_FILE "BCM43430A1.hcd")
set(BTSTACK_BCM43430A1_PATCHRAM_SHA256 "591b2279e01f462a09c113e4b9ea80236c5d26d3b1b109aa189838b2bc6004ae")
set(BTSTACK_BCM43430A1_PATCHRAM_URL
    "https://raw.githubusercontent.com/OpenELEC/misc-firmware/bea4b9eb8b509c713e9155f9f041856331644c64/firmware/brcm/${BTSTACK_BCM43430A1_PATCHRAM_FILE}")

# Infineon AIROC CYW55310. A fixed commit is used because future releases may
# use a different PatchRAM filename.
set(BTSTACK_CYW55310_PATCHRAM_NAME "CYW55500A1_001.002.032.0211.0125")
set(BTSTACK_CYW55310_PATCHRAM_FILE "${BTSTACK_CYW55310_PATCHRAM_NAME}.hcd")
set(BTSTACK_CYW55310_PATCHRAM_SHA256 "81b802dfa53e0492531580e33d90eb4d6a67dc7da4278afb49c106f34ddeeb99")
set(BTSTACK_CYW55310_PATCHRAM_BASE_URL "https://github.com/Ezurio/ifx_flasher/raw/d36466a5a6dbcaee0c175af932cf88344edde8a2/files/if310/firmware")
set(BTSTACK_CYW55310_PATCHRAM_URL "${BTSTACK_CYW55310_PATCHRAM_BASE_URL}/${BTSTACK_CYW55310_PATCHRAM_FILE}")
