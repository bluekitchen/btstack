/*
 * Copyright (C) 2010 by Matthias Ringwald
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
 */

/*
 *  sdp_util.c
 */

#include <btstack/sdp_util.h>
#include <btstack/utils.h>

#include <arpa/inet.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    DE_NIL = 0,
    DE_UINT,
    DE_INT,
    DE_UUID,
    DE_STRING,
    DE_BOOL,
    DE_DES,
    DE_DEA,
    DE_URL
} de_type_t;

typedef enum {
    DE_SIZE_8 = 0,
    DE_SIZE_16,
    DE_SIZE_32,
    DE_SIZE_64,
    DE_SIZE_128,
    DE_SIZE_VAR_8,
    DE_SIZE_VAR_16,
    DE_SIZE_VAR_32
} de_size_t;

// date element type names
const char *type_names[] = { "NIL", "UINT", "INT", "UUID", "STRING", "BOOL", "DES", "DEA", "URL"};

// Bluetooth Base UUID: 00000000-0000-1000-8000- 00805F9B34FB
const uint8_t sdp_bluetooth_base_uuid[] = { 0x00, 0x00, 0x00, 0x00, /* - */ 0x00, 0x00, /* - */ 0x10, 0x00, /* - */
    0x80, 0x00, /* - */ 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB };

// AttributeIDList used to remove ServiceRecordHandle
const uint8_t removeServiceRecordHandleAttributeIDList[] = { 0x36, 0x00, 0x05, 0x0A, 0x00, 0x01, 0xFF, 0xFF };

// max reserved ServiceRecordHandle
#define maxReservedServiceRecordHandle 0xffff

// our handles start after the reserve range
static uint32_t sdp_next_service_record_handle = maxReservedServiceRecordHandle + 1;

/** list of service records */
// static linked_list_t sdp_records = NULL;

// temp copy & paste from util.h/c 

// helper for SDP big endian format
#define READ_NET_16( buffer, pos) ( ((uint16_t) buffer[pos+1]) | (((uint16_t)buffer[pos  ]) << 8))
#define READ_NET_32( buffer, pos) ( ((uint32_t) buffer[pos+3]) | (((uint32_t)buffer[pos+2]) << 8) | (((uint32_t)buffer[pos+1]) << 16) | (((uint32_t) buffer[pos])) << 24)

void sdp_normalize_uuid(uint8_t *uuid, uint32_t shortUUID){
    memcpy(uuid, sdp_bluetooth_base_uuid, 16);
    net_store_32(uuid, 0, shortUUID);
}

static int de_dump_data_element(uint8_t * record);

#pragma mark DataElement getter
de_size_t de_get_size_type(uint8_t *header){
    return header[0] & 7;
}

de_type_t de_get_element_type(uint8_t *header){
    return header[0] >> 3;
}

static int de_get_header_size(uint8_t * header){
    de_size_t de_size = de_get_size_type(header);
    if (de_size <= DE_SIZE_128) {
        return 1;
    }
    return 1 + (1 << (de_size-DE_SIZE_VAR_8));
}

static int de_get_data_size(uint8_t * header){
    uint32_t result = 0;
    de_type_t de_type = de_get_element_type(header);
    de_size_t de_size = de_get_size_type(header);
    if (de_size <= DE_BOOL){
        if (de_type == 0) return 0;
        return 1 << de_size;
    }
    switch (de_size){
        case DE_SIZE_VAR_8:
            result = header[1];
            break;
        case DE_SIZE_VAR_16:
            result = READ_NET_16(header,1);
            break;
        case DE_SIZE_VAR_32:
            result = READ_NET_32(header,1);
            break;
    }
    return result;    
}

static int de_get_len(uint8_t *header){
    return de_get_header_size(header) + de_get_data_size(header); 
}

// @returns: element is valid UUID
int de_get_normalized_uuid(uint8_t *uuid128, uint8_t *element){
    de_type_t uuidType = de_get_element_type(element);
    de_size_t uuidSize = de_get_size_type(element);
    if (uuidType != DE_UUID) return 0;
    uint32_t shortUUID;
    switch (uuidSize){
        case DE_SIZE_16:
            shortUUID = READ_NET_16(element, 1);
            break;
        case DE_SIZE_32:
            shortUUID = READ_NET_32(element, 1);
            break;
        case DE_SIZE_128:
            memcpy(uuid128, element+1, 16);
            return 1;
        default:
            return 0;
    }
    sdp_normalize_uuid(uuid128, shortUUID);
    return 1;
}

