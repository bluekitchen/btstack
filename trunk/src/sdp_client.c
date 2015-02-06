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

/*
 *  sdp_client.c
 */

#include "btstack-config.h"
#include "sdp_client.h"

#include <btstack/hci_cmds.h>

#include "l2cap.h"
#include "sdp_parser.h"
#include "sdp.h"
#include "debug.h"

typedef enum {
    INIT, W4_CONNECT, W2_SEND, W4_RESPONSE, QUERY_COMPLETE
} sdp_client_state_t;


void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t setup_service_search_attribute_request(uint8_t * data);

#ifdef HAVE_SDP_EXTRA_QUERIES
static uint16_t setup_service_search_request(uint8_t * data);
static uint16_t setup_service_attribute_request(uint8_t * data);
static void     parse_service_search_response(uint8_t* packet);
static void     parse_service_attribute_response(uint8_t* packet);
static uint32_t serviceRecordHandle;
#endif

// SDP Client Query
static uint16_t  mtu;
static uint16_t  sdp_cid = 0x40;
static uint8_t * serviceSearchPattern;
static uint8_t * attributeIDList;
static uint16_t  transactionID = 0;
static uint8_t   continuationState[16];
static uint8_t   continuationStateLen;
static sdp_client_state_t sdp_client_state = INIT;
static SDP_PDU_ID_t PDU_ID = SDP_Invalid;

// TODO: inline if not needed (des(des))
void parse_attribute_lists(uint8_t* packet, uint16_t length){
    sdp_parser_handle_chunk(packet, length);
}

/* Queries the SDP service of the remote device given a service search pattern 
and a list of attribute IDs. The remote data is handled by the SDP parser. The 
SDP parser delivers attribute values and done event via a registered callback. */

void sdp_client_query(bd_addr_t remote, uint8_t * des_serviceSearchPattern, uint8_t * des_attributeIDList){
    serviceSearchPattern = des_serviceSearchPattern;
    attributeIDList = des_attributeIDList;
    continuationStateLen = 0;
    PDU_ID = SDP_ServiceSearchAttributeResponse;

    sdp_client_state = W4_CONNECT;
    l2cap_create_channel_internal(NULL, sdp_packet_handler, remote, PSM_SDP, l2cap_max_mtu());
}

static int can_send_now(uint16_t channel){
    if (sdp_client_state != W2_SEND) return 0;
    if (!l2cap_can_send_packet_now(channel)) return 0;
    return 1;
}

static void send_request(uint16_t channel){
    l2cap_reserve_packet_buffer();
    uint8_t * data = l2cap_get_outgoing_buffer();
    uint16_t request_len = 0;

    switch (PDU_ID){
#ifdef HAVE_SDP_EXTRA_QUERIES
        case SDP_ServiceSearchResponse:
            request_len = setup_service_search_request(data);
            break;
        case SDP_ServiceAttributeResponse:
            request_len = setup_service_attribute_request(data);
            break;
#endif
        case SDP_ServiceSearchAttributeResponse:
            request_len = setup_service_search_attribute_request(data);
            break;
        default:
            log_error("SDP Client send_request :: PDU ID invalid. %u", PDU_ID);
            return;
    }

    // prevent re-entrance
    sdp_client_state = W4_RESPONSE;
    int err = l2cap_send_prepared(channel, request_len);
    // l2cap_send_prepared shouldn't have failed as l2ap_can_send_packet_now() was true
    switch (err){
        case 0:
            log_debug("l2cap_send_internal() -> OK");
            PDU_ID = SDP_Invalid;
            break;
        case BTSTACK_ACL_BUFFERS_FULL:
            sdp_client_state = W2_SEND;
            log_info("l2cap_send_internal() ->BTSTACK_ACL_BUFFERS_FULL");
            break;
        default:
            sdp_client_state = W2_SEND;
            log_error("l2cap_send_internal() -> err %d", err);
            break;
    }
}


