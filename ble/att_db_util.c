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

#include <stdlib.h>
#include <string.h>

#include "att_db_util.h"
#include "att.h"
#include <btstack/utils.h>
#include "debug.h"

// ATT DB Storage
#ifndef HAVE_MALLOC
#ifdef MAX_ATT_DB_SIZE
static uint8_t att_db_storage[MAX_ATT_DB_SIZE];
#else
#error Neither HAVE_MALLOC] nor MAX_ATT_DB_SIZE is defined. 
#endif
#endif

static uint8_t * att_db;
static uint16_t  att_db_size;
static uint16_t  att_db_max_size;
static uint16_t  att_db_next_handle;

static void att_db_util_set_end_tag(void){
	// end tag
	att_db[att_db_size] = 0;
	att_db[att_db_size+1] = 0;
}

void att_db_util_init(void){
#ifdef HAVE_MALLOC
	att_db = (uint8_t*) malloc(128);
	att_db_max_size = 128;
#else
	att_db = att_db_storage;
	att_db_max_size = sizeof(att_db_storage);
#endif
	att_db_size = 0;
	att_db_next_handle = 1;
	att_db_util_set_end_tag();
}

/**
 * asserts that the requested amount of bytes can be stored in the att_db
 * @returns TRUE if space is available
 */
static int att_db_util_assert_space(uint16_t size){
	size += 2; // for end tag
	if (att_db_size + size <= att_db_max_size) return 1;
#ifdef HAVE_MALLOC
	int new_size = att_db_size + att_db_size / 2;
	att_db = (uint8_t*) realloc(att_db, new_size);
	if (!att_db) {
		log_error("att_db: realloc failed");
		return 0;
	}
	att_db_max_size = new_size;
	return 1;
#else
	log_error("att_db: out of memory");
	return 0;
#endif
}

// attribute size in bytes (16), flags(16), handle (16), uuid (16/128), value(...)

// db endds with 0x00 0x00

static void att_db_util_add_attribute_uuid16(uint16_t uuid16, uint16_t flags, uint8_t * data, uint16_t data_len){
	int size = 2 + 2 + 2 + 2 + data_len;
	if (!att_db_util_assert_space(size)) return;
	bt_store_16(att_db, att_db_size, size);
	att_db_size += 2;
	bt_store_16(att_db, att_db_size, flags);
	att_db_size += 2;
	bt_store_16(att_db, att_db_size, att_db_next_handle);
	att_db_size += 2;
	att_db_next_handle++;
	bt_store_16(att_db, att_db_size, uuid16);
	att_db_size += 2;
	memcpy(&att_db[att_db_size], data, data_len);
	att_db_size += data_len;
	att_db_util_set_end_tag();
}

static void att_db_util_add_attribute_uuid128(uint8_t * uuid128, uint16_t flags, uint8_t * data, uint16_t data_len){
	int size = 2 + 2 + 2 + 16 + data_len;
	if (!att_db_util_assert_space(size)) return;
	flags |= ATT_PROPERTY_UUID128;
	bt_store_16(att_db, att_db_size, size);
	att_db_size += 2;
	bt_store_16(att_db, att_db_size, flags);
	att_db_size += 2;
	bt_store_16(att_db, att_db_size, att_db_next_handle);
	att_db_size += 2;
	att_db_next_handle++;
	swap128(uuid128, &att_db[att_db_size]);
	att_db_size += 16;
	memcpy(&att_db[att_db_size], data, data_len);
	att_db_size += data_len;
	att_db_util_set_end_tag();
}

void att_db_util_add_service_uuid16(uint16_t uuid16){
	uint8_t buffer[2];
	bt_store_16(buffer, 0, uuid16);
	att_db_util_add_attribute_uuid16(GATT_PRIMARY_SERVICE_UUID, ATT_PROPERTY_READ, buffer, 2);
}

void att_db_util_add_service_uuid128(uint8_t * uuid128){
	uint8_t buffer[16];
	swap128(uuid128, buffer);
	att_db_util_add_attribute_uuid16(GATT_PRIMARY_SERVICE_UUID, ATT_PROPERTY_READ, buffer, 16);
}

uint16_t att_db_util_add_characteristic_uuid16(uint16_t uuid16, uint16_t properties, uint8_t * data, uint16_t data_len){
	uint8_t buffer[5];
	buffer[0] = properties;
	bt_store_16(buffer, 1, att_db_next_handle + 1);
	bt_store_16(buffer, 3, uuid16);
	att_db_util_add_attribute_uuid16(GATT_CHARACTERISTICS_UUID, ATT_PROPERTY_READ, buffer, sizeof(buffer));
	uint16_t value_handle = att_db_next_handle;
	att_db_util_add_attribute_uuid16(uuid16, properties, data, data_len);
	if (properties & (ATT_PROPERTY_NOTIFY | ATT_PROPERTY_INDICATE)){
		uint16_t flags = ATT_PROPERTY_READ | ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC;
		bt_store_16(buffer, 0, 0); 
		att_db_util_add_attribute_uuid16(GATT_CLIENT_CHARACTERISTICS_CONFIGURATION, flags, buffer, 2);
	}
	return value_handle;
}

uint16_t att_db_util_add_characteristic_uuid128(uint8_t * uuid128, uint16_t properties, uint8_t * data, uint16_t data_len){
	uint8_t buffer[19];
	buffer[0] = properties;
	bt_store_16(buffer, 1, att_db_next_handle + 1);
	swap128(uuid128, &buffer[3]);
	att_db_util_add_attribute_uuid16(GATT_CHARACTERISTICS_UUID, ATT_PROPERTY_READ, buffer, sizeof(buffer));
	uint16_t value_handle = att_db_next_handle;
	att_db_util_add_attribute_uuid128(uuid128, properties, data, data_len);
	if (properties & (ATT_PROPERTY_NOTIFY | ATT_PROPERTY_INDICATE)){
		uint16_t flags = ATT_PROPERTY_READ | ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC;
		bt_store_16(buffer, 0, 0); 
		att_db_util_add_attribute_uuid16(GATT_CLIENT_CHARACTERISTICS_CONFIGURATION, flags, buffer, 2);
	}
	return value_handle;
}

uint8_t * att_db_util_get_address(void){
	return att_db;
}

uint16_t att_db_util_get_size(void){
	return att_db_size + 2;	// end tag 
}
