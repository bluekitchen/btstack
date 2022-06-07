/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "ble/le_device_db.h"
#include "ble/le_device_db_tlv.h"

#include "btstack_util.h"
#include "bluetooth.h"
#include "btstack_tlv_flash_bank.h"
#include "hal_flash_bank_memory.h"

#define HAL_FLASH_BANK_MEMORY_STORAGE_SIZE 4096
static uint8_t hal_flash_bank_memory_storage[HAL_FLASH_BANK_MEMORY_STORAGE_SIZE];
static int empty_db_index = NVM_NUM_DEVICE_DB_ENTRIES-1;


TEST_GROUP(LE_DEVICE_DB_TLV){
    const hal_flash_bank_t * hal_flash_bank_impl;
    hal_flash_bank_memory_t  hal_flash_bank_context;

    const btstack_tlv_t *      btstack_tlv_impl;
    btstack_tlv_flash_bank_t btstack_tlv_context;

    bd_addr_t addr_zero,    addr_aa,  addr_bb,   addr_cc;
    sm_key_t  sm_key_zero, sm_key_aa, sm_key_bb, sm_key_cc;

	void set_addr_and_sm_key(uint8_t value, bd_addr_t addr, sm_key_t sm_key){
	    memset(addr, value, 6);
	    memset(sm_key, value, 16);
	}

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

        set_addr_and_sm_key(0x00, addr_zero, sm_key_zero);
        set_addr_and_sm_key(0xaa, addr_aa,   sm_key_aa);
        set_addr_and_sm_key(0xbb, addr_bb,   sm_key_bb);
        set_addr_and_sm_key(0xcc, addr_cc,   sm_key_cc);
	}
};


TEST(LE_DEVICE_DB_TLV, Empty){
    CHECK_EQUAL(0, le_device_db_count());
}

TEST(LE_DEVICE_DB_TLV, AddZero){
    int index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_zero, sm_key_zero);

    CHECK_TRUE(index >= 0);
    CHECK_EQUAL(1, le_device_db_count());
}

TEST(LE_DEVICE_DB_TLV, AddOne){
    int index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_aa, sm_key_aa);
    CHECK_TRUE(index >= 0);
    CHECK_EQUAL(1, le_device_db_count());
}

TEST(LE_DEVICE_DB_TLV, RetrieveOne){
    int index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_aa, sm_key_aa);
    CHECK_TRUE(index >= 0);
    CHECK_EQUAL(1, le_device_db_count());
    bd_addr_t addr;
    sm_key_t sm_key;
    int addr_type;
    le_device_db_info((uint16_t) index, &addr_type, addr, sm_key);
    MEMCMP_EQUAL(sm_key_aa, sm_key, 16);
    MEMCMP_EQUAL(addr_aa, addr, 6);
}

TEST(LE_DEVICE_DB_TLV, AddTwo){
    le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_aa, sm_key_aa);
    le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_bb, sm_key_bb);
    CHECK_EQUAL(2, le_device_db_count());
}

TEST(LE_DEVICE_DB_TLV, AddOTwoRemoveOne){
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
    MEMCMP_EQUAL(sm_key_bb, sm_key, 16);
    MEMCMP_EQUAL(addr_bb, addr, 6);
}

TEST(LE_DEVICE_DB_TLV, AddOTwoRemoveThree){
	int index_a = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_aa, sm_key_aa);
    CHECK_TRUE(index_a >= 0);
    int index_b = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_bb, sm_key_bb);
    CHECK_TRUE(index_b >= 0);
    CHECK_EQUAL(2, le_device_db_count());

    le_device_db_remove((uint16_t) index_a);
    CHECK_EQUAL(1, le_device_db_count());
    le_device_db_remove((uint16_t) index_b);
    CHECK_EQUAL(0, le_device_db_count());
    le_device_db_remove((uint16_t) index_b);
    CHECK_EQUAL(0, le_device_db_count());
}

TEST(LE_DEVICE_DB_TLV, RemoveOneFromEmpty){
	CHECK_EQUAL(0, le_device_db_count());

    le_device_db_remove(0);
    CHECK_EQUAL(0, le_device_db_count());
}