// functions to create record
static void de_store_descriptor(uint8_t * header, de_type_t type, de_size_t size){
    header[0] = (type << 3) | size; 
}

static void de_store_descriptor_with_len(uint8_t * header, de_type_t type, de_size_t size, uint32_t len){
    header[0] = (type << 3) | size; 
    switch (size){
        case DE_SIZE_VAR_8:
            header[1] = len;
            break;
        case DE_SIZE_VAR_16:
            net_store_16(header, 1, len);
            break;
        case DE_SIZE_VAR_32:
            net_store_32(header, 1, len);
            break;
        default:
            break;
    }
}

#pragma mark DataElement creation
/* starts a new sequence in empty buffer - first call */
void de_create_sequence(uint8_t *header){
    de_store_descriptor_with_len( header, DE_DES, DE_SIZE_VAR_16, 0); // DES, 2 Byte Length
};

/* starts a sub-sequence, @returns handle for sub-sequence */
uint8_t * de_push_sequence(uint8_t *header){
    int element_len = de_get_len(header);
    de_store_descriptor_with_len(header+element_len, DE_DES, DE_SIZE_VAR_16, 0); // DES, 2 Byte Length
    return header + element_len;
}

/* closes the current sequence and updates the parent sequence */
void de_pop_sequence(uint8_t * parent, uint8_t * child){
    int child_len = de_get_len(child);
    int data_size_parent = READ_NET_16(parent,1);
    net_store_16(parent, 1, data_size_parent + child_len);
}

/* adds a single number value and 16+32 bit UUID to the sequence */
void de_add_number(uint8_t *seq, de_type_t type, de_size_t size, uint32_t value){
    int data_size   = READ_NET_16(seq,1);
    int element_size = 1;   // e.g. for DE_TYPE_NIL
    de_store_descriptor(seq+3+data_size, type, size); 
    switch (size){
        case DE_SIZE_8:
            if (type != DE_NIL){
                seq[4+data_size] = value;
                element_size = 2;
            }
            break;
        case DE_SIZE_16:
            net_store_16(seq, 4+data_size, value);
            element_size = 3;
            break;
        case DE_SIZE_32:
            net_store_32(seq, 4+data_size, value);
            element_size = 5;
            break;
        default:
            break;
    }
    net_store_16(seq, 1, data_size+element_size);
}

/* add a single block of data, e.g. as DE_STRING, DE_URL */
void de_add_data( uint8_t *seq, de_type_t type, uint16_t size, uint8_t *data){
    int data_size   = READ_NET_16(seq,1);
    de_store_descriptor_with_len(seq+3+data_size, type, DE_SIZE_VAR_16, size); 
    memcpy( seq + 6 + data_size, data, size);
    net_store_16(seq, 1, data_size+1+2+size);
}

void de_add_uuid128(uint8_t * seq, uint8_t * uuid){
    int data_size   = READ_NET_16(seq,1);
    de_store_descriptor(seq+3+data_size, DE_UUID, DE_SIZE_128); 
    memcpy( seq + 4 + data_size, uuid, 16);
    net_store_16(seq, 1, data_size+1+16);
}

#pragma mark DataElementSequence traversal
typedef (*de_traversal_callback_t)(uint8_t * element, de_type_t type, de_size_t size, void *context);
static void de_traverse_sequence(uint8_t * element, de_traversal_callback_t handler, void *context){
    de_type_t type = de_get_element_type(element);
    if (type != DE_DES) return;
    int pos = de_get_header_size(element);
    int end_pos = de_get_len(element);
    while (pos < end_pos){
        de_type_t elemType = de_get_element_type(element + pos);
        de_size_t elemSize = de_get_size_type(element + pos);
        uint8_t done = (*handler)(element + pos, elemType, elemSize, context); 
        if (done) break;
        pos += de_get_len(element + pos);
    }
}