static void parse_service_search_attribute_response(uint8_t* packet){
    uint16_t offset = 3;
    uint16_t parameterLength = READ_NET_16(packet,offset);
    offset+=2;
    // AttributeListByteCount <= mtu
    uint16_t attributeListByteCount = READ_NET_16(packet,offset);
    offset+=2;

    if (attributeListByteCount > mtu){
        log_error("Error parsing ServiceSearchAttributeResponse: Number of bytes in found attribute list is larger then the MaximumAttributeByteCount.");
        return;
    }

    // AttributeLists
    parse_attribute_lists(packet+offset, attributeListByteCount);
    offset+=attributeListByteCount;

    continuationStateLen = packet[offset];
    offset++;

    if (continuationStateLen > 16){
        log_error("Error parsing ServiceSearchAttributeResponse: Number of bytes in continuation state exceedes 16.");
        return;
    }
    memcpy(continuationState, packet+offset, continuationStateLen);
    offset+=continuationStateLen;

    if (parameterLength != offset - 5){
        log_error("Error parsing ServiceSearchAttributeResponse: wrong size of parameters, number of expected bytes%u, actual number %u.", parameterLength, offset);
    }
}

void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    // uint16_t handle;
    if (packet_type == L2CAP_DATA_PACKET){
        uint16_t responseTransactionID = READ_NET_16(packet,1);
        if ( responseTransactionID != transactionID){
            log_error("Missmatching transaction ID, expected %u, found %u.", transactionID, responseTransactionID);
            return;
        } 
        
        if (packet[0] != SDP_ServiceSearchAttributeResponse 
            && packet[0] != SDP_ServiceSearchResponse
            && packet[0] != SDP_ServiceAttributeResponse){
            log_error("Not a valid PDU ID, expected %u, %u or %u, found %u.", SDP_ServiceSearchResponse, 
                                    SDP_ServiceAttributeResponse, SDP_ServiceSearchAttributeResponse, packet[0]);
            return;
        }

        PDU_ID = packet[0];
        log_info("SDP Client :: PDU ID. %u ,%u", PDU_ID, packet[0]);
        switch (PDU_ID){
#ifdef HAVE_SDP_EXTRA_QUERIES
            case SDP_ServiceSearchResponse:
                parse_service_search_response(packet);
                break;
            case SDP_ServiceAttributeResponse:
                parse_service_attribute_response(packet);
                break;
#endif
            case SDP_ServiceSearchAttributeResponse:
                parse_service_search_attribute_response(packet);
                break;
            default:
                log_error("SDP Client :: PDU ID invalid. %u ,%u", PDU_ID, packet[0]);
                return;
        }

        // continuation set or DONE?
        if (continuationStateLen == 0){
            log_info("SDP Client Query DONE! ");
            sdp_client_state = QUERY_COMPLETE;
            l2cap_disconnect_internal(sdp_cid, 0);
            // sdp_parser_handle_done(0);
            return;
        }
        // prepare next request and send
        sdp_client_state = W2_SEND;
        if (can_send_now(sdp_cid)) send_request(sdp_cid);
        return;
    }
    
    if (packet_type != HCI_EVENT_PACKET) return;
    
    switch(packet[0]){
        case L2CAP_EVENT_TIMEOUT_CHECK:
            log_info("sdp client: L2CAP_EVENT_TIMEOUT_CHECK");
            break;
        case L2CAP_EVENT_CHANNEL_OPENED:
            if (sdp_client_state != W4_CONNECT) break;
            // data: event (8), len(8), status (8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16), local_mtu(16), remote_mtu(16) 
            if (packet[2]) {
                log_error("SDP Client Connection failed.");
                sdp_parser_handle_done(packet[2]);
                break;
            }
            sdp_cid = channel;
            mtu = READ_BT_16(packet, 17);
            // handle = READ_BT_16(packet, 9);
            log_info("SDP Client Connected, cid %x, mtu %u.", sdp_cid, mtu);

            sdp_client_state = W2_SEND;
            if (can_send_now(sdp_cid)) send_request(sdp_cid);
        
            break;
        case L2CAP_EVENT_CREDITS:
        case DAEMON_EVENT_HCI_PACKET_SENT:
            if (can_send_now(sdp_cid)) send_request(sdp_cid);
            break;
        case L2CAP_EVENT_CHANNEL_CLOSED: {
            if (sdp_cid != READ_BT_16(packet, 2)) {
                // log_info("Received L2CAP_EVENT_CHANNEL_CLOSED for cid %x, current cid %x\n",  READ_BT_16(packet, 2),sdp_cid);
                break;
            }
            log_info("SDP Client disconnected.");
            uint8_t status = sdp_client_state == QUERY_COMPLETE ? 0 : SDP_QUERY_INCOMPLETE;
            sdp_client_state = INIT;
            sdp_parser_handle_done(status);
            break;
        }
        default:
            break;
    }
}


