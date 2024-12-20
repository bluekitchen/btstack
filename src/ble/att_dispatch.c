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

#define BTSTACK_FILE__ "att_dispatch.c"


/**
 * Dispatcher for independent implementation of ATT client and server
 */

#include "ble/att_dispatch.h"
#include "ble/core.h"
#include "btstack_debug.h"
#include "l2cap.h"
#include "btstack_event.h"

#define ATT_SERVER 0u
#define ATT_CLIENT 1u
#define ATT_MAX    2u

struct {
    btstack_packet_handler_t packet_handler;
    bool                  waiting_for_can_send;
} subscriptions[ATT_MAX];

// index of subscription that will get can send now first if waiting for it
static uint8_t att_round_robin;

// track can send now requests
static bool can_send_now_pending;

#ifdef ENABLE_GATT_OVER_CLASSIC
static att_server_t * att_dispatch_att_server_for_l2cap_cid(uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;
    hci_connections_get_iterator(&it);
    while(btstack_linked_list_iterator_has_next(&it)) {
        hci_connection_t *hci_connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
        att_server_t *att_server = &hci_connection->att_server;
        if (att_server->l2cap_cid == l2cap_cid){
            return att_server;
        }
    }
    return NULL;
}
#endif

static void att_dispatch_handle_can_send_now(uint8_t *packet, uint16_t size){
    uint8_t i;
    uint8_t index;
    uint16_t l2cap_cid = l2cap_event_can_send_now_get_local_cid(packet);
#ifdef ENABLE_GATT_OVER_CLASSIC
    if (l2cap_cid != L2CAP_CID_ATTRIBUTE_PROTOCOL){
        att_server_t * att_server = att_dispatch_att_server_for_l2cap_cid(l2cap_cid);
        if (att_server != NULL){
            for (i = 0u; i < ATT_MAX; i++) {
                index = (att_round_robin + i) & 1u;
                if (att_server->send_requests[index]) {
                    att_server->send_requests[index] = false;
                    // registered packet handlers from Unenhanced LE
                    subscriptions[index].packet_handler(HCI_EVENT_PACKET, l2cap_cid, packet, size);
                    // fairness: prioritize next service
                    att_round_robin = (index + 1u) % ATT_MAX;
                    // stop if client cannot send anymore
                    if (!l2cap_can_send_packet_now(l2cap_cid)) break;
                }
            }
            // check if more can send now events are needed
            bool send_request_pending = att_server->send_requests[ATT_CLIENT]
                                        || att_server->send_requests[ATT_SERVER];
            if (send_request_pending){
                l2cap_request_can_send_now_event(att_server->l2cap_cid);
            }
            return;
        }
    }
#endif
    can_send_now_pending = false;
    for (i = 0u; i < ATT_MAX; i++){
        index = (att_round_robin + i) & 1u;
        if ( (subscriptions[index].packet_handler != NULL) && subscriptions[index].waiting_for_can_send){
            subscriptions[index].waiting_for_can_send = false;
            subscriptions[index].packet_handler(HCI_EVENT_PACKET, l2cap_cid, packet, size);
            // fairness: prioritize next service
            att_round_robin = (index + 1u) % ATT_MAX;
            // stop if client cannot send anymore
            if (!hci_can_send_acl_le_packet_now()) break;
        }
    }
    // check if more can send now events are needed
    if (!can_send_now_pending){
        for (i = 0u; i < ATT_MAX; i++){
            if ((subscriptions[i].packet_handler != NULL) && subscriptions[i].waiting_for_can_send){
                can_send_now_pending = true;
                // note: con_handle is not used, so we can pass in anything
                l2cap_request_can_send_fix_channel_now_event(0, L2CAP_CID_ATTRIBUTE_PROTOCOL);
                break;
            }
        }
    }
}

