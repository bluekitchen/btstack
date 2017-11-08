/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "avrcp_browsing_controller.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "classic/avrcp.h"
#include "classic/avrcp_browsing_controller.h"

void avrcp_browser_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avrcp_context_t * context);
static void avrcp_browsing_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static avrcp_connection_t * get_avrcp_connection_for_browsing_cid(uint16_t browsing_cid, avrcp_context_t * context){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *)  &context->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
    	avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->avrcp_browsing_cid != browsing_cid) continue;
        return connection;
    }
    return NULL;
}

static avrcp_connection_t * get_avrcp_connection_for_browsing_l2cap_cid(uint16_t browsing_l2cap_cid, avrcp_context_t * context){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *)  &context->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
    	avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->l2cap_browsing_cid != browsing_l2cap_cid) continue;
        return connection;
    }
    return NULL;
}

static avrcp_browsing_connection_t * get_avrcp_browsing_connection_for_l2cap_cid(uint16_t l2cap_cid, avrcp_context_t * context){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *)  &context->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
    	avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->l2cap_browsing_cid != l2cap_cid) continue;
        return connection->browsing_connection;
    }
    return NULL;
}

static void avrcp_emit_browsing_connection_established(btstack_packet_handler_t callback, uint16_t browsing_cid, bd_addr_t addr, uint8_t status){
    if (!callback) return;
    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED;
    event[pos++] = status;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_emit_browsing_connection_closed(btstack_packet_handler_t callback, uint16_t browsing_cid){
    if (!callback) return;
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_CONNECTION_RELEASED;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static avrcp_browsing_connection_t * avrcp_browsing_create_connection(avrcp_connection_t * avrcp_connection){
	avrcp_browsing_connection_t * connection = btstack_memory_avrcp_browsing_connection_get();
    memset(connection, 0, sizeof(avrcp_browsing_connection_t));
    connection->state = AVCTP_CONNECTION_IDLE;
    connection->transaction_label = 0xFF;
    avrcp_connection->avrcp_browsing_cid = avrcp_get_next_cid();
    avrcp_connection->browsing_connection = connection;
    return connection;
}

static uint8_t avrcp_browsing_connect(bd_addr_t remote_addr, avrcp_context_t * context, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config, uint16_t * browsing_cid){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_bd_addr(remote_addr, context);
    if (!avrcp_connection){
        log_error("avrcp: there is no previously established AVRCP controller connection.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (connection){
        return ERROR_CODE_SUCCESS;
    }

    connection = avrcp_browsing_create_connection(avrcp_connection);
    if (!connection){
        printf("avrcp: could not allocate connection struct.");
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
    
    if (!browsing_cid) return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    
    *browsing_cid = avrcp_connection->avrcp_browsing_cid; 
    connection->ertm_buffer = ertm_buffer;
    connection->ertm_buffer_size = size;
    avrcp_connection->browsing_connection = connection;

    memcpy(&connection->ertm_config, ertm_config, sizeof(l2cap_ertm_config_t));

    return l2cap_create_ertm_channel(avrcp_browsing_controller_packet_handler, remote_addr, avrcp_connection->browsing_l2cap_psm, 
                    &connection->ertm_config, connection->ertm_buffer, connection->ertm_buffer_size, NULL);

}

void avrcp_browser_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avrcp_context_t * context){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status;
    avrcp_browsing_connection_t * connection = NULL;
    avrcp_connection_t * avrcp_connection = NULL;
    
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            avrcp_emit_browsing_connection_closed(context->avrcp_callback, 0);
            break;
        case L2CAP_EVENT_INCOMING_CONNECTION:
            l2cap_event_incoming_connection_get_address(packet, event_addr);
            local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
            avrcp_connection = get_avrcp_connection_for_bd_addr(event_addr, context);
            if (!avrcp_connection) {
                log_error("No previously created AVRCP controller connections");
                l2cap_decline_connection(local_cid);
                break;
            }
            connection = avrcp_browsing_create_connection(avrcp_connection);
            avrcp_connection->browsing_connection = connection;
            avrcp_connection->l2cap_browsing_cid = local_cid;
            connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
            log_info("L2CAP_EVENT_INCOMING_CONNECTION browsing_cid 0x%02x, l2cap_signaling_cid 0x%02x", avrcp_connection->avrcp_browsing_cid, avrcp_connection->l2cap_browsing_cid);
            // l2cap_accept_connection(local_cid);
            printf("L2CAP Accepting incoming connection request in ERTM\n"); 
            l2cap_accept_ertm_connection(local_cid, &connection->ertm_config, connection->ertm_buffer, connection->ertm_buffer_size);
            break;
            
        case L2CAP_EVENT_CHANNEL_OPENED:
            l2cap_event_channel_opened_get_address(packet, event_addr);
            status = l2cap_event_channel_opened_get_status(packet);
            local_cid = l2cap_event_channel_opened_get_local_cid(packet);
            
            avrcp_connection = get_avrcp_connection_for_bd_addr(event_addr, context);
            if (!avrcp_connection){
                log_error("Failed to find AVRCP connection for bd_addr %s", bd_addr_to_str(event_addr));
                avrcp_emit_browsing_connection_established(context->avrcp_callback, local_cid, event_addr, L2CAP_LOCAL_CID_DOES_NOT_EXIST);
                l2cap_disconnect(local_cid, 0); // reason isn't used
                break;
            }

            connection = avrcp_connection->browsing_connection;
            if (!connection){
                log_error("Failed to alloc AVRCP connection structure");
                avrcp_emit_browsing_connection_established(context->avrcp_callback, local_cid, event_addr, BTSTACK_MEMORY_ALLOC_FAILED);
                l2cap_disconnect(local_cid, 0); // reason isn't used
                break;
            }

            if (status != ERROR_CODE_SUCCESS){
                log_info("L2CAP connection to connection %s failed. status code 0x%02x", bd_addr_to_str(event_addr), status);
                avrcp_emit_browsing_connection_established(context->avrcp_callback, avrcp_connection->avrcp_browsing_cid, event_addr, status);
                btstack_memory_avrcp_browsing_connection_free(connection);
                avrcp_connection->browsing_connection = NULL;
                break;
            }
            avrcp_connection->l2cap_browsing_cid = local_cid;

            log_info("L2CAP_EVENT_CHANNEL_OPENED browsing cid 0x%02x, l2cap cid 0x%02x", avrcp_connection->avrcp_browsing_cid, avrcp_connection->l2cap_browsing_cid);
            connection->state = AVCTP_CONNECTION_OPENED;
            avrcp_emit_browsing_connection_established(context->avrcp_callback, avrcp_connection->avrcp_browsing_cid, event_addr, ERROR_CODE_SUCCESS);
            break;
        
        case L2CAP_EVENT_CHANNEL_CLOSED:
            // data: event (8), len(8), channel (16)
            local_cid = l2cap_event_channel_closed_get_local_cid(packet);
            avrcp_connection = get_avrcp_connection_for_browsing_l2cap_cid(local_cid, context);
            
            if (avrcp_connection && avrcp_connection->browsing_connection){
                avrcp_emit_browsing_connection_closed(context->avrcp_callback, avrcp_connection->avrcp_browsing_cid);
                // free connection
                btstack_memory_avrcp_browsing_connection_free(avrcp_connection->browsing_connection);
                avrcp_connection->browsing_connection = NULL;
                break;
            }
            break;
        default:
            break;
    }
}

// static void avrcp_handle_l2cap_data_packet_for_browsing_connection(avrcp_browsing_connection_t * connection, uint8_t *packet, uint16_t size){

// }

// static void avrcp_browsing_controller_handle_can_send_now(avrcp_browsing_connection_t * connection){
//     int i;
//     switch (connection->state){
//         case AVCTP_CONNECTION_OPENED:
//             return;
//         default:
//             return;
//     }
// }

static void avrcp_browsing_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avrcp_browsing_connection_t * connection;

    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            connection = get_avrcp_browsing_connection_for_l2cap_cid(channel, &avrcp_controller_context);
            if (!connection) break;
            // avrcp_handle_l2cap_data_packet_for_browsing_connection(connection, packet, size);
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case L2CAP_EVENT_CAN_SEND_NOW:
                    connection = get_avrcp_browsing_connection_for_l2cap_cid(channel, &avrcp_controller_context);
                    if (!connection) break;
                    // avrcp_browsing_controller_handle_can_send_now(connection);
                    break;
            default:
                avrcp_browser_packet_handler(packet_type, channel, packet, size, &avrcp_controller_context);
                break;
        }
        default:
            break;
    }
}

void avrcp_browsing_controller_init(void){
	avrcp_controller_context.browsing_packet_handler = avrcp_browsing_controller_packet_handler;
    l2cap_register_service(&avrcp_browsing_controller_packet_handler, BLUETOOTH_PROTOCOL_AVCTP, 0xffff, LEVEL_0);
}

uint8_t avrcp_browsing_controller_connect(bd_addr_t bd_addr, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config, uint16_t * avrcp_browsing_cid){
    return avrcp_browsing_connect(bd_addr, &avrcp_controller_context, ertm_buffer, size, ertm_config, avrcp_browsing_cid);
}

uint8_t avrcp_browsing_controller_disconnect(uint16_t avrcp_browsing_cid){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(avrcp_browsing_cid, &avrcp_controller_context);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_disconnect: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (avrcp_connection->browsing_connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    l2cap_disconnect(avrcp_connection->l2cap_browsing_cid, 0);
    return ERROR_CODE_SUCCESS;
}