static uint16_t setup_service_search_attribute_request(uint8_t * data){

    uint16_t offset = 0;
    transactionID++;
    // uint8_t SDP_PDU_ID_t.SDP_ServiceSearchRequest;
    data[offset++] = SDP_ServiceSearchAttributeRequest;
    // uint16_t transactionID
    net_store_16(data, offset, transactionID);
    offset += 2;

    // param legnth
    offset += 2;

    // parameters: 
    //     ServiceSearchPattern - DES (min 1 UUID, max 12)
    uint16_t serviceSearchPatternLen = de_get_len(serviceSearchPattern);
    memcpy(data + offset, serviceSearchPattern, serviceSearchPatternLen);
    offset += serviceSearchPatternLen;

    //     MaximumAttributeByteCount - uint16_t  0x0007 - 0xffff -> mtu
    net_store_16(data, offset, mtu);
    offset += 2;

    //     AttibuteIDList  
    uint16_t attributeIDListLen = de_get_len(attributeIDList);
    memcpy(data + offset, attributeIDList, attributeIDListLen);
    offset += attributeIDListLen;

    //     ContinuationState - uint8_t number of cont. bytes N<=16 
    data[offset++] = continuationStateLen;
    //                       - N-bytes previous response from server
    memcpy(data + offset, continuationState, continuationStateLen);
    offset += continuationStateLen;

    // uint16_t paramLength 
    net_store_16(data, 3, offset - 5);

    return offset;
}

#ifdef HAVE_SDP_EXTRA_QUERIES
void parse_service_record_handle_list(uint8_t* packet, uint16_t total_count, uint16_t current_count){
    sdp_parser_handle_service_search(packet, total_count, current_count);
}

static uint16_t setup_service_search_request(uint8_t * data){
    uint16_t offset = 0;
    transactionID++;
    // uint8_t SDP_PDU_ID_t.SDP_ServiceSearchRequest;
    data[offset++] = SDP_ServiceSearchRequest;
    // uint16_t transactionID
    net_store_16(data, offset, transactionID);
    offset += 2;

    // param legnth
    offset += 2;

    // parameters: 
    //     ServiceSearchPattern - DES (min 1 UUID, max 12)
    uint16_t serviceSearchPatternLen = de_get_len(serviceSearchPattern);
    memcpy(data + offset, serviceSearchPattern, serviceSearchPatternLen);
    offset += serviceSearchPatternLen;

    //     MaximumAttributeByteCount - uint16_t  0x0007 - 0xffff -> mtu
    net_store_16(data, offset, mtu);
    offset += 2;

    //     ContinuationState - uint8_t number of cont. bytes N<=16 
    data[offset++] = continuationStateLen;
    //                       - N-bytes previous response from server
    memcpy(data + offset, continuationState, continuationStateLen);
    offset += continuationStateLen;

    // uint16_t paramLength 
    net_store_16(data, 3, offset - 5);

    return offset;
}


