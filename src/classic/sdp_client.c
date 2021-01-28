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

#define BTSTACK_FILE__ "sdp_client.c"

/*
 *  sdp_client.c
 */
#include "btstack_config.h"

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "classic/core.h"
#include "classic/sdp_client.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "hci_cmd.h"
#include "l2cap.h"

// Types SDP Parser - Data Element stream helper
typedef enum { 
    GET_LIST_LENGTH = 1,
    GET_RECORD_LENGTH,
    GET_ATTRIBUTE_ID_HEADER_LENGTH,
    GET_ATTRIBUTE_ID,
    GET_ATTRIBUTE_VALUE_LENGTH,
    GET_ATTRIBUTE_VALUE
} sdp_parser_state_t;

// Types SDP Client 
typedef enum {
    INIT, W4_CONNECT, W2_SEND, W4_RESPONSE, QUERY_COMPLETE
} sdp_client_state_t;


// Prototypes SDP Parser
void sdp_parser_init(btstack_packet_handler_t callback);
void sdp_parser_handle_chunk(uint8_t * data, uint16_t size);
void sdp_parser_handle_done(uint8_t status);
void sdp_parser_init_service_attribute_search(void);
void sdp_parser_init_service_search(void);
void sdp_parser_handle_service_search(uint8_t * data, uint16_t total_count, uint16_t record_handle_count);

// Prototypes SDP Client
void sdp_client_reset(void);
void sdp_client_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t sdp_client_setup_service_search_attribute_request(uint8_t * data);
#ifdef ENABLE_SDP_EXTRA_QUERIES
static uint16_t sdp_client_setup_service_search_request(uint8_t * data);
static uint16_t sdp_client_setup_service_attribute_request(uint8_t * data);
static void     sdp_client_parse_service_search_response(uint8_t* packet, uint16_t size);
static void     sdp_client_parse_service_attribute_response(uint8_t* packet, uint16_t size);
#endif

static uint8_t des_attributeIDList[] = { 0x35, 0x05, 0x0A, 0x00, 0x00, 0xff, 0xff};  // Attribute: 0x0000 - 0xffff

// State DES Parser
static de_state_t de_header_state;

// State SDP Parser
static sdp_parser_state_t state;
static uint16_t attribute_id = 0;
static uint16_t attribute_bytes_received;
static uint16_t attribute_bytes_delivered;
static uint16_t list_offset;
static uint16_t list_size;
static uint16_t record_offset;
static uint16_t record_size;
static uint16_t attribute_value_size;
static int record_counter;
static btstack_packet_handler_t sdp_parser_callback;

// State SDP Client
static uint16_t  mtu;
static uint16_t  sdp_cid = 0x40;
static const uint8_t * service_search_pattern;
static const uint8_t * attribute_id_list;
static uint16_t  transactionID;
static uint8_t   continuationState[16];
static uint8_t   continuationStateLen;
static sdp_client_state_t sdp_client_state = INIT;
static SDP_PDU_ID_t PDU_ID = SDP_Invalid;

// Query registration
static btstack_linked_list_t sdp_client_query_requests;

#ifdef ENABLE_SDP_EXTRA_QUERIES
static uint32_t serviceRecordHandle;
static uint32_t record_handle;
#endif

// DES Parser
void de_state_init(de_state_t * de_state){
    de_state->in_state_GET_DE_HEADER_LENGTH = 1;
    de_state->addon_header_bytes = 0;
    de_state->de_size = 0;
    de_state->de_offset = 0;
}

