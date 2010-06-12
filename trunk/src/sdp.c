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

#include <btstack/sdp.h>

#include <stdio.h>

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

    // data is contained in same memory
    uint32_t        service_record_handle;
    uint8_t         service_record[0];
} service_record_item_t;

static void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// registered service records
linked_list_t sdp_service_records;

// our handles start after the reserved range
static uint32_t sdp_next_service_record_handle = maxReservedServiceRecordHandle + 1;

// AttributeIDList used to remove ServiceRecordHandle
const uint8_t removeServiceRecordHandleAttributeIDList[] = { 0x36, 0x00, 0x05, 0x0A, 0x00, 0x01, 0xFF, 0xFF };

void sdp_init(){
    // register with l2cap psm sevices
    l2cap_register_service_internal(NULL, sdp_packet_handler, PSM_SDP, 250);
}

uint32_t sdp_get_service_record_handle(uint8_t * record){
    uint8_t * serviceRecordHandleAttribute = sdp_get_attribute_value_for_attribute_id(record, 0x0000);
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
uint32_t sdp_register_service_internal(uint8_t * record){

    // get user record handle
    uint32_t record_handle = sdp_get_service_record_handle(record);

    // validate service record handle is not in reserved range
    if (record_handle <= maxReservedServiceRecordHandle) record_handle = 0;
    
    // check if already in use
    if (sdp_get_record_for_handle(record_handle)) record_handle = 0;
    
    // create new handle if needed
    if (!record_handle){
        record_handle = sdp_create_service_record_handle();
    }
    
    // calculate size of new service record: DES (2 byte len) 
    // + ServiceRecordHandle attribute (DES UINT16 UINT32) + size of existing attributes
    uint16_t recordSize =  3 + (3 + 3 + 5) + de_get_data_size(record);
        
    // alloc memory for new service_record_item
    service_record_item_t * newRecordItem = (service_record_item_t *) malloc(recordSize + sizeof(service_record_item_t));
    if (!newRecordItem) return 0;
    
    // set new handle
    newRecordItem->service_record_handle = record_handle;

    // create updated service record
    uint8_t * newRecord = (uint8_t *) &(newRecordItem->service_record);
    
    // create DES for new record
    de_create_sequence(newRecord);
    
    // set service record handle
    uint8_t * serviceRecordHandleAttribute = de_push_sequence(newRecord);
    de_add_number(serviceRecordHandleAttribute, DE_UINT, DE_SIZE_16, 0);
    de_add_number(serviceRecordHandleAttribute, DE_UINT, DE_SIZE_32, record_handle);
    de_pop_sequence(newRecord, serviceRecordHandleAttribute);
    
    // add other attributes
    sdp_append_attributes_in_attributeIDList(record, (uint8_t *) removeServiceRecordHandleAttributeIDList, newRecord, recordSize);
    
    // dump for now
    de_dump_data_element(newRecord);
    printf("reserved size %u, actual size %u\n", recordSize, de_get_len(newRecord));
    
    // add to linked list
    linked_list_add(&sdp_service_records, (linked_item_t *) newRecordItem);
    
    return record_handle;
}

// unregister service record internally
void sdp_unregister_service(uint32_t service_record_handle){
    service_record_item_t * record_item = sdp_get_record_for_handle(service_record_handle);
    if (record_item) {
        linked_list_remove(&sdp_service_records, (linked_item_t *) record_item);
    }
}

// PDU
// PDU ID (1), Transaction ID (2), Param Length (2), Param 1, Param 2, ..

static uint8_t sdp_response_buffer[250];

int sdp_handle_service_search_request(uint8_t * packet){
    
    // get request details
    uint16_t  transaction_id = READ_NET_16(packet, 1);
    // not used yet - uint16_t  param_len = READ_NET_16(packet, 3);
    uint8_t * serviceSearchPattern = &packet[5];
    // not used yet - uint16_t  serviceSearchPatternLen = de_get_len(serviceSearchPattern);
    // not used yet - uint16_t  maximumServiceRecordCount = READ_NET_16(packet, 5 + serviceSearchPatternLen);
    // not used yet - uint8_t * continuationState = &packet[5+serviceSearchPatternLen+2];
    
    // header
    sdp_response_buffer[0] = SDP_ServiceSearchResponse;
    net_store_16(sdp_response_buffer, 1, transaction_id);
    
    // ServiceRecordHandleList at 9
    uint16_t pos = 9;
    uint16_t service_count = 0;
    
    // for all service records that match
    linked_item_t *it;
    for (it = (linked_item_t *) sdp_service_records; it ; it = it->next){
        service_record_item_t * item = (service_record_item_t *) it;
        if (sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)){
            net_store_32(sdp_response_buffer, pos, item->service_record_handle);
            pos += 4;
            service_count++;
        }
    }
    
    // TotalServiceRecordCount at 5
    net_store_16(sdp_response_buffer, 5, service_count);
    
    // CurrentServiceRecordCount at 7
    net_store_16(sdp_response_buffer, 7, service_count);

    // Continuation State: none
    // @TODO send correct continuation state
    sdp_response_buffer[pos++] = 0;
    
    
    // update len info
    net_store_16(sdp_response_buffer, 3, pos - 5); // empty list
    
    return pos;
}

