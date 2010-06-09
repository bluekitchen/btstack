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

#include <btstack/sdp_util.h>
#include "l2cap.h"

static void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void sdp_init(){
    // register with l2cap psm sevices
    l2cap_register_service_internal(NULL, sdp_packet_handler, PSM_SDP, 250);
}

// register service record internally
// @returns ServiceRecordHandle or 0 if registration failed
uint32_t sdp_register_service_internal(uint8_t * service_record){
    return 0;
}

// unregister service record internally
void sdp_unregister_service(uint32_t service_record_handle){
}

// PDU
// PDU ID (1), Transaction ID (2), Param Length (2), Param 1, Param 2, ..

static uint8_t sdp_response_buffer[250];

static void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
	uint16_t handle;
	uint16_t psm;
	uint16_t local_cid;
	uint16_t remote_cid;
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