int de_state_size(uint8_t eventByte, de_state_t *de_state){
    if (de_state->in_state_GET_DE_HEADER_LENGTH){
        de_state->addon_header_bytes = de_get_header_size(&eventByte) - 1;
        de_state->de_size = 0;
        de_state->de_offset = 0;

        if (de_state->addon_header_bytes == 0){
            de_state->de_size = de_get_data_size(&eventByte);
            if (de_state->de_size == 0) {
                log_error("  ERROR: ID size is zero");
            }
            // log_info("Data element payload is %d bytes.", de_state->de_size);
            return 1;
        }
        de_state->in_state_GET_DE_HEADER_LENGTH = 0;
        return 0;
    }
   
    if (de_state->addon_header_bytes > 0){
        de_state->de_size = (de_state->de_size << 8) | eventByte;
        de_state->addon_header_bytes--;
    } 
    if (de_state->addon_header_bytes > 0) return 0;
    // log_info("Data element payload is %d bytes.", de_state->de_size);
    de_state->in_state_GET_DE_HEADER_LENGTH = 1;
    return 1;
}

// SDP Parser
static void sdp_parser_emit_value_byte(uint8_t event_byte){
    uint8_t event[11];
    event[0] = SDP_EVENT_QUERY_ATTRIBUTE_VALUE;
    event[1] = 9;
    little_endian_store_16(event, 2, record_counter);
    little_endian_store_16(event, 4, attribute_id);
    little_endian_store_16(event, 6, attribute_value_size);
    little_endian_store_16(event, 8, attribute_bytes_delivered);
    event[10] = event_byte;
    (*sdp_parser_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event)); 
}

static void sdp_parser_process_byte(uint8_t eventByte){
    // count all bytes
    list_offset++;
    record_offset++;

    // log_info(" parse BYTE_RECEIVED %02x", eventByte);
    switch(state){
        case GET_LIST_LENGTH:
            if (!de_state_size(eventByte, &de_header_state)) break;
            list_offset = de_header_state.de_offset;
            list_size = de_header_state.de_size;
            // log_info("parser: List offset %u, list size %u", list_offset, list_size);
            
            record_counter = 0;
            state = GET_RECORD_LENGTH;
            break;

        case GET_RECORD_LENGTH:
            // check size
            if (!de_state_size(eventByte, &de_header_state)) break;
            // log_info("parser: Record payload is %d bytes.", de_header_state.de_size);
            record_offset = de_header_state.de_offset;
            record_size = de_header_state.de_size;
            state = GET_ATTRIBUTE_ID_HEADER_LENGTH;
            break;

        case GET_ATTRIBUTE_ID_HEADER_LENGTH:
            if (!de_state_size(eventByte, &de_header_state)) break;
            attribute_id = 0;
            log_debug("ID data is stored in %d bytes.", (int) de_header_state.de_size);
            state = GET_ATTRIBUTE_ID;
            break;
        
        case GET_ATTRIBUTE_ID:
            attribute_id = (attribute_id << 8) | eventByte;
            de_header_state.de_size--;
            if (de_header_state.de_size > 0) break;
            log_debug("parser: Attribute ID: %04x.", attribute_id);

            state = GET_ATTRIBUTE_VALUE_LENGTH;
            attribute_bytes_received  = 0;
            attribute_bytes_delivered = 0;
            attribute_value_size      = 0;
            de_state_init(&de_header_state);
            break;
        
        case GET_ATTRIBUTE_VALUE_LENGTH:
            attribute_bytes_received++;
            sdp_parser_emit_value_byte(eventByte);
            attribute_bytes_delivered++;
            if (!de_state_size(eventByte, &de_header_state)) break;

            attribute_value_size = de_header_state.de_size + attribute_bytes_received;

            state = GET_ATTRIBUTE_VALUE;
            break;
        
        case GET_ATTRIBUTE_VALUE: 
            attribute_bytes_received++;
            sdp_parser_emit_value_byte(eventByte);
            attribute_bytes_delivered++;
            // log_debug("paser: attribute_bytes_received %u, attribute_value_size %u", attribute_bytes_received, attribute_value_size);

            if (attribute_bytes_received < attribute_value_size) break;
            // log_debug("parser: Record offset %u, record size %u", record_offset, record_size);
            if (record_offset != record_size){
                state = GET_ATTRIBUTE_ID_HEADER_LENGTH;
                // log_debug("Get next attribute");
                break;
            } 
            record_offset = 0;
            // log_debug("parser: List offset %u, list size %u", list_offset, list_size);
            
            if ((list_size > 0) && (list_offset != list_size)){
                record_counter++;
                state = GET_RECORD_LENGTH;
                log_debug("parser: END_OF_RECORD");
                break;
            }
            list_offset = 0;
            de_state_init(&de_header_state);
            state = GET_LIST_LENGTH;
            record_counter = 0;
            log_debug("parser: END_OF_RECORD & DONE");
            break;
        default:
            break;
    }
}

