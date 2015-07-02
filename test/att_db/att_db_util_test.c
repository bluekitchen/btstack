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
 
// *****************************************************************************
//
// test rfcomm query tests
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "att.h"
#include "att_db_util.h"
#include "le_counter.h"
#include <btstack/utils.h>

#if 0
PRIMARY_SERVICE, GAP_SERVICE
CHARACTERISTIC, GAP_DEVICE_NAME, READ, "SPP+LE Counter"

PRIMARY_SERVICE, GATT_SERVICE
CHARACTERISTIC, GATT_SERVICE_CHANGED, READ,

// Counter Service
PRIMARY_SERVICE, 0000FF10-0000-1000-8000-00805F9B34FB
// Counter Characteristic, with read and notify
CHARACTERISTIC,  0000FF11-0000-1000-8000-00805F9B34FB, READ | NOTIFY | DYNAMIC,
#endif

// 0000FF10-0000-1000-8000-00805F9B34FB
uint8_t counter_service_uuid[] = { 0x00, 0x00, 0xFF, 0x10, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
// 0000FF11-0000-1000-8000-00805F9B34FB
uint8_t counter_characteristic_uuid[] = { 0x00, 0x00, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};

void CHECK_EQUAL_ARRAY(const uint8_t * expected, uint8_t * actual, int size){
    for (int i=0; i<size; i++){
        // printf("%03u: %02x - %02x\n", i, expected[i], actual[i]);
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

TEST_GROUP(AttDbUtil){
    void setup(void){
        att_db_util_init();
    }
};


TEST(AttDbUtil, LeCounterDb){
    att_db_util_add_service_uuid16(GAP_SERVICE_UUID);
    att_db_util_add_characteristic_uuid16(GAP_DEVICE_NAME_UUID, ATT_PROPERTY_READ, (uint8_t*)"SPP+LE Counter", 14);

    att_db_util_add_service_uuid16(0x1801);
    att_db_util_add_characteristic_uuid16(0x2a05, ATT_PROPERTY_READ, NULL, 0);

    att_db_util_add_service_uuid128(counter_service_uuid);
    att_db_util_add_characteristic_uuid128(counter_characteristic_uuid, ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY | ATT_PROPERTY_DYNAMIC, NULL, 0);

    uint8_t * addr = att_db_util_get_address();
    uint16_t  size = att_db_util_get_size();

    // hexdumpf(addr, size);    
    CHECK_EQUAL(size, (uint16_t)sizeof(profile_data));
    CHECK_EQUAL_ARRAY(profile_data, addr, size);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}