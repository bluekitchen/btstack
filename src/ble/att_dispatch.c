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

#define __BTSTACK_FILE__ "att_dispatch.c"


/**
 * Dispatcher for independent implementation of ATT client and server
 */

#include "att_dispatch.h"
#include "ble/core.h"
#include "btstack_debug.h"
#include "l2cap.h"

static btstack_packet_handler_t att_client_handler;
static btstack_packet_handler_t att_server_handler;

static uint8_t att_client_waiting_for_can_send;
static uint8_t att_server_waiting_for_can_send;

static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
	switch (packet_type){
		case ATT_DATA_PACKET:
			// log_info("att_data_packet with opcode 0x%x", packet[0]);
			if (packet[0] & 1){
				// odd PDUs are sent from server to client
				if (!att_client_handler) return;
				att_client_handler(packet_type, handle, packet, size);
			} else {
				// even PDUs are sent from client to server
				if (!att_server_handler) return;
				att_server_handler(packet_type, handle, packet, size);
			}
			break;
		case HCI_EVENT_PACKET:
			if (packet[0] != L2CAP_EVENT_CAN_SEND_NOW) break;
			if (att_server_handler && att_server_waiting_for_can_send){
				att_server_waiting_for_can_send = 0;
				att_server_handler(packet_type, handle, packet, size);
				// stop if client cannot send anymore
				if (!hci_can_send_acl_le_packet_now()) break;
			}
			if (att_client_handler && att_client_waiting_for_can_send){
				att_client_waiting_for_can_send = 0;
				att_client_handler(packet_type, handle, packet, size);
			}
			break;
		default:
			break;
	}
}

/**
 * @brief reset att dispatchter
 * @param packet_hander for ATT client packets
 */
void att_dispatch_register_client(btstack_packet_handler_t packet_handler){
	att_client_handler = packet_handler;
	l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
}

/**
 * @brief reset att dispatchter
 * @param packet_hander for ATT server packets
 */
void att_dispatch_register_server(btstack_packet_handler_t packet_handler){
	att_server_handler = packet_handler;
	l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
}

/**
 * @brief can send packet for client
 * @param handle
 */
int att_dispatch_client_can_send_now(hci_con_handle_t con_handle){
	return l2cap_can_send_fixed_channel_packet_now(con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL);
}

/**
 * @brief can send packet for server
 * @param handle
 */
int att_dispatch_server_can_send_now(hci_con_handle_t con_handle){
	return l2cap_can_send_fixed_channel_packet_now(con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL);
}

/** 
 * @brief Request emission of L2CAP_EVENT_CAN_SEND_NOW as soon as possible for client
 * @note L2CAP_EVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param con_handle
 */
void att_dispatch_client_request_can_send_now_event(hci_con_handle_t con_handle){
	att_client_waiting_for_can_send = 1;
	l2cap_request_can_send_fix_channel_now_event(con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL);
}

/** 
 * @brief Request emission of L2CAP_EVENT_CAN_SEND_NOW as soon as possible for server
 * @note L2CAP_EVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param con_handle
 */
void att_dispatch_server_request_can_send_now_event(hci_con_handle_t con_handle){
	att_server_waiting_for_can_send = 1;
	l2cap_request_can_send_fix_channel_now_event(con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL);
}