void sdp_parser_init(btstack_packet_handler_t callback){
    // init
    sdp_parser_callback = callback;
    de_state_init(&de_header_state);
    state = GET_LIST_LENGTH;
    list_offset = 0;
    list_size = 0;
    record_offset = 0;
    record_counter = 0;
    record_size = 0;
    attribute_id = 0;
    attribute_bytes_received = 0;
    attribute_bytes_delivered = 0;
}

static void sdp_parser_deinit(void) {
    sdp_parser_callback = NULL;
    attribute_value_size = 0;
    record_counter = 0;
}

void sdp_client_init(void){
}

void sdp_client_deinit(void){
    sdp_parser_deinit();
    sdp_client_state = INIT;
    sdp_cid = 0x40;
    service_search_pattern = NULL;
    attribute_id_list = NULL;
    transactionID = 0;
    continuationStateLen = 0;
    sdp_client_state = INIT;
    PDU_ID = SDP_Invalid;
#ifdef ENABLE_SDP_EXTRA_QUERIES
    serviceRecordHandle = 0;
    record_handle = 0;
#endif
}

// for testing only
void sdp_client_reset(void){
    sdp_client_deinit();
}

void sdp_parser_handle_chunk(uint8_t * data, uint16_t size){
    int i;
    for (i=0;i<size;i++){
        sdp_parser_process_byte(data[i]);
    }
}

#ifdef ENABLE_SDP_EXTRA_QUERIES
void sdp_parser_init_service_attribute_search(void){
    // init
    de_state_init(&de_header_state);
    state = GET_RECORD_LENGTH;
    list_offset = 0;
    record_offset = 0;
    record_counter = 0;
}

void sdp_parser_init_service_search(void){
    record_offset = 0;
}

void sdp_parser_handle_service_search(uint8_t * data, uint16_t total_count, uint16_t record_handle_count){
    int i;
    for (i=0;i<record_handle_count;i++){
        record_handle = big_endian_read_32(data, i*4);
        record_counter++;
        uint8_t event[10];
        event[0] = SDP_EVENT_QUERY_SERVICE_RECORD_HANDLE;
        event[1] = 8;
        little_endian_store_16(event, 2, total_count);
        little_endian_store_16(event, 4, record_counter);
        little_endian_store_32(event, 6, record_handle);
        (*sdp_parser_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event)); 
    }        
}
#endif

static void sdp_client_notify_callbacks(void){
    if (sdp_client_ready() == false) {
        return;
    }
    btstack_context_callback_registration_t * callback = (btstack_context_callback_registration_t*) btstack_linked_list_pop(&sdp_client_query_requests);
    if (callback == NULL) {
        return;
    }
    (*callback->callback)(callback->context);
}

void sdp_parser_handle_done(uint8_t status){
    // reset state
    sdp_client_state = INIT;

    // emit query complete event
    uint8_t event[3];
    event[0] = SDP_EVENT_QUERY_COMPLETE;
    event[1] = 1;
    event[2] = status;
    (*sdp_parser_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event)); 

    // trigger next query if pending
    sdp_client_notify_callbacks();
}

// SDP Client

// TODO: inline if not needed (des(des))

static void sdp_client_parse_attribute_lists(uint8_t* packet, uint16_t length){
    sdp_parser_handle_chunk(packet, length);
}


