/*
 * Copyright (C) 2009-2013 by Matthias Ringwald
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

/*
 *  sdp_client.c
 */

#include "config.h"
#include "sdp_client.h"

#include <btstack/hci_cmds.h>

#include "l2cap.h"
#include "sdp_parser.h"
#include "sdp.h"
#include "debug.h"

typedef enum {
    INIT, W4_CONNECT, W2_SEND, W4_RESPONSE
} sdp_client_state_t;


void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t setup_sdp_request(uint8_t * data);

// SDP Client Query
static uint16_t  mtu;
static uint16_t  sdp_cid = 0x40;
static uint8_t * serviceSearchPattern;
static uint8_t * attributeIDList;
static uint16_t  transactionID = 0;
static uint8_t   continuationState[16];
static uint8_t   continuationStateLen;

static sdp_client_state_t sdp_client_state;

void sdp_client_handle_done(uint8_t status){
    if (status == 0){
        l2cap_disconnect_internal(sdp_cid, 0);
    }
    sdp_parser_handle_done(status);
}

// TODO: inline if not needed
void parse_AttributeLists(uint8_t* packet, uint16_t length){
    sdp_parser_handle_chunk(packet, length);
}


void hexdump2(void *data, int size){
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

/* Queries the SDP service of the remote device given a service search pattern 
and a list of attribute IDs. The remote data is handled by the SDP parser. The 
SDP parser delivers attribute values and done event via a registered callback. */

void sdp_client_query(bd_addr_t remote, uint8_t * des_serviceSearchPattern, uint8_t * des_attributeIDList){

    serviceSearchPattern = des_serviceSearchPattern;
    attributeIDList = des_attributeIDList;
    continuationStateLen = 0;
    
    sdp_client_state = W4_CONNECT;
    l2cap_create_channel_internal(NULL, sdp_packet_handler, remote, PSM_SDP, l2cap_max_mtu());
}


void tryToSend(uint16_t channel){


    if (sdp_client_state != W2_SEND) return;

    if (!l2cap_can_send_packet_now(channel)) return;

    uint8_t * data = l2cap_get_outgoing_buffer();
    uint16_t request_len = setup_sdp_request(data);

    printf("tryToSend channel %x, size %u\n", channel, request_len);

    int err = l2cap_send_prepared(channel, request_len);
    switch (err){
        case 0:
            // packet is sent prepare next one
            printf("l2cap_send_internal() -> OK\n\r");
            sdp_client_state = W4_RESPONSE;
            break;
        case BTSTACK_ACL_BUFFERS_FULL:
            printf("l2cap_send_internal() ->BTSTACK_ACL_BUFFERS_FULL\n\r");
            break;
        default:
            printf("l2cap_send_internal() -> err %d\n\r", err);
            break;
    }
}

void parse_ServiceSearchAttributeResponse(uint8_t* packet){
    uint16_t offset = 3;
    uint16_t parameterLength = READ_NET_16(packet,offset);
    offset+=2;
    // AttributeListByteCount <= mtu
    uint16_t attributeListByteCount = READ_NET_16(packet,offset);
    offset+=2;

    if (attributeListByteCount > mtu){
        log_error("Error parsing ServiceSearchAttributeResponse: Number of bytes in found attribute list is larger then MTU.\n");
        return;
    }

    // AttributeLists
    parse_AttributeLists(packet+offset, attributeListByteCount);
    offset+=attributeListByteCount;

    continuationStateLen = packet[offset];
    offset++;

    if (continuationStateLen > 16){
        log_error("Error parsing ServiceSearchAttributeResponse: Number of bytes in continuation state exceedes 16.\n");
        return;
    }
    memcpy(continuationState, packet+offset, continuationStateLen);
    offset+=continuationStateLen;

    if (parameterLength != offset - 5){
        log_error("Error parsing ServiceSearchAttributeResponse: wrong size of parameters, number of expected bytes%u, actual number %u.\n", parameterLength, offset);
    }
}

void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    uint16_t handle;

    printf("l2cap_packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);

    if (packet_type == L2CAP_DATA_PACKET){
        uint16_t responseTransactionID = READ_NET_16(packet,1);
        if ( responseTransactionID != transactionID){
            printf("Missmatching transaction ID, expected %u, found %u.\n", transactionID, responseTransactionID);
            return;
        } 
        
        if (packet[0] == SDP_ServiceSearchAttributeResponse){
            parse_ServiceSearchAttributeResponse(packet);

            // continuation set or DONE?
            if (continuationStateLen == 0){
                printf("DONE! All clients already notified.\n");
                sdp_client_handle_done(0);
                sdp_client_state = INIT;
                return;
            }

            // prepare next request and send
            sdp_client_state = W2_SEND;
            tryToSend(sdp_cid);
        }
        return;
    }
    
    if (packet_type != HCI_EVENT_PACKET) return;
    
    switch(packet[0]){

        case L2CAP_EVENT_CHANNEL_OPENED: 
            if (sdp_client_state != W4_CONNECT) break;

            // data: event (8), len(8), status (8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16), local_mtu(16), remote_mtu(16) 
            if (packet[2]) {
                printf("Connection failed.\n\r");
                sdp_client_handle_done(packet[2]);
                break;
            }
            sdp_cid = channel;
            mtu = READ_BT_16(packet, 17);
            handle = READ_BT_16(packet, 9);
            printf("Connected, cid %x, mtu %u.\n\r", sdp_cid, mtu);

            sdp_client_state = W2_SEND;
            tryToSend(sdp_cid);
            break;
        case L2CAP_EVENT_CREDITS:
        case DAEMON_EVENT_HCI_PACKET_SENT:
            tryToSend(sdp_cid);
            break;
        case L2CAP_EVENT_CHANNEL_CLOSED:
            printf("Channel closed.\n\r");
            if (sdp_client_state == INIT) break;
            sdp_client_handle_done(SDP_QUERY_INCOMPLETE);
            break;
        default:
            break;
    }
}

static uint16_t setup_sdp_request(uint8_t * data){

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



