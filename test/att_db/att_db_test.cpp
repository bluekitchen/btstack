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
	READ_CALLBACK_MODE_RETURN_DEFAULT = 0,
	READ_CALLBACK_MODE_RETURN_ONE_BYTE,
	READ_CALLBACK_MODE_RETURN_PENDING
} read_callback_mode_t;

typedef enum {
	WRITE_CALLBACK_MODE_RETURN_DEFAULT = 0,
	WRITE_CALLBACK_MODE_RETURN_ERROR_WRITE_RESPONSE_PENDING,
	WRITE_CALLBACK_MODE_RETURN_INVALID_ATTRIBUTE_VALUE_LENGTH
} write_callback_mode_t;


static uint8_t battery_level = 100;
static uint8_t cgm_status = 0;

static uint8_t att_request[200];
static uint8_t att_response[1000];

static read_callback_mode_t read_callback_mode   = READ_CALLBACK_MODE_RETURN_DEFAULT;
static write_callback_mode_t write_callback_mode = WRITE_CALLBACK_MODE_RETURN_DEFAULT;

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
	switch (write_callback_mode){
		case WRITE_CALLBACK_MODE_RETURN_ERROR_WRITE_RESPONSE_PENDING:
			return ATT_ERROR_WRITE_RESPONSE_PENDING;
		case WRITE_CALLBACK_MODE_RETURN_INVALID_ATTRIBUTE_VALUE_LENGTH:
			return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
		default:
			return 0;
	}
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

static uint16_t att_write_request(uint16_t request_type, uint16_t attribute_handle, uint16_t value_length, const uint8_t * value){
    att_request[0] = request_type;
    little_endian_store_16(att_request, 1, attribute_handle);
    (void)memcpy(&att_request[3], value, value_length);
    return 3 + value_length;
}

static uint16_t att_prepare_write_request(uint16_t request_type, uint16_t attribute_handle, uint16_t value_offset){
    att_request[0] = request_type;
    little_endian_store_16(att_request, 1, attribute_handle);
    little_endian_store_16(att_request, 3, value_offset);
    return 5;
}

// ignore for now
extern "C" void btstack_crypto_aes128_cmac_generator(btstack_crypto_aes128_cmac_t * request, const uint8_t * key, uint16_t size, uint8_t (*get_byte_callback)(uint16_t pos), uint8_t * hash, void (* callback)(void * arg), void * callback_arg){
}