static void sdp_client_send_request(uint16_t channel){

    if (sdp_client_state != W2_SEND) return;

    l2cap_reserve_packet_buffer();
    uint8_t * data = l2cap_get_outgoing_buffer();
    uint16_t request_len = 0;

    switch (PDU_ID){
#ifdef ENABLE_SDP_EXTRA_QUERIES
        case SDP_ServiceSearchResponse:
            request_len = sdp_client_setup_service_search_request(data);
            break;
        case SDP_ServiceAttributeResponse:
            request_len = sdp_client_setup_service_attribute_request(data);
            break;
#endif
        case SDP_ServiceSearchAttributeResponse:
            request_len = sdp_client_setup_service_search_attribute_request(data);
            break;
        default:
            log_error("SDP Client sdp_client_send_request :: PDU ID invalid. %u", PDU_ID);
            return;
    }

    // prevent re-entrance
    sdp_client_state = W4_RESPONSE;
    PDU_ID = SDP_Invalid;
    l2cap_send_prepared(channel, request_len);
}


static void sdp_client_parse_service_search_attribute_response(uint8_t* packet, uint16_t size){

    uint16_t offset = 3;
    if ((offset + 2 + 2) > size) return;  // parameterLength + attributeListByteCount
    uint16_t parameterLength = big_endian_read_16(packet,offset);
    offset+=2;
    if ((offset + parameterLength) > size) return;

    // AttributeListByteCount <= mtu
    uint16_t attributeListByteCount = big_endian_read_16(packet,offset);
    offset+=2;
    if (attributeListByteCount > mtu){
        log_error("Error parsing ServiceSearchAttributeResponse: Number of bytes in found attribute list is larger then the MaximumAttributeByteCount.");
        return;
    }

    // AttributeLists
    if ((offset + attributeListByteCount) > size) return;
    sdp_client_parse_attribute_lists(packet+offset, attributeListByteCount);
    offset+=attributeListByteCount;

    // continuation state len
    if ((offset + 1) > size) return;
    continuationStateLen = packet[offset];
    offset++;
    if (continuationStateLen > 16){
        continuationStateLen = 0;
        log_error("Error parsing ServiceSearchAttributeResponse: Number of bytes in continuation state exceedes 16.");
        return;
    }

    // continuation state
    if ((offset + continuationStateLen) > size) return;
    (void)memcpy(continuationState, packet + offset, continuationStateLen);
    // offset+=continuationStateLen;
}

void sdp_client_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    
    // uint16_t handle;
    if (packet_type == L2CAP_DATA_PACKET){
        if (size < 3) return;
        uint16_t responseTransactionID = big_endian_read_16(packet,1);
        if (responseTransactionID != transactionID){
            log_error("Mismatching transaction ID, expected %u, found %u.", transactionID, responseTransactionID);
            return;
        } 
        
        PDU_ID = (SDP_PDU_ID_t)packet[0];
        switch (PDU_ID){
            case SDP_ErrorResponse:
                log_error("Received error response with code %u, disconnecting", packet[2]);
                l2cap_disconnect(sdp_cid, 0);
                return;
#ifdef ENABLE_SDP_EXTRA_QUERIES
            case SDP_ServiceSearchResponse:
                sdp_client_parse_service_search_response(packet, size);
                break;
            case SDP_ServiceAttributeResponse:
                sdp_client_parse_service_attribute_response(packet, size);
                break;
#endif
            case SDP_ServiceSearchAttributeResponse:
                sdp_client_parse_service_search_attribute_response(packet, size);
                break;
            default:
                log_error("PDU ID %u unexpected/invalid", PDU_ID);
                return;
        }

        // continuation set or DONE?
        if (continuationStateLen == 0){
            log_debug("SDP Client Query DONE! ");
            sdp_client_state = QUERY_COMPLETE;
            l2cap_disconnect(sdp_cid, 0);
            return;
        }
        // prepare next request and send
        sdp_client_state = W2_SEND;
        l2cap_request_can_send_now_event(sdp_cid);
        return;
    }
    
    if (packet_type != HCI_EVENT_PACKET) return;
    
    switch(hci_event_packet_get_type(packet)){
        case L2CAP_EVENT_CHANNEL_OPENED:
            if (sdp_client_state != W4_CONNECT) break;
            // data: event (8), len(8), status (8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16), local_mtu(16), remote_mtu(16) 
            if (packet[2]) {
                log_info("SDP Client Connection failed, status 0x%02x.", packet[2]);
                sdp_parser_handle_done(packet[2]);
                break;
            }
            sdp_cid = channel;
            mtu = little_endian_read_16(packet, 17);
            // handle = little_endian_read_16(packet, 9);
            log_debug("SDP Client Connected, cid %x, mtu %u.", sdp_cid, mtu);

            sdp_client_state = W2_SEND;
            l2cap_request_can_send_now_event(sdp_cid);
            break;

        case L2CAP_EVENT_CAN_SEND_NOW:
            if(l2cap_event_can_send_now_get_local_cid(packet) == sdp_cid){
                sdp_client_send_request(sdp_cid);
            }
            break;
        case L2CAP_EVENT_CHANNEL_CLOSED: {
            if (sdp_cid != little_endian_read_16(packet, 2)) {
                // log_info("Received L2CAP_EVENT_CHANNEL_CLOSED for cid %x, current cid %x\n",  little_endian_read_16(packet, 2),sdp_cid);
                break;
            }
            log_info("SDP Client disconnected.");
            uint8_t status = (sdp_client_state == QUERY_COMPLETE) ? 0 : SDP_QUERY_INCOMPLETE;
            sdp_parser_handle_done(status);
            break;
        }
        default:
            break;
    }
}


