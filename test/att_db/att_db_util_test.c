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

#include "hci.h"
#include "ble/att_db.h"
#include "ble/att_db_util.h"
#include "btstack_util.h"
#include "bluetooth.h"

#include "btstack_crypto.h"

#include "att_db_util_test.h"

static btstack_crypto_aes128_cmac_t cmac_context;
static uint8_t cmac_calculated[16];

// 0000FF10-0000-1000-8000-00805F9B34FB
uint8_t counter_service_uuid[] = { 0x00, 0x00, 0xFF, 0x10, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
// 0000FF11-0000-1000-8000-00805F9B34FB
uint8_t counter_characteristic_uuid[] = { 0x00, 0x00, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};

// gatt database hash test message
static const uint8_t gatt_database_hash_test_message[] = {
    0x01, 0x00, 0x00, 0x28, 0x00, 0x18, 0x02, 0x00, 0x03, 0x28, 0x0A, 0x03, 0x00, 0x00, 0x2A, 0x04,
    0x00, 0x03, 0x28, 0x02, 0x05, 0x00, 0x01, 0x2A, 0x06, 0x00, 0x00, 0x28, 0x01, 0x18, 0x07, 0x00,
    0x03, 0x28, 0x20, 0x08, 0x00, 0x05, 0x2A, 0x09, 0x00, 0x02, 0x29, 0x0A, 0x00, 0x03, 0x28, 0x0A,
    0x0B, 0x00, 0x29, 0x2B, 0x0C, 0x00, 0x03, 0x28, 0x02, 0x0D, 0x00, 0x2A, 0x2B, 0x0E, 0x00, 0x00,
    0x28, 0x08, 0x18, 0x0F, 0x00, 0x02, 0x28, 0x14, 0x00, 0x16, 0x00, 0x0F, 0x18, 0x10, 0x00, 0x03,
    0x28, 0xA2, 0x11, 0x00, 0x18, 0x2A, 0x12, 0x00, 0x02, 0x29, 0x13, 0x00, 0x00, 0x29, 0x00, 0x00,
    0x14, 0x00, 0x01, 0x28, 0x0F, 0x18, 0x15, 0x00, 0x03, 0x28, 0x02, 0x16, 0x00, 0x19, 0x2A
};

static const uint8_t gatt_database_hash_expected[] = {
    0xF1, 0xCA, 0x2D, 0x48, 0xEC, 0xF5, 0x8B, 0xAC, 0x8A, 0x88, 0x30, 0xBB, 0xB9, 0xFB, 0xA9, 0x90
};

static void gatt_hash_calculated(void * arg){
    UNUSED(arg);
}

void CHECK_EQUAL_ARRAY(const uint8_t * expected, uint8_t * actual, int size){
    for (int i=0; i<size; i++){
        // printf("%03u: %02x - %02x\n", i, expected[i], actual[i]);
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

// mock
extern "C" {

    void att_set_db(uint8_t const * db){
    }
    void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    }
    int hci_can_send_command_packet_now(void){
        return 1;
    }
    HCI_STATE hci_get_state(void){
        return HCI_STATE_WORKING;
    }
    void hci_halting_defer(void){
    }
    int hci_send_cmd(const hci_cmd_t *cmd, ...){
        printf("hci_send_cmd opcode %04x\n", cmd->opcode);
        return 0;
    }
}

TEST_GROUP(AttDbUtil){
    void setup(void){
        att_db_util_init();
    }
};

TEST(AttDbUtil, LeCounterDb){
    att_db_util_add_service_uuid16(GAP_SERVICE_UUID);
    att_db_util_add_characteristic_uuid16(GAP_DEVICE_NAME_UUID, ATT_PROPERTY_READ, ATT_SECURITY_NONE, ATT_SECURITY_NONE, (uint8_t*)"SPP+LE Counter", 14);

    att_db_util_add_service_uuid16(0x1801);
    att_db_util_add_characteristic_uuid16(GAP_SERVICE_CHANGED, ATT_PROPERTY_READ, ATT_SECURITY_NONE, ATT_SECURITY_NONE, NULL, 0);

    att_db_util_add_service_uuid128(counter_service_uuid);
    att_db_util_add_characteristic_uuid128(counter_characteristic_uuid, ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY | ATT_PROPERTY_DYNAMIC, ATT_SECURITY_NONE, ATT_SECURITY_NONE, NULL, 0);

    uint8_t * addr = att_db_util_get_address();
    uint16_t  size = att_db_util_get_size();

    printf("LE Counter DB\n");
    printf_hexdump(addr, size);

    CHECK_EQUAL((uint16_t)sizeof(profile_data), size);
    CHECK_EQUAL_ARRAY(profile_data, addr, size);
}

TEST(AttDbUtil, GattHash){
    const uint8_t appearance[] = {0};
    const uint8_t service_changed[] = {0} ;
    const uint8_t supported_features[] = {0} ;
    const uint8_t extended_properties[] = {0,0} ;
    const uint8_t battery_level[] = { 100 } ;
    att_db_util_add_service_uuid16(GAP_SERVICE_UUID);
    att_db_util_add_characteristic_uuid16(GAP_DEVICE_NAME_UUID, ATT_PROPERTY_READ | ATT_PROPERTY_WRITE, ATT_SECURITY_NONE, ATT_SECURITY_NONE, (uint8_t*)"HASH", 4);
    att_db_util_add_characteristic_uuid16(GAP_APPEARANCE_UUID, ATT_PROPERTY_READ, ATT_SECURITY_NONE, ATT_SECURITY_NONE, (uint8_t*)appearance, sizeof(appearance));
    att_db_util_add_service_uuid16(0x1801);
    att_db_util_add_characteristic_uuid16(GAP_SERVICE_CHANGED, ATT_PROPERTY_INDICATE, ATT_SECURITY_NONE, ATT_SECURITY_NONE, (uint8_t*)service_changed, sizeof(service_changed));
    att_db_util_add_characteristic_uuid16(0x2b29, ATT_PROPERTY_READ | ATT_PROPERTY_WRITE, ATT_SECURITY_NONE, ATT_SECURITY_NONE, (uint8_t*)supported_features, sizeof(supported_features));
    att_db_util_add_characteristic_uuid16(0x2b2a, ATT_PROPERTY_READ | ATT_PROPERTY_DYNAMIC, ATT_SECURITY_NONE, ATT_SECURITY_NONE, NULL, 0);
    att_db_util_add_service_uuid16(0x1808);
    att_db_util_add_included_service_uuid16(0x0014, 0x0016, 0x180f);
    att_db_util_add_characteristic_uuid16(0x2a18, ATT_PROPERTY_READ | ATT_PROPERTY_EXTENDED_PROPERTIES | ATT_PROPERTY_INDICATE, ATT_SECURITY_NONE, ATT_SECURITY_NONE, NULL, 0);
    att_db_util_add_descriptor_uuid16(0x2900, 0, ATT_SECURITY_NONE, ATT_SECURITY_NONE, (uint8_t*)extended_properties, sizeof(extended_properties));
    att_db_util_add_secondary_service_uuid16(0x180f);
    att_db_util_add_characteristic_uuid16(0x2a19, ATT_PROPERTY_READ, ATT_SECURITY_NONE, ATT_SECURITY_NONE, (uint8_t*)battery_level, sizeof(battery_level));

    uint16_t hash_len = att_db_util_hash_len();
    CHECK_EQUAL((uint16_t)sizeof(gatt_database_hash_test_message), hash_len);

    uint16_t i;
    att_db_util_hash_init();
    for (i=0;i<hash_len;i++){
        uint8_t actual_byte = att_db_util_hash_get_next();
        uint8_t expected_byte = gatt_database_hash_test_message[i];
        CHECK_EQUAL(expected_byte, actual_byte);
    }

    // calc cmac
    att_db_util_hash_calc(&cmac_context, cmac_calculated, &gatt_hash_calculated, NULL);
    CHECK_EQUAL_ARRAY(gatt_database_hash_expected, cmac_calculated, 16);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
