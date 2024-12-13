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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "att_db_util.c"

#include <string.h>
#include <stdlib.h>

#include "ble/att_db_util.h"
#include "ble/att_db.h"
#include "ble/core.h"
#include "btstack_util.h"
#include "btstack_debug.h"
#include "bluetooth.h"

// ATT DB Storage
#ifdef HAVE_MALLOC
// number of bytes that the att db buffer is increased on init / realloc
#define ATT_DB_BUFFER_INCREMENT 128
#else
#ifdef MAX_ATT_DB_SIZE
static uint8_t att_db_storage[MAX_ATT_DB_SIZE];
#else
#error Neither HAVE_MALLOC nor MAX_ATT_DB_SIZE is defined. 
#endif
#endif

static uint8_t * att_db;
static uint16_t  att_db_size;
static uint16_t  att_db_max_size;
static uint16_t  att_db_next_handle;
static uint16_t  att_db_hash_len;

static void att_db_util_set_end_tag(void){
	// end tag
	att_db[att_db_size] = 0u;
	att_db[att_db_size+1u] = 0u;
}

void att_db_util_init(void){
#ifdef HAVE_MALLOC
    if (att_db == NULL){
        att_db = (uint8_t*) malloc((size_t)ATT_DB_BUFFER_INCREMENT);
        att_db_max_size = (uint16_t)ATT_DB_BUFFER_INCREMENT;
    }
#else
	att_db = att_db_storage;
	att_db_max_size = sizeof(att_db_storage);
#endif
	// store att version
	att_db[0] = (uint8_t)ATT_DB_VERSION;
	att_db_size = 1u;
	att_db_next_handle = 1u;
	att_db_hash_len = 0u;
	att_db_util_set_end_tag();
}

void att_db_util_set_next_handle(uint16_t handle){
   btstack_assert(handle >= att_db_next_handle); // do not reuse assigned handles
   att_db_next_handle = handle;
}

static bool att_db_util_hash_include_with_value(uint16_t uuid16){
    /* «Primary Service», «Secondary Service», «Included Service», «Characteristic», or «Characteristic Extended Properties» */
    switch (uuid16){
        case GATT_PRIMARY_SERVICE_UUID:
        case GATT_SECONDARY_SERVICE_UUID:
        case GATT_INCLUDE_SERVICE_UUID:
        case GATT_CHARACTERISTICS_UUID:
        case GATT_CHARACTERISTIC_EXTENDED_PROPERTIES:
            return true;
        default:
            return false;
    }
}

static bool att_db_util_hash_include_without_value(uint16_t uuid16){
    /*  «Characteristic User Description», «Client Characteristic Configuration», «Server Characteristic Configuration»,
     * «Characteristic Aggregate Format», «Characteristic Format» */
    switch (uuid16){
        case GATT_CHARACTERISTIC_USER_DESCRIPTION:
        case GATT_CLIENT_CHARACTERISTICS_CONFIGURATION:
        case GATT_SERVER_CHARACTERISTICS_CONFIGURATION:
        case GATT_CHARACTERISTIC_PRESENTATION_FORMAT:
        case GATT_CHARACTERISTIC_AGGREGATE_FORMAT:
            return true;
        default:
            return false;
    }
}

/**
 * asserts that the requested amount of bytes can be stored in the att_db
 * @return TRUE if space is available
 */
static int att_db_util_assert_space(uint16_t size){
	uint16_t required_size = att_db_size + size + 2u;
	if (required_size <= att_db_max_size) return 1;
#ifdef HAVE_MALLOC
    uint16_t new_size = att_db_max_size;
	while (new_size < required_size){
        new_size += (uint16_t)ATT_DB_BUFFER_INCREMENT;
	}
	uint8_t * new_db = (uint8_t*) realloc(att_db, new_size);
	if (!new_db) {
		log_error("att_db: realloc failed");
		return 0;
	}
	att_db = new_db;
	att_db_max_size = new_size;
    att_set_db(att_db); // Update att_db with the new db
	return 1;
#else
	log_error("att_db: out of memory");
	return 0;
#endif
}

// attribute size in bytes (16), flags(16), handle (16), uuid (16/128), value(...)

// db endds with 0x00 0x00