static uint16_t sdp_client_setup_service_search_attribute_request(uint8_t * data){

    uint16_t offset = 0;
    transactionID++;
    // uint8_t SDP_PDU_ID_t.SDP_ServiceSearchRequest;
    data[offset++] = SDP_ServiceSearchAttributeRequest;
    // uint16_t transactionID
    big_endian_store_16(data, offset, transactionID);
    offset += 2;

    // param legnth
    offset += 2;

    // parameters: 
    //     Service_search_pattern - DES (min 1 UUID, max 12)
    uint16_t service_search_pattern_len = de_get_len(service_search_pattern);
    (void)memcpy(data + offset, service_search_pattern,
                 service_search_pattern_len);
    offset += service_search_pattern_len;

    //     MaximumAttributeByteCount - uint16_t  0x0007 - 0xffff -> mtu
    big_endian_store_16(data, offset, mtu);
    offset += 2;

    //     AttibuteIDList  
    uint16_t attribute_id_list_len = de_get_len(attribute_id_list);
    (void)memcpy(data + offset, attribute_id_list, attribute_id_list_len);
    offset += attribute_id_list_len;

    //     ContinuationState - uint8_t number of cont. bytes N<=16 
    data[offset++] = continuationStateLen;
    //                       - N-bytes previous response from server
    (void)memcpy(data + offset, continuationState, continuationStateLen);
    offset += continuationStateLen;

    // uint16_t paramLength 
    big_endian_store_16(data, 3, offset - 5);

    return offset;
}

#ifdef ENABLE_SDP_EXTRA_QUERIES
void sdp_client_parse_service_record_handle_list(uint8_t* packet, uint16_t total_count, uint16_t current_count){
    sdp_parser_handle_service_search(packet, total_count, current_count);
}

static uint16_t sdp_client_setup_service_search_request(uint8_t * data){
    uint16_t offset = 0;
    transactionID++;
    // uint8_t SDP_PDU_ID_t.SDP_ServiceSearchRequest;
    data[offset++] = SDP_ServiceSearchRequest;
    // uint16_t transactionID
    big_endian_store_16(data, offset, transactionID);
    offset += 2;

    // param legnth
    offset += 2;

    // parameters: 
    //     Service_search_pattern - DES (min 1 UUID, max 12)
    uint16_t service_search_pattern_len = de_get_len(service_search_pattern);
    (void)memcpy(data + offset, service_search_pattern,
                 service_search_pattern_len);
    offset += service_search_pattern_len;

    //     MaximumAttributeByteCount - uint16_t  0x0007 - 0xffff -> mtu
    big_endian_store_16(data, offset, mtu);
    offset += 2;

    //     ContinuationState - uint8_t number of cont. bytes N<=16 
    data[offset++] = continuationStateLen;
    //                       - N-bytes previous response from server
    (void)memcpy(data + offset, continuationState, continuationStateLen);
    offset += continuationStateLen;

    // uint16_t paramLength 
    big_endian_store_16(data, 3, offset - 5);

    return offset;
}


