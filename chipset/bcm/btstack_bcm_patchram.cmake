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

# Infineon AIROC CYW55513 (CYW55500A1). This is Infineon's generic UART
# firmware for the wLBGA package with the internal PA, sLNA, and antenna 0.
set(BTSTACK_CYW55513_PATCHRAM_NAME "CYW55500A1_001.002.032.0253.0000_Generic_UART_37_4MHz_wlbga_iPA_sLNA_ANT0")
set(BTSTACK_CYW55513_PATCHRAM_FILE "${BTSTACK_CYW55513_PATCHRAM_NAME}.hcd")
set(BTSTACK_CYW55513_PATCHRAM_SHA256 "02609e8ebcf4e1d84d8250e9f5e1dd1d205b89d0f2f78a16a5c3078a2214cee2")
set(BTSTACK_CYW55513_PATCHRAM_URL
    "https://raw.githubusercontent.com/Infineon/ifx-linux-bluetooth/e5f0a31b95d51bbeb7ea5ef152ab0ff563961ff8/fw/${BTSTACK_CYW55513_PATCHRAM_FILE}")

# Infineon AIROC CYW55573 (CYW55560A1). This is Infineon's generic UART
# firmware for the FCBGA package with the internal PA, sLNA, and antenna 0.
set(BTSTACK_CYW55573_PATCHRAM_NAME "CYW55560A1_001.002.087.0415.0000_Generic_UART_37_4MHz_fcbga_iPA_sLNA_ANT0")
set(BTSTACK_CYW55573_PATCHRAM_FILE "${BTSTACK_CYW55573_PATCHRAM_NAME}.hcd")
set(BTSTACK_CYW55573_PATCHRAM_SHA256 "edb4c739a84747386523612dd2a49cbbcff6f61963da81ebc5698ed03522004e")
set(BTSTACK_CYW55573_PATCHRAM_URL
    "https://raw.githubusercontent.com/Infineon/ifx-linux-bluetooth/e5f0a31b95d51bbeb7ea5ef152ab0ff563961ff8/fw/${BTSTACK_CYW55573_PATCHRAM_FILE}")
