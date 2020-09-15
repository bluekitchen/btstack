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
#include "bluetooth_gatt.h"

static uint8_t battery_level = 100;

// these can be tweaked to report errors or some data as needed by test case
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	return 0;
}

static int att_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	return 0;
}

// ignore for now
extern "C" void btstack_crypto_aes128_cmac_generator(btstack_crypto_aes128_cmac_t * request, const uint8_t * key, uint16_t size, uint8_t (*get_byte_callback)(uint16_t pos), uint8_t * hash, void (* callback)(void * arg), void * callback_arg){
}

TEST_GROUP(AttDb){
    void setup(void){
    	// init att db util and add a service and characteristic
		att_db_util_init();
		att_db_util_add_service_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL, ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
		// set callbacks
		att_set_db(NULL);
		att_set_read_callback(&att_read_callback);
		att_set_write_callback(&att_write_callback);
	}
};

static 	uint8_t att_response[1000];

TEST(AttDb, MtuExchange){
	// test some function
	att_connection_t att_connection = { 0 };
	att_connection.max_mtu = 150;
	att_connection.mtu = ATT_DEFAULT_MTU;
	uint16_t att_request_len = 3;
	const uint8_t att_request[3] = { ATT_EXCHANGE_MTU_REQUEST, 0, 150};
	uint16_t att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);
	CHECK_EQUAL(3, att_response_len);
	const uint8_t expected_response[] = { ATT_EXCHANGE_MTU_RESPONSE, 150, 0};
	MEMCMP_EQUAL(expected_response, att_response, 3);
}

TEST(AttDb, B){
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