#pragma mark AttributeID in AttributeIDList 
// attribute ID in AttributeIDList
// context { result, attributeID }
struct sdp_context_attributeID_search {
    int result;
    uint16_t attributeID;
};
static int sdp_traversal_attributeID_search(uint8_t * element, de_type_t type, de_size_t size, void *my_context){
    struct sdp_context_attributeID_search * context = (struct sdp_context_attributeID_search *) my_context;
    if (type != DE_UINT) return 0;
    switch (size) {
        case DE_SIZE_16:
            if (READ_NET_16(element, 1) == context->attributeID) {
                context->result = 1;
                return 1;
            }
            break;
        case DE_SIZE_32:
            if (READ_NET_16(element, 1) <= context->attributeID
            &&  context->attributeID <= READ_NET_16(element, 3)) {
                context->result = 1;
                return 1;
            }
            break;
        default:
            break;
    }
    return 0;
}
int sdp_attribute_list_constains_id(uint8_t *attributeIDList, uint16_t attributeID){
    struct sdp_context_attributeID_search attributeID_search;
    attributeID_search.result = 0;
    attributeID_search.attributeID = attributeID;
    de_traverse_sequence(attributeIDList, sdp_traversal_attributeID_search, &attributeID_search);
    return attributeID_search.result;
}

#pragma mark Append Attributes for AttributeIDList
// pre: buffer contains DES with 2 byte length field
// context: { buffer, data_size, maxBytes, attributeListID }
struct sdp_context_append_attributes {
    uint8_t * buffer;
    uint16_t dataSize;
    uint16_t maxBytes;
    uint8_t *attributeIDList;
};
static int sdp_traversal_append_attributes(uint8_t * element, de_type_t type, de_size_t size, void *my_context){
    struct sdp_context_append_attributes * context = (struct sdp_context_append_attributes *) my_context;
    // check each Attribute in root DES
    int attributeDataSize   = de_get_data_size(element);
    int attributeLen        = de_get_len(element);
    // Attribute must be DES with more than sizeof(UINT16 element)
    if (type == DE_DES && attributeDataSize >= 3){
        int attributeIDPos        = de_get_header_size(element);
        de_size_t attributeIDSize = de_get_size_type(element+attributeIDPos);
        if (attributeIDSize == DE_SIZE_16) {
            uint16_t attributeID = READ_NET_16(element, attributeIDPos+1);
            if ((context->dataSize + 3< context->maxBytes) && sdp_attribute_list_constains_id(context->attributeIDList, attributeID)){
                // copy Attribute
                memcpy( context->buffer + 3 + context->dataSize, element, attributeLen);
                context->dataSize += attributeLen;
            }
        }
    }
    return 0;
}
void sdp_append_attributes_in_attributeIDList(uint8_t *record, uint8_t *attributeIDList, uint8_t *buffer, uint16_t maxBytes){
    struct sdp_context_append_attributes context;
    context.buffer = buffer;
    context.dataSize = READ_NET_16(buffer,1);
    context.maxBytes = maxBytes;
    context.attributeIDList = attributeIDList;
    de_traverse_sequence(record, sdp_traversal_append_attributes, &context);
    net_store_16(buffer, 1, context.dataSize);
}

#pragma mark Get AttributeValue for AttributeID
// find attribute (ELEMENT) by ID
// context { attribute ptr, attributeQueryID }
struct sdp_context_attribute_by_id {
    uint16_t attributeID;
    uint8_t * attributeValue;
};
static int sdp_traversal_attribute_by_id(uint8_t * element, de_type_t attributeType, de_size_t size, void *my_context){
    struct sdp_context_attribute_by_id * context = (struct sdp_context_attribute_by_id *) my_context;

    // check each Attribute in root DES
    int attributeDataSize   = de_get_data_size(element);
    int attributeLen        = de_get_len(element);
    // Attribute must be DES with more than sizeof(UINT16 element)
    if (attributeType == DE_DES && attributeDataSize >= 3){
        int attributeIDPos        = de_get_header_size(element);
        de_size_t attributeIDSize = de_get_size_type(element+attributeIDPos);
        if (attributeIDSize == DE_SIZE_16) {
            uint16_t attributeID = READ_NET_16(element,attributeIDPos+1);
            if (context->attributeID == attributeID && attributeIDPos < attributeLen){
                context->attributeValue = element + attributeIDPos+3;
                return 1;
            }
        }
    }
    return 0;
}
uint8_t * sdp_get_attribute_value_for_attribute_id(uint8_t * record, uint16_t attributeID){
    struct sdp_context_attribute_by_id context;
    context.attributeValue = NULL;
    context.attributeID = attributeID;
    de_traverse_sequence(record, sdp_traversal_attribute_by_id, &context);
    return context.attributeValue;
}