static uint16_t sdp_client_setup_service_attribute_request(uint8_t * data){

    uint16_t offset = 0;
    transactionID++;
    // uint8_t SDP_PDU_ID_t.SDP_ServiceSearchRequest;
    data[offset++] = SDP_ServiceAttributeRequest;
    // uint16_t transactionID
    big_endian_store_16(data, offset, transactionID);
    offset += 2;

    // param legnth
    offset += 2;

    // parameters: 
    //     ServiceRecordHandle
    big_endian_store_32(data, offset, serviceRecordHandle);
    offset += 4;

    //     MaximumAttributeByteCount - uint16_t  0x0007 - 0xffff -> mtu
    big_endian_store_16(data, offset, mtu);
    offset += 2;

    //     AttibuteIDList  
    uint16_t attribute_id_list_len = de_get_len(attribute_id_list);
    (void)memcpy(data + offset, attribute_id_list, attribute_id_list_len);
    offset += attribute_id_list_len;

    //     ContinuationState - uint8_t number of cont. bytes N<=16 
    data[offset++] = continuationStateLen;
    //                       - N-bytes previous response from server
    (void)memcpy(data + offset, continuationState, continuationStateLen);
    offset += continuationStateLen;

    // uint16_t paramLength 
    big_endian_store_16(data, 3, offset - 5);

    return offset;
}

static void sdp_client_parse_service_search_response(uint8_t* packet, uint16_t size){

    uint16_t offset = 3;
    if (offset + 2 + 2 + 2 > size) return;  // parameterLength, totalServiceRecordCount, currentServiceRecordCount

    uint16_t parameterLength = big_endian_read_16(packet,offset);
    offset+=2;
    if (offset + parameterLength > size) return;

    uint16_t totalServiceRecordCount = big_endian_read_16(packet,offset);
    offset+=2;

    uint16_t currentServiceRecordCount = big_endian_read_16(packet,offset);
    offset+=2;
    if (currentServiceRecordCount > totalServiceRecordCount){
        log_error("CurrentServiceRecordCount is larger then TotalServiceRecordCount.");
        return;
    }
    
    if (offset + currentServiceRecordCount * 4 > size) return;
    sdp_client_parse_service_record_handle_list(packet+offset, totalServiceRecordCount, currentServiceRecordCount);
    offset+= currentServiceRecordCount * 4;

    if (offset + 1 > size) return;
    continuationStateLen = packet[offset];
    offset++;
    if (continuationStateLen > 16){
        continuationStateLen = 0;
        log_error("Error parsing ServiceSearchResponse: Number of bytes in continuation state exceedes 16.");
        return;
    }
    if (offset + continuationStateLen > size) return;
    (void)memcpy(continuationState, packet + offset, continuationStateLen);
    // offset+=continuationStateLen;
}

