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
    
    // calculate size of new service_record_item_t: DES (2 byte) + size of existing attributes
    uint16_t recordSize = sizeof(service_record_item_t) + 3 + de_get_data_size(record);
    
    // plus ServiceRecordHandle attribute (DES UINT16 UINT32) if not set
    if (!record_handle) recordSize += 3 + 3 + 5;
    
    // alloc memory for new service_record_item
    service_record_item_t * newRecordItem = (service_record_item_t *) malloc(recordSize);
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
    printf("calculated size %u, actual size %u\n", recordSize, de_get_len(newRecord));
    
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

static void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	uint16_t transaction_id;
    SDP_PDU_ID_t pdu_id;
    uint16_t param_len;
    
    
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
            pdu_id = packet[0];
            transaction_id = READ_NET_16(packet, 1);
            param_len = READ_NET_16(packet, 3);
            printf("SDP Request: type %u, transaction id %u, len %u\n", pdu_id, transaction_id, param_len);
            switch (pdu_id){
                    
                case SDP_ServiceSearchRequest:
                    // header
                    sdp_response_buffer[0] = SDP_ServiceSearchResponse;
                    net_store_16(sdp_response_buffer, 1, transaction_id);
                    net_store_16(sdp_response_buffer, 3, 5); // empty list
                    
                    // TotalServiceRecordCount:
                    net_store_16(sdp_response_buffer, 5, 0); // none
                    // CurrentServiceRecordCount:
                    net_store_16(sdp_response_buffer, 7, 0); // none
                    // ServiceRecordHandleList:
                    // empty
                    // Continuation State: none
                    sdp_response_buffer[9] = 0;
                    l2cap_send_internal(channel, sdp_response_buffer, 5 + 5);
                    break;
                    
                case SDP_ServiceAttributeRequest:
                    // header
                    sdp_response_buffer[0] = SDP_ServiceAttributeResponse;
                    net_store_16(sdp_response_buffer, 1, transaction_id);
                    net_store_16(sdp_response_buffer, 3, 5); // empty list
                    
                    // AttributeListByteCount::
                    net_store_16(sdp_response_buffer, 5, 2); // 2 bytes in DES with 1 byte len
                    // AttributeLists
                    sdp_response_buffer[7] = 0x35; 
                    sdp_response_buffer[8] = 0;
                    // Continuation State: none
                    sdp_response_buffer[9] = 0;
                    l2cap_send_internal(channel, sdp_response_buffer, 5 + 5);
                    break;
                    

                case SDP_ServiceSearchAttributeRequest:
                    // header
                    sdp_response_buffer[0] = SDP_ServiceSearchAttributeResponse;
                    net_store_16(sdp_response_buffer, 1, transaction_id);
                    net_store_16(sdp_response_buffer, 3, 5); // empty list
                    
                    // AttributeListsByteCount
                    net_store_16(sdp_response_buffer, 5, 2); // 2 bytes in DES with 1 byte len
                    // AttributeLists
                    sdp_response_buffer[7] = 0x35; 
                    sdp_response_buffer[8] = 0;
                    // Continuation State: none
                    sdp_response_buffer[9] = 0;
                    l2cap_send_internal(channel, sdp_response_buffer, 5 + 5);
                    break;
                    
                default:
                    // just dump data for now
                    printf("Unknown SDP Request: ");
                    hexdump( packet, size );
                    printf("\n");
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



