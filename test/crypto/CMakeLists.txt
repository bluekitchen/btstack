cmake_minimum_required (VERSION 3.5)

project(test-crypto)

include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/rijndael)
include_directories(../../src)
include_directories(..)

add_executable(aes_ccm_test
        ../../3rd-party/rijndael/rijndael.c
        ../../src/btstack_crypto.c
        ../../src/btstack_linked_list.c
        ../../src/hci_cmd.c
        ../../src/btstack_util.c
        ../../src/hci_dump.c
        aes_ccm.c
        aes_ccm_test.c
        aes_cmac.c
        mock.c
)

add_executable(aestest
        ../../3rd-party/rijndael/rijndael.c
        aestest.c
)

add_executable(ecc_micro_ecc
        ../../3rd-party/micro-ecc/uECC.c
        ecc_micro_ecc.c
)

add_executable(aes_cmac_test
        ../../3rd-party/rijndael/rijndael.c
        aes_cmac_test.c
        aes_cmac.c
)
