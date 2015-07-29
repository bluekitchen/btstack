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
 *  l2cap_le.c
 *
 *  Logical Link Control and Adaption Protocol (L2CAP) for Bluetooth Low Energy
 *
 *  Created by Matthias Ringwald on 5/16/09.
 */

#include "l2cap.h"
#include "hci.h"
#include "hci_dump.h"
#include "debug.h"
#include "btstack_memory.h"

#include <stdarg.h>
#include <string.h>

#include <stdio.h>

static void l2cap_packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size);

static void (*packet_handler) (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static btstack_packet_handler_t attribute_protocol_packet_handler;
static btstack_packet_handler_t security_protocol_packet_handler;

void l2cap_init(void){
    
    packet_handler = NULL;
    attribute_protocol_packet_handler = NULL;
    security_protocol_packet_handler = NULL;
    
    // 
    // register callback with HCI
    //
    hci_register_packet_handler(&l2cap_packet_handler);
    hci_connectable_control(0); // no services yet
}

uint16_t l2cap_max_mtu(void){
    return HCI_ACL_PAYLOAD_SIZE - L2CAP_HEADER_SIZE;
}

uint16_t l2cap_max_le_mtu(void){
    return l2cap_max_mtu();
}

/** Register L2CAP packet handlers */
void l2cap_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

// @deprecated
int l2cap_can_send_connectionless_packet_now(void){
    // TODO provide real handle
    return l2cap_can_send_fixed_channel_packet_now(0x1234);
}

int  l2cap_can_send_fixed_channel_packet_now(uint16_t handle){
    return hci_can_send_acl_packet_now(handle);
}

uint8_t *l2cap_get_outgoing_buffer(void){
    return hci_get_outgoing_packet_buffer() + COMPLETE_L2CAP_HEADER; // 8 bytes
}

int l2cap_reserve_packet_buffer(void){
    return hci_reserve_packet_buffer();
}

void l2cap_release_packet_buffer(void){
    hci_release_packet_buffer();
}

int l2cap_send_prepared_connectionless(uint16_t handle, uint16_t cid, uint16_t len){
    
    if (!hci_is_packet_buffer_reserved()){
        log_error("l2cap_send_prepared_connectionless called without reserving packet first");
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    if (!hci_can_send_prepared_acl_packet_now(handle)){
        log_info("l2cap_send_prepared_connectionless handle %u,, cid %u, cannot send", handle, cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    log_debug("l2cap_send_prepared_connectionless handle %u, cid %u", handle, cid);
    
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();

    // 0 - Connection handle : PB=00 : BC=00 
    bt_store_16(acl_buffer, 0, handle | (0 << 12) | (0 << 14));
    // 2 - ACL length
    bt_store_16(acl_buffer, 2,  len + 4);
    // 4 - L2CAP packet length
    bt_store_16(acl_buffer, 4,  len + 0);
    // 6 - L2CAP channel DEST
    bt_store_16(acl_buffer, 6, cid);    
    // send
    int err = hci_send_acl_packet_buffer(len+8);
        
    return err;
}

int l2cap_send_connectionless(uint16_t handle, uint16_t cid, uint8_t *data, uint16_t len){

    if (!hci_can_send_acl_packet_now(handle)){
        log_info("l2cap_send_connectionless cid %u, cannot send", cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();

    memcpy(&acl_buffer[8], data, len);

    return l2cap_send_prepared_connectionless(handle, cid, len);
}

void l2cap_emit_connection_parameter_update_response(uint16_t handle, uint16_t result){
    uint8_t event[6];
    event[0] = L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE;
    event[1] = 4;
    bt_store_16(event, 2, handle);
    bt_store_16(event, 4, result);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    (*packet_handler)(NULL, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void l2cap_run(void){ 
    // send l2cap con paramter update if necessary
    hci_connections_get_iterator(&it);
    while(linked_list_iterator_has_next(&it)){
        hci_connection_t * connection = (hci_connection_t *) linked_list_iterator_next(&it);
        int result;

        switch (connection->le_con_parameter_update_state){
            case CON_PARAMETER_UPDATE_SEND_RESPONSE:
                result = 0;
                break;
            case CON_PARAMETER_UPDATE_DENY:
                result = 1;
                break;
            default:
                result = -1;
                break;
        }
        if (result < 0) break;
        
        if (!hci_can_send_acl_packet_now(connection->con_handle)) break;
        hci_reserve_packet_buffer();
        uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
        connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_NONE;
        uint16_t len = l2cap_le_create_connection_parameter_update_response(acl_buffer, connection->con_handle, result);
        hci_send_acl_packet_buffer(len);
        if (result == 0){
            connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_CHANGE_HCI_CON_PARAMETERS;
        }
    }
}

void l2cap_event_handler( uint8_t *packet, uint16_t size ){
    
    // pass on
    if (packet_handler) {
        (*packet_handler)(NULL, HCI_EVENT_PACKET, 0, packet, size);
    }
    if (attribute_protocol_packet_handler){
        (*attribute_protocol_packet_handler)(HCI_EVENT_PACKET, 0, packet, size);
    } 
    if (security_protocol_packet_handler) {
        (*security_protocol_packet_handler)(HCI_EVENT_PACKET, 0, packet, size);
    }
}

void l2cap_acl_handler( uint8_t *packet, uint16_t size ){
        
    // Get Channel ID
    uint16_t channel_id = READ_L2CAP_CHANNEL_ID(packet); 
    hci_con_handle_t handle = READ_ACL_CONNECTION_HANDLE(packet);
    
    switch (channel_id) {
            
        case L2CAP_CID_ATTRIBUTE_PROTOCOL:
            if (attribute_protocol_packet_handler) {
                (*attribute_protocol_packet_handler)(ATT_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;

        case L2CAP_CID_SECURITY_MANAGER_PROTOCOL:
            if (security_protocol_packet_handler) {
                (*security_protocol_packet_handler)(SM_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;
            
        
        case L2CAP_CID_SIGNALING_LE:
            switch (packet[8]){
                case CONNECTION_PARAMETER_UPDATE_RESPONSE: {
                    uint16_t result = READ_BT_16(packet, 12);
                    l2cap_emit_connection_parameter_update_response(handle, result);
                    break;
                }
                case CONNECTION_PARAMETER_UPDATE_REQUEST: {
                    uint8_t event[10];
                    event[0] = L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_REQUEST;
                    event[1] = 8;
                    memcpy(&event[2], &packet[12], 8);
                
                    hci_connection_t * connection = hci_connection_for_handle(handle);
                    if (connection){ 
                        int update_parameter = 1;
                        le_connection_parameter_range_t existing_range = gap_le_get_connection_parameter_range();
                        uint16_t le_conn_interval_min = READ_BT_16(packet,12);
                        uint16_t le_conn_interval_max = READ_BT_16(packet,14);
                        uint16_t le_conn_latency = READ_BT_16(packet,16);
                        uint16_t le_supervision_timeout = READ_BT_16(packet,18);

                        if (le_conn_interval_min < existing_range.le_conn_interval_min) update_parameter = 0;
                        if (le_conn_interval_max > existing_range.le_conn_interval_max) update_parameter = 0;
                        
                        if (le_conn_latency < existing_range.le_conn_latency_min) update_parameter = 0;
                        if (le_conn_latency > existing_range.le_conn_latency_max) update_parameter = 0;

                        if (le_supervision_timeout < existing_range.le_supervision_timeout_min) update_parameter = 0;
                        if (le_supervision_timeout > existing_range.le_supervision_timeout_max) update_parameter = 0;

                        if (update_parameter){
                            connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_SEND_RESPONSE;
                            connection->le_conn_interval_min = le_conn_interval_min;
                            connection->le_conn_interval_max = le_conn_interval_max;
                            connection->le_conn_latency = le_conn_latency;
                            connection->le_supervision_timeout = le_supervision_timeout;
                        } else {
                            connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_DENY;
                        }
                    }
                    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
                    (*packet_handler)(NULL, HCI_EVENT_PACKET, 0, event, sizeof(event));
                    break;
                }
                default: {
                    // TODO: send cmd unknown
                    // uint8_t sig_id = packet[COMPLETE_L2CAP_HEADER + 1]; 
                    // l2cap_register_signaling_response(handle, COMMAND_REJECT_LE, sig_id, L2CAP_REJ_CMD_UNKNOWN);
                    break;
                }
            }
            break;

        default:
            break;

    }
}

static void l2cap_packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            l2cap_event_handler(packet, size);
            break;
        case HCI_ACL_DATA_PACKET:
            l2cap_acl_handler(packet, size);
            break;
        default:
            break;
    }
    l2cap_run();
}


// Bluetooth 4.0 - allow to register handler for Attribute Protocol and Security Manager Protocol
void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id) {
    switch(channel_id){
        case L2CAP_CID_ATTRIBUTE_PROTOCOL:
            attribute_protocol_packet_handler = packet_handler;
            break;
        case L2CAP_CID_SECURITY_MANAGER_PROTOCOL:
            security_protocol_packet_handler = packet_handler;
            break;
    }
}