static uint16_t setup_service_attribute_request(uint8_t * data){

    uint16_t offset = 0;
    transactionID++;
    // uint8_t SDP_PDU_ID_t.SDP_ServiceSearchRequest;
    data[offset++] = SDP_ServiceAttributeRequest;
    // uint16_t transactionID
    net_store_16(data, offset, transactionID);
    offset += 2;

    // param legnth
    offset += 2;

    // parameters: 
    //     ServiceRecordHandle
    net_store_32(data, offset, serviceRecordHandle);
    offset += 4;

    //     MaximumAttributeByteCount - uint16_t  0x0007 - 0xffff -> mtu
    net_store_16(data, offset, mtu);
    offset += 2;

    //     AttibuteIDList  
    uint16_t attributeIDListLen = de_get_len(attributeIDList);
    memcpy(data + offset, attributeIDList, attributeIDListLen);
    offset += attributeIDListLen;

    //     ContinuationState - uint8_t number of cont. bytes N<=16 
    data[offset++] = continuationStateLen;
    //                       - N-bytes previous response from server
    memcpy(data + offset, continuationState, continuationStateLen);
    offset += continuationStateLen;

    // uint16_t paramLength 
    net_store_16(data, 3, offset - 5);

    return offset;
}

static void parse_service_search_response(uint8_t* packet){
    uint16_t offset = 3;
    uint16_t parameterLength = READ_NET_16(packet,offset);
    offset+=2;

    uint16_t totalServiceRecordCount = READ_NET_16(packet,offset);
    offset+=2;

    uint16_t currentServiceRecordCount = READ_NET_16(packet,offset);
    offset+=2;
    if (currentServiceRecordCount > totalServiceRecordCount){
        log_error("CurrentServiceRecordCount is larger then TotalServiceRecordCount.");
        return;
    }
    
    parse_service_record_handle_list(packet+offset, totalServiceRecordCount, currentServiceRecordCount);
    offset+=(currentServiceRecordCount * 4);

    continuationStateLen = packet[offset];
    offset++;
    if (continuationStateLen > 16){
        log_error("Error parsing ServiceSearchResponse: Number of bytes in continuation state exceedes 16.");
        return;
    }
    memcpy(continuationState, packet+offset, continuationStateLen);
    offset+=continuationStateLen;

    if (parameterLength != offset - 5){
        log_error("Error parsing ServiceSearchResponse: wrong size of parameters, number of expected bytes%u, actual number %u.", parameterLength, offset);
    }
}

static void parse_service_attribute_response(uint8_t* packet){
    uint16_t offset = 3;
    uint16_t parameterLength = READ_NET_16(packet,offset);
    offset+=2;

    // AttributeListByteCount <= mtu
    uint16_t attributeListByteCount = READ_NET_16(packet,offset);
    offset+=2;

    if (attributeListByteCount > mtu){
        log_error("Error parsing ServiceSearchAttributeResponse: Number of bytes in found attribute list is larger then the MaximumAttributeByteCount.");
        return;
    }

    // AttributeLists
    parse_attribute_lists(packet+offset, attributeListByteCount);
    offset+=attributeListByteCount;

    continuationStateLen = packet[offset];
    offset++;

    if (continuationStateLen > 16){
        log_error("Error parsing ServiceAttributeResponse: Number of bytes in continuation state exceedes 16.");
        return;
    }
    memcpy(continuationState, packet+offset, continuationStateLen);
    offset+=continuationStateLen;

    if (parameterLength != offset - 5){
        log_error("Error parsing ServiceAttributeResponse: wrong size of parameters, number of expected bytes%u, actual number %u.", parameterLength, offset);
    }
}

void sdp_client_service_attribute_search(bd_addr_t remote, uint32_t search_serviceRecordHandle, uint8_t * des_attributeIDList){
    serviceRecordHandle = search_serviceRecordHandle;
    attributeIDList = des_attributeIDList;
    continuationStateLen = 0;
    PDU_ID = SDP_ServiceAttributeResponse;

    sdp_client_state = W4_CONNECT;
    l2cap_create_channel_internal(NULL, sdp_packet_handler, remote, PSM_SDP, l2cap_max_mtu());
}

void sdp_client_service_search(bd_addr_t remote, uint8_t * des_serviceSearchPattern){
    serviceSearchPattern = des_serviceSearchPattern;
    continuationStateLen = 0;
    PDU_ID = SDP_ServiceSearchResponse;

    sdp_client_state = W4_CONNECT;
    l2cap_create_channel_internal(NULL, sdp_packet_handler, remote, PSM_SDP, l2cap_max_mtu());
}
#endif

