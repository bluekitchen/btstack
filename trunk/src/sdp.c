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
 * Implementation of the Service Discovery Protocol Server 
 */

#include "sdp.h"

#include <stdio.h>
#include <string.h>

#include <btstack/linked_list.h>
#include <btstack/sdp_util.h>
#include "l2cap.h"

// max reserved ServiceRecordHandle
#define maxReservedServiceRecordHandle 0xffff

// service record
// -- uses user_data field for actual
typedef struct {
    // linked list - assert: first field
    linked_item_t   item;

    // client connection
    connection_t *  connection;
    
    // data is contained in same memory
    uint32_t        service_record_handle;
    uint8_t         service_record[0];
} service_record_item_t;

static void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// registered service records
linked_list_t sdp_service_records;

// our handles start after the reserved range
static uint32_t sdp_next_service_record_handle = maxReservedServiceRecordHandle + 2;

// AttributeIDList used to remove ServiceRecordHandle
const uint8_t removeServiceRecordHandleAttributeIDList[] = { 0x36, 0x00, 0x05, 0x0A, 0x00, 0x01, 0xFF, 0xFF };

void sdp_init(){
    // register with l2cap psm sevices
    l2cap_register_service_internal(NULL, sdp_packet_handler, PSM_SDP, 250);
}

uint32_t sdp_get_service_record_handle(uint8_t * record){
    uint8_t * serviceRecordHandleAttribute = sdp_get_attribute_value_for_attribute_id(record, SDP_ServiceRecordHandle);
    if (!serviceRecordHandleAttribute) return 0;
    if (de_get_element_type(serviceRecordHandleAttribute) != DE_UINT) return 0;
    if (de_get_size_type(serviceRecordHandleAttribute) != DE_SIZE_32) return 0;
    return READ_NET_32(serviceRecordHandleAttribute, 1); 
}

service_record_item_t * sdp_get_record_for_handle(uint32_t handle){
    linked_item_t *it;
    for (it = (linked_item_t *) sdp_service_records; it ; it = it->next){
        service_record_item_t * item = (service_record_item_t *) it;
        if (item->service_record_handle == handle){
            return item;
        }
    }
    return NULL;
}

// get next free, unregistered service record handle
uint32_t sdp_create_service_record_handle(){
    uint32_t handle = 0;
    do {
        handle = sdp_next_service_record_handle++;
        if (sdp_get_record_for_handle(handle)) handle = 0;
    } while (handle == 0);
    return handle;
}

// register service record internally
// pre: AttributeIDs are in ascending order => ServiceRecordHandle is first attribute if present
// @returns ServiceRecordHandle or 0 if registration failed
uint32_t sdp_register_service_internal(connection_t *connection, uint8_t * record){

    // dump for now
    // printf("Register service record\n");
    // de_dump_data_element(record);
    
    // get user record handle
    uint32_t record_handle = sdp_get_service_record_handle(record);

    // validate service record handle is not in reserved range
    if (record_handle <= maxReservedServiceRecordHandle) record_handle = 0;
    
    // check if already in use
    if (record_handle) {
        if (sdp_get_record_for_handle(record_handle)) {
            record_handle = 0;
        }
    }
    
    // create new handle if needed
    if (!record_handle){
        record_handle = sdp_create_service_record_handle();
    }
    
    // calculate size of new service record: DES (2 byte len) 
    // + ServiceRecordHandle attribute (UINT16 UINT32) + size of existing attributes
    uint16_t recordSize =  3 + (3 + 5) + de_get_data_size(record);
        
    // alloc memory for new service_record_item
    service_record_item_t * newRecordItem = (service_record_item_t *) malloc(recordSize + sizeof(service_record_item_t));
    if (!newRecordItem) return 0;

    // link new service item to client connection
    newRecordItem->connection = connection;
    
    // set new handle
    newRecordItem->service_record_handle = record_handle;

    // create updated service record
    uint8_t * newRecord = (uint8_t *) &(newRecordItem->service_record);
    
    // create DES for new record
    de_create_sequence(newRecord);
    
    // set service record handle
    de_add_number(newRecord, DE_UINT, DE_SIZE_16, 0);
    de_add_number(newRecord, DE_UINT, DE_SIZE_32, record_handle);
    
    // add other attributes
    sdp_append_attributes_in_attributeIDList(record, (uint8_t *) removeServiceRecordHandleAttributeIDList, 0, recordSize, newRecord);
    
    // dump for now
    // de_dump_data_element(newRecord);
    // printf("reserved size %u, actual size %u\n", recordSize, de_get_len(newRecord));
    
    // add to linked list
    linked_list_add(&sdp_service_records, (linked_item_t *) newRecordItem);
    return record_handle;
}