TEST(LE_DEVICE_DB_TLV, AddTwoRemoveOneAddOne){
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
    MEMCMP_EQUAL(sm_key_cc, sm_key, 16);
    MEMCMP_EQUAL(addr_cc, addr, 6);
}

TEST(LE_DEVICE_DB_TLV, AddExisting){
	int index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_aa, sm_key_aa);
    CHECK_TRUE(index >= 0);
    CHECK_EQUAL(1, le_device_db_count());

    index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_aa, sm_key_aa);
    CHECK_TRUE(index >= 0);
    CHECK_EQUAL(1, le_device_db_count());
}

TEST(LE_DEVICE_DB_TLV, ReplaceOldest){
    bd_addr_t addr;
    sm_key_t  sm_key;
    set_addr_and_sm_key(0x10, addr, sm_key);
    int oldest_index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr, sm_key);
    CHECK_TRUE(oldest_index >= 0);
    // fill table
    int i;
    for (i=1;i<NVM_NUM_DEVICE_DB_ENTRIES;i++){
        set_addr_and_sm_key(0x10 + i, addr, sm_key);
        int index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr, sm_key);
        CHECK_TRUE(index >= 0);
    }
    uint16_t num_entries = le_device_db_count();
    // add another one that overwrites first one
    set_addr_and_sm_key(0x22 + i, addr, sm_key);
    int index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr, sm_key);
    CHECK_EQUAL(oldest_index, index);
    uint16_t num_entries_test = le_device_db_count();
    CHECK_EQUAL(num_entries, num_entries_test);
}

TEST(LE_DEVICE_DB_TLV, le_device_db_encryption_set_non_existing){
    uint16_t ediv = 16;
    int encryption_key_size = 10;
    int le_db_index = 10;

    le_device_db_encryption_set(le_db_index, ediv, NULL, NULL, encryption_key_size, 1, 1, 1);
    
    int expected_authenticated = 0;
    int expected_authorized = 0;
    int expected_secure_connection = 0;
    uint16_t expected_ediv = 0;
    uint8_t  expected_rand[8];
    sm_key_t expected_ltk;
    int      expected_encryption_key_size = 0;

    le_device_db_encryption_get(le_db_index, &expected_ediv, expected_rand, expected_ltk, &expected_encryption_key_size, 
        &expected_authenticated, &expected_authorized, &expected_secure_connection);

    CHECK_TRUE(expected_ediv != ediv);
    CHECK_TRUE(expected_authenticated != 1);
    CHECK_TRUE(expected_authorized != 1);
    CHECK_TRUE(expected_secure_connection != 1);
    CHECK_TRUE(expected_encryption_key_size != encryption_key_size);
}

TEST(LE_DEVICE_DB_TLV, le_device_db_encryption_set_zero_ltk){
    uint16_t ediv = 16;
    int encryption_key_size = 10;
    
    int le_db_index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr_zero, sm_key_zero);
    CHECK_TRUE(le_db_index >= 0);

    le_device_db_encryption_set(le_db_index, ediv, NULL, NULL, encryption_key_size, 1, 1, 1);
    
    le_device_db_encryption_get(le_db_index, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

TEST(LE_DEVICE_DB_TLV, le_device_db_encryption_set){
    bd_addr_t addr;
    sm_key_t  sm_key;
    set_addr_and_sm_key(0x10, addr, sm_key);

    uint16_t ediv = 1;
    int encryption_key_size = 10;
    uint8_t zero_rand[8];
    memset(zero_rand, 5, 8);

    int le_db_index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr, sm_key);
    CHECK_TRUE(le_db_index >= 0);
    le_device_db_encryption_set(le_db_index, ediv, zero_rand, sm_key, encryption_key_size, 1, 1, 1);

    int expected_authenticated;
    int expected_authorized;
    int expected_secure_connection;

    uint16_t expected_ediv;
    uint8_t  expected_rand[8];
    sm_key_t expected_ltk;
    int      expected_encryption_key_size;

    le_device_db_encryption_get(le_db_index, &expected_ediv, expected_rand, expected_ltk, &expected_encryption_key_size, 
        &expected_authenticated, &expected_authorized, &expected_secure_connection);

    CHECK_EQUAL(expected_ediv, ediv);
    CHECK_EQUAL(expected_authenticated, 1);
    CHECK_EQUAL(expected_authorized, 1);
    CHECK_EQUAL(expected_secure_connection, 1);
    CHECK_EQUAL(expected_encryption_key_size, encryption_key_size);
    MEMCMP_EQUAL(expected_rand, zero_rand, 8);
}