TEST_GROUP(AttDb){
	att_connection_t att_connection;
	uint16_t att_request_len;
	uint16_t att_response_len;
	uint8_t  callback_buffer[10];

    void setup(void){
    	memset(&callback_buffer, 0, sizeof(callback_buffer));

    	read_callback_mode = READ_CALLBACK_MODE_RETURN_ONE_BYTE;
    	memset(&att_connection, 0, sizeof(att_connection));
    	att_connection.max_mtu = 150;
		att_connection.mtu = ATT_DEFAULT_MTU;
		
		write_callback_mode = WRITE_CALLBACK_MODE_RETURN_DEFAULT;
		read_callback_mode  = READ_CALLBACK_MODE_RETURN_DEFAULT;

    	// init att db util and add a service and characteristic
		att_db_util_init();
		// 0x180F
		att_db_util_add_service_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
		// 0x2A19
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL,       ATT_PROPERTY_WRITE | ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
		// 0x2A1B
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_STATE, ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
		// 0x2A1A 
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_POWER_STATE, ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_AUTHENTICATED, ATT_SECURITY_AUTHENTICATED, &battery_level, 1);
		// 0x2A49 
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BLOOD_PRESSURE_FEATURE, ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
		// 0x2A35
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BLOOD_PRESSURE_MEASUREMENT, ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC, ATT_SECURITY_AUTHENTICATED, ATT_SECURITY_AUTHENTICATED, &battery_level, 1);
		// 0x2A38
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BODY_SENSOR_LOCATION, ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);

		const uint8_t uuid128[] = {0x00, 0x00, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
		att_db_util_add_characteristic_uuid128(uuid128, ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
		
		// 0x2AA7
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_CGM_MEASUREMENT, ATT_PROPERTY_WRITE_WITHOUT_RESPONSE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_AUTHENTICATED, ATT_SECURITY_AUTHENTICATED, &battery_level, 1);
		
		// 0x2AAB
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_CGM_SESSION_RUN_TIME, ATT_PROPERTY_WRITE_WITHOUT_RESPONSE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
		// 0x2A5C
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_CSC_FEATURE, ATT_PROPERTY_AUTHENTICATED_SIGNED_WRITE | ATT_PROPERTY_DYNAMIC, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        
        const uint8_t uuid128_service[] = {0xAA, 0xBB, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
        att_db_util_add_service_uuid128(uuid128_service);
        // 0x2AA9
		att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_CGM_STATUS, ATT_PROPERTY_WRITE_WITHOUT_RESPONSE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &cgm_status, 1);
		
		const uint8_t uuid128_chr_no_notify[] = {0xAA, 0xCC, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
		att_db_util_add_characteristic_uuid128(uuid128_chr_no_notify, ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
		
		att_db_util_add_included_service_uuid16(0x50, 0x51, 0xAACC);

		// const uint8_t uuid128_incl_service[] = {0xAA, 0xEE, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
		// att_db_util_add_included_service_uuid128(0x50, 0x51, uuid128_incl_service);
		// set callbacks
		att_set_db(att_db_util_get_address());
		att_set_read_callback(&att_read_callback);
		att_set_write_callback(&att_write_callback);
	}
};

TEST(AttDb, SetDB_NullAddress){
	// test some function
	att_set_db(NULL);
}


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


	// static read
	num_value_handles = 2;
	value_handles[0] = 0x03;
	value_handles[1] = 0x05;
	{
		read_callback_mode = READ_CALLBACK_MODE_RETURN_ONE_BYTE;

		att_request_len = att_read_multiple_request(num_value_handles, value_handles);
		CHECK_EQUAL(1 + 2 * num_value_handles, att_request_len);		
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_READ_MULTIPLE_RESPONSE, 0x64, 0x10, 0x06, 0x00, 0x1B, 0x2A};
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);

		read_callback_mode = READ_CALLBACK_MODE_RETURN_DEFAULT;
	}

#ifdef ENABLE_ATT_DELAYED_RESPONSE
	// dynamic read	
	num_value_handles = 2;
	value_handles[0] = 0x03;
	value_handles[1] = 0x0c;
	{
		read_callback_mode = READ_CALLBACK_MODE_RETURN_PENDING;
		
		att_request_len = att_read_multiple_request(num_value_handles, value_handles);
		CHECK_EQUAL(1 + 2 * num_value_handles, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_READ_MULTIPLE_RESPONSE, 0x64};
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);

		read_callback_mode = READ_CALLBACK_MODE_RETURN_DEFAULT;
	}
#endif
}

