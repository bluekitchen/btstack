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

#define BTSTACK_FILE__ "sdp_server.c"

/*
 * Implementation of the Service Discovery Protocol Server 
 */

#include <string.h>

#include "bluetooth.h"
#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "classic/core.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"

// max number of incoming l2cap connections that can be queued instead of getting rejected
#ifndef SDP_WAITING_LIST_MAX_COUNT
#define SDP_WAITING_LIST_MAX_COUNT 8
#endif

// max reserved ServiceRecordHandle
#define MAX_RESERVED_SERVICE_RECORD_HANDLE 0xffff

// max SDP response matches L2CAP PDU -- allow to use smaller buffer
#ifndef SDP_RESPONSE_BUFFER_SIZE
#define SDP_RESPONSE_BUFFER_SIZE (HCI_ACL_PAYLOAD_SIZE-L2CAP_HEADER_SIZE)
#endif

static void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// registered service records
static btstack_linked_list_t sdp_server_service_records;

// our handles start after the reserved range
static uint32_t sdp_server_next_service_record_handle;

static uint8_t sdp_response_buffer[SDP_RESPONSE_BUFFER_SIZE];

static uint16_t sdp_server_l2cap_cid;
static uint16_t sdp_server_response_size;
static uint16_t sdp_server_l2cap_waiting_list_cids[SDP_WAITING_LIST_MAX_COUNT];
static int      sdp_server_l2cap_waiting_list_count;

void sdp_init(void){
    sdp_server_next_service_record_handle = ((uint32_t) MAX_RESERVED_SERVICE_RECORD_HANDLE) + 2;
    // register with l2cap psm sevices - max MTU
    l2cap_register_service(sdp_packet_handler, BLUETOOTH_PSM_SDP, 0xffff, LEVEL_0);
}

void sdp_deinit(void){
    sdp_server_service_records = NULL;
    sdp_server_l2cap_cid = 0;
    sdp_server_response_size = 0;
    sdp_server_l2cap_waiting_list_count = 0;
}

uint32_t sdp_get_service_record_handle(const uint8_t * record){
    // TODO: make sdp_get_attribute_value_for_attribute_id accept const data to remove cast
    uint8_t * serviceRecordHandleAttribute = sdp_get_attribute_value_for_attribute_id((uint8_t *)record, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    if (!serviceRecordHandleAttribute) return 0;
    if (de_get_element_type(serviceRecordHandleAttribute) != DE_UINT) return 0;
    if (de_get_size_type(serviceRecordHandleAttribute) != DE_SIZE_32) return 0;
    return big_endian_read_32(serviceRecordHandleAttribute, 1); 
}

static service_record_item_t * sdp_get_record_item_for_handle(uint32_t handle){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) sdp_server_service_records; it ; it = it->next){
        service_record_item_t * item = (service_record_item_t *) it;
        if (item->service_record_handle == handle){
            return item;
        }
    }
    return NULL;
}

uint8_t * sdp_get_record_for_handle(uint32_t handle){
    service_record_item_t * record_item =  sdp_get_record_item_for_handle(handle);
    if (!record_item) return NULL;
    return record_item->service_record;
}

// get next free, unregistered service record handle
uint32_t sdp_create_service_record_handle(void){
    uint32_t handle = 0;
    do {
        handle = sdp_server_next_service_record_handle++;
        if (sdp_get_record_item_for_handle(handle)) handle = 0;
    } while (handle == 0);
    return handle;
}

/**
 * @brief Register Service Record with database using ServiceRecordHandle stored in record
 * @pre AttributeIDs are in ascending order
 * @pre ServiceRecordHandle is first attribute and valid
 * @param record is not copied!
 * @result status
 */
uint8_t sdp_register_service(const uint8_t * record){

    // validate service record handle. it must: exist, be in valid range, not have been already used
    uint32_t record_handle = sdp_get_service_record_handle(record);
    if (!record_handle) return SDP_HANDLE_INVALID;
    if (record_handle <= MAX_RESERVED_SERVICE_RECORD_HANDLE) return SDP_HANDLE_INVALID;
    if (sdp_get_record_item_for_handle(record_handle)) return SDP_HANDLE_ALREADY_REGISTERED;

    // alloc memory for new service_record_item
    service_record_item_t * newRecordItem = btstack_memory_service_record_item_get();
    if (!newRecordItem) return BTSTACK_MEMORY_ALLOC_FAILED;

    // set handle and record
    newRecordItem->service_record_handle = record_handle;
    newRecordItem->service_record = (uint8_t*) record;
    
    // add to linked list
    btstack_linked_list_add(&sdp_server_service_records, (btstack_linked_item_t *) newRecordItem);
    
    return 0;
}