// unregister service record internally
// 
// makes sure one client cannot remove service records of other clients
//
void sdp_unregister_service_internal(connection_t *connection, uint32_t service_record_handle){
    service_record_item_t * record_item = sdp_get_record_for_handle(service_record_handle);
    if (record_item && record_item->connection == connection) {
        linked_list_remove(&sdp_service_records, (linked_item_t *) record_item);
    }
}

// remove all service record for a client connection
void sdp_unregister_services_for_connection(connection_t *connection){
    linked_item_t *it = (linked_item_t *) &sdp_service_records;
    while (it->next){
        service_record_item_t *record_item = (service_record_item_t *) it->next;
        if (record_item->connection == connection){
            it->next = it->next->next;
            free(record_item);
        } else {
            it = it->next;
        }
    }
}

// PDU
// PDU ID (1), Transaction ID (2), Param Length (2), Param 1, Param 2, ..

static uint8_t sdp_response_buffer[400];

int sdp_create_error_response(uint16_t transaction_id, uint16_t error_code){
    sdp_response_buffer[0] = SDP_ErrorResponse;
    net_store_16(sdp_response_buffer, 1, transaction_id);
    net_store_16(sdp_response_buffer, 3, 2);
    net_store_16(sdp_response_buffer, 5, error_code); // invalid syntax
    return 7;
}

int sdp_handle_service_search_request(uint8_t * packet, uint16_t remote_mtu){
    
    // get request details
    uint16_t  transaction_id = READ_NET_16(packet, 1);
    // not used yet - uint16_t  param_len = READ_NET_16(packet, 3);
    uint8_t * serviceSearchPattern = &packet[5];
    uint16_t  serviceSearchPatternLen = de_get_len(serviceSearchPattern);
    uint16_t  maximumServiceRecordCount = READ_NET_16(packet, 5 + serviceSearchPatternLen);
    uint8_t * continuationState = &packet[5+serviceSearchPatternLen+2];
    
    // calc maxumumServiceRecordCount based on remote MTU
    uint16_t maximumServiceRecordCount2 = (remote_mtu - (9+3))/4;
    if (maximumServiceRecordCount2 < maximumServiceRecordCount) {
        maximumServiceRecordCount = maximumServiceRecordCount2;
    }
    
    // continuation state contains index of next service record to examine
    int      continuation = 0;
    uint16_t continuation_index = 0;
    if (continuationState[0] == 2){
        continuation_index = READ_NET_16(continuationState, 1);
    }
    
    // header
    sdp_response_buffer[0] = SDP_ServiceSearchResponse;
    net_store_16(sdp_response_buffer, 1, transaction_id);
    
    // ServiceRecordHandleList at 9
    uint16_t pos = 9;
    uint16_t total_service_count   = 0;
    uint16_t current_service_count = 0;
    uint16_t current_service_index = 0;
    // for all service records that match
    linked_item_t *it;
    for (it = (linked_item_t *) sdp_service_records; it ; it = it->next, ++current_service_index){
        service_record_item_t * item = (service_record_item_t *) it;
        if (sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)){

            // get total count
            total_service_count++;
            
            // add to list if index higher than last continuation index and space left
            if (current_service_index >= continuation_index && !continuation) {
                if ( current_service_count < maximumServiceRecordCount) {
                    net_store_32(sdp_response_buffer, pos, item->service_record_handle);
                    current_service_count++;
                    pos += 4;
                } else {
                    // next time start with this one
                    continuation = 1;
                    continuation_index = current_service_index;
                }
            }
        }
    }
    
    // Store continuation state
    if (continuation) {
        sdp_response_buffer[pos++] = 2;
        net_store_16(sdp_response_buffer, pos, continuation_index);
        pos += 2;
    } else {
        sdp_response_buffer[pos++] = 0;
    }

    // update header info
    net_store_16(sdp_response_buffer, 3, pos - 5); // size of variable payload
    net_store_16(sdp_response_buffer, 5, total_service_count);
    net_store_16(sdp_response_buffer, 7, current_service_count);
    
    return pos;
}