TEST(AttDb, handle_write_request){
	uint16_t attribute_handle = 0x03;

	// not sufficient request length
	{
		att_request[0] = ATT_WRITE_REQUEST;
		att_request_len = 1;
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], 0x00, 0x00, ATT_ERROR_INVALID_PDU};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}

	{
		att_request[0] = ATT_WRITE_REQUEST;
		att_request[1] = 0x03;
		att_request_len = 2;
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], 0x00, 0x00, ATT_ERROR_INVALID_PDU};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}

	// invalid handle
	{
		att_request[0] = ATT_WRITE_REQUEST;
		att_request[1] = 0;
		att_request[2] = 0;
		att_request_len = 3;
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_INVALID_HANDLE};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}

	// write not permited: invalid write callback
	{
		att_set_write_callback(NULL);
		
		const uint8_t value[] = {0x50};
		att_request_len = att_write_request(ATT_WRITE_REQUEST, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_WRITE_NOT_PERMITTED};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		
		att_set_write_callback(&att_write_callback);
	}

	// write not permited: no ATT_PROPERTY_WRITE
	{
		const uint8_t value[] = {0x50};
		attribute_handle = 0x000c; // 0x2A49
		att_request_len = att_write_request(ATT_WRITE_REQUEST, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_WRITE_NOT_PERMITTED};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}	

	// write not permited: no ATT_PROPERTY_DYNAMIC
	{
		const uint8_t value[] = {0x50};
		attribute_handle = 0x0003; 
		att_request_len = att_write_request(ATT_WRITE_REQUEST, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_WRITE_NOT_PERMITTED};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}

	// security validation 
	{
		const uint8_t value[] = {0x50};
		attribute_handle = 0x000f; // 0x2A35
		att_request_len = att_write_request(ATT_WRITE_REQUEST, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_INSUFFICIENT_AUTHENTICATION};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}

	// att_persistent_ccc_cache: ATT_PROPERTY_UUID16
	// att_persistent_ccc_cache: ATT_PROPERTY_UUID128

	// some callback error other then ATT_INTERNAL_WRITE_RESPONSE_PENDING
	{
		write_callback_mode = WRITE_CALLBACK_MODE_RETURN_INVALID_ATTRIBUTE_VALUE_LENGTH;
		
		const uint8_t value[] = {0x50};
		attribute_handle = 0x0011; // 0x2A38
		att_request_len = att_write_request(ATT_WRITE_REQUEST, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		
		write_callback_mode = WRITE_CALLBACK_MODE_RETURN_DEFAULT;
	}

#ifdef ENABLE_ATT_DELAYED_RESPONSE
	// delayed response 
	{
		write_callback_mode = WRITE_CALLBACK_MODE_RETURN_ERROR_WRITE_RESPONSE_PENDING;
		
		const uint8_t value[] = {0x50};
		attribute_handle = 0x0011; // 0x2A38
		att_request_len = att_write_request(ATT_WRITE_REQUEST, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		CHECK_EQUAL(ATT_INTERNAL_WRITE_RESPONSE_PENDING, att_response_len);
		
		write_callback_mode = WRITE_CALLBACK_MODE_RETURN_DEFAULT;
	}
#endif

	// correct write
	{
		const uint8_t value[] = {0x50};
		attribute_handle = 0x0011;
		att_request_len = att_write_request(ATT_WRITE_REQUEST, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_WRITE_RESPONSE};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}
}

TEST(AttDb, handle_prepare_write_request){
	uint16_t attribute_handle = 0x0011;
	uint16_t value_offset = 0x10;

	// not sufficient request length
	{
		att_request[0] = ATT_PREPARE_WRITE_REQUEST;
		att_request_len = 1;
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], 0x00, 0x00, ATT_ERROR_INVALID_PDU};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}
	
	// write not permited: invalid write callback
	{
		att_set_write_callback(NULL);
		
		att_request_len = att_prepare_write_request(ATT_PREPARE_WRITE_REQUEST, attribute_handle, value_offset); 
		CHECK_EQUAL(5, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_WRITE_NOT_PERMITTED};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
		
		att_set_write_callback(&att_write_callback);
	}

	// invalid handle
	{
		att_request_len = att_prepare_write_request(ATT_PREPARE_WRITE_REQUEST, 0, 0);
		CHECK_EQUAL(5, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_INVALID_HANDLE};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}

	// write not permited: no ATT_PROPERTY_WRITE
	{
		attribute_handle = 0x000c; // 0x2A49
		att_request_len = att_prepare_write_request(ATT_PREPARE_WRITE_REQUEST, attribute_handle, value_offset); 
		CHECK_EQUAL(5, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_WRITE_NOT_PERMITTED};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}	


	// write not permited: no ATT_PROPERTY_DYNAMIC
	{
		attribute_handle = 0x0003; 
		att_request_len = att_prepare_write_request(ATT_PREPARE_WRITE_REQUEST, attribute_handle, value_offset); 
		CHECK_EQUAL(5, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_WRITE_NOT_PERMITTED};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}

	// security validation 
	{
		attribute_handle = 0x000f; // 0x2A35
		att_request_len = att_prepare_write_request(ATT_PREPARE_WRITE_REQUEST, attribute_handle, value_offset); 
		CHECK_EQUAL(5, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_INSUFFICIENT_AUTHENTICATION};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}

	// some callback error other then ATT_INTERNAL_WRITE_RESPONSE_PENDING
	{
		write_callback_mode = WRITE_CALLBACK_MODE_RETURN_INVALID_ATTRIBUTE_VALUE_LENGTH;
		
		attribute_handle = 0x0011; // 0x2A38
		
		// prepare write
		att_request_len = att_prepare_write_request(ATT_PREPARE_WRITE_REQUEST, attribute_handle, value_offset); 
		CHECK_EQUAL(5, att_request_len);
		
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response_prepare_write[] = {ATT_PREPARE_WRITE_RESPONSE, 0x11, 0x00, 0x10, 0x00};
		MEMCMP_EQUAL(expected_response_prepare_write, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response_prepare_write), att_response_len);
		
		// execute write
		att_request_len = att_prepare_write_request(ATT_EXECUTE_WRITE_REQUEST, attribute_handle, value_offset); 
		CHECK_EQUAL(5, att_request_len);
		
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response_write_execute[] = {ATT_ERROR_RESPONSE, att_request[0], att_request[1], att_request[2], ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH};
		MEMCMP_EQUAL(expected_response_write_execute, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response_write_execute), att_response_len);
		
		write_callback_mode = WRITE_CALLBACK_MODE_RETURN_DEFAULT;
	}

