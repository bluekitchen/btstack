/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "map_notification_server.c"
 
#include "btstack_config.h"

#include "btstack_debug.h"
#include "btstack_event.h"
#include "classic/obex.h"
#include "classic/goep_server.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAP_MAX_NUM_ENTRIES 1024

static const uint8_t map_client_notification_service_uuid[] = {0xbb, 0x58, 0x2b, 0x41, 0x42, 0xc, 0x11, 0xdb, 0xb0, 0xde, 0x8, 0x0, 0x20, 0xc, 0x9a, 0x66};
// map access service bb582b40-420c-11db-b0de-0800200c9a66

static uint16_t maximum_obex_packet_length;

typedef enum {
    MAP_INIT = 0,
    MAP_W2_SEND_CONNECT_RESPONSE,
    MAP_CONNECTED,
    MAP_W2_SEND_DISCONNECT_RESPONSE,
} map_notification_connection_state_t;


typedef struct {
    uint16_t  goep_cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    map_notification_connection_state_t state;
    uint16_t  cid;
    uint16_t maximum_obex_packet_length;
    uint8_t  flags;
} map_notification_connection_t;

typedef struct {
    btstack_packet_handler_t callback;
//    map_connection_t connection;
} map_notification_server_t;

static map_notification_connection_t  map_notification_connection;
static map_notification_server_t      map_notification_server;

// dummy lookup
static map_notification_connection_t * map_notification_connection_for_goep_cid(uint16_t goep_cid){
    return &map_notification_connection;
}

static void map_notification_send_connect_response(uint16_t goep_cid){
    goep_server_response_create_connect(goep_cid, OBEX_VERSION, 0, maximum_obex_packet_length);
    goep_server_header_add_who(goep_cid, map_client_notification_service_uuid);
    goep_server_execute(goep_cid, OBEX_RESP_SUCCESS);
}

static void map_notification_send_general_response(uint16_t goep_cid, uint8_t response_code){
    goep_server_response_create_general(goep_cid);
    goep_server_execute(goep_cid, response_code);
}

static uint8_t goep_data_packet_get_opcode(uint8_t *packet){
    return packet[0];
}

static void map_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel); 
    UNUSED(size);    
    int i;
    uint8_t status;
    map_notification_connection_t * notification_connection;
    uint16_t goep_cid;

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_GOEP_META:
                    switch (hci_event_goep_meta_get_subevent_code(packet)){
                        case GOEP_SUBEVENT_INCOMING_CONNECTION:
                            goep_cid = goep_subevent_incoming_connection_get_goep_cid(packet);
                            // TODO: setup connection struct from pool, replace dummy code
                            notification_connection = &map_notification_connection;
                            notification_connection->goep_cid = goep_subevent_incoming_connection_get_goep_cid(packet);
                            status = goep_server_accept_connection(goep_cid);
                            btstack_assert(status == ERROR_CODE_SUCCESS);
                            break;
                        case GOEP_SUBEVENT_CONNECTION_OPENED:
                            goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
                            notification_connection = map_notification_connection_for_goep_cid(goep_cid);
                            btstack_assert(notification_connection != NULL);
                            status = goep_subevent_connection_opened_get_status(packet);
                            printf("MNS GOEP Connection opened 0x%02x\n", status);
                            if (status == ERROR_CODE_SUCCESS){
                                notification_connection->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                                goep_subevent_connection_opened_get_bd_addr(packet, notification_connection->bd_addr);
                                notification_connection->state = MAP_CONNECTED;
                                // TODO: emit connection success event
                                // map_emit_connected_event(&notification_connection, status);
                            } else {
                                log_info("MAP notification server: connection failed %u", status);
                                // TODO: emit connection failed event
                                // map_emit_connected_event(&map_server.connection, status);
                            }
                            break;
                        case GOEP_SUBEVENT_CONNECTION_CLOSED:
                            goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
                            notification_connection = map_notification_connection_for_goep_cid(goep_cid);
                            btstack_assert(notification_connection != NULL);
                            log_info("MAP notification server: connection closed");
                            printf("MNS GOEP Connection closed\n");
                            notification_connection->state = MAP_INIT;
                            // TODO: emit connection closed event
                            // map_emit_connection_closed_event(&map_server.connection);
                            break;
                        case GOEP_SUBEVENT_CAN_SEND_NOW:
                            goep_cid = goep_subevent_incoming_connection_get_goep_cid(packet);
                            notification_connection = map_notification_connection_for_goep_cid(goep_cid);
                            btstack_assert(notification_connection != NULL);
                            switch (notification_connection->state){
                                case MAP_W2_SEND_CONNECT_RESPONSE:
                                    notification_connection->state = MAP_CONNECTED;
                                    map_notification_send_connect_response(goep_cid);
                                    break;
                                case MAP_W2_SEND_DISCONNECT_RESPONSE:
                                    // TODO: should we disconnect after sending the disconnect response?
                                    notification_connection->state = MAP_CONNECTED;
                                    map_notification_send_general_response(goep_cid, OBEX_RESP_SUCCESS);
                                    break;
                                default:
                                    break;
                            }
                            break;
                        default:
                            break;
                    }
                default:
                    break;
            }
            break;
        case GOEP_DATA_PACKET:
            notification_connection = map_notification_connection_for_goep_cid(channel);
            btstack_assert(notification_connection != NULL);
            if (notification_connection->state != MAP_CONNECTED) return;
            
            if (size < 3) break;
            switch (goep_data_packet_get_opcode(packet)){
                case OBEX_OPCODE_CONNECT:
                    notification_connection->state = MAP_W2_SEND_CONNECT_RESPONSE;
                    notification_connection->flags = packet[2];
                    notification_connection->maximum_obex_packet_length = btstack_min(maximum_obex_packet_length, big_endian_read_16(packet, 3));
                    break;
                 case OBEX_OPCODE_DISCONNECT:
                     notification_connection->state = MAP_W2_SEND_DISCONNECT_RESPONSE;
                     break;
                // case OBEX_OPCODE_ABORT:
                // case OBEX_OPCODE_PUT:
                // case OBEX_OPCODE_GET:
                // case OBEX_OPCODE_SETPATH:
                default:
                    printf("MAP server: GOEP data packet'");
                    for (i=0;i<size;i++){
                        printf("%02x ", packet[i]);
                    }
                    printf("'\n"); 
                    return;
            }
            goep_server_request_can_send_now(channel);
            break;
        
        default:
            break;
    }
}

void map_notification_server_init(uint8_t rfcomm_channel_nr, uint16_t l2cap_psm, uint16_t mtu){
    maximum_obex_packet_length = mtu;
    goep_server_register_service(&map_packet_handler, rfcomm_channel_nr, 0xFFFF, l2cap_psm, 0xFFFF, LEVEL_0);
}

void map_notification_server_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    map_notification_server.callback = callback;
}