static void sdp_client_parse_service_attribute_response(uint8_t* packet, uint16_t size){

    uint16_t offset = 3;
    if (offset + 2 + 2 > size) return;  // parameterLength, attributeListByteCount
    uint16_t parameterLength = big_endian_read_16(packet,offset);
    offset+=2;
    if (offset+parameterLength > size) return;

    // AttributeListByteCount <= mtu
    uint16_t attributeListByteCount = big_endian_read_16(packet,offset);
    offset+=2;
    if (attributeListByteCount > mtu){
        log_error("Error parsing ServiceSearchAttributeResponse: Number of bytes in found attribute list is larger then the MaximumAttributeByteCount.");
        return;
    }

    // AttributeLists
    if (offset+attributeListByteCount > size) return;
    sdp_client_parse_attribute_lists(packet+offset, attributeListByteCount);
    offset+=attributeListByteCount;

    // continuationStateLen
    if (offset + 1 > size) return;
    continuationStateLen = packet[offset];
    offset++;
    if (continuationStateLen > 16){
        continuationStateLen = 0;
        log_error("Error parsing ServiceAttributeResponse: Number of bytes in continuation state exceedes 16.");
        return;
    }
    if (offset + continuationStateLen > size) return;
    (void)memcpy(continuationState, packet + offset, continuationStateLen);
    // offset+=continuationStateLen;
}
#endif

// Public API

bool sdp_client_ready(void){
    return sdp_client_state == INIT;
}

uint8_t sdp_client_register_query_callback(btstack_context_callback_registration_t * callback_registration){
    bool added = btstack_linked_list_add_tail(&sdp_client_query_requests, (btstack_linked_item_t*) callback_registration);
    if (!added) return ERROR_CODE_COMMAND_DISALLOWED;
    sdp_client_notify_callbacks();
    return ERROR_CODE_SUCCESS;
}

uint8_t sdp_client_query(btstack_packet_handler_t callback, bd_addr_t remote, const uint8_t * des_service_search_pattern, const uint8_t * des_attribute_id_list){
    if (!sdp_client_ready()) return SDP_QUERY_BUSY;

    sdp_parser_init(callback);
    service_search_pattern = des_service_search_pattern;
    attribute_id_list = des_attribute_id_list;
    continuationStateLen = 0;
    PDU_ID = SDP_ServiceSearchAttributeResponse;

    sdp_client_state = W4_CONNECT;
    return l2cap_create_channel(sdp_client_packet_handler, remote, BLUETOOTH_PSM_SDP, l2cap_max_mtu(), NULL);
}

uint8_t sdp_client_query_uuid16(btstack_packet_handler_t callback, bd_addr_t remote, uint16_t uuid){
    if (!sdp_client_ready()) return SDP_QUERY_BUSY;
    return sdp_client_query(callback, remote, sdp_service_search_pattern_for_uuid16(uuid), des_attributeIDList);
}

uint8_t sdp_client_query_uuid128(btstack_packet_handler_t callback, bd_addr_t remote, const uint8_t* uuid){
    if (!sdp_client_ready()) return SDP_QUERY_BUSY;
    return sdp_client_query(callback, remote, sdp_service_search_pattern_for_uuid128(uuid), des_attributeIDList);
}

#ifdef ENABLE_SDP_EXTRA_QUERIES
uint8_t sdp_client_service_attribute_search(btstack_packet_handler_t callback, bd_addr_t remote, uint32_t search_service_record_handle, const uint8_t * des_attribute_id_list){
    if (!sdp_client_ready()) return SDP_QUERY_BUSY;

    sdp_parser_init(callback);
    serviceRecordHandle = search_service_record_handle;
    attribute_id_list = des_attribute_id_list;
    continuationStateLen = 0;
    PDU_ID = SDP_ServiceAttributeResponse;

    sdp_client_state = W4_CONNECT;
    l2cap_create_channel(sdp_client_packet_handler, remote, BLUETOOTH_PSM_SDP, l2cap_max_mtu(), NULL);
    return 0;
}

uint8_t sdp_client_service_search(btstack_packet_handler_t callback, bd_addr_t remote, const uint8_t * des_service_search_pattern){

    if (!sdp_client_ready()) return SDP_QUERY_BUSY;

    sdp_parser_init(callback);
    service_search_pattern = des_service_search_pattern;
    continuationStateLen = 0;
    PDU_ID = SDP_ServiceSearchResponse;

    sdp_client_state = W4_CONNECT;
    l2cap_create_channel(sdp_client_packet_handler, remote, BLUETOOTH_PSM_SDP, l2cap_max_mtu(), NULL);
    return 0;
}
#endif