int sdp_handle_service_attribute_request(uint8_t * packet, uint16_t remote_mtu){
    
    // get request details
    uint16_t  transaction_id = READ_NET_16(packet, 1);
    // not used yet - uint16_t  param_len = READ_NET_16(packet, 3);
    uint32_t  serviceRecordHandle = READ_NET_32(packet, 5);
    uint16_t  maximumAttributeByteCount = READ_NET_16(packet, 9);
    uint8_t * attributeIDList = &packet[11];
    uint16_t  attributeIDListLen = de_get_len(attributeIDList);
    uint8_t * continuationState = &packet[11+attributeIDListLen];
    
    // calc maximumAttributeByteCount based on remote MTU
    uint16_t maximumAttributeByteCount2 = remote_mtu - (7+3);
    if (maximumAttributeByteCount2 < maximumAttributeByteCount) {
        maximumAttributeByteCount = maximumAttributeByteCount2;
    }
    
    // continuation state contains index of next attribute to examine
    uint16_t continuation_index = 0;
    if (continuationState[0] == 2){
        continuation_index = READ_NET_16(continuationState, 1);
    }
    
    // get service record
    service_record_item_t * item = sdp_get_record_for_handle(serviceRecordHandle);
    if (!item){
        // service record handle doesn't exist
        return sdp_create_error_response(transaction_id, 0x0002); /// invalid Service Record Handle
    }
    
    // header
    sdp_response_buffer[0] = SDP_ServiceAttributeResponse;
    net_store_16(sdp_response_buffer, 1, transaction_id);
    
    // AttributeList - starts at offset 7
    uint16_t pos = 7;
    uint8_t *attributeList = &sdp_response_buffer[pos];
    de_create_sequence(attributeList);
    // copy specified attributes
    int result = sdp_append_attributes_in_attributeIDList(item->service_record, attributeIDList, continuation_index, maximumAttributeByteCount, attributeList);
    pos += de_get_len(attributeList);
    
    // Continuation State
    if (result >= 0) {
        sdp_response_buffer[pos++] = 2;
        net_store_16(sdp_response_buffer, pos, (uint16_t) result);
        pos += 2;
    } else {
        sdp_response_buffer[pos++] = 0;
    }

    // update header
    net_store_16(sdp_response_buffer, 3, pos - 5);  // size of variable payload
    net_store_16(sdp_response_buffer, 5, de_get_len(attributeList)); 
    
    return pos;
}

int sdp_handle_service_search_attribute_request(uint8_t * packet, uint16_t remote_mtu){
    
    // get request details
    uint16_t  transaction_id = READ_NET_16(packet, 1);
    // not used yet - uint16_t  param_len = READ_NET_16(packet, 3);
    uint8_t * serviceSearchPattern = &packet[5];
    uint16_t  serviceSearchPatternLen = de_get_len(serviceSearchPattern);
    uint16_t  maximumAttributeByteCount = READ_NET_16(packet, 5 + serviceSearchPatternLen);
    uint8_t * attributeIDList = &packet[5+serviceSearchPatternLen+2];
    uint16_t  attributeIDListLen = de_get_len(attributeIDList);
    uint8_t * continuationState = &packet[5+serviceSearchPatternLen+2+attributeIDListLen];
    
    // calc maximumAttributeByteCount based on remote MTU
    uint16_t maximumAttributeByteCount2 = remote_mtu - (7+5);
    if (maximumAttributeByteCount2 < maximumAttributeByteCount) {
        maximumAttributeByteCount = maximumAttributeByteCount2;
    }
    
    // continuation state contains index of next service record to examine
    // continuation state contains index of next attribute to examine
    uint16_t continuation_service_index   = 0;
    uint16_t continuation_attribute_index = 0;
    int      continuation = 0;
    if (continuationState[0] == 4){
        continuation_service_index   = READ_NET_16(continuationState, 1);
        continuation_attribute_index = READ_NET_16(continuationState, 3);
    }
    
    // header
    sdp_response_buffer[0] = SDP_ServiceSearchAttributeResponse;
    net_store_16(sdp_response_buffer, 1, transaction_id);
    
    // AttributeLists - starts at offset 7
    uint16_t pos = 7;
    uint8_t *attributeLists = &sdp_response_buffer[pos];
    de_create_sequence(attributeLists);
    
    // for all service records that match
    uint16_t current_service_index = 0;
    linked_item_t *it;
    for (it = (linked_item_t *) sdp_service_records; it ; it = it->next, ++current_service_index){
        service_record_item_t * item = (service_record_item_t *) it;
        if (current_service_index >= continuation_service_index ) {
            if (sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)){
                
                // record found
                // de_dump_data_element(item->service_record);
                
                // check if DES header fits in
                uint16_t attributeListsSize = de_get_len(attributeLists);
                if (attributeListsSize + 3 > maximumAttributeByteCount) {
                    continuation = 1;
                    continuation_service_index   = current_service_index;
                    continuation_attribute_index = 0;
                    break;
                }
                
                // create sequecne and copy specified attributes
                uint8_t * attributes = de_push_sequence(attributeLists);
                int result = sdp_append_attributes_in_attributeIDList(item->service_record, attributeIDList, continuation_attribute_index,
                                                                      maximumAttributeByteCount - attributeListsSize, attributes);
                de_pop_sequence(attributeLists, attributes);
                
                // no space left?
                if (result >= 0){
                    continuation = 1;
                    continuation_service_index   = current_service_index;
                    continuation_attribute_index = (uint16_t) result;
                    break;
                }
            }
        }
        current_service_index++;
    }
    pos += de_get_len(attributeLists);


    // Continuation State
    if (continuation) {
        sdp_response_buffer[pos++] = 4;
        net_store_16(sdp_response_buffer, pos, continuation_service_index);
        pos += 2;
        net_store_16(sdp_response_buffer, pos, continuation_attribute_index);
        pos += 2;
    } else {
        sdp_response_buffer[pos++] = 0;
    }
    
    // update header
    net_store_16(sdp_response_buffer, 3, pos - 5);  // size of variable payload
    net_store_16(sdp_response_buffer, 5, de_get_len(attributeLists));  // AttributeListsByteCount
    
    return pos;
}