//
// unregister service record
//
void sdp_unregister_service(uint32_t service_record_handle){
    service_record_item_t * record_item = sdp_get_record_item_for_handle(service_record_handle);
    if (!record_item) return;
    btstack_linked_list_remove(&sdp_server_service_records, (btstack_linked_item_t *) record_item);
    btstack_memory_service_record_item_free(record_item);
}

// PDU
// PDU ID (1), Transaction ID (2), Param Length (2), Param 1, Param 2, ..

static int sdp_create_error_response(uint16_t transaction_id, uint16_t error_code){
    sdp_response_buffer[0] = SDP_ErrorResponse;
    big_endian_store_16(sdp_response_buffer, 1, transaction_id);
    big_endian_store_16(sdp_response_buffer, 3, 2);
    big_endian_store_16(sdp_response_buffer, 5, error_code); // invalid syntax
    return 7;
}

int sdp_handle_service_search_request(uint8_t * packet, uint16_t remote_mtu){
    
    // get request details
    uint16_t  transaction_id = big_endian_read_16(packet, 1);
    uint16_t  param_len = big_endian_read_16(packet, 3);
    uint8_t * serviceSearchPattern = &packet[5];
    uint16_t  serviceSearchPatternLen = de_get_len_safe(serviceSearchPattern, param_len);
    // assert service search pattern is contained
    if (!serviceSearchPatternLen) return 0;
    param_len -= serviceSearchPatternLen;
    // assert max record count is contained
    if (param_len < 2) return 0;
    uint16_t  maximumServiceRecordCount = big_endian_read_16(packet, 5 + serviceSearchPatternLen);
    param_len -= 2;
    // assert continuation state len is contained in param_len
    if (param_len < 1) return 0;
    uint8_t * continuationState = &packet[5+serviceSearchPatternLen+2];
    // assert continuation state is contained in param_len
    if ((1 + continuationState[0]) > param_len) return 0;

    // calc maximumServiceRecordCount based on remote MTU
    uint16_t maxNrServiceRecordsPerResponse = (remote_mtu - (9+3))/4;
    
    // continuation state contains index of next service record to examine
    int      continuation = 0;
    uint16_t continuation_index = 0;
    if (continuationState[0] == 2){
        continuation_index = big_endian_read_16(continuationState, 1);
    }
    
    // get and limit total count
    btstack_linked_item_t *it;
    uint16_t total_service_count   = 0;
    for (it = (btstack_linked_item_t *) sdp_server_service_records; it ; it = it->next){
        service_record_item_t * item = (service_record_item_t *) it;
        if (!sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)) continue;
        total_service_count++;
    }
    if (total_service_count > maximumServiceRecordCount){
        total_service_count = maximumServiceRecordCount;
    }
    
    // ServiceRecordHandleList at 9
    uint16_t pos = 9;
    uint16_t current_service_count  = 0;
    uint16_t current_service_index  = 0;
    uint16_t matching_service_count = 0;
    for (it = (btstack_linked_item_t *) sdp_server_service_records; it ; it = it->next, ++current_service_index){
        service_record_item_t * item = (service_record_item_t *) it;

        if (!sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)) continue;
        matching_service_count++;
        
        if (current_service_index < continuation_index) continue;

        big_endian_store_32(sdp_response_buffer, pos, item->service_record_handle);
        pos += 4;
        current_service_count++;
        
        if (matching_service_count >= total_service_count) break;

        if (current_service_count >= maxNrServiceRecordsPerResponse){
            continuation = 1;
            continuation_index = current_service_index + 1;
            break;
        }
    }
    
    // Store continuation state
    if (continuation) {
        sdp_response_buffer[pos++] = 2;
        big_endian_store_16(sdp_response_buffer, pos, continuation_index);
        pos += 2;
    } else {
        sdp_response_buffer[pos++] = 0;
    }

    // header
    sdp_response_buffer[0] = SDP_ServiceSearchResponse;
    big_endian_store_16(sdp_response_buffer, 1, transaction_id);
    big_endian_store_16(sdp_response_buffer, 3, pos - 5); // size of variable payload
    big_endian_store_16(sdp_response_buffer, 5, total_service_count);
    big_endian_store_16(sdp_response_buffer, 7, current_service_count);
    
    return pos;
}

