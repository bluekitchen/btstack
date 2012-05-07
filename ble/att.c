/*
 * Copyright (C) 2011-2012 by Matthias Ringwald
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

#include <stdio.h>
#include <string.h>

#include "att.h"

// from src/utils.
#define READ_BT_16( buffer, pos) ( ((uint16_t) buffer[pos]) | (((uint16_t)buffer[pos+1]) << 8))

// Buetooth Base UUID 00000000-0000-1000-8000-00805F9B34FB in little endian
static const uint8_t bluetooth_base_uuid[] = { 0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static void bt_store_16(uint8_t *buffer, uint16_t pos, uint16_t value){
    buffer[pos++] = value;
    buffer[pos++] = value >> 8;
}

static void hexdump2(void const *data, int size){
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

static void printUUID128(const uint8_t * uuid){
    int i;
    for (i=15; i >= 0 ; i--){
        printf("%02X", uuid[i]);
        switch (i){
            case 4:
            case 6:
            case 8:
            case 10:
                printf("-");
                break;
            default:
                break;
        }
    }
}

static int is_Bluetooth_Base_UUID(uint8_t const *uuid){
    if (memcmp(&uuid[0],  &bluetooth_base_uuid[0], 12)) return 0;
    if (memcmp(&uuid[14], &bluetooth_base_uuid[14], 2)) return 0;
    return 1;
    
}

// ATT Database
static uint8_t const * att_db = NULL;
static att_read_callback_t  att_read_callback  = NULL;
static att_write_callback_t att_write_callback = NULL;

// new java-style iterator
typedef struct att_iterator {
    // private
    uint8_t const * att_ptr;
    // public
    uint16_t size;
    uint16_t flags;
    uint16_t handle;
    uint8_t  const * uuid;
    uint16_t value_len;
    uint8_t  const * value;
} att_iterator_t;

void att_iterator_init(att_iterator_t *it){
    it->att_ptr = att_db;
}

int att_iterator_has_next(att_iterator_t *it){
    return it->att_ptr != NULL;
}

void att_iterator_fetch_next(att_iterator_t *it){
    it->size   = READ_BT_16(it->att_ptr, 0);
    if (it->size == 0){
        it->flags = 0;
        it->handle = 0;
        it->uuid = NULL;
        it->value_len = 0;
        it->value = NULL;
        it->att_ptr = NULL;
        return;
    }
    it->flags  = READ_BT_16(it->att_ptr, 2);
    it->handle = READ_BT_16(it->att_ptr, 4);
    it->uuid   = &it->att_ptr[6];
    // handle 128 bit UUIDs
    if (it->flags & ATT_PROPERTY_UUID128){
        it->value_len = it->size - 22;
        it->value  = &it->att_ptr[22];
    } else {
        it->value_len = it->size - 8;
        it->value  = &it->att_ptr[8];
    }
    // advance AFTER setting values
    it->att_ptr += it->size;
}

int att_iterator_match_uuid16(att_iterator_t *it, uint16_t uuid){
    if (it->handle == 0) return 0;
    if (it->flags & ATT_PROPERTY_UUID128){
        if (!is_Bluetooth_Base_UUID(it->uuid)) return 0;
        return READ_BT_16(it->uuid, 12) == uuid;
    }
    return READ_BT_16(it->uuid, 0)  == uuid;
}

int att_iterator_match_uuid(att_iterator_t *it, uint8_t *uuid, uint16_t uuid_len){
    if (it->handle == 0) return 0;
    // input: UUID16
    if (uuid_len == 2) {
        return att_iterator_match_uuid16(it, READ_BT_16(uuid, 0));
    }
    // input and db: UUID128 
    if (it->flags & ATT_PROPERTY_UUID128){
        return memcmp(it->uuid, uuid, 16) == 0;
    }
    // input: UUID128, db: UUID16
    if (!is_Bluetooth_Base_UUID(uuid)) return 0;
    return READ_BT_16(uuid, 12) == READ_BT_16(it->uuid, 0);
}


int att_find_handle(att_iterator_t *it, uint16_t handle){
    att_iterator_init(it);
    while (att_iterator_has_next(it)){
        att_iterator_fetch_next(it);
        if (it->handle != handle) continue;
        return 1;
    }
    return 0;
}

static void att_update_value_len(att_iterator_t *it){
    if ((it->flags & ATT_PROPERTY_DYNAMIC) == 0 || !att_read_callback) return;
    it->value_len = (*att_read_callback)(it->handle, 0, NULL, 0);
    return;
}

static int att_copy_value(att_iterator_t *it, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    
    // DYNAMIC 
    if ((it->flags & ATT_PROPERTY_DYNAMIC) && att_read_callback) {
        return (*att_read_callback)(it->handle, offset, buffer, buffer_size);
    }
    
    // STATIC
    uint16_t bytes_to_copy = it->value_len;
    if (bytes_to_copy > buffer_size){
        bytes_to_copy = buffer_size;
    }
    memcpy(buffer, it->value, bytes_to_copy);
    return bytes_to_copy;
}

void att_set_db(uint8_t const * db){
    att_db = db;
}

void att_set_read_callback(att_read_callback_t callback){
    att_read_callback = callback;
}

void att_set_write_callback(att_write_callback_t callback){
    att_write_callback = callback;
}

void att_dump_attributes(void){
    att_iterator_t it;
    att_iterator_init(&it);
    while (att_iterator_has_next(&it)){
        att_iterator_fetch_next(&it);
        if (it.handle == 0) {
            printf("Handle: END\n");
            return;
        }
        printf("Handle: 0x%04x, flags: 0x%04x, uuid: ", it.handle, it.flags);
        if (it.flags & ATT_PROPERTY_UUID128){
            printUUID128(it.uuid);
        } else {
            printf("%04x", READ_BT_16(it.uuid, 0));
        }
        printf(", value_len: %u, value: ", it.value_len);
        hexdump2(it.value, it.value_len);
    }
}

static uint16_t setup_error(uint8_t * response_buffer, uint16_t request, uint16_t handle, uint8_t error_code){
    response_buffer[0] = ATT_ERROR_RESPONSE;
    response_buffer[1] = request;
    bt_store_16(response_buffer, 2, handle);
    response_buffer[4] = error_code;
    return 5;
}

static uint16_t setup_error_atribute_not_found(uint8_t * response_buffer, uint16_t request, uint16_t start_handle){
    return setup_error(response_buffer, request, start_handle, ATT_ERROR_ATTRIBUTE_NOT_FOUND);
}

static uint16_t setup_error_invalid_handle(uint8_t * response_buffer, uint16_t request, uint16_t handle){
    return setup_error(response_buffer, request, handle, ATT_ERROR_ATTRIBUTE_INVALID);
}

static uint16_t setup_error_invalid_offset(uint8_t * response_buffer, uint16_t request, uint16_t handle){
    return setup_error(response_buffer, request, handle, ATT_ERROR_INVALID_OFFSET);
}

//
// MARK: ATT_EXCHANGE_MTU_REQUEST
//
static uint16_t handle_exchange_mtu_request(att_connection_t * att_connection, uint8_t * request_buffer,  uint16_t request_len,
                                         uint8_t * response_buffer){

    uint16_t client_rx_mtu = READ_BT_16(request_buffer, 1);
    if (client_rx_mtu < att_connection->mtu){
        att_connection->mtu = client_rx_mtu;
    }
    
    response_buffer[0] = ATT_EXCHANGE_MTU_RESPONSE;
    bt_store_16(response_buffer, 1, att_connection->mtu);
    return 3;
}


//
// MARK: ATT_FIND_INFORMATION_REQUEST
//
// TODO: handle other types then GATT_PRIMARY_SERVICE_UUID and GATT_SECONDARY_SERVICE_UUID
//
static uint16_t handle_find_information_request2(uint8_t * response_buffer, uint16_t response_buffer_size,
                                           uint16_t start_handle, uint16_t end_handle){
    
    printf("ATT_FIND_INFORMATION_REQUEST: from %04X to %04X\n", start_handle, end_handle);
    
    uint16_t offset   = 1;
    uint16_t pair_len = 0;
    
    att_iterator_t it;
    att_iterator_init(&it);
    while (att_iterator_has_next(&it)){
        att_iterator_fetch_next(&it);
        if (!it.handle) break;
        if (it.handle > end_handle) break;
        if (it.handle < start_handle) continue;
        
        att_update_value_len(&it);
        
        // printf("Handle 0x%04x\n", it.handle);
        
        // check if value has same len as last one
        uint16_t this_pair_len = 2 + it.value_len;
        if (offset > 1){
            if (pair_len != this_pair_len) {
                break;
            }
        }
        
        // first
        if (offset == 1) {
            pair_len = this_pair_len;
            if (it.value_len == 2) {
                response_buffer[offset] = 0x01; // format
            } else {
                response_buffer[offset] = 0x02;
            }
            offset++;
        }
        
        // space?
        if (offset + pair_len > response_buffer_size) {
            if (offset > 2) break;
            it.value_len = response_buffer_size - 4;
        }
        
        // store
        bt_store_16(response_buffer, offset, it.handle);
        offset += 2;
        uint16_t bytes_copied = att_copy_value(&it, 0, response_buffer + offset, it.value_len);
        offset += bytes_copied;
    }
    
    if (offset == 1){
        return setup_error_atribute_not_found(response_buffer, ATT_FIND_INFORMATION_REQUEST, start_handle);
    }
    
    response_buffer[0] = ATT_FIND_INFORMATION_REPLY;
    return offset;
}

static uint16_t handle_find_information_request(uint8_t * request_buffer,  uint16_t request_len,
                                         uint8_t * response_buffer, uint16_t response_buffer_size){
    return handle_find_information_request2(response_buffer, response_buffer_size, READ_BT_16(request_buffer, 1), READ_BT_16(request_buffer, 3));
}

//
// MARK: ATT_FIND_BY_TYPE_VALUE
//
// "Only attributes with attribute handles between and including the Starting Handle parameter
// and the Ending Handle parameter that match the requested attri- bute type and the attribute
// value that have sufficient permissions to allow reading will be returned" -> (1)
//
// TODO: handle other types then GATT_PRIMARY_SERVICE_UUID and GATT_SECONDARY_SERVICE_UUID
//
// NOTE: doesn't handle DYNAMIC values
// NOTE: only supports 16 bit UUIDs
// 
static uint16_t handle_find_by_type_value_request2(uint8_t * response_buffer, uint16_t response_buffer_size,
                                           uint16_t start_handle, uint16_t end_handle,
                                           uint16_t attribute_type, uint16_t attribute_len, uint8_t* attribute_value){
    
    printf("ATT_FIND_BY_TYPE_VALUE_REQUEST: from %04X to %04X, type %04X, value: ", start_handle, end_handle, attribute_type);
    hexdump2(attribute_value, attribute_len);
    
    uint16_t offset      = 1;
    uint16_t in_group    = 0;
    uint16_t prev_handle = 0;
    
    att_iterator_t it;
    att_iterator_init(&it);
    while (att_iterator_has_next(&it)){
        att_iterator_fetch_next(&it);
        
        if (it.handle && it.handle < start_handle) continue;
        if (it.handle > end_handle) break;  // (1)
        
        // close current tag, if within a group and a new service definition starts or we reach end of att db
        if (in_group &&
            (it.handle == 0 || att_iterator_match_uuid16(&it, GATT_PRIMARY_SERVICE_UUID) || att_iterator_match_uuid16(&it, GATT_SECONDARY_SERVICE_UUID))){
            
            printf("End of group, handle 0x%04x\n", prev_handle);
            bt_store_16(response_buffer, offset, prev_handle);
            offset += 2;
            in_group = 0;
            
            // check if space for another handle pair available
            if (offset + 4 > response_buffer_size){
                break;
            }
        }
        
        // keep track of previous handle
        prev_handle = it.handle;
        
        // does current attribute match
        if (it.handle && att_iterator_match_uuid16(&it, attribute_type) && attribute_len == it.value_len && memcmp(attribute_value, it.value, it.value_len) == 0){
            printf("Begin of group, handle 0x%04x\n", it.handle);
            bt_store_16(response_buffer, offset, it.handle);
            offset += 2;
            in_group = 1;
        }
    }
    
    if (offset == 1){
        return setup_error_atribute_not_found(response_buffer, ATT_FIND_BY_TYPE_VALUE_REQUEST, start_handle);
    }
    
    response_buffer[0] = ATT_FIND_BY_TYPE_VALUE_RESPONSE;
    return offset;
}
                                         
static uint16_t handle_find_by_type_value_request(uint8_t * request_buffer,  uint16_t request_len,
                                           uint8_t * response_buffer, uint16_t response_buffer_size){
    int attribute_len = request_len - 7;
    return handle_find_by_type_value_request2(response_buffer, response_buffer_size, READ_BT_16(request_buffer, 1),
                                              READ_BT_16(request_buffer, 3), READ_BT_16(request_buffer, 5), attribute_len, &request_buffer[7]);
}
                                                                                  
//
// MARK: ATT_READ_BY_TYPE_REQUEST
//
static uint16_t handle_read_by_type_request2(uint8_t * response_buffer, uint16_t response_buffer_size,
                                      uint16_t start_handle, uint16_t end_handle,
                                      uint16_t attribute_type_len, uint8_t * attribute_type){
    
    printf("ATT_READ_BY_TYPE_REQUEST: from %04X to %04X, type: ", start_handle, end_handle); 
    hexdump2(attribute_type, attribute_type_len);
    
    uint16_t offset   = 1;
    uint16_t pair_len = 0;

    att_iterator_t it;
    att_iterator_init(&it);
    while (att_iterator_has_next(&it)){
        att_iterator_fetch_next(&it);
        
        if (!it.handle) break;
        if (it.handle < start_handle) continue;
        if (it.handle > end_handle) break;  // (1)

        // does current attribute match
        if (!att_iterator_match_uuid(&it, attribute_type, attribute_type_len)) continue;
        
        att_update_value_len(&it);
        
        // check if value has same len as last one
        uint16_t this_pair_len = 2 + it.value_len;
        if (offset > 1){
            if (pair_len != this_pair_len) {
                break;
            }
        }
        
        // first
        if (offset == 1) {
            pair_len = this_pair_len;
            response_buffer[offset] = pair_len;
            offset++;
        }
        
        // space?
        if (offset + pair_len > response_buffer_size) {
            if (offset > 2) break;
            it.value_len = response_buffer_size - 4;
        }
        
        // store
        bt_store_16(response_buffer, offset, it.handle);
        offset += 2;
        uint16_t bytes_copied = att_copy_value(&it, 0, response_buffer + offset, it.value_len);
        offset += bytes_copied;
    }
    
    if (offset == 1){
        return setup_error_atribute_not_found(response_buffer, ATT_READ_BY_TYPE_REQUEST, start_handle);
    }
    
    response_buffer[0] = ATT_READ_BY_TYPE_RESPONSE;
    return offset;
}

static uint16_t handle_read_by_type_request(uint8_t * request_buffer,  uint16_t request_len,
                                     uint8_t * response_buffer, uint16_t response_buffer_size){
    int attribute_type_len;
    if (request_len <= 7){
        attribute_type_len = 2;
    } else {
        attribute_type_len = 16;
    }
    return handle_read_by_type_request2(response_buffer, response_buffer_size, READ_BT_16(request_buffer, 1), READ_BT_16(request_buffer, 3), attribute_type_len, &request_buffer[5]);
}

//
// MARK: ATT_READ_BY_TYPE_REQUEST
//
static uint16_t handle_read_request2(uint8_t * response_buffer, uint16_t response_buffer_size, uint16_t handle){
    
    printf("ATT_READ_REQUEST: handle %04x\n", handle);
    
    att_iterator_t it;
    int ok = att_find_handle(&it, handle);
    if (!ok){
        return setup_error_atribute_not_found(response_buffer, ATT_READ_REQUEST, handle);
    }
    
    att_update_value_len(&it);

    uint16_t offset   = 1;
    // limit data
    if (offset + it.value_len > response_buffer_size) {
        it.value_len = response_buffer_size - 1;
    }
    
    // store
    uint16_t bytes_copied = att_copy_value(&it, 0, response_buffer + offset, it.value_len);
    offset += bytes_copied;
    
    response_buffer[0] = ATT_READ_RESPONSE;
    return offset;
}

static uint16_t handle_read_request(uint8_t * request_buffer,  uint16_t request_len,
                             uint8_t * response_buffer, uint16_t response_buffer_size){
    return handle_read_request2(response_buffer, response_buffer_size, READ_BT_16(request_buffer, 1));
}

//
// MARK: ATT_READ_BLOB_REQUEST 0x0c
//
static uint16_t handle_read_blob_request2(uint8_t * response_buffer, uint16_t response_buffer_size, uint16_t handle, uint16_t value_offset){
    printf("ATT_READ_BLOB_REQUEST: handle %04x, offset %u\n", handle, value_offset);

    att_iterator_t it;
    int ok = att_find_handle(&it, handle);
    if (!ok){
        return setup_error_atribute_not_found(response_buffer, ATT_READ_BLOB_REQUEST, handle);
    }
    
    att_update_value_len(&it);

    if (value_offset >= it.value_len){
        return setup_error_invalid_offset(response_buffer, ATT_READ_BLOB_REQUEST, handle);
    }
    
    // limit data
    uint16_t offset   = 1;
    if (offset + it.value_len - value_offset > response_buffer_size) {
        it.value_len = response_buffer_size - 1 + value_offset;
    }
    
    // store
    uint16_t bytes_copied = att_copy_value(&it, value_offset, response_buffer + offset, it.value_len - value_offset);
    offset += bytes_copied;
    
    response_buffer[0] = ATT_READ_BLOB_RESPONSE;
    return offset;
}

uint16_t handle_read_blob_request(uint8_t * request_buffer,  uint16_t request_len,
                                  uint8_t * response_buffer, uint16_t response_buffer_size){
    return handle_read_blob_request2(response_buffer, response_buffer_size, READ_BT_16(request_buffer, 1), READ_BT_16(request_buffer, 3));
}

//
// MARK: ATT_READ_MULTIPLE_REQUEST 0x0e
//
static uint16_t handle_read_multiple_request2(uint8_t * response_buffer, uint16_t response_buffer_size, uint16_t num_handles, uint16_t * handles){
    printf("ATT_READ_MULTIPLE_REQUEST: num handles %u\n", num_handles);
    
    uint16_t offset   = 1;

    int i;
    for (i=0;i<num_handles;i++){
        uint16_t handle = handles[i];
        
        if (handle == 0){
            return setup_error_invalid_handle(response_buffer, ATT_READ_MULTIPLE_REQUEST, handle);
        }
        
        att_iterator_t it;

        int ok = att_find_handle(&it, handle);
        if (!ok){
            return setup_error_invalid_handle(response_buffer, ATT_READ_MULTIPLE_REQUEST, handle);
        }

        att_update_value_len(&it);
        
        // limit data
        if (offset + it.value_len > response_buffer_size) {
            it.value_len = response_buffer_size - 1;
        }
        
        // store
        uint16_t bytes_copied = att_copy_value(&it, 0, response_buffer + offset, it.value_len);
        offset += bytes_copied;
    }
    
    response_buffer[0] = ATT_READ_MULTIPLE_RESPONSE;
    return offset;
}
uint16_t handle_read_multiple_request(uint8_t * request_buffer,  uint16_t request_len,
                                      uint8_t * response_buffer, uint16_t response_buffer_size){
    int num_handles = (request_len - 1) >> 1;
    return handle_read_multiple_request2(response_buffer, response_buffer_size, num_handles, (uint16_t*) &request_buffer[1]);
}

//
// MARK: ATT_READ_BY_GROUP_TYPE_REQUEST 0x10
//
// TODO: handle other types then GATT_PRIMARY_SERVICE_UUID and GATT_SECONDARY_SERVICE_UUID
//
// NOTE: doesn't handle DYNAMIC values
//
static uint16_t handle_read_by_group_type_request2(uint8_t * response_buffer, uint16_t response_buffer_size,
                                            uint16_t start_handle, uint16_t end_handle,
                                            uint16_t attribute_type_len, uint8_t * attribute_type){
    
    printf("ATT_READ_BY_GROUP_TYPE_REQUEST: from %04X to %04X, buffer size %u, type: ", start_handle, end_handle, response_buffer_size);
    hexdump2(attribute_type, attribute_type_len);
    
    uint16_t offset   = 1;
    uint16_t pair_len = 0;
    uint16_t in_group = 0;
    uint16_t group_start_handle = 0;
    uint8_t const * group_start_value = NULL;
    uint16_t prev_handle = 0;

    att_iterator_t it;
    att_iterator_init(&it);
    while (att_iterator_has_next(&it)){
        att_iterator_fetch_next(&it);
        
        if (it.handle && it.handle < start_handle) continue;
        if (it.handle > end_handle) break;  // (1)
        
        // close current tag, if within a group and a new service definition starts or we reach end of att db
        if (in_group &&
            (it.handle == 0 || att_iterator_match_uuid16(&it, GATT_PRIMARY_SERVICE_UUID) || att_iterator_match_uuid16(&it, GATT_SECONDARY_SERVICE_UUID))){
            // TODO: check if handle is included in start/end range
            // printf("End of group, handle 0x%04x, val_len: %u\n", prev_handle, pair_len - 4);
            
            bt_store_16(response_buffer, offset, group_start_handle);
            offset += 2;
            bt_store_16(response_buffer, offset, prev_handle);
            offset += 2;
            memcpy(response_buffer + offset, group_start_value, pair_len - 4);
            offset += pair_len - 4;
            in_group = 0;
            
            // check if space for another handle pair available
            if (offset + pair_len > response_buffer_size){
                break;
            }
        }
        
        // keep track of previous handle
        prev_handle = it.handle;
        
        // does current attribute match
        // printf("compare: %04x == %04x\n", *(uint16_t*) context->attribute_type, *(uint16_t*) uuid);
        if (it.handle && att_iterator_match_uuid(&it, attribute_type, attribute_type_len)) {
            
            // check if value has same len as last one
            uint16_t this_pair_len = 4 + it.value_len;
            if (offset > 1){
                if (this_pair_len != pair_len) {
                    break;
                }
            }
            
            // printf("Begin of group, handle 0x%04x\n", it.handle);
            
            // first
            if (offset == 1) {
                pair_len = this_pair_len;
                response_buffer[offset] = this_pair_len;
                offset++;
            }
            
            group_start_handle = it.handle;
            group_start_value  = it.value;
            in_group = 1;
        }
    }        
    
    if (offset == 1){
        return setup_error_atribute_not_found(response_buffer, ATT_READ_BY_GROUP_TYPE_REQUEST, start_handle);
    }
    
    response_buffer[0] = ATT_READ_BY_GROUP_TYPE_RESPONSE;
    return offset;
}
uint16_t handle_read_by_group_type_request(uint8_t * request_buffer,  uint16_t request_len,
                                           uint8_t * response_buffer, uint16_t response_buffer_size){
    int attribute_type_len;
    if (request_len <= 7){
        attribute_type_len = 2;
    } else {
        attribute_type_len = 16;
    }
    return handle_read_by_group_type_request2(response_buffer, response_buffer_size, READ_BT_16(request_buffer, 1), READ_BT_16(request_buffer, 3), attribute_type_len, &request_buffer[5]);
}

//
// MARK: ATT_WRITE_REQUEST 0x12
static uint16_t handle_write_request(uint8_t * request_buffer,  uint16_t request_len,
                              uint8_t * response_buffer, uint16_t response_buffer_size){
    uint16_t handle = READ_BT_16(request_buffer, 1);
    if (!att_write_callback) {
        // TODO: Use "Write Not Permitted"
        return setup_error_atribute_not_found(response_buffer, ATT_WRITE_REQUEST, handle);
    }
    att_iterator_t it;
    int ok = att_find_handle(&it, handle);
    if (!ok) {
        return setup_error_atribute_not_found(response_buffer, ATT_WRITE_REQUEST, handle);
    }
    if ((it.flags & ATT_PROPERTY_DYNAMIC) == 0) {
        // TODO: Use "Write Not Permitted"
        return setup_error_atribute_not_found(response_buffer, ATT_WRITE_REQUEST, handle);
    }
    (*att_write_callback)(handle, ATT_TRANSACTION_MODE_NONE, 0, request_buffer + 3, request_len - 3, NULL);
    response_buffer[0] = ATT_WRITE_RESPONSE;
    return 1;
}

//
// MARK: ATT_PREPARE_WRITE_REQUEST 0x16
static uint16_t handle_prepare_write_request(uint8_t * request_buffer,  uint16_t request_len,
                                      uint8_t * response_buffer, uint16_t response_buffer_size){
    uint16_t handle = READ_BT_16(request_buffer, 1);
    if (!att_write_callback) {
        // TODO: Use "Write Not Permitted"
        return setup_error_atribute_not_found(response_buffer, ATT_PREPARE_WRITE_REQUEST, handle);
    }
    att_iterator_t it;
    int ok = att_find_handle(&it, handle);
    if (!ok) {
        return setup_error_atribute_not_found(response_buffer, ATT_WRITE_REQUEST, handle);
    }
    if ((it.flags & ATT_PROPERTY_DYNAMIC) == 0) {
        // TODO: Use "Write Not Permitted"
        return setup_error_atribute_not_found(response_buffer, ATT_WRITE_REQUEST, handle);
    }
    (*att_write_callback)(handle, ATT_TRANSACTION_MODE_ACTIVE, 0, request_buffer + 3, request_len - 3, NULL);
    
    // response: echo request
    memcpy(response_buffer, request_buffer, request_len);
    response_buffer[0] = ATT_PREPARE_WRITE_RESPONSE;
    return request_len;
}

// MARK: ATT_EXECUTE_WRITE_REQUEST 0x18
static uint16_t handle_execute_write_request(uint8_t * request_buffer,  uint16_t request_len,
                                      uint8_t * response_buffer, uint16_t response_buffer_size){
    if (!att_write_callback) {
        // TODO: Use "Write Not Permitted"
        return setup_error_atribute_not_found(response_buffer, ATT_EXECUTE_WRITE_REQUEST, 0);
    }
    if (request_buffer[1]) {
        (*att_write_callback)(0, ATT_TRANSACTION_MODE_EXECUTE, 0, request_buffer + 3, request_len - 3, NULL);
    } else {
        (*att_write_callback)(0, ATT_TRANSACTION_MODE_CANCEL, 0, request_buffer + 3, request_len - 3, NULL);
    }
    response_buffer[0] = ATT_EXECUTE_WRITE_RESPONSE;
    return 1;
}

// MARK: ATT_WRITE_COMMAND 0x52
static void handle_write_command(uint8_t * request_buffer,  uint16_t request_len,
                                           uint8_t * response_buffer, uint16_t response_buffer_size){
    if (!att_write_callback) return;
    uint16_t handle = READ_BT_16(request_buffer, 1);
    att_iterator_t it;
    int ok = att_find_handle(&it, handle);
    if (!ok) return;
    if ((it.flags & ATT_PROPERTY_DYNAMIC) == 0) return;
    (*att_write_callback)(handle, ATT_TRANSACTION_MODE_NONE, 0, request_buffer + 3, request_len - 3, NULL);
}

// MARK: ATT_SIGNED_WRITE_COMAND 0xD2
static void handle_signed_write_command(uint8_t * request_buffer,  uint16_t request_len,
                                 uint8_t * response_buffer, uint16_t response_buffer_size){

    if (request_len < 15) return;
    if (!att_write_callback) return;
    uint16_t handle = READ_BT_16(request_buffer, 1);
    att_iterator_t it;
    int ok = att_find_handle(&it, handle);
    if (!ok) return;
    if ((it.flags & ATT_PROPERTY_DYNAMIC) == 0) return;
    (*att_write_callback)(handle, ATT_TRANSACTION_MODE_NONE, 0, request_buffer + 3, request_len - 3 - 12, (signature_t *) request_buffer + request_len - 12);
}

// MARK: helper for ATT_HANDLE_VALUE_NOTIFICATION and ATT_HANDLE_VALUE_INDICATION
static uint16_t prepare_handle_value(att_connection_t * att_connection,
                                     uint16_t handle,
                                     uint8_t *value,
                                     uint16_t value_len, 
                                     uint8_t * response_buffer){
    bt_store_16(response_buffer, 1, handle);
    if (value_len > att_connection->mtu - 3){
        value_len = att_connection->mtu - 3;
    }
    memcpy(&response_buffer[3], value, value_len);
    return value_len + 3;
}

// MARK: ATT_HANDLE_VALUE_NOTIFICATION 0x1b
uint16_t att_prepare_handle_value_notification(att_connection_t * att_connection,
                                               uint16_t handle,
                                               uint8_t *value,
                                               uint16_t value_len, 
                                               uint8_t * response_buffer){

    response_buffer[0] = ATT_HANDLE_VALUE_NOTIFICATION;
    return prepare_handle_value(att_connection, handle, value, value_len, response_buffer);
}

// MARK: ATT_HANDLE_VALUE_INDICATION 0x1d
uint16_t att_prepare_handle_value_indication(att_connection_t * att_connection,
                                             uint16_t handle,
                                             uint8_t *value,
                                             uint16_t value_len, 
                                             uint8_t * response_buffer){

    response_buffer[0] = ATT_HANDLE_VALUE_INDICATION;
    return prepare_handle_value(att_connection, handle, value, value_len, response_buffer);
}
    
// MARK: Dispatcher
uint16_t att_handle_request(att_connection_t * att_connection,
                            uint8_t * request_buffer,
                            uint16_t request_len,
                            uint8_t * response_buffer){
    uint16_t response_len = 0;
    uint16_t response_buffer_size = att_connection->mtu;
    
    switch (request_buffer[0]){
        case ATT_EXCHANGE_MTU_REQUEST:
            response_len = handle_exchange_mtu_request(att_connection, request_buffer, request_len, response_buffer);
            break;
        case ATT_FIND_INFORMATION_REQUEST:
            response_len = handle_find_information_request(request_buffer, request_len,response_buffer, response_buffer_size);
            break;
        case ATT_FIND_BY_TYPE_VALUE_REQUEST:
            response_len = handle_find_by_type_value_request(request_buffer, request_len, response_buffer, response_buffer_size);
            break;
        case ATT_READ_BY_TYPE_REQUEST:  
            response_len = handle_read_by_type_request(request_buffer, request_len, response_buffer, response_buffer_size);
            break;
        case ATT_READ_REQUEST:  
            response_len = handle_read_request(request_buffer, request_len, response_buffer, response_buffer_size);
            break;
        case ATT_READ_BLOB_REQUEST:  
            response_len = handle_read_blob_request(request_buffer, request_len, response_buffer, response_buffer_size);
            break;
        case ATT_READ_MULTIPLE_REQUEST:  
            response_len = handle_read_multiple_request(request_buffer, request_len, response_buffer, response_buffer_size);
            break;
        case ATT_READ_BY_GROUP_TYPE_REQUEST:  
            response_len = handle_read_by_group_type_request(request_buffer, request_len, response_buffer, response_buffer_size);
            break;
        case ATT_WRITE_REQUEST:
            response_len = handle_write_request(request_buffer, request_len, response_buffer, response_buffer_size);
            break;
        case ATT_PREPARE_WRITE_REQUEST:
            response_len = handle_prepare_write_request(request_buffer, request_len, response_buffer, response_buffer_size);
            break;
        case ATT_EXECUTE_WRITE_REQUEST:
            response_len = handle_execute_write_request(request_buffer, request_len, response_buffer, response_buffer_size);
            break;
        case ATT_WRITE_COMMAND:
            handle_write_command(request_buffer, request_len, response_buffer, response_buffer_size);
            break;
        case ATT_SIGNED_WRITE_COMAND:
            handle_signed_write_command(request_buffer, request_len, response_buffer, response_buffer_size);
            break;
        default:
            printf("Unhandled ATT Command: %02X, DATA: ", request_buffer[0]);
            hexdump2(&request_buffer[9], request_len-9);
            break;
    }
    return response_len;
}

#if 0

// test profile
#include "profile.h"

int main(){
    int acl_buffer_size;
    uint8_t acl_buffer[27];
    att_set_db(profile_data);
    att_dump_attributes();

    uint8_t uuid_1[] = { 0x00, 0x18};
    acl_buffer_size = handle_find_by_type_value_request2(acl_buffer, 19, 0, 0xffff, 0x2800, 2, (uint8_t *) &uuid_1);
    hexdump2(acl_buffer, acl_buffer_size);
    
    uint8_t uuid_3[] = { 0x00, 0x2a};
    acl_buffer_size = handle_read_by_type_request2(acl_buffer, 19, 0, 0xffff, 2, (uint8_t *) &uuid_3);
    hexdump2(acl_buffer, acl_buffer_size);
        
    acl_buffer_size = handle_find_by_type_value_request2(acl_buffer, 19, 0, 0xffff, 0x2800, 2, (uint8_t *) &uuid_1);
    hexdump2(acl_buffer, acl_buffer_size);

    uint8_t uuid_4[] = { 0x00, 0x28};
    acl_buffer_size = handle_read_by_group_type_request2(acl_buffer, 20, 0, 0xffff, 2, (uint8_t *) &uuid_4);
    hexdump2(acl_buffer, acl_buffer_size);
    
    acl_buffer_size = handle_find_information_request2(acl_buffer, 20, 0, 0xffff);
    hexdump2(acl_buffer, acl_buffer_size);
    acl_buffer_size = handle_find_information_request2(acl_buffer, 20, 3, 0xffff);
    hexdump2(acl_buffer, acl_buffer_size);
    acl_buffer_size = handle_find_information_request2(acl_buffer, 20, 5, 0xffff);
    hexdump2(acl_buffer, acl_buffer_size);

    acl_buffer_size = handle_read_request2(acl_buffer, 19, 0x0003);
    hexdump2(acl_buffer, acl_buffer_size);

    return 0;
}
#endif