#pragma mark ServiceRecord contains UUID
// service record contains UUID
// context { normalizedUUID }
struct sdp_context_contains_uuid128 {
    uint8_t * uuid128;
    int result;
};
int sdp_record_contains_UUID128(uint8_t *record, uint8_t *uuid128);
static int sdp_traversal_contains_UUID128(uint8_t * element, de_type_t type, de_size_t size, void *my_context){
    struct sdp_context_contains_uuid128 * context = (struct sdp_context_contains_uuid128 *) my_context;
    uint8_t normalizedUUID[16];
    uint32_t uuid;
    if (type == DE_UUID){
        uint8_t uuidOK = de_get_normalized_uuid(normalizedUUID, element);
        context->result = uuidOK && memcmp(context->uuid128, normalizedUUID, 16) == 0;
    }
    if (type == DE_DES){
        context->result = sdp_record_contains_UUID128(element, context->uuid128);
    }
    return context->result;
}
int sdp_record_contains_UUID128(uint8_t *record, uint8_t *uuid128){
    struct sdp_context_contains_uuid128 context;
    context.uuid128 = uuid128;
    context.result = 0;
    de_traverse_sequence(record, sdp_traversal_contains_UUID128, &context);
    return context.result;
}
    
#pragma mark ServiceRecord matches SearchServicePattern
// if UUID in searchServicePattern is not found in record => false
// context { result, record }
struct sdp_context_match_pattern {
    uint8_t * record;
    int result;
};
int sdp_traversal_match_pattern(uint8_t * element, de_type_t attributeType, de_size_t size, void *my_context){
    struct sdp_context_match_pattern * context = (struct sdp_context_match_pattern *) my_context;
    uint8_t normalizedUUID[16];
    uint8_t uuidOK = de_get_normalized_uuid(normalizedUUID, element);
    if (!sdp_record_contains_UUID128(context->record, normalizedUUID)){
        context->result = 0;
        return 1;
    }
    return 0;
}
int sdp_record_matches_service_search_pattern(uint8_t *record, uint8_t *serviceSearchPattern){
    struct sdp_context_match_pattern context;
    context.record = record;
    context.result = 1;
    de_traverse_sequence(record, sdp_traversal_match_pattern, &context);
    return context.result;
}

#pragma mark Dump DataElement
// context { indent }
static int de_traversal_dump_data(uint8_t * element, de_type_t de_type, de_size_t de_size, void *my_context){
    int indent = *(int*) my_context;
    int i;
    for (i=0; i<indent;i++) printf("    ");
    int pos     = de_get_header_size(element);
    int end_pos = de_get_len(element);
    printf("type %5s (%u), element len %2u ", type_names[de_type], de_type, end_pos);
    if (de_type == DE_DES) {
		printf("\n");
        indent++;
        de_traverse_sequence(element, de_traversal_dump_data, (void *)&indent);
    } else if (de_type == DE_UUID && de_size == DE_SIZE_128) {
        printf(", value: ");
        printUUID(element+1);
        printf("\n");
    } else {
        uint32_t value = 0;
        switch (de_size) {
            case DE_SIZE_8:
                if (de_type != DE_NIL){
                    value = element[pos];
                }
                break;
            case DE_SIZE_16:
				value = READ_NET_16(element,pos);
                break;
            case DE_SIZE_32:
				value = READ_NET_32(element,pos);
                break;
            default:
                break;
        }
        printf(", value: 0x%08x\n", value);
    }
    return 0;
}
int de_dump_data_element(uint8_t * record){
    int indent = 0;
    // hack to get root DES, too.
    de_type_t type = de_get_element_type(record);
    de_size_t size = de_get_size_type(record);
    de_traversal_dump_data(record, type, size, (void*) &indent);
}