#ifdef ENABLE_ATT_DELAYED_RESPONSE
	// delayed response 
	{
		write_callback_mode = WRITE_CALLBACK_MODE_RETURN_ERROR_WRITE_RESPONSE_PENDING;
		
		attribute_handle = 0x0011; // 0x2A38
		att_request_len = att_prepare_write_request(ATT_PREPARE_WRITE_REQUEST, attribute_handle, value_offset); 
		CHECK_EQUAL(5, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		

		CHECK_EQUAL(ATT_INTERNAL_WRITE_RESPONSE_PENDING, att_response_len);
		
		write_callback_mode = WRITE_CALLBACK_MODE_RETURN_DEFAULT;
	}
#endif

	// correct write
	{
		attribute_handle = 0x0011;
		att_request_len = att_prepare_write_request(ATT_PREPARE_WRITE_REQUEST, attribute_handle, value_offset); 
		CHECK_EQUAL(5, att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		const uint8_t expected_response[] = {ATT_PREPARE_WRITE_RESPONSE, 0x11, 0x00, 0x10, 0x00};
		MEMCMP_EQUAL(expected_response, att_response, att_response_len);
		CHECK_EQUAL(sizeof(expected_response), att_response_len);
	}
}

TEST(AttDb, att_uuid_for_handle){
	// existing attribute handle
	uint16_t uuid = att_uuid_for_handle(0x0011);
	uint16_t expected_response = 0x2A38;
	CHECK_EQUAL(expected_response, uuid);

	// unknown attribute handle
	uuid = att_uuid_for_handle(0xFF00);
	expected_response = 0;
	CHECK_EQUAL(expected_response, uuid);

	// attribute handle for uuid128
	uuid = att_uuid_for_handle(0x0014);
	expected_response = 0;
	CHECK_EQUAL(expected_response, uuid);
}

TEST(AttDb, gatt_server_get_handle_range){
	uint16_t start_handle;
	uint16_t end_handle;

	bool service_exists = gatt_server_get_handle_range_for_service_with_uuid16(0x00, &start_handle, &end_handle);
	CHECK_EQUAL(false, service_exists);

	uint16_t attribute_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, 0x00);
	CHECK_EQUAL(0x00, attribute_handle);
	
	attribute_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
	uint16_t configuration_handle = gatt_server_get_server_configuration_handle_for_characteristic_with_uuid16(0, 0xffff, attribute_handle);
	CHECK_EQUAL(0x00, configuration_handle);
}