TEST(LE_DEVICE_DB_TLV, le_device_db_remote_csrk_set){
    le_device_db_remote_csrk_set(empty_db_index, NULL);

    sm_key_t csrk;
    (void)memset(csrk, 5, 16);

    le_device_db_remote_csrk_set(empty_db_index, csrk);

    bd_addr_t addr;
    sm_key_t  sm_key;
    set_addr_and_sm_key(0x10, addr, sm_key);
    int le_db_index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr, sm_key);

    le_device_db_remote_csrk_set(le_db_index, NULL);
    le_device_db_remote_csrk_set(le_db_index, csrk);
}

TEST(LE_DEVICE_DB_TLV, le_device_db_remote_csrk_get){
    le_device_db_remote_csrk_get(empty_db_index, NULL);

    bd_addr_t addr;
    sm_key_t  sm_key;
    set_addr_and_sm_key(0x10, addr, sm_key);
    int le_db_index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr, sm_key);

    le_device_db_remote_csrk_get(le_db_index, NULL);
    
    sm_key_t csrk;
    (void)memset(csrk, 5, 16);
    le_device_db_remote_csrk_set(le_db_index, csrk);
    
    sm_key_t expected_csrk;
    le_device_db_remote_csrk_get(le_db_index, expected_csrk);
    MEMCMP_EQUAL(expected_csrk, csrk, 16);
}

TEST(LE_DEVICE_DB_TLV, le_device_db_local_csrk_set){
    le_device_db_local_csrk_set(empty_db_index, NULL);

    sm_key_t csrk;
    (void)memset(csrk, 5, 16);

    le_device_db_local_csrk_set(empty_db_index, csrk);

    bd_addr_t addr;
    sm_key_t  sm_key;
    set_addr_and_sm_key(0x10, addr, sm_key);
    int le_db_index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr, sm_key);

    le_device_db_local_csrk_set(le_db_index, NULL);
    le_device_db_local_csrk_set(le_db_index, csrk);
}

TEST(LE_DEVICE_DB_TLV, le_device_db_local_csrk_get){
    le_device_db_local_csrk_get(empty_db_index, NULL);

    bd_addr_t addr;
    sm_key_t  sm_key;
    set_addr_and_sm_key(0x10, addr, sm_key);
    int le_db_index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr, sm_key);

    le_device_db_local_csrk_get(le_db_index, NULL);
    
    sm_key_t csrk;
    (void)memset(csrk, 5, 16);
    le_device_db_local_csrk_set(le_db_index, csrk);
    
    sm_key_t expected_csrk;
    le_device_db_local_csrk_get(le_db_index, expected_csrk);
    MEMCMP_EQUAL(expected_csrk, csrk, 16);
}

TEST(LE_DEVICE_DB_TLV, le_device_db_remote_counter){
    le_device_db_remote_counter_set(empty_db_index, 0);

    uint32_t expected_counter = le_device_db_remote_counter_get(empty_db_index);
    CHECK_EQUAL(expected_counter, 0);
    
    bd_addr_t addr;
    sm_key_t  sm_key;
    set_addr_and_sm_key(0x10, addr, sm_key);
    int le_db_index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr, sm_key);

    le_device_db_remote_counter_set(le_db_index, 10);


    expected_counter = le_device_db_remote_counter_get(le_db_index);
    CHECK_EQUAL(expected_counter, 10);
}

TEST(LE_DEVICE_DB_TLV, le_device_db_local_counter){
    le_device_db_local_counter_set(empty_db_index, 0);

    uint32_t expected_counter = le_device_db_local_counter_get(empty_db_index);
    CHECK_EQUAL(expected_counter, 0);

    bd_addr_t addr;
    sm_key_t  sm_key;
    set_addr_and_sm_key(0x10, addr, sm_key);
    int le_db_index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr, sm_key);

    le_device_db_local_counter_set(le_db_index, 10);

    expected_counter = le_device_db_local_counter_get(le_db_index);
    CHECK_EQUAL(expected_counter, 10);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