uint32_t sdp_get_service_record_handle(uint8_t * record){
    uint8_t * serviceRecordHandleAttribute = sdp_get_attribute_value_for_attribute_id(record, 0x0000);
    if (!serviceRecordHandleAttribute) return 0;
    if (de_get_element_type(serviceRecordHandleAttribute) != DE_UINT) return 0;
    if (de_get_size_type(serviceRecordHandleAttribute) != DE_SIZE_32) return 0;
    return READ_NET_32(serviceRecordHandleAttribute, 1); 
}

uint32_t sdp_create_service_record_handle(){
    return sdp_next_service_record_handle++;
}

// pre: AttributeIDs are in ascencding order => ServiceRecordHandle is first attribute if present
// returns: new service record handle or 0
uint32_t sdp_add_service_record(uint8_t * record){
    
    // size of new record
    uint16_t recordSize = de_get_len(record);

    // get user record handle
    uint32_t record_handle = sdp_get_service_record_handle(record);

    // increase by ServiceRecordHandle attribute (DES UINT16 UINT32) if not set
    if (!record_handle) recordSize += 3 + 3 + 5;
    
    // alloc memory for new record
    uint8_t * newRecord = malloc(recordSize);
    if (!newRecord) return 0;
    
    // create DES for new record
    de_create_sequence(newRecord);

    // validate service record handle is not in reserved range
    if (record_handle <= maxReservedServiceRecordHandle) record_handle = 0;

    // @TODO check that it's not already in use
    
    // get new handle if needed
    if (!record_handle){
        record_handle = sdp_create_service_record_handle();
    }
    
    // set service record handle
    uint8_t * serviceRecordHandleAttribute = de_push_sequence(newRecord);
    de_add_number(serviceRecordHandleAttribute, DE_UINT, DE_SIZE_16, 0);
    de_add_number(serviceRecordHandleAttribute, DE_UINT, DE_SIZE_32, record_handle);
    de_pop_sequence(newRecord, serviceRecordHandleAttribute);
    
    // add other attributes
    sdp_append_attributes_in_attributeIDList(record, (uint8_t *) removeServiceRecordHandleAttributeIDList, newRecord, recordSize);
 
    // dump for now
    de_dump_data_element(newRecord);
    printf("calculated size %u, actual size %u\n", recordSize, de_get_len(newRecord));
    
    // add to linked list
    
    return record_handle;
}

int sdp_get_record_handles_for_service_search_pattern(uint8_t * searchServicePattern, uint16_t maxRecordCount){
    return 0;
}

#if 0

