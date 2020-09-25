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

#include "bluetooth_data_types.h"
#include "btstack_util.h"
#include "ad_parser.h"

static const uint8_t ad_data[] =    { 0x02, 0x01, 0x05, /* -- */ 0x03, 0x02, 0xF0, 0xFF};
static const uint8_t adv_data_2[] = { 8, 0x09, 'B', 'T', 's', 't', 'a', 'c', 'k' }; 
static const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, 
    // Name
    0x0c, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'L', 'E', ' ', 'S', 't', 'r', 'e', 'a', 'm', 'e', 'r', 
    // Incomplete List of 16-bit Service Class UUIDs -- FF10 - only valid for testing!
    0x03, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x10, 0xff,

    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS, 0x9e, 0xca, 0xdc, 0x24, 0xe, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x1, 0x0, 0x40, 0x6e,

    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS, 0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, /* - */ 0x00, 0x80, /* - */ 0x00, 0x10, /* - */ 0x00, 0x00, /* - */  0x10, 0xaa, 0x00, 0x00
};


bool nameHasPrefix(const char * name_prefix, uint16_t data_length, const uint8_t * data){
    ad_context_t context;
    int name_prefix_len = strlen(name_prefix);
    for (ad_iterator_init(&context, data_length, data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len  = ad_iterator_get_data_len(&context);
        const uint8_t * data    = ad_iterator_get_data(&context);
        int compare_len = name_prefix_len;
        switch(data_type){
            case 8: // shortented local name
            case 9: // complete local name
                if (compare_len > data_len) compare_len = data_len;
                if (strncmp(name_prefix, (const char*) data, compare_len) == 0) return true;
                break;
            default:
                break;
        }
    }
    return false;
}

TEST_GROUP(ADParser){
    void setup(void){
    }
};

TEST(ADParser, TestNamePrefix){
    CHECK(nameHasPrefix("BTstack", sizeof(adv_data_2), adv_data_2));
}

TEST(ADParser, TestDataParsing){
    ad_context_t context;
    uint8_t  expected_len[] = {1, 2};
    uint8_t  expected_type[] = {0x01, 0x02};
    uint8_t  expected_data[][2] = {{0x05, 0x00}, {0xF0, 0xFF}};

    int i = 0;
    uint8_t  ad_len = sizeof(ad_data);

    for (ad_iterator_init(&context, ad_len, ad_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len  = ad_iterator_get_data_len(&context);
        const uint8_t * data    = ad_iterator_get_data(&context);
        
        CHECK_EQUAL(expected_len[i],  data_len);
        CHECK_EQUAL(expected_type[i], data_type);

        int j;
        for (j = 0; j < data_len; j++){
            CHECK_EQUAL(expected_data[i][j], data[j]);
        }
        i++; 
    }
}

TEST(ADParser, TestMalformed){
    ad_context_t context;

    // len = 0xff, but only one byte type
    uint8_t data[] = { 0xff, 0x01 };
    ad_iterator_init(&context, sizeof(data), data);
    CHECK_EQUAL(ad_iterator_has_more(&context), 0);

    // len = 0x01, but not type
    uint8_t data2[] = { 0x00, 0x01 };
    ad_iterator_init(&context, sizeof(data2), data2);
    CHECK_EQUAL(ad_iterator_has_more(&context), 0);
}

TEST(ADParser, ad_data_contains_uuid16){
    bool contains_uuid;

    contains_uuid = ad_data_contains_uuid16(sizeof(adv_data_2), adv_data_2, 0x00);
    CHECK(!contains_uuid);

    contains_uuid = ad_data_contains_uuid16(sizeof(adv_data), adv_data, 0x00);
    CHECK(!contains_uuid);

    contains_uuid = ad_data_contains_uuid16(sizeof(adv_data), adv_data,  0xff10);
    CHECK(contains_uuid);

    contains_uuid = ad_data_contains_uuid16(sizeof(adv_data), adv_data,  0xaa10);
    CHECK(contains_uuid);
}

TEST(ADParser, ad_data_contains_uuid128){
    bool contains_uuid;
    {
        uint8_t ad_uuid128[16];
        memset(ad_uuid128, 0, 16);
        contains_uuid = ad_data_contains_uuid128(sizeof(adv_data_2), adv_data_2, ad_uuid128);
        CHECK(!contains_uuid);

        contains_uuid = ad_data_contains_uuid128(sizeof(adv_data), adv_data, ad_uuid128);
        CHECK(!contains_uuid);
    }


    {
        uint8_t uuid128_le[] = {0x9e, 0xca, 0xdc, 0x24, 0xe, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x1, 0x0, 0x40, 0x6e};
        uint8_t ad_uuid128[16];
        reverse_128(uuid128_le, ad_uuid128);

        contains_uuid = ad_data_contains_uuid128(sizeof(adv_data), adv_data, ad_uuid128);
        CHECK(contains_uuid);
    }

    {
        uint8_t uuid128_le[] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x10, 0xaa, 0x00, 0x00};
        uint8_t ad_uuid128[16];
        reverse_128(uuid128_le, ad_uuid128);

        contains_uuid = ad_data_contains_uuid128(sizeof(adv_data), adv_data, ad_uuid128);
        CHECK(contains_uuid);
    }
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