int sdp_handle_service_attribute_request(uint8_t * packet, uint16_t remote_mtu){
    
    // get request details
    uint16_t  transaction_id = big_endian_read_16(packet, 1);
    uint16_t  param_len = big_endian_read_16(packet, 3);
    // assert serviceRecordHandle and maximumAttributeByteCount are in param_len
    if (param_len < 6) return 0;
    uint32_t  serviceRecordHandle = big_endian_read_32(packet, 5);
    uint16_t  maximumAttributeByteCount = big_endian_read_16(packet, 9);
    param_len -= 6;
    uint8_t * attributeIDList = &packet[11];
    uint16_t  attributeIDListLen = de_get_len_safe(attributeIDList, param_len);
    // assert attributeIDList are in param_len
    if (!attributeIDListLen) return 0;
    param_len -= attributeIDListLen;
    // assert continuation state len is contained in param_len
    if (param_len < 1) return 0;
    uint8_t * continuationState = &packet[11+attributeIDListLen];
    // assert continuation state is contained in param_len
    if ((1 + continuationState[0]) > param_len) return 0;
    
    // calc maximumAttributeByteCount based on remote MTU
    uint16_t maximumAttributeByteCount2 = remote_mtu - (7+3);
    if (maximumAttributeByteCount2 < maximumAttributeByteCount) {
        maximumAttributeByteCount = maximumAttributeByteCount2;
    }
    
    // continuation state contains the offset into the complete response
    uint16_t continuation_offset = 0;
    if (continuationState[0] == 2){
        continuation_offset = big_endian_read_16(continuationState, 1);
    }
    
    // get service record
    service_record_item_t * item = sdp_get_record_item_for_handle(serviceRecordHandle);
    if (!item){
        // service record handle doesn't exist
        return sdp_create_error_response(transaction_id, 0x0002); /// invalid Service Record Handle
    }
    
    
    // AttributeList - starts at offset 7
    uint16_t pos = 7;
    
    if (continuation_offset == 0){
        
        // get size of this record
        uint16_t filtered_attributes_size = sdp_get_filtered_size(item->service_record, attributeIDList);
        
        // store DES
        de_store_descriptor_with_len(&sdp_response_buffer[pos], DE_DES, DE_SIZE_VAR_16, filtered_attributes_size);
        maximumAttributeByteCount -= 3;
        pos += 3;
    }

    // copy maximumAttributeByteCount from record
    uint16_t bytes_used;
    int complete = sdp_filter_attributes_in_attributeIDList(item->service_record, attributeIDList, continuation_offset, maximumAttributeByteCount, &bytes_used, &sdp_response_buffer[pos]);
    pos += bytes_used;
    
    uint16_t attributeListByteCount = pos - 7;

    if (complete) {
        sdp_response_buffer[pos++] = 0;
    } else {
        continuation_offset += bytes_used;
        sdp_response_buffer[pos++] = 2;
        big_endian_store_16(sdp_response_buffer, pos, continuation_offset);
        pos += 2;
    }

    // header
    sdp_response_buffer[0] = SDP_ServiceAttributeResponse;
    big_endian_store_16(sdp_response_buffer, 1, transaction_id);
    big_endian_store_16(sdp_response_buffer, 3, pos - 5);  // size of variable payload
    big_endian_store_16(sdp_response_buffer, 5, attributeListByteCount); 
    
    return pos;
}

static uint16_t sdp_get_size_for_service_search_attribute_response(uint8_t * serviceSearchPattern, uint8_t * attributeIDList){
    uint16_t total_response_size = 0;
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) sdp_server_service_records; it ; it = it->next){
        service_record_item_t * item = (service_record_item_t *) it;
        
        if (!sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)) continue;
        
        // for all service records that match
        total_response_size += 3 + sdp_get_filtered_size(item->service_record, attributeIDList);
    }
    return total_response_size;
}