static void att_db_util_add_attribute_uuid16(uint16_t uuid16, uint16_t flags, uint8_t * data, uint16_t data_len){
	int size = 2u + 2u + 2u + 2u + data_len;
	if (!att_db_util_assert_space(size)) return;
	little_endian_store_16(att_db, att_db_size, size);
	att_db_size += 2u;
	little_endian_store_16(att_db, att_db_size, flags);
	att_db_size += 2u;
	little_endian_store_16(att_db, att_db_size, att_db_next_handle);
	att_db_size += 2u;
	att_db_next_handle++;
	little_endian_store_16(att_db, att_db_size, uuid16);
	att_db_size += 2u;
	(void)memcpy(&att_db[att_db_size], data, data_len);
	att_db_size += data_len;
	att_db_util_set_end_tag();

	if (att_db_util_hash_include_with_value(uuid16)){
	    att_db_hash_len += 4u + data_len;
	} else if (att_db_util_hash_include_without_value(uuid16)){
        att_db_hash_len += 4u;
	}
}

static void att_db_util_add_attribute_uuid128(const uint8_t * uuid128, uint16_t flags, uint8_t * data, uint16_t data_len){
	int size = 2u + 2u + 2u + 16u + data_len;
	if (!att_db_util_assert_space(size)) return;
	uint16_t flags_to_store = flags | (uint16_t)ATT_PROPERTY_UUID128;
	little_endian_store_16(att_db, att_db_size, size);
	att_db_size += 2u;
	little_endian_store_16(att_db, att_db_size, flags_to_store);
	att_db_size += 2u;
	little_endian_store_16(att_db, att_db_size, att_db_next_handle);
	att_db_size += 2u;
	att_db_next_handle++;
	reverse_128(uuid128, &att_db[att_db_size]);
	att_db_size += 16u;
	(void)memcpy(&att_db[att_db_size], data, data_len);
	att_db_size += data_len;
	att_db_util_set_end_tag();
}

uint16_t att_db_util_add_service_uuid16(uint16_t uuid16){
	uint8_t buffer[2];
	little_endian_store_16(buffer, 0, uuid16);
	uint16_t service_handle = att_db_next_handle;
	att_db_util_add_attribute_uuid16(GATT_PRIMARY_SERVICE_UUID, ATT_PROPERTY_READ, buffer, 2);
	return service_handle;
}

uint16_t att_db_util_add_service_uuid128(const uint8_t * uuid128){
	uint8_t buffer[16];
	reverse_128(uuid128, buffer);
	uint16_t service_handle = att_db_next_handle;
	att_db_util_add_attribute_uuid16(GATT_PRIMARY_SERVICE_UUID, ATT_PROPERTY_READ, buffer, 16);
	return service_handle;
}

uint16_t att_db_util_add_secondary_service_uuid16(uint16_t uuid16){
    uint8_t buffer[2];
    little_endian_store_16(buffer, 0, uuid16);
    uint16_t service_handle = att_db_next_handle;
    att_db_util_add_attribute_uuid16(GATT_SECONDARY_SERVICE_UUID, ATT_PROPERTY_READ, buffer, 2);
    return service_handle;
}

uint16_t att_db_util_add_secondary_service_uuid128(const uint8_t * uuid128){
    uint8_t buffer[16];
    reverse_128(uuid128, buffer);
    uint16_t service_handle = att_db_next_handle;
    att_db_util_add_attribute_uuid16(GATT_SECONDARY_SERVICE_UUID, ATT_PROPERTY_READ, buffer, 16);
    return service_handle;
}

uint16_t att_db_util_add_included_service_uuid16(uint16_t start_group_handle, uint16_t  end_group_handle, uint16_t uuid16){
    uint8_t buffer[6];
    little_endian_store_16(buffer, 0, start_group_handle);
    little_endian_store_16(buffer, 2, end_group_handle);
    little_endian_store_16(buffer, 4, uuid16);
    uint16_t service_handle = att_db_next_handle;
    att_db_util_add_attribute_uuid16(GATT_INCLUDE_SERVICE_UUID, ATT_PROPERTY_READ, buffer, sizeof(buffer));
    return service_handle;
}

