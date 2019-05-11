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

#define BTSTACK_FILE__ "att_dispatch.c"


/**
 * Dispatcher for independent implementation of ATT client and server
 */

#include "att_dispatch.h"
#include "ble/core.h"
#include "btstack_debug.h"
#include "l2cap.h"

#define ATT_SERVER 0
#define ATT_CLIENT 1
#define ATT_MAX    2

struct {
    btstack_packet_handler_t packet_handler;
    uint8_t                  waiting_for_can_send;
} subscriptions[ATT_MAX];

// index of subscription that will get can send now first if waiting for it
static uint8_t att_round_robin;

// track can send now requests
static uint8_t can_send_now_pending;

static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    uint8_t index;
    uint8_t i;
    switch (packet_type){
        case ATT_DATA_PACKET:
            // odd PDUs are sent from server to client - even PDUs are sent from client to server
            index = packet[0] & 1;
            // log_info("att_data_packet with opcode 0x%x", packet[0]);
            if (!subscriptions[index].packet_handler) return;
            subscriptions[index].packet_handler(packet_type, handle, packet, size);
            break;
        case HCI_EVENT_PACKET:
            if (packet[0] != L2CAP_EVENT_CAN_SEND_NOW) break;
            can_send_now_pending = 0;
            for (i = 0; i < ATT_MAX; i++){
                index = (att_round_robin + i) & 1;
                if (subscriptions[index].packet_handler && subscriptions[index].waiting_for_can_send){
                    subscriptions[index].waiting_for_can_send = 0;
                    subscriptions[index].packet_handler(packet_type, handle, packet, size);
                    // fairness: prioritize next service
                    att_round_robin = (index + 1) % ATT_MAX;
                    // stop if client cannot send anymore
                    if (!hci_can_send_acl_le_packet_now()) break;
                }
            }
            // check if more can send now events are needed
            if (!can_send_now_pending){
                for (i = 0; i < ATT_MAX; i++){
                    if (subscriptions[i].packet_handler && subscriptions[i].waiting_for_can_send){
                        can_send_now_pending = 1;        
                        // note: con_handle is not used, so we can pass in anything
                        l2cap_request_can_send_fix_channel_now_event(0, L2CAP_CID_ATTRIBUTE_PROTOCOL);
                        break;
                    }
                }
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
    subscriptions[ATT_CLIENT].packet_handler = packet_handler;
    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
}

/**
 * @brief reset att dispatchter
 * @param packet_hander for ATT server packets
 */
void att_dispatch_register_server(btstack_packet_handler_t packet_handler){
    subscriptions[ATT_SERVER].packet_handler = packet_handler;
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
    subscriptions[ATT_CLIENT].waiting_for_can_send = 1;
    if (!can_send_now_pending){
        can_send_now_pending = 1;        
        l2cap_request_can_send_fix_channel_now_event(con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    }
}

/** 
 * @brief Request emission of L2CAP_EVENT_CAN_SEND_NOW as soon as possible for server
 * @note L2CAP_EVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param con_handle
 */
void att_dispatch_server_request_can_send_now_event(hci_con_handle_t con_handle){
    subscriptions[ATT_SERVER].waiting_for_can_send = 1;
    if (!can_send_now_pending){
        can_send_now_pending = 1;        
        l2cap_request_can_send_fix_channel_now_event(con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    }
}

static void emit_mtu_exchange_complete(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle, uint16_t new_mtu){
    if (!packet_handler) return;
    uint8_t packet[6];
    packet[0] = ATT_EVENT_MTU_EXCHANGE_COMPLETE;
    packet[1] = sizeof(packet) - 2;
    little_endian_store_16(packet, 2, con_handle);
    little_endian_store_16(packet, 4, new_mtu);
    packet_handler(HCI_EVENT_PACKET, con_handle, packet, 1);
}

void att_dispatch_server_mtu_exchanged(hci_con_handle_t con_handle, uint16_t new_mtu){
    emit_mtu_exchange_complete(subscriptions[ATT_CLIENT].packet_handler, con_handle, new_mtu);
}

void att_dispatch_client_mtu_exchanged(hci_con_handle_t con_handle, uint16_t new_mtu){
    emit_mtu_exchange_complete(subscriptions[ATT_SERVER].packet_handler, con_handle, new_mtu);
}