int sdp_handle_service_search_attribute_request(uint8_t * packet, uint16_t remote_mtu){
    
    // SDP header before attribute sevice list: 7
    // Continuation, worst case: 5
    
    // get request details
    uint16_t  transaction_id = big_endian_read_16(packet, 1);
    uint16_t  param_len = big_endian_read_16(packet, 3);
    uint8_t * serviceSearchPattern = &packet[5];
    uint16_t  serviceSearchPatternLen = de_get_len_safe(serviceSearchPattern, param_len);
    // assert serviceSearchPattern header is contained in param_len
    if (!serviceSearchPatternLen) return 0;
    param_len -= serviceSearchPatternLen;
    // assert maximumAttributeByteCount contained in param_len
    if (param_len < 2) return 0;
    uint16_t  maximumAttributeByteCount = big_endian_read_16(packet, 5 + serviceSearchPatternLen);
    param_len -= 2;
    uint8_t * attributeIDList = &packet[5+serviceSearchPatternLen+2];
    uint16_t  attributeIDListLen = de_get_len_safe(attributeIDList, param_len);
    // assert attributeIDList is contained in param_len
    if (!attributeIDListLen) return 0;
    // assert continuation state len is contained in param_len
    if (param_len < 1) return 0;
    uint8_t * continuationState = &packet[5+serviceSearchPatternLen+2+attributeIDListLen];
    // assert continuation state is contained in param_len
    if ((1 + continuationState[0]) > param_len) return 0;

    // calc maximumAttributeByteCount based on remote MTU, SDP header and reserved Continuation block
    uint16_t maximumAttributeByteCount2 = remote_mtu - 12;
    if (maximumAttributeByteCount2 < maximumAttributeByteCount) {
        maximumAttributeByteCount = maximumAttributeByteCount2;
    }
    
    // continuation state contains: index of next service record to examine
    // continuation state contains: byte offset into this service record
    uint16_t continuation_service_index = 0;
    uint16_t continuation_offset = 0;
    if (continuationState[0] == 4){
        continuation_service_index = big_endian_read_16(continuationState, 1);
        continuation_offset = big_endian_read_16(continuationState, 3);
    }

    // log_info("--> sdp_handle_service_search_attribute_request, cont %u/%u, max %u", continuation_service_index, continuation_offset, maximumAttributeByteCount);
    
    // AttributeLists - starts at offset 7
    uint16_t pos = 7;
    
    // add DES with total size for first request
    if ((continuation_service_index == 0) && (continuation_offset == 0)){
        uint16_t total_response_size = sdp_get_size_for_service_search_attribute_response(serviceSearchPattern, attributeIDList);
        de_store_descriptor_with_len(&sdp_response_buffer[pos], DE_DES, DE_SIZE_VAR_16, total_response_size);
        // log_info("total response size %u", total_response_size);
        pos += 3;
        maximumAttributeByteCount -= 3;
    }
    
    // create attribute list
    int      first_answer = 1;
    int      continuation = 0;
    uint16_t current_service_index = 0;
    btstack_linked_item_t *it = (btstack_linked_item_t *) sdp_server_service_records;
    for ( ; it ; it = it->next, ++current_service_index){
        service_record_item_t * item = (service_record_item_t *) it;
        
        if (current_service_index < continuation_service_index ) continue;
        if (!sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)) continue;

        if (continuation_offset == 0){
            
            // get size of this record
            uint16_t filtered_attributes_size = sdp_get_filtered_size(item->service_record, attributeIDList);
            
            // stop if complete record doesn't fits into response but we already have a partial response
            if (((filtered_attributes_size + 3) > maximumAttributeByteCount) && !first_answer) {
                continuation = 1;
                break;
            }
            
            // store DES
            de_store_descriptor_with_len(&sdp_response_buffer[pos], DE_DES, DE_SIZE_VAR_16, filtered_attributes_size);
            pos += 3;
            maximumAttributeByteCount -= 3;
        }
        
        first_answer = 0;
    
        // copy maximumAttributeByteCount from record
        uint16_t bytes_used;
        int complete = sdp_filter_attributes_in_attributeIDList(item->service_record, attributeIDList, continuation_offset, maximumAttributeByteCount, &bytes_used, &sdp_response_buffer[pos]);
        pos += bytes_used;
        maximumAttributeByteCount -= bytes_used;
        
        if (complete) {
            continuation_offset = 0;
            continue;
        }
        
        continuation = 1;
        continuation_offset += bytes_used;
        break;
    }
    
    uint16_t attributeListsByteCount = pos - 7;
    
    // Continuation State
    if (continuation){
        sdp_response_buffer[pos++] = 4;
        big_endian_store_16(sdp_response_buffer, pos, (uint16_t) current_service_index);
        pos += 2;
        big_endian_store_16(sdp_response_buffer, pos, continuation_offset);
        pos += 2;
    } else {
        // complete
        sdp_response_buffer[pos++] = 0;
    }
        
    // create SDP header
    sdp_response_buffer[0] = SDP_ServiceSearchAttributeResponse;
    big_endian_store_16(sdp_response_buffer, 1, transaction_id);
    big_endian_store_16(sdp_response_buffer, 3, pos - 5);  // size of variable payload
    big_endian_store_16(sdp_response_buffer, 5, attributeListsByteCount);
    
    return pos;
}