static void att_db_util_add_client_characteristic_configuration(uint16_t flags){
	uint8_t buffer[2];
	// drop permission for read (0xc00), keep write permissions (0x0091)
	uint16_t flags_to_store = (flags & 0x1f391u) | (uint16_t)ATT_PROPERTY_READ | (uint16_t)ATT_PROPERTY_WRITE | (uint16_t)ATT_PROPERTY_DYNAMIC;
	little_endian_store_16(buffer, 0, 0); 
	att_db_util_add_attribute_uuid16(GATT_CLIENT_CHARACTERISTICS_CONFIGURATION, flags_to_store, buffer, 2);
}

static uint16_t att_db_util_encode_permissions(uint16_t properties, uint8_t read_permission, uint8_t write_permission){
    // drop Broadcast (0x01), Notify (0x10), Indicate (0x20), Extended Attributes (0x80) - not used for flags 
    uint16_t flags = properties & 0xfff4eu;
    // if encryption requested, set encryption key size to 16
    if ((read_permission > (uint8_t)ATT_SECURITY_NONE) || (write_permission > (uint8_t)ATT_SECURITY_NONE)){
    	flags |= 0xf000u;
    }
    // map SC requirement
    uint8_t final_read_permission;
    if (read_permission == (uint8_t)ATT_SECURITY_AUTHENTICATED_SC){
        final_read_permission =  (uint8_t)ATT_SECURITY_AUTHENTICATED;
        flags |= (uint16_t)ATT_PROPERTY_READ_PERMISSION_SC;
    } else {
        final_read_permission = read_permission;
    }
    uint8_t final_write_permission;
    if (write_permission == (uint8_t)ATT_SECURITY_AUTHENTICATED_SC){
        final_write_permission = (uint8_t)ATT_SECURITY_AUTHENTICATED;
        flags |= (uint16_t)ATT_PROPERTY_WRITE_PERMISSION_SC;
    } else {
        final_write_permission = write_permission;
    }
    // encode read/write security levels
    if ((final_read_permission & 1u) != 0u){
    	flags |= (uint16_t)ATT_PROPERTY_READ_PERMISSION_BIT_0;
    }
    if ((final_read_permission & 2u) != 0u){
    	flags |= (uint16_t)ATT_PROPERTY_READ_PERMISSION_BIT_1;
    }
    if ((final_write_permission & 1u) != 0u){
    	flags |= (uint16_t)ATT_PROPERTY_WRITE_PERMISSION_BIT_0;
    }
    if ((final_write_permission & 2u) != 0u){
    	flags |= (uint16_t)ATT_PROPERTY_WRITE_PERMISSION_BIT_1;
    }
	return flags;
}

uint16_t att_db_util_add_characteristic_uuid16(uint16_t uuid16, uint16_t properties, uint8_t read_permission, uint8_t write_permission, uint8_t * data, uint16_t data_len){
	uint8_t buffer[5];
	buffer[0] = (uint8_t) (properties & 0xff);
	little_endian_store_16(buffer, 1u, att_db_next_handle + 1u);
	little_endian_store_16(buffer, 3, uuid16);
	att_db_util_add_attribute_uuid16(GATT_CHARACTERISTICS_UUID, ATT_PROPERTY_READ, buffer, sizeof(buffer));
	uint16_t flags = att_db_util_encode_permissions(properties, read_permission, write_permission);	
	uint16_t value_handle = att_db_next_handle;
	att_db_util_add_attribute_uuid16(uuid16, flags, data, data_len);
	if ((properties & (uint8_t)(ATT_PROPERTY_NOTIFY | ATT_PROPERTY_INDICATE)) != 0u){
		att_db_util_add_client_characteristic_configuration(flags);
	}
	return value_handle;
}

