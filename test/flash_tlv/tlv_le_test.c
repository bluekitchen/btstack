
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hal_flash_bank.h"
#include "hal_flash_bank_memory.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "hci_dump.h"
#include "ble/le_device_db.h"
#include "ble/le_device_db_tlv.h"
#include "btstack_util.h"
#include "btstack_config.h"
#include "btstack_debug.h"

#define HAL_FLASH_BANK_MEMORY_STORAGE_SIZE 512
static uint8_t hal_flash_bank_memory_storage[HAL_FLASH_BANK_MEMORY_STORAGE_SIZE];

static void CHECK_EQUAL_ARRAY(uint8_t * expected, uint8_t * actual, int size){
    int i;
    for (i=0; i<size; i++){
        if (expected[i] != actual[i]) {
            printf("offset %u wrong\n", i);
            printf("expected: "); printf_hexdump(expected, size);
            printf("actual:   "); printf_hexdump(actual, size);
        }
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

//

TEST_GROUP(LE_DEVICE_DB){
    const hal_flash_bank_t * hal_flash_bank_impl;
    hal_flash_bank_memory_t  hal_flash_bank_context;

    const btstack_tlv_t *      btstack_tlv_impl;
    btstack_tlv_flash_bank_t btstack_tlv_context;

    bd_addr_t addr_aa, addr_bb, addr_cc;
    sm_key_t sm_key_aa, sm_key_bb, sm_key_cc;

    void setup(void){
        // hal_flash_bank
        hal_flash_bank_impl = hal_flash_bank_memory_init_instance(&hal_flash_bank_context, hal_flash_bank_memory_storage, HAL_FLASH_BANK_MEMORY_STORAGE_SIZE);
        hal_flash_bank_impl->erase(&hal_flash_bank_context, 0);
        hal_flash_bank_impl->erase(&hal_flash_bank_context, 1);
        // btstack_tlv
        btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
        // le_device_db_tlv
        le_device_db_tlv_configure(btstack_tlv_impl, &btstack_tlv_context);
        le_device_db_init();

        bd_addr_t addr_1 = { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa };
        bd_addr_t addr_2 = { 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb };
        bd_addr_t addr_3 = { 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc };
        bd_addr_copy(addr_aa, addr_1); 
        bd_addr_copy(addr_bb, addr_2); 
        bd_addr_copy(addr_cc, addr_3); 
        memset(sm_key_aa, 0xaa, 16);
        memset(sm_key_bb, 0xbb, 16);
        memset(sm_key_cc, 0xcc, 16);
    }
};

TEST(LE_DEVICE_DB, Empty){
    CHECK_EQUAL(0, le_device_db_count());
}

TEST(LE_DEVICE_DB, AddOne){
    int index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_aa, sm_key_aa);
    CHECK_TRUE(index >= 0);
    CHECK_EQUAL(1, le_device_db_count());
}

TEST(LE_DEVICE_DB, RetrieveOne){
    int index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_aa, sm_key_aa);
    CHECK_TRUE(index >= 0);
    CHECK_EQUAL(1, le_device_db_count());
    bd_addr_t addr;
    sm_key_t sm_key;
    int addr_type;
    le_device_db_info((uint16_t) index, &addr_type, addr, sm_key);
    CHECK_EQUAL_ARRAY(sm_key_aa, sm_key, 16);
    CHECK_EQUAL_ARRAY(addr_aa, addr, 6);
}

TEST(LE_DEVICE_DB, AddTwo){
    le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_aa, sm_key_aa);
    le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_bb, sm_key_bb);
    CHECK_EQUAL(2, le_device_db_count());
}

TEST(LE_DEVICE_DB, AddOTwoRemoveOne){
    int index_a = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_aa, sm_key_aa);
    CHECK_TRUE(index_a >= 0);
    int index_b = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_bb, sm_key_bb);
    CHECK_TRUE(index_b >= 0);
    le_device_db_remove((uint16_t) index_a);
    CHECK_EQUAL(1, le_device_db_count());
    bd_addr_t addr;
    sm_key_t sm_key;
    int addr_type;
    le_device_db_info((uint16_t) index_b, &addr_type, addr, sm_key);
    CHECK_EQUAL_ARRAY(sm_key_bb, sm_key, 16);
    CHECK_EQUAL_ARRAY(addr_bb, addr, 6);
}

TEST(LE_DEVICE_DB, AddTwoRemoveOneAddOne){
    int index_a = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_aa, sm_key_aa);
    CHECK_TRUE(index_a >= 0);
    int index_b = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_bb, sm_key_bb);
    CHECK_TRUE(index_b >= 0);
    le_device_db_remove((uint16_t) index_a);
    int index_c = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_cc, sm_key_cc);
    CHECK_TRUE(index_c >= 0);
    CHECK_EQUAL(2, le_device_db_count());
    bd_addr_t addr;
    sm_key_t sm_key;
    int addr_type;
    le_device_db_info((uint16_t) index_c, &addr_type, addr, sm_key);
    CHECK_EQUAL_ARRAY(sm_key_cc, sm_key, 16);
    CHECK_EQUAL_ARRAY(addr_cc, addr, 6);
}


int main (int argc, const char * argv[]){
    hci_dump_open("tlv_le_test.pklg", HCI_DUMP_PACKETLOGGER);
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