static void sdp_respond(void){
    if (!sdp_server_response_size ) return;
    if (!sdp_server_l2cap_cid) return;
    
    // update state before sending packet (avoid getting called when new l2cap credit gets emitted)
    uint16_t size = sdp_server_response_size;
    sdp_server_response_size = 0;
    l2cap_send(sdp_server_l2cap_cid, sdp_response_buffer, size);
}

// @pre space in list
static void sdp_waiting_list_add(uint16_t cid){
    sdp_server_l2cap_waiting_list_cids[sdp_server_l2cap_waiting_list_count++] = cid;
}

// @pre at least one item in list
static uint16_t sdp_waiting_list_get(void){
    uint16_t cid = sdp_server_l2cap_waiting_list_cids[0];
    sdp_server_l2cap_waiting_list_count--;
    if (sdp_server_l2cap_waiting_list_count){
        memmove(&sdp_server_l2cap_waiting_list_cids[0], &sdp_server_l2cap_waiting_list_cids[1], sdp_server_l2cap_waiting_list_count * sizeof(uint16_t));
    }
    return cid;
}

// we assume that we don't get two requests in a row
static void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	uint16_t transaction_id;
    sdp_pdu_id_t pdu_id;
    uint16_t remote_mtu;
    uint16_t param_len;
    
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
            pdu_id = (sdp_pdu_id_t) packet[0];
            transaction_id = big_endian_read_16(packet, 1);
            param_len = big_endian_read_16(packet, 3);
            remote_mtu = l2cap_get_remote_mtu_for_local_cid(channel);
            // account for our buffer
            if (remote_mtu > SDP_RESPONSE_BUFFER_SIZE){
                remote_mtu = SDP_RESPONSE_BUFFER_SIZE;
            }
            // validate parm_len against packet size
            if ((param_len + 5) > size) {
                // just clear pdu_id
                pdu_id = SDP_ErrorResponse;
            }
            
            // log_info("SDP Request: type %u, transaction id %u, len %u, mtu %u", pdu_id, transaction_id, param_len, remote_mtu);
            switch (pdu_id){
                    
                case SDP_ServiceSearchRequest:
                    sdp_server_response_size = sdp_handle_service_search_request(packet, remote_mtu);
                    break;
                                        
                case SDP_ServiceAttributeRequest:
                    sdp_server_response_size = sdp_handle_service_attribute_request(packet, remote_mtu);
                    break;
                    
                case SDP_ServiceSearchAttributeRequest:
                    sdp_server_response_size = sdp_handle_service_search_attribute_request(packet, remote_mtu);
                    break;
                    
                default:
                    sdp_server_response_size = sdp_create_error_response(transaction_id, 0x0003); // invalid syntax
                    break;
            }
            if (!sdp_server_response_size) break;
            l2cap_request_can_send_now_event(sdp_server_l2cap_cid);
			break;
			
		case HCI_EVENT_PACKET:
			
			switch (hci_event_packet_get_type(packet)) {

				case L2CAP_EVENT_INCOMING_CONNECTION:
                    if (sdp_server_l2cap_cid) {
                        // try to queue up
                        if (sdp_server_l2cap_waiting_list_count < SDP_WAITING_LIST_MAX_COUNT){
                            sdp_waiting_list_add(channel);
                            log_info("busy, queing incoming cid 0x%04x, now %u waiting", channel, sdp_server_l2cap_waiting_list_count);
                            break;
                        }

                        // CONNECTION REJECTED DUE TO LIMITED RESOURCES 
                        l2cap_decline_connection(channel);
                        break;
                    }
                    // accept
                    sdp_server_l2cap_cid = channel;
                    sdp_server_response_size = 0;
                    l2cap_accept_connection(sdp_server_l2cap_cid);
					break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    if (packet[2]) {
                        // open failed -> reset
                        sdp_server_l2cap_cid = 0;
                    }
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    sdp_respond();
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    if (channel == sdp_server_l2cap_cid){
                        // reset
                        sdp_server_l2cap_cid = 0;

                        // other request queued?
                        if (!sdp_server_l2cap_waiting_list_count) break;

                        // get first item 
                        sdp_server_l2cap_cid = sdp_waiting_list_get();

                        log_info("disconnect, accept queued cid 0x%04x, now %u waiting", sdp_server_l2cap_cid, sdp_server_l2cap_waiting_list_count);

                        // accept connection
                        sdp_server_response_size = 0;
                        l2cap_accept_connection(sdp_server_l2cap_cid);
                    }
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