int sdp_handle_service_search_attribute_request(uint8_t * packet){
    
    // get request details
    uint16_t  transaction_id = READ_NET_16(packet, 1);
    // not used yet - uint16_t  param_len = READ_NET_16(packet, 3);
    uint8_t * serviceSearchPattern = &packet[5];
    uint16_t  serviceSearchPatternLen = de_get_len(serviceSearchPattern);
    uint16_t  maximumAttributeByteCount = READ_NET_16(packet, 5 + serviceSearchPatternLen);
    uint8_t * attributeIDList = &packet[5+serviceSearchPatternLen+2];
    // not used yet - uint16_t  attributeIDListLen = de_get_len(attributeIDList);
    // not used yet - uint8_t * continuationState = &packet[5+serviceSearchPatternLen+2+attributeIDListLen];
    
    // header
    sdp_response_buffer[0] = SDP_ServiceSearchAttributeResponse;
    net_store_16(sdp_response_buffer, 1, transaction_id);
    
    // AttributeLists - starts at offset 7
    uint16_t pos = 7;
    uint8_t *attributeLists = &sdp_response_buffer[pos];
    de_create_sequence(attributeLists);
    
    // dump
    // printf("ServiceSearchPattern:\n");
    // de_dump_data_element(serviceSearchPattern);
    
    // for all service records that match
    linked_item_t *it;
    for (it = (linked_item_t *) sdp_service_records; it ; it = it->next){
        service_record_item_t * item = (service_record_item_t *) it;
        // printf("ServiceRecord:\n");
        // de_dump_data_element(item->service_record);
        if (sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)){
            // copy specified attributes
            uint8_t * attributes = de_push_sequence(attributeLists);
            sdp_append_attributes_in_attributeIDList(item->service_record, attributeIDList, attributes, maximumAttributeByteCount);
            de_pop_sequence(attributeLists, attributes);
        }
    }
    pos += de_get_len(attributeLists);

    // AttributeListsByteCount - at offset 5
    net_store_16(sdp_response_buffer, 5, de_get_len(attributeLists)); 

    // Continuation State: none
    // @TODO send correct continuation state
    sdp_response_buffer[pos++] = 0;
    
    // update len info
    net_store_16(sdp_response_buffer, 3, pos - 5); // empty list
    
    return pos;
}

static void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	uint16_t transaction_id;
    SDP_PDU_ID_t pdu_id;
    uint16_t param_len;
    int pos = 5;
    
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
            pdu_id = packet[0];
            transaction_id = READ_NET_16(packet, 1);
            param_len = READ_NET_16(packet, 3);
            printf("SDP Request: type %u, transaction id %u, len %u\n", pdu_id, transaction_id, param_len);
            switch (pdu_id){
                    
                case SDP_ServiceSearchRequest:
                    pos = sdp_handle_service_search_attribute_request(packet);
                    l2cap_send_internal(channel, sdp_response_buffer, pos);
                    break;
                                        
                case SDP_ServiceAttributeRequest:
                    // header
                    sdp_response_buffer[0] = SDP_ServiceAttributeResponse;
                    net_store_16(sdp_response_buffer, 1, transaction_id);
                    
                    // AttributeListByteCount::
                    net_store_16(sdp_response_buffer, pos, 2); // 2 bytes in DES with 1 byte len
                    pos += 2;
                    // AttributeLists
                    sdp_response_buffer[pos++] = 0x35; 
                    sdp_response_buffer[pos++] = 0;
                    // Continuation State: none
                    sdp_response_buffer[pos++] = 0;
                    
                    // update len info
                    net_store_16(sdp_response_buffer, 3, pos - 5); // empty list

                    l2cap_send_internal(channel, sdp_response_buffer, pos);
                    break;
                    

                case SDP_ServiceSearchAttributeRequest:
                    pos = sdp_handle_service_search_attribute_request(packet);
                    l2cap_send_internal(channel, sdp_response_buffer, pos);
                    break;
                    
                default:
                    sdp_response_buffer[0] = SDP_ErrorResponse;
                    net_store_16(sdp_response_buffer, 1, transaction_id);
                    net_store_16(sdp_response_buffer, 3, 2);
                    net_store_16(sdp_response_buffer, 5, 0x0003); // invalid syntax
                    l2cap_send_internal(channel, sdp_response_buffer, 7);
                    break;
            }
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