static void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	uint16_t transaction_id;
    SDP_PDU_ID_t pdu_id;
    uint16_t param_len;
    uint16_t remote_mtu;
    int pos = 5;
    
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
            pdu_id = packet[0];
            transaction_id = READ_NET_16(packet, 1);
            param_len = READ_NET_16(packet, 3);
            remote_mtu = l2cap_get_remote_mtu_for_local_cid(channel);
            printf("SDP Request: type %u, transaction id %u, len %u\n", pdu_id, transaction_id, param_len);
            switch (pdu_id){
                    
                case SDP_ServiceSearchRequest:
                    pos = sdp_handle_service_search_request(packet, remote_mtu);
                    break;
                                        
                case SDP_ServiceAttributeRequest:
                    pos = sdp_handle_service_attribute_request(packet, remote_mtu);
                    break;
                    
                case SDP_ServiceSearchAttributeRequest:
                    pos = sdp_handle_service_search_attribute_request(packet, remote_mtu);
                    break;
                    
                default:
                    pos = sdp_create_error_response(transaction_id, 0x0003); // invalid syntax
                    break;
            }
            l2cap_send_internal(channel, sdp_response_buffer, pos);
			break;
			
		case HCI_EVENT_PACKET:
			
			switch (packet[0]) {
                    					
				case L2CAP_EVENT_INCOMING_CONNECTION:
					// accept
                    l2cap_accept_connection_internal(channel);
					break;
					                    
				default:
					// other event
					break;
			}
			break;
			
		default:
			// other packet type
			break;
	}
}

#if 1
static uint8_t record[100];
static uint8_t request[100];
static void dump_service_search_response(){
    uint16_t nr_services = READ_NET_16(sdp_response_buffer, 7);
    int i;
    printf("Nr service handles: %u\n", nr_services);
    for (i=0; i<nr_services;i++){
        printf("ServiceHandle %x\n", READ_NET_32(sdp_response_buffer, 9+i*4));
    }
    if (sdp_response_buffer[9 + nr_services * 4]){
        printf("Continuation index %u\n", READ_NET_16(sdp_response_buffer, 9+nr_services*4+1));
    } else {
        printf("Continuation: NO\n");
    }
}