TEST(AttDb, gatt_server_get_handle_range_for_service){
	uint16_t start_handle;
	uint16_t end_handle;

	const uint8_t uuid128_1[] = {0x00, 0x00, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	const uint8_t uuid128_2[] = {0xAA, 0xBB, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	const uint8_t uuid128_3[] = {0xAA, 0xDD, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	

	bool service_exists = gatt_server_get_handle_range_for_service_with_uuid128(uuid128_1, &start_handle, &end_handle);
	CHECK_EQUAL(false, service_exists);
	
	service_exists = gatt_server_get_handle_range_for_service_with_uuid128(uuid128_2, &start_handle, &end_handle);
	CHECK_EQUAL(true, service_exists);

	uint16_t out_included_service_handle;
	uint16_t out_included_service_start_handle;
	uint16_t out_included_service_end_handle;

	service_exists = gatt_server_get_included_service_with_uuid16(0, 0xffff, 0xAA,
		&out_included_service_handle, &out_included_service_start_handle, &out_included_service_end_handle);
	CHECK_EQUAL(false, service_exists);

	service_exists = gatt_server_get_included_service_with_uuid16(0, 0, 0xAA,
		&out_included_service_handle, &out_included_service_start_handle, &out_included_service_end_handle);
	CHECK_EQUAL(false, service_exists);

	service_exists = gatt_server_get_included_service_with_uuid16(0, 0xffff, 0xAACC,
		&out_included_service_handle, &out_included_service_start_handle, &out_included_service_end_handle);
	CHECK_EQUAL(true, service_exists);
}


TEST(AttDb, gatt_server_get_value_handle_for_characteristic_with_uuid128){
	const uint8_t uuid128_1[] = {0x00, 0x00, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	const uint8_t uuid128_2[] = {0xAA, 0xBB, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	
	uint16_t value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(0, 0xffff, uuid128_1);
	CHECK_EQUAL(20, value_handle);
	CHECK_EQUAL(false, att_is_persistent_ccc(value_handle));
	CHECK_EQUAL(false, att_is_persistent_ccc(0x60));

	value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(0, 0xffff, uuid128_2);
	CHECK_EQUAL(0, value_handle);

	value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(0, 0, uuid128_2);
	CHECK_EQUAL(0, value_handle);

	value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(0xffff, 0, uuid128_2);
	CHECK_EQUAL(0, value_handle);
}


TEST(AttDb, gatt_server_get_client_configuration_handle_for_characteristic){
	const uint8_t uuid128_1[] = {0x00, 0x00, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	const uint8_t uuid128_2[] = {0xAA, 0xBB, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	const uint8_t uuid128_3[] = {0xAA, 0xCC, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	
	uint16_t value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(0, 0xffff, uuid128_1);
	CHECK_EQUAL(21, value_handle);
	
	value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(0, 0xffff, uuid128_2);
	CHECK_EQUAL(0, value_handle);

	value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(0, 0, uuid128_2);
	CHECK_EQUAL(0, value_handle);

	value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(0xffff, 0, uuid128_2);
	CHECK_EQUAL(0, value_handle);

	value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(0xffff, 0, uuid128_3);
	CHECK_EQUAL(0, value_handle); 


	value_handle = gatt_server_get_descriptor_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BLOOD_PRESSURE_MEASUREMENT, GATT_SERVER_CHARACTERISTICS_CONFIGURATION);
	CHECK_EQUAL(0, value_handle);
	
	value_handle = gatt_server_get_descriptor_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL, GATT_SERVER_CHARACTERISTICS_CONFIGURATION);
	CHECK_EQUAL(0, value_handle);

	value_handle = gatt_server_get_descriptor_handle_for_characteristic_with_uuid16(0, 0, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL, GATT_SERVER_CHARACTERISTICS_CONFIGURATION);
	CHECK_EQUAL(0, value_handle);
}


TEST(AttDb, handle_signed_write_command){
	uint16_t attribute_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_CSC_FEATURE);
	
	att_request[0] = ATT_SIGNED_WRITE_COMMAND;
	att_request_len = 1;
	att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
	CHECK_EQUAL(0, att_response_len);
}

TEST(AttDb, handle_write_command){
	uint16_t attribute_handle = 0x03;	
	att_dump_attributes();

	// not sufficient request length
	{
		att_request[0] = ATT_WRITE_COMMAND;
		att_request_len = 1;
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		CHECK_EQUAL(0, att_response_len);
	}

	{
		att_request[0] = ATT_WRITE_COMMAND;
		att_request[1] = 0x03;
		att_request_len = 2;
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		CHECK_EQUAL(0, att_response_len);
	}
	
	// invalid write callback
	{
		att_set_write_callback(NULL);
		
		const uint8_t value[] = {0x50};
		att_request_len = att_write_request(ATT_WRITE_COMMAND, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		CHECK_EQUAL(0, att_response_len);

		att_set_write_callback(&att_write_callback);
	}

	// invalid handle
	{
		att_request[0] = ATT_WRITE_COMMAND;
		att_request[1] = 0;
		att_request[2] = 0;
		att_request_len = 3;
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		CHECK_EQUAL(0, att_response_len);
	}

	// write not permited: no ATT_PROPERTY_DYNAMIC
	{
		const uint8_t value[] = {0x50};
		attribute_handle = 0x0003; 
		att_request_len = att_write_request(ATT_WRITE_COMMAND, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		CHECK_EQUAL(0, att_response_len);
	}

	// write not permited: no ATT_PROPERTY_WRITE_WITHOUT_RESPONSE
	{
		const uint8_t value[] = {0x50};
		attribute_handle = 0x000c; // 0x2A49
		att_request_len = att_write_request(ATT_WRITE_COMMAND, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		CHECK_EQUAL(0, att_response_len);
	}	

	// security validation 
	{
		const uint8_t value[] = {0x50};
		attribute_handle = 0x0017; // 0x2AA7
		att_request_len = att_write_request(ATT_WRITE_COMMAND, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		CHECK_EQUAL(0, att_response_len);
	}

	// correct write 
	{
		const uint8_t value[] = {0x50};
		attribute_handle = 0x001A; // 0x2AAB
		att_request_len = att_write_request(ATT_WRITE_COMMAND, attribute_handle, sizeof(value), value); 
		CHECK_EQUAL(3 + sizeof(value), att_request_len);
		att_response_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);	
		CHECK_EQUAL(0, att_response_len);
	}
}

TEST(AttDb, att_read_callback_handle_blob){
	{
		const uint8_t blob[] = {0x44, 0x55};
		uint16_t blob_size = att_read_callback_handle_blob(blob, sizeof(blob), 0, NULL, 0);
		CHECK_EQUAL(2, blob_size);
	}

	{
		const uint8_t blob[] = {};
		uint16_t blob_size = att_read_callback_handle_blob(blob, sizeof(blob), 0, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(0, blob_size);
	}

	{
		const uint8_t blob[] = {};
		uint16_t blob_size = att_read_callback_handle_blob(blob, sizeof(blob), 10, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(0, blob_size);
	}

	{
		const uint8_t blob[] = {0x55};
		uint16_t blob_size = att_read_callback_handle_blob(blob, sizeof(blob), 0, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(1, blob_size);
	}

	{
		const uint8_t blob[] = {0x55};
		uint16_t blob_size = att_read_callback_handle_blob(blob, sizeof(blob), 10, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(0, blob_size);
	}

	{
		const uint8_t blob[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA};
		uint16_t blob_size = att_read_callback_handle_blob(blob, sizeof(blob), 0, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(10, blob_size);
	}

	{
		const uint8_t blob[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA};
		uint16_t blob_size = att_read_callback_handle_blob(blob, sizeof(blob), 1, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(9, blob_size);
	}

	{
		const uint8_t blob[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA};
		uint16_t blob_size = att_read_callback_handle_blob(blob, sizeof(blob), 10, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(0, blob_size);
	}
}
	
TEST(AttDb, att_read_callback_handle_byte){
	{
		uint16_t blob_size = att_read_callback_handle_byte(0x55, 0, NULL, 0);
		CHECK_EQUAL(1, blob_size);
	}

	{
		uint16_t blob_size = att_read_callback_handle_byte(0x55, 10, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(0, blob_size);
	}
	
	{
		uint16_t blob_size = att_read_callback_handle_byte(0x55, 0, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(1, blob_size);
	}
}

TEST(AttDb, att_read_callback_handle_little_endian_16){
	{
		uint16_t blob_size = att_read_callback_handle_little_endian_16(0x1122, 0, NULL, 0);
		CHECK_EQUAL(2, blob_size);
	}

	{
		uint16_t blob_size = att_read_callback_handle_little_endian_16(0x1122, 10, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(0, blob_size);
	}
	
	{
		uint16_t blob_size = att_read_callback_handle_little_endian_16(0x1122, 1, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(1, blob_size);
	}
	
	{
		uint16_t blob_size = att_read_callback_handle_little_endian_16(0x1122, 0, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(2, blob_size);
	}
}

TEST(AttDb, att_read_callback_handle_little_endian_32){
	{
		uint16_t blob_size = att_read_callback_handle_little_endian_32(0x11223344, 0, NULL, 0);
		CHECK_EQUAL(4, blob_size);
	}

	{
		uint16_t blob_size = att_read_callback_handle_little_endian_32(0x11223344, 10, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(0, blob_size);
	}
	
	{
		uint16_t blob_size = att_read_callback_handle_little_endian_32(0x11223344, 3, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(1, blob_size);
	}
	
	{
		uint16_t blob_size = att_read_callback_handle_little_endian_32(0x11223344, 0, callback_buffer, sizeof(callback_buffer));
		CHECK_EQUAL(4, blob_size);
	}
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
