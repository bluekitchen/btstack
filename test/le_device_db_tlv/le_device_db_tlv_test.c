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


TEST_GROUP(LE_DEVICE_DB_TLV){
    const hal_flash_bank_t * hal_flash_bank_impl;
    hal_flash_bank_memory_t  hal_flash_bank_context;

    const btstack_tlv_t *      btstack_tlv_impl;
    btstack_tlv_flash_bank_t btstack_tlv_context;

    bd_addr_t addr_aa, addr_bb, addr_cc;
    sm_key_t sm_key_aa, sm_key_bb, sm_key_cc;

	bd_addr_t addr;
	sm_key_t sm_key;

	void init_addr_and_key(uint8_t a){
		uint8_t value = 0;
		if (a <= 0x0F){
			value = 0x11 * a;
		} 
		memset(sm_key, value, 16);
		memset(addr,   value,  6);
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

        memset(addr_aa, 0xaa, 6);
        memset(addr_bb, 0xbb, 6);
        memset(addr_cc, 0xcc, 6);

        memset(sm_key_aa, 0xaa, 16);
        memset(sm_key_bb, 0xbb, 16);
        memset(sm_key_cc, 0xcc, 16);
	}
};


TEST(LE_DEVICE_DB_TLV, Empty){
    CHECK_EQUAL(0, le_device_db_count());
}

TEST(LE_DEVICE_DB_TLV, AddZero){
    int index = le_device_db_add(BD_ADDR_TYPE_LE_PUBLIC, addr, sm_key);

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


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