static void att_dispatch_handle_att_pdu(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    uint8_t index;
    uint8_t opcode;
    uint8_t method;
    bool for_server;
    bool invalid;

    // parse opcode
    opcode  = packet[0u];
    method  = opcode & 0x03fu;
    invalid = method > ATT_MULTIPLE_HANDLE_VALUE_NTF;
    // odd PDUs are sent from server to client - even PDUs are sent from client to server, also let server handle invalid ones
    for_server = ((method & 1u) == 0u) || invalid;
    index = for_server ? ATT_SERVER : ATT_CLIENT;
    if (!subscriptions[index].packet_handler) return;
    subscriptions[index].packet_handler(packet_type, channel, packet, size);
}

static void att_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
#ifdef ENABLE_GATT_OVER_CLASSIC
    hci_connection_t * hci_connection;
    att_server_t * att_server;
    hci_con_handle_t con_handle;
    bool outgoing_active;
    uint8_t index;
    bd_addr_t address;
    uint16_t l2cap_cid;
    uint8_t  status;
#endif
    switch (packet_type){
        case ATT_DATA_PACKET:
            att_dispatch_handle_att_pdu(packet_type, channel, packet, size);
            break;
#ifdef ENABLE_GATT_OVER_CLASSIC
        case L2CAP_DATA_PACKET:
            att_dispatch_handle_att_pdu(packet_type, channel, packet, size);
            break;
#endif
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_CAN_SEND_NOW:
                    att_dispatch_handle_can_send_now(packet, size);
                    break;
#ifdef ENABLE_GATT_OVER_CLASSIC
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, address);
                    l2cap_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    // reject if outgoing l2cap connection active, L2CAP/TIM/BV-01-C
                    con_handle = l2cap_event_incoming_connection_get_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    btstack_assert(hci_connection != NULL);
                    outgoing_active = hci_connection->att_server.l2cap_cid != 0;
                    if (outgoing_active) {
                        hci_connection->att_server.incoming_connection_request = true;
                        l2cap_decline_connection(l2cap_cid);
                        log_info("Decline incoming connection from %s", bd_addr_to_str(address));
                    } else {
                        l2cap_accept_connection(l2cap_cid);
                        log_info("Accept incoming connection from %s", bd_addr_to_str(address));
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_OPENED:
                    // store l2cap_cid in att_server
                    status = l2cap_event_channel_opened_get_status(packet);
                    con_handle = l2cap_event_channel_opened_get_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    if (status == ERROR_CODE_SUCCESS){
                        btstack_assert(hci_connection != NULL);
                        hci_connection->att_server.l2cap_cid = l2cap_event_channel_opened_get_local_cid(packet);
                    } else {
                        if (hci_connection != NULL){
                            hci_connection->att_server.l2cap_cid = 0;
                        }
                    }
                    // dispatch to all roles
                    for (index = 0; index < ATT_MAX; index++){
                        if (subscriptions[index].packet_handler != NULL){
                            subscriptions[index].packet_handler(packet_type, channel, packet, size);
                        }
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    // clear l2cap_cid in att_server
                    l2cap_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    att_server = att_dispatch_att_server_for_l2cap_cid(l2cap_cid);
                    att_server->l2cap_cid = 0;
                    // dispatch to all roles
                    for (index = 0; index < ATT_MAX; index++){
                        if (subscriptions[index].packet_handler != NULL){
                            subscriptions[index].packet_handler(packet_type, channel, packet, size);
                        }
                    }
                    break;
#endif
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void att_dispatch_register_client(btstack_packet_handler_t packet_handler){
    subscriptions[ATT_CLIENT].packet_handler = packet_handler;
    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
}

void att_dispatch_register_server(btstack_packet_handler_t packet_handler){
    subscriptions[ATT_SERVER].packet_handler = packet_handler;
    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
}

#ifdef ENABLE_GATT_OVER_CLASSIC
static uint16_t att_dispatch_classic_get_l2cap_cid(hci_con_handle_t con_handle){
    // get l2cap_cid from att_server in hci_connection
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (hci_connection != NULL) {
        return hci_connection->att_server.l2cap_cid;
    } else {
        return 0;
    }
}
#endif


static bool att_dispatch_can_send_now(hci_con_handle_t con_handle) {
#ifdef ENABLE_GATT_OVER_CLASSIC
    uint16_t l2cap_cid = att_dispatch_classic_get_l2cap_cid(con_handle);
    if (l2cap_cid != 0){
        return l2cap_can_send_packet_now(l2cap_cid);
    }
#endif
    return l2cap_can_send_fixed_channel_packet_now(con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL);
}

bool att_dispatch_client_can_send_now(hci_con_handle_t con_handle){
    return att_dispatch_can_send_now(con_handle);
}

bool att_dispatch_server_can_send_now(hci_con_handle_t con_handle){
    return att_dispatch_can_send_now(con_handle);
}

static void att_dispatch_request_can_send_now_event(hci_con_handle_t con_handle, uint8_t type) {
#ifdef ENABLE_GATT_OVER_CLASSIC
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (hci_connection != NULL) {
        att_server_t * att_server = &hci_connection->att_server;
        if (att_server->l2cap_cid != 0){
            bool send_request_pending = att_server->send_requests[ATT_CLIENT]
                                     || att_server->send_requests[ATT_SERVER];
            att_server->send_requests[type] = true;
            if (send_request_pending == false){
                l2cap_request_can_send_now_event(att_server->l2cap_cid);
            }
            return;
        }
    }
#endif
    subscriptions[type].waiting_for_can_send = true;
    if (can_send_now_pending == false) {
        can_send_now_pending = true;
        l2cap_request_can_send_fix_channel_now_event(con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    }
}

void att_dispatch_client_request_can_send_now_event(hci_con_handle_t con_handle){
    att_dispatch_request_can_send_now_event(con_handle, ATT_CLIENT);
}

void att_dispatch_server_request_can_send_now_event(hci_con_handle_t con_handle){
    att_dispatch_request_can_send_now_event(con_handle, ATT_SERVER);
}

static void emit_mtu_exchange_complete(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle, uint16_t new_mtu){
    if (!packet_handler) return;
    uint8_t packet[6];
    packet[0] = ATT_EVENT_MTU_EXCHANGE_COMPLETE;
    packet[1] = sizeof(packet) - 2u;
    little_endian_store_16(packet, 2, con_handle);
    little_endian_store_16(packet, 4, new_mtu);
    packet_handler(HCI_EVENT_PACKET, con_handle, packet, 6);
}

void att_dispatch_server_mtu_exchanged(hci_con_handle_t con_handle, uint16_t new_mtu){
    emit_mtu_exchange_complete(subscriptions[ATT_CLIENT].packet_handler, con_handle, new_mtu);
}

void att_dispatch_client_mtu_exchanged(hci_con_handle_t con_handle, uint16_t new_mtu){
    emit_mtu_exchange_complete(subscriptions[ATT_SERVER].packet_handler, con_handle, new_mtu);
}

#ifdef ENABLE_GATT_OVER_CLASSIC
void att_dispatch_classic_register_service(void){
    l2cap_register_service(&att_packet_handler, PSM_ATT, 0xffff, gap_get_security_level());
}
uint8_t att_dispatch_classic_connect(bd_addr_t address, uint16_t l2cap_psm, uint16_t *out_cid) {
    uint16_t l2cap_cid;
    uint8_t status = l2cap_create_channel(&att_packet_handler, address, l2cap_psm, 0xffff, &l2cap_cid);
    // store l2cap_cid in hci_connection
    if (status == ERROR_CODE_SUCCESS){
        hci_connection_t * hci_connection = hci_connection_for_bd_addr_and_type(address, BD_ADDR_TYPE_ACL);
        if (hci_connection != NULL) {
            hci_connection->att_server.l2cap_cid = l2cap_cid;
            hci_connection->att_server.incoming_connection_request = false;
        }
    }
    *out_cid = l2cap_cid;
    return status;
}

#endif