uint16_t att_db_util_add_characteristic_uuid128(const uint8_t * uuid128, uint16_t properties, uint8_t read_permission, uint8_t write_permission, uint8_t * data, uint16_t data_len){
	uint8_t buffer[19];
	buffer[0] = (uint8_t) (properties & 0xff);
	little_endian_store_16(buffer, 1u, att_db_next_handle + 1u);
	reverse_128(uuid128, &buffer[3]);
	att_db_util_add_attribute_uuid16(GATT_CHARACTERISTICS_UUID, ATT_PROPERTY_READ, buffer, sizeof(buffer));
	uint16_t flags = att_db_util_encode_permissions(properties, read_permission, write_permission);	
	uint16_t value_handle = att_db_next_handle;
	att_db_util_add_attribute_uuid128(uuid128, flags, data, data_len);
	if ((properties & (uint8_t)(ATT_PROPERTY_NOTIFY | ATT_PROPERTY_INDICATE)) != 0u){
		att_db_util_add_client_characteristic_configuration(flags);
	}
	return value_handle;
}

uint16_t att_db_util_add_descriptor_uuid16(uint16_t uuid16, uint16_t properties, uint8_t read_permission, uint8_t write_permission, uint8_t * data, uint16_t data_len){
    uint16_t descriptor_handler = att_db_next_handle;
	uint16_t flags = att_db_util_encode_permissions(properties, read_permission, write_permission);	
    att_db_util_add_attribute_uuid16(uuid16, flags, data, data_len);
    return descriptor_handler;
}

uint16_t att_db_util_add_descriptor_uuid128(const uint8_t * uuid128, uint16_t properties, uint8_t read_permission, uint8_t write_permission, uint8_t * data, uint16_t data_len){
    uint16_t descriptor_handler = att_db_next_handle;
	uint16_t flags = att_db_util_encode_permissions(properties, read_permission, write_permission);	
    att_db_util_add_attribute_uuid128(uuid128, flags, data, data_len);
    return descriptor_handler;
 }

uint8_t * att_db_util_get_address(void){
	return att_db;
}

uint16_t att_db_util_get_size(void){
	return att_db_size + 2u;	// end tag 
}

static uint8_t * att_db_util_hash_att_ptr;
static uint16_t att_db_util_hash_offset;
static uint16_t att_db_util_hash_bytes_available;

static void att_db_util_hash_fetch_next_attribute(void){
    while (true){
        uint16_t size = little_endian_read_16(att_db_util_hash_att_ptr, 0);
        btstack_assert(size != 0);
        UNUSED(size);
        uint16_t flags = little_endian_read_16(att_db_util_hash_att_ptr, 2);
        if ((flags & (uint16_t)ATT_PROPERTY_UUID128) == 0u) {
            uint16_t uuid16 = little_endian_read_16(att_db_util_hash_att_ptr, 6);
            if (att_db_util_hash_include_with_value(uuid16)){
                att_db_util_hash_offset = 4;
                att_db_util_hash_bytes_available = size - 4u;
                return;
            } else if (att_db_util_hash_include_without_value(uuid16)){
                att_db_util_hash_offset = 4;
                att_db_util_hash_bytes_available = 4;
                return;
            }
        }
        att_db_util_hash_att_ptr += size;
    }
}

uint16_t att_db_util_hash_len(void){
    return att_db_hash_len;
}

void att_db_util_hash_init(void){
    // skip version info
    att_db_util_hash_att_ptr = &att_db[1];
    att_db_util_hash_bytes_available = 0u;
}

uint8_t att_db_util_hash_get_next(void){
    // find next hashable data blob
    if (att_db_util_hash_bytes_available == 0u){
        att_db_util_hash_fetch_next_attribute();
    }

    // get next byte
    uint8_t next = att_db_util_hash_att_ptr[att_db_util_hash_offset++];
    att_db_util_hash_bytes_available--;

    // go to next attribute if blob used up
    if (att_db_util_hash_bytes_available == 0u){
        uint16_t size = little_endian_read_16(att_db_util_hash_att_ptr, 0);
        att_db_util_hash_att_ptr += size;
    }
    return next;
}

static uint8_t att_db_util_hash_get(uint16_t offset){
    UNUSED(offset);
    return att_db_util_hash_get_next();
}

void att_db_util_hash_calc(btstack_crypto_aes128_cmac_t * request, uint8_t * db_hash, void (* callback)(void * arg), void * callback_arg){
    static const uint8_t zero_key[16] = { 0 };
    att_db_util_hash_init();
    btstack_crypto_aes128_cmac_generator(request, zero_key, att_db_hash_len, &att_db_util_hash_get, db_hash, callback, callback_arg);
}