void sdp_test(){
    const uint16_t remote_mtu = 150;

    // create two records with 2 attributes each
    de_create_sequence(record);
    de_add_number(record, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle); 
    de_add_number(record, DE_UINT, DE_SIZE_32, 0x10001);
    de_add_number(record, DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
    de_add_number(record, DE_UUID, DE_SIZE_16, 0x0001);
    de_add_number(record, DE_UINT, DE_SIZE_16, SDP_BrowseGroupList);
    de_add_number(record, DE_UUID, DE_SIZE_16, 0x0001);
    uint32_t handle_1 = sdp_register_service_internal(NULL, record);
    
    de_create_sequence(record);
    de_add_number(record, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle);
    de_add_number(record, DE_UINT, DE_SIZE_32, 0x10002);
    de_add_number(record, DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
    de_add_number(record, DE_UUID, DE_SIZE_16, 0x0002);
    de_add_number(record, DE_UINT, DE_SIZE_16, SDP_BrowseGroupList);
    de_add_number(record, DE_UUID, DE_SIZE_16, 0x0001);
    uint32_t handle_2 = sdp_register_service_internal(NULL, record);

    // sdp_handle_service_search_request
    uint16_t transactionID = 1;
    uint16_t nr_services = 1;
    request[0] = SDP_ServiceSearchRequest;
    net_store_16(request, 1, transactionID++); // transaction ID
    uint8_t * serviceSearchPattern = &request[5];
    de_create_sequence(serviceSearchPattern);
    {
        de_add_number(serviceSearchPattern, DE_UUID, DE_SIZE_16, 0x0001);
    }
    uint16_t serviceSearchPatternLen = de_get_len(serviceSearchPattern);
    net_store_16(request, 5 + serviceSearchPatternLen, 1);
    request[5 + serviceSearchPatternLen + 2] = 0;
    sdp_handle_service_search_request(request, remote_mtu);
    dump_service_search_response();
    memcpy(request + 5 + serviceSearchPatternLen + 2, sdp_response_buffer + 9 + nr_services*4, 3); 
    sdp_handle_service_search_request(request, remote_mtu);
    dump_service_search_response();

    // sdp_handle_service_attribute_request
    uint16_t attributeListLen;
    request[0] = SDP_ServiceAttributeRequest;
    net_store_16(request, 1, transactionID++); // transaction ID
    net_store_32(request, 5, handle_1); // record handle
    net_store_16(request, 9, 200); // max bytes
    uint8_t * attributeIDList = request + 11;
    de_create_sequence(attributeIDList);
    de_add_number(attributeIDList, DE_UINT, DE_SIZE_32, 0x0000ffff);
    uint16_t attributeIDListLen = de_get_len(attributeIDList);
    request[11+attributeIDListLen] = 0;
    while(1) {
        sdp_handle_service_attribute_request(request, remote_mtu);
        de_dump_data_element(sdp_response_buffer+7);
        attributeListLen = de_get_len(sdp_response_buffer+7);
        printf("Continuation %u\n", sdp_response_buffer[7+attributeListLen]);
        if (sdp_response_buffer[7+attributeListLen] == 0) break;
        memcpy(request+11+attributeIDListLen, sdp_response_buffer+7+attributeListLen, 3);
    }
    
    // sdp_handle_service_search_attribute_request
    request[0] = SDP_ServiceSearchAttributeRequest;
    net_store_16(request, 1, transactionID++); // transaction ID
    de_create_sequence(serviceSearchPattern);
    {
        de_add_number(serviceSearchPattern, DE_UUID, DE_SIZE_16, 0x0001);
    }
    serviceSearchPatternLen = de_get_len(serviceSearchPattern);
    net_store_16(request, 5 + serviceSearchPatternLen, 15 + 3); // MaximumAttributeByteCount:
    attributeIDList = request + 5 + serviceSearchPatternLen + 2;
    de_create_sequence(attributeIDList);
    de_add_number(attributeIDList, DE_UINT, DE_SIZE_32, 0x0000ffff);
    attributeIDListLen = de_get_len(attributeIDList);
    request[5 + serviceSearchPatternLen + 2 + attributeIDListLen] = 0;
    while (1) {
        sdp_handle_service_search_attribute_request(request, remote_mtu);
        de_dump_data_element(sdp_response_buffer+7);
        attributeListLen = de_get_len(sdp_response_buffer+7);
        printf("Continuation %u\n", sdp_response_buffer[7+attributeListLen]);
        if (sdp_response_buffer[7+attributeListLen] == 0) break;
        printf("Continuation {%u,%u}\n", READ_NET_16(sdp_response_buffer, 7+attributeListLen+1),
               READ_NET_16(sdp_response_buffer, 7+attributeListLen+3));
        memcpy(request+5 + serviceSearchPatternLen + 2 + attributeIDListLen, sdp_response_buffer+7+attributeListLen, 5);
    }
    /////
}
#endif
