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

typedef enum {
	READ_CALLBACK_MODE_RETURN_ONE_BYTE = 0,
	READ_CALLBACK_MODE_RETURN_PENDING
} read_callback_mode_t;

static uint8_t battery_level = 100;
static uint8_t att_request[200];
static uint8_t att_response[1000];

static read_callback_mode_t read_callback_mode = READ_CALLBACK_MODE_RETURN_ONE_BYTE;

// these can be tweaked to report errors or some data as needed by test case
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	switch (read_callback_mode){
		case READ_CALLBACK_MODE_RETURN_ONE_BYTE:
			return att_read_callback_handle_byte(0x55, offset, buffer, buffer_size);
		case READ_CALLBACK_MODE_RETURN_PENDING:
			return ATT_READ_RESPONSE_PENDING;
		default:
			return 0;
	}
}

static int att_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	return 0;
}

static uint16_t att_read_multiple_request(uint16_t num_value_handles, uint16_t * value_handles){
    att_request[0] = ATT_READ_MULTIPLE_REQUEST;
    int i;
    int offset = 1;
    for (i=0;i<num_value_handles;i++){
        little_endian_store_16(att_request, offset, value_handles[i]);
        offset += 2;
    }
	return offset;
}

// ignore for now
extern "C" void btstack_crypto_aes128_cmac_generator(btstack_crypto_aes128_cmac_t * request, const uint8_t * key, uint16_t size, uint8_t (*get_byte_callback)(uint16_t pos), uint8_t * hash, void (* callback)(void * arg), void * callback_arg){
}

TEST_GROUP(AttDb){
	att_connection_t att_connection;
	uint16_t att_request_len;
	uint16_t att_response_len;

    void setup(void){
    	read_callback_mode = READ_CALLBACK_MODE_RETURN_ONE_BYTE;
    	memset(&att_connection, 0, sizeof(att_connection));
    	att_connection.max_mtu = 150;
		att_connection.mtu = ATT_DEFAULT_MTU;
	
    	// init att db util and add a service and characteristic
		att_db_util_init();
		// 0x180F
		att_db_util_add_service_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
		// 0x2A19
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL,       ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
		// 0x2A1B
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_STATE, ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
		// 0x2A1A 
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_POWER_STATE, ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_AUTHENTICATED, ATT_SECURITY_AUTHENTICATED, &battery_level, 1);
		// 0x2A49 
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BLOOD_PRESSURE_FEATURE, ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
		
		// set callbacks
		att_set_db(att_db_util_get_address());
		att_set_read_callback(&att_read_callback);
		att_set_write_callback(&att_write_callback);
	}
};


TEST(AttDb, MtuExchange){
	// test some function
	att_request_len = 3;
	const uint8_t att_request[3] = { ATT_EXCHANGE_MTU_REQUEST, 0, att_connection.max_mtu};
	att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);
	CHECK_EQUAL(att_request_len, att_response_len);
	const uint8_t expected_response[] = { ATT_EXCHANGE_MTU_RESPONSE, att_connection.max_mtu, 0};
	MEMCMP_EQUAL(expected_response, att_response, att_response_len);
}

TEST(AttDb, handle_read_multiple_request){
	uint16_t value_handles[2];
	uint16_t num_value_handles;
	att_dump_attributes();

	// less then two values
	num_value_handles = 0;
	memset(&value_handles, 0, sizeof(value_handles));
	{
		att_request_len = att_read_multiple_request(num_value_handles, value_handles);
		CHECK_EQUAL(1 + 2 * num_value_handles, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, ATT_READ_MULTIPLE_REQUEST, 0, 0, ATT_ERROR_INVALID_PDU};
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
	}

	num_value_handles = 1;
	value_handles[0] = 0x1;
	{
		att_request_len = att_read_multiple_request(num_value_handles, value_handles);
		CHECK_EQUAL(1 + 2 * num_value_handles, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, ATT_READ_MULTIPLE_REQUEST, 0, 0, ATT_ERROR_INVALID_PDU};
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
	}

	// invalid handle
	num_value_handles = 2;
	value_handles[0] = 0x0;
	{
		att_request_len = att_read_multiple_request(num_value_handles, value_handles);
		CHECK_EQUAL(1 + 2 * num_value_handles, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, ATT_READ_MULTIPLE_REQUEST, 0, 0, ATT_ERROR_INVALID_HANDLE};
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
	}

	num_value_handles = 2;
	value_handles[0] = 0xF1;
	value_handles[1] = 0xF2;
	{
		att_request_len = att_read_multiple_request(num_value_handles, value_handles);
		CHECK_EQUAL(1 + 2 * num_value_handles, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, ATT_READ_MULTIPLE_REQUEST, value_handles[0], 0, ATT_ERROR_INVALID_HANDLE};
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
	}

	// handle read not permitted
	num_value_handles = 2;
	value_handles[0] = 0x05;
	value_handles[1] = 0x06;
	{
		att_request_len = att_read_multiple_request(num_value_handles, value_handles);
		CHECK_EQUAL(1 + 2 * num_value_handles, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, ATT_READ_MULTIPLE_REQUEST, value_handles[1], 0, ATT_ERROR_READ_NOT_PERMITTED};
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
	}

	// security validation (single case)
	num_value_handles = 2;
	value_handles[0] = 0x05;
	value_handles[1] = 0x09;
	{
		att_request_len = att_read_multiple_request(num_value_handles, value_handles);
		CHECK_EQUAL(1 + 2 * num_value_handles, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, ATT_READ_MULTIPLE_REQUEST, value_handles[1], 0, ATT_ERROR_INSUFFICIENT_AUTHENTICATION};
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
	}

#ifdef ENABLE_ATT_DELAYED_RESPONSE
	// TODO
#endif
	// static read
	read_callback_mode = READ_CALLBACK_MODE_RETURN_ONE_BYTE;
	num_value_handles = 2;
	value_handles[0] = 0x03;
	value_handles[1] = 0x05;
	{
		att_request_len = att_read_multiple_request(num_value_handles, value_handles);
		CHECK_EQUAL(1 + 2 * num_value_handles, att_request_len);		
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_READ_MULTIPLE_RESPONSE, 0x64, 0x10, 0x06, 0x00, 0x1B, 0x2A};
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
	}

	// dynamic read
	read_callback_mode = READ_CALLBACK_MODE_RETURN_PENDING;
	num_value_handles = 2;
	value_handles[0] = 0x03;
	value_handles[1] = 0x0c;
	{
		att_request_len = att_read_multiple_request(num_value_handles, value_handles);
		CHECK_EQUAL(1 + 2 * num_value_handles, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_READ_MULTIPLE_RESPONSE, 0x64};
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
	}
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
