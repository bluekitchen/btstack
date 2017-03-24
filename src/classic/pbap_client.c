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

#define __BTSTACK_FILE__ "pbap_client.c"
 
// *****************************************************************************
//
#if 0
    0x0000 = uint32(65542),
    // BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS_PSE
    0x0001 = { uuid16(11 2f) }, 
    // BLUETOOTH_PROTOCOL_L2CAP, BLUETOOTH_PROTOCOL_RFCOMM, BLUETOOTH_PROTOCOL_OBEX
    0x0004 = { { uuid16(01 00) }, { uuid16(00 03), uint8(19) }, { uuid16(00 08) } }
    0x0005 = { uuid16(10 02) },
    // BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS, v1.01 = 0x101
    0x0009 = { { uuid16(11 30), uint16(257) } },
    0x0100 = string(OBEX Phonebook Access Server
    // BLUETOOTH_ATTRIBUTE_SUPPORTED_FEATURES -- should be 0x317 BLUETOOTH_ATTRIBUTE_PBAP_SUPPORTED_FEATURES?
    0x0311 = uint8(3),
    // BLUETOOTH_ATTRIBUTE_SUPPORTED_REPOSITORIES
    0x0314 = uint8(1),
#endif
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hci_cmd.h"
#include "btstack_run_loop.h"
#include "btstack_debug.h"
#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "bluetooth_sdp.h"
#include "classic/sdp_client_rfcomm.h"
#include "btstack_event.h"

#include "classic/obex.h"
#include "classic/obex_iterator.h"
#include "classic/goep_client.h"
#include "classic/pbap_client.h"

// 796135f0-f0c5-11d8-0966- 0800200c9a66
uint8_t pbap_uuid[] = { 0x79, 0x61, 0x35, 0xf0, 0xf0, 0xc5, 0x11, 0xd8, 0x09, 0x66, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66};
const char * pbap_type = "x-bt/phonebook";
const char * pbap_name = "pb.vcf";

typedef enum {
    PBAP_INIT = 0,
    PBAP_W4_GOEP_CONNECTION,
    PBAP_W2_SEND_CONNECT_REQUEST,
    PBAP_W4_CONNECT_RESPONSE,
    PBAP_CONNECT_RESPONSE_RECEIVED,
    PBAP_CONNECTED,
    //
    PBAP_W2_PULL_PHONE_BOOK,
    PBAP_W4_PHONE_BOOK,
    PBAP_W2_SET_PATH_ROOT,
    PBAP_W4_SET_PATH_ROOT_COMPLETE,
    PBAP_W2_SET_PATH_ELEMENT,
    PBAP_W4_SET_PATH_ELEMENT_COMPLETE,
} pbap_state_t;

typedef struct pbap_client {
    pbap_state_t state;
    uint16_t  cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint8_t   incoming;
    uint16_t  goep_cid;
    btstack_packet_handler_t client_handler;
    const char * current_folder;
    uint16_t set_path_offset;
} pbap_client_t;

static pbap_client_t _pbap_client;
static pbap_client_t * pbap_client = &_pbap_client;

static inline void pbap_client_emit_connected_event(pbap_client_t * context, uint8_t status){
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_PBAP_META;
    pos++;  // skip len
    event[pos++] = PBAP_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++] = status;
    memcpy(&event[pos], context->bd_addr, 6);
    pos += 6;
    little_endian_store_16(event,pos,context->con_handle);
    pos += 2;
    event[pos++] = context->incoming;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("goep_client_emit_connected_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static inline void pbap_client_emit_connection_closed_event(pbap_client_t * context){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_PBAP_META;
    pos++;  // skip len
    event[pos++] = PBAP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("pbap_client_emit_connection_closed_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static inline void pbap_client_emit_operation_complete_event(pbap_client_t * context, uint8_t status){
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_PBAP_META;
    pos++;  // skip len
    event[pos++] = PBAP_SUBEVENT_OPERATION_COMPLETED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++]= status;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("pbap_client_emit_can_send_now_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}

static void pbap_handle_can_send_now(void){
    uint8_t  path_element[20];
    uint16_t path_element_start;
    uint16_t path_element_len;

    switch (pbap_client->state){
        case PBAP_W2_SEND_CONNECT_REQUEST:
            goep_client_create_connect_request(pbap_client->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_client_add_header_target(pbap_client->goep_cid, 16, pbap_uuid);
            // state
            pbap_client->state = PBAP_W4_CONNECT_RESPONSE;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            return;
        case PBAP_W2_PULL_PHONE_BOOK:
            goep_client_create_get_request(pbap_client->goep_cid);
            goep_client_add_header_type(pbap_client->goep_cid, pbap_type);
            goep_client_add_header_name(pbap_client->goep_cid, pbap_name);
            // state
            pbap_client->state = PBAP_W4_PHONE_BOOK;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_SET_PATH_ROOT:
            goep_client_create_set_path_request(pbap_client->goep_cid, 1 << 1); // Don’t create directory
            // On Android 4.2 Cyanogenmod, using "" as path fails
            // goep_client_add_header_name(pbap_client->goep_cid, "");     // empty == /
            // state
            pbap_client->state = PBAP_W4_SET_PATH_ROOT_COMPLETE;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_SET_PATH_ELEMENT:
            // find '/' or '\0'
            path_element_start = pbap_client->set_path_offset;
            while (pbap_client->current_folder[pbap_client->set_path_offset] != '\0' &&
                pbap_client->current_folder[pbap_client->set_path_offset] != '/'){
                pbap_client->set_path_offset++;              
            }
            // skip /
            if (pbap_client->current_folder[pbap_client->set_path_offset] == '/'){
                pbap_client->set_path_offset++;  
            }
            path_element_len = pbap_client->set_path_offset-path_element_start;
            memcpy(path_element, &pbap_client->current_folder[path_element_start], path_element_len);
            path_element[path_element_len] = 0;

            goep_client_create_set_path_request(pbap_client->goep_cid, 1 << 1); // Don’t create directory
            goep_client_add_header_name(pbap_client->goep_cid, (const char *) path_element); // next element
            // state
            pbap_client->state = PBAP_W4_SET_PATH_ELEMENT_COMPLETE;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            break;
        default:
            break;
    }
}

static void pbap_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    obex_iterator_t it;
    uint8_t status;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_GOEP_META:
                    switch (hci_event_goep_meta_get_subevent_code(packet)){
                        case GOEP_SUBEVENT_CONNECTION_OPENED:
                            status = goep_subevent_connection_opened_get_status(packet);
                            pbap_client->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                            pbap_client->incoming = goep_subevent_connection_opened_get_incoming(packet);
                            goep_subevent_connection_opened_get_bd_addr(packet, pbap_client->bd_addr); 
                            if (status){
                                log_info("pbap: connection failed %u", status);
                                pbap_client->state = PBAP_INIT;
                                pbap_client_emit_connected_event(pbap_client, status);
                            } else {
                                log_info("pbap: connection established");
                                pbap_client->goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
                                pbap_client->state = PBAP_W2_SEND_CONNECT_REQUEST;
                                goep_client_request_can_send_now(pbap_client->goep_cid);
                            }
                            break;
                        case GOEP_SUBEVENT_CONNECTION_CLOSED:
                            if (pbap_client->state != PBAP_CONNECTED){
                                pbap_client_emit_operation_complete_event(pbap_client, OBEX_DISCONNECTED);
                            }
                            pbap_client->state = PBAP_INIT;
                            pbap_client_emit_connection_closed_event(pbap_client);
                            break;
                        case GOEP_SUBEVENT_CAN_SEND_NOW:
                            pbap_handle_can_send_now();
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        case GOEP_DATA_PACKET:
            // TODO: handle chunked data
#if 0
            obex_dump_packet(goep_client_get_request_opcode(pbap_client->goep_cid), packet, size);
#endif
            switch (pbap_client->state){
                case PBAP_W4_CONNECT_RESPONSE:
                    for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(pbap_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
                        uint8_t hi = obex_iterator_get_hi(&it);
                        if (hi == OBEX_HEADER_CONNECTION_ID){
                            goep_client_set_connection_id(pbap_client->goep_cid, obex_iterator_get_data_32(&it));
                        }
                    }
                    if (packet[0] == OBEX_RESP_SUCCESS){
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_connected_event(pbap_client, 0);
                    } else {
                        log_info("pbap: obex connect failed, result 0x%02x", packet[0]);
                        pbap_client->state = PBAP_INIT;
                        pbap_client_emit_connected_event(pbap_client, OBEX_CONNECT_FAILED);
                    }
                    break;
                case PBAP_W4_SET_PATH_ROOT_COMPLETE:
                case PBAP_W4_SET_PATH_ELEMENT_COMPLETE:
                    if (packet[0] == OBEX_RESP_SUCCESS){
                        if (pbap_client->current_folder){
                            pbap_client->state = PBAP_W2_SET_PATH_ELEMENT;
                            goep_client_request_can_send_now(pbap_client->goep_cid);
                        } else {
                            pbap_client_emit_operation_complete_event(pbap_client, 0);
                        }
                    } else if (packet[0] == OBEX_RESP_NOT_FOUND){
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, OBEX_NOT_FOUND);
                    } else {
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, OBEX_UNKNOWN_ERROR);
                    }
                    break;
                case PBAP_W4_PHONE_BOOK:
                    for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(pbap_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
                        uint8_t hi = obex_iterator_get_hi(&it);
                        if (hi == OBEX_HEADER_BODY || hi == OBEX_HEADER_END_OF_BODY){
                            uint16_t     data_len = obex_iterator_get_data_len(&it);
                            const uint8_t  * data =  obex_iterator_get_data(&it);
                            pbap_client->client_handler(PBAP_DATA_PACKET, pbap_client->cid, (uint8_t *) data, data_len);
                        }
                    }
                    if (packet[0] == OBEX_RESP_CONTINUE){
                        pbap_client->state = PBAP_W2_PULL_PHONE_BOOK;
                        goep_client_request_can_send_now(pbap_client->goep_cid);                
                    } else if (packet[0] == OBEX_RESP_SUCCESS){
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, 0);
                    } else {
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, OBEX_UNKNOWN_ERROR);
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void pbap_client_init(void){
    memset(pbap_client, 0, sizeof(pbap_client_t));
    pbap_client->state = PBAP_INIT;
    pbap_client->cid = 1;
}

uint8_t pbap_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid){
    if (pbap_client->state != PBAP_INIT) return BTSTACK_MEMORY_ALLOC_FAILED;
    pbap_client->state = PBAP_W4_GOEP_CONNECTION;
    pbap_client->client_handler = handler;
    uint8_t err = goep_client_create_connection(&pbap_packet_handler, addr, BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS_PSE, &pbap_client->goep_cid);
    *out_cid = pbap_client->cid;
    if (err) return err;
    return 0;
}

uint8_t pbap_disconnect(uint16_t pbap_cid){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED) return BTSTACK_BUSY;
    goep_client_disconnect(pbap_client->goep_cid);
    return 0;
}

uint8_t pbap_pull_phonebook(uint16_t pbap_cid){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED) return BTSTACK_BUSY;
    pbap_client->state = PBAP_W2_PULL_PHONE_BOOK;
    goep_client_request_can_send_now(pbap_client->goep_cid);                
    return 0;
}

uint8_t pbap_set_phonebook(uint16_t pbap_cid, const char * path){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED) return BTSTACK_BUSY;
    pbap_client->state = PBAP_W2_SET_PATH_ROOT;
    pbap_client->current_folder = path;
    pbap_client->set_path_offset = 0;
    goep_client_request_can_send_now(pbap_client->goep_cid);                
    return 0;
}