uint8_t buffer[100];
uint8_t record[100];
uint8_t attributes[100];
uint8_t serviceSearchPattern[100];
uint8_t attributeIDList[100];
int main(){

    // add all kinds of elements
    de_create_sequence(buffer);
    de_add_number(buffer, DE_NIL,  DE_SIZE_8,  0);
    de_add_number(buffer, DE_BOOL, DE_SIZE_8,  0);
    de_add_number(buffer, DE_UINT, DE_SIZE_8,  1);
    de_add_number(buffer, DE_UINT, DE_SIZE_16, 2);
    de_add_number(buffer, DE_UINT, DE_SIZE_32, 3);
    de_add_number(buffer, DE_INT,  DE_SIZE_8,  4);
    de_add_number(buffer, DE_INT,  DE_SIZE_16, 5);
    de_add_number(buffer, DE_INT,  DE_SIZE_32, 6);
    de_add_number(buffer, DE_UUID, DE_SIZE_16,  7);
    de_add_number(buffer, DE_UUID, DE_SIZE_32,  8);
    uint8_t uuid[16] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    de_add_uuid128(buffer, uuid);
    uint8_t * seq2 = de_push_sequence(buffer);
    de_add_number(seq2, DE_UINT, DE_SIZE_16, 9);
    uint8_t * seq3 = de_push_sequence(seq2);
    de_add_number(seq3, DE_UINT, DE_SIZE_16, 10);
    de_add_number(seq3, DE_UUID, DE_SIZE_32, 11);
    de_pop_sequence(seq2,seq3);
    de_pop_sequence(buffer,seq2);
    
    de_dump_data_element(buffer);
    
    // test sdp_record_contains_UUID
    uint8_t uuid128 [16];
    sdp_normalize_uuid(uuid128, 7);
    printf("Contains UUID %u: %u\n", 7, sdp_record_contains_UUID128(buffer, uuid128));
    sdp_normalize_uuid(uuid128, 9);
    printf("Contains UUID %u: %u\n", 9, sdp_record_contains_UUID128(buffer, uuid128));
    sdp_normalize_uuid(uuid128, 11);
    printf("Contains UUID %u: %u\n", 11, sdp_record_contains_UUID128(buffer, uuid128));
    
    // create attribute ID list
    de_create_sequence(attributeIDList);
    de_add_number(attributeIDList, DE_UINT, DE_SIZE_16, 10);
    de_add_number(attributeIDList, DE_UINT, DE_SIZE_32, 15 << 16 | 20);
    de_dump_data_element(attributeIDList);
    
    // test sdp_attribute_list_constains_id
    printf("Contains ID %u: %u\n", 5,  sdp_attribute_list_constains_id(attributeIDList, 5));
    printf("Contains ID %u: %u\n", 10, sdp_attribute_list_constains_id(attributeIDList, 10));
    printf("Contains ID %u: %u\n", 17, sdp_attribute_list_constains_id(attributeIDList, 17));
    
    // create test service record/attribute list
    de_create_sequence(record);
    
    seq2 = de_push_sequence(record);
    de_add_number(seq2, DE_UINT, DE_SIZE_16, 1);
    de_add_number(seq2, DE_UINT, DE_SIZE_32, 0x11111);
    de_pop_sequence(record, seq2);

    seq2 = de_push_sequence(record);
    de_add_number(seq2, DE_UINT, DE_SIZE_16, 10);
    de_add_number(seq2, DE_UUID, DE_SIZE_32, 12);
    de_pop_sequence(record, seq2);

    seq2 = de_push_sequence(record);
    de_add_number(seq2, DE_UINT, DE_SIZE_16, 17);
    de_add_number(seq2, DE_UUID, DE_SIZE_32, 13);
    de_pop_sequence(record, seq2);

    seq2 = de_push_sequence(record);
    de_add_number(seq2, DE_UINT, DE_SIZE_16, 20);
    de_add_number(seq2, DE_UUID, DE_SIZE_32, 14);
    de_pop_sequence(record, seq2);

    seq2 = de_push_sequence(record);
    de_add_number(seq2, DE_UINT, DE_SIZE_16, 22);
    de_add_number(seq2, DE_UUID, DE_SIZE_32, 15);
    de_pop_sequence(record, seq2);

    de_dump_data_element(record);
    de_create_sequence(attributes);
    sdp_append_attributes_in_attributeIDList(record, attributeIDList, attributes, sizeof(attributes));
    de_dump_data_element(attributes);

    // test sdp_get_service_record_handle
    printf("Service Record Handle (att id 0) = %08x\n", sdp_get_service_record_handle(record));

    // test sdp_record_matches_service_search_pattern
    de_create_sequence(serviceSearchPattern);
    de_add_number(serviceSearchPattern, DE_UUID, DE_SIZE_16, 12);
    de_add_number(serviceSearchPattern, DE_UUID, DE_SIZE_32, 13);
    de_dump_data_element(serviceSearchPattern);
    printf("service search pattern matches: %u\n", sdp_record_matches_service_search_pattern(record, serviceSearchPattern)); 
    de_add_number(serviceSearchPattern, DE_UUID, DE_SIZE_16, 66);
    printf("service search pattern matches: %u\n", sdp_record_matches_service_search_pattern(record, serviceSearchPattern)); 
    
    // implement list of records
    // test sdp_get_record_handles_for_service_search_pattern
    
    uint32_t handle = sdp_add_service_record(record);
    printf("new handle %08x \n", handle);
}

#endif