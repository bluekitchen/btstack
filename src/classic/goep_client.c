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

#define __BTSTACK_FILE__ "goep_client.c"
 
#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_debug.h"
#include "hci_dump.h"
#include "bluetooth_sdp.h"
#include "btstack_event.h"
#include "classic/goep_client.h"
#include "classic/obex.h"
#include "classic/obex_iterator.h"
#include "classic/rfcomm.h"
#include "classic/sdp_client_rfcomm.h"

//------------------------------------------------------------------------------------------------------------
// goep_client.c
//

typedef enum {
    GOEP_INIT,
    GOEP_W4_SDP,
    GOEP_W4_CONNECTION,
    GOEP_CONNECTED,
} goep_state_t;

typedef struct {
    uint16_t         cid;
    goep_state_t     state;
    bd_addr_t        bd_addr;
    hci_con_handle_t con_handle;
    uint8_t          incoming;
    uint8_t          bearer_l2cap;
    uint16_t         bearer_port;   // l2cap: psm, rfcomm: channel nr
    uint16_t         bearer_cid;
    uint16_t         bearer_mtu;

    uint8_t          obex_opcode;
    uint32_t         obex_connection_id;
    int              obex_connection_id_set;

    btstack_packet_handler_t client_handler;
} goep_client_t;

static goep_client_t _goep_client;
static goep_client_t * goep_client = &_goep_client;

static inline void goep_client_emit_connected_event(goep_client_t * context, uint8_t status){
    uint8_t event[22];
    int pos = 0;
    event[pos++] = HCI_EVENT_GOEP_META;
    pos++;  // skip len
    event[pos++] = GOEP_SUBEVENT_CONNECTION_OPENED;
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

static inline void goep_client_emit_connection_closed_event(goep_client_t * context){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_GOEP_META;
    pos++;  // skip len
    event[pos++] = GOEP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("goep_client_emit_connection_closed_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static inline void goep_client_emit_can_send_now_event(goep_client_t * context){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_GOEP_META;
    pos++;  // skip len
    event[pos++] = GOEP_SUBEVENT_CAN_SEND_NOW;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("goep_client_emit_can_send_now_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static void goep_client_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case RFCOMM_EVENT_CHANNEL_OPENED:
                    status = rfcomm_event_channel_opened_get_status(packet);
                    if (status) {
                        log_info("goep_client: RFCOMM channel open failed, status %u", rfcomm_event_channel_opened_get_status(packet));
                        goep_client->state = GOEP_INIT;
                    } else {
                        goep_client->bearer_mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        log_info("goep_client: RFCOMM channel open succeeded. cid %u, max frame size %u", goep_client->bearer_cid, goep_client->bearer_mtu);
                        goep_client->state = GOEP_CONNECTED;
                    }                    
                    goep_client_emit_connected_event(goep_client, status);
                    return;
                case RFCOMM_EVENT_CAN_SEND_NOW:
                    goep_client_emit_can_send_now_event(goep_client);
                    break;
                case RFCOMM_CHANNEL_CLOSED:
                    goep_client->state = GOEP_INIT;
                    goep_client_emit_connection_closed_event(goep_client);
                    break;
                default:
                    break;
            }
            break;
        case RFCOMM_DATA_PACKET:
            goep_client->client_handler(GOEP_DATA_PACKET, goep_client->cid, packet, size);
            break;
        default:
            break;
    }
}

static void goep_client_handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch (packet[0]){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            goep_client->bearer_port = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (goep_client->bearer_port == 0){
                log_info("Remote GOEP RFCOMM Server Channel not found");
                goep_client->state = GOEP_INIT;
                goep_client_emit_connected_event(goep_client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                break;
            }
            log_info("Remote GOEP RFCOMM Server Channel: %u", goep_client->bearer_port);
            rfcomm_create_channel(&goep_client_packet_handler, goep_client->bd_addr, goep_client->bearer_port, &goep_client->bearer_cid);
            break;
    }
}

static void goep_client_packet_append(const uint8_t * data, uint16_t len){
     uint8_t * buffer = rfcomm_get_outgoing_buffer();
     uint16_t pos = big_endian_read_16(buffer, 1);
     memcpy(&buffer[pos], data, len);
     pos += len;
     big_endian_store_16(buffer, 1, pos);
}

static void goep_client_packet_init(uint16_t goep_cid, uint8_t opcode){
    UNUSED(goep_cid);
    rfcomm_reserve_packet_buffer();
    uint8_t * buffer = rfcomm_get_outgoing_buffer();
    buffer[0] = opcode;
    big_endian_store_16(buffer, 1, 3);
    // store opcode for parsing of response
    goep_client->obex_opcode = opcode;
}

static void goep_client_packet_add_connection_id(uint16_t goep_cid){
    UNUSED(goep_cid);
    // add connection_id header if set, must be first header if used
    if (goep_client->obex_connection_id != OBEX_CONNECTION_ID_INVALID){
        uint8_t header[5];
        header[0] = OBEX_HEADER_CONNECTION_ID;
        big_endian_store_32(header, 1, goep_client->obex_connection_id);
        goep_client_packet_append(&header[0], sizeof(header));
    }
}

void goep_client_init(void){
    memset(goep_client, 0, sizeof(goep_client_t));
    goep_client->state = GOEP_INIT;
    goep_client->cid = 1;
    goep_client->obex_connection_id = OBEX_CONNECTION_ID_INVALID;
}

uint8_t goep_client_create_connection(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t uuid, uint16_t * out_cid){
    if (goep_client->state != GOEP_INIT) return BTSTACK_MEMORY_ALLOC_FAILED;
    goep_client->client_handler = handler;
    goep_client->state = GOEP_W4_SDP;
    memcpy(goep_client->bd_addr, addr, 6);
    sdp_client_query_rfcomm_channel_and_name_for_uuid(&goep_client_handle_query_rfcomm_event, goep_client->bd_addr, uuid);
    *out_cid = goep_client->cid;
    return 0;
}

uint8_t goep_client_disconnect(uint16_t goep_cid){
    UNUSED(goep_cid);
    rfcomm_disconnect(goep_client->bearer_cid);
    return 0;
}

void goep_client_set_connection_id(uint16_t goep_cid, uint32_t connection_id){
    UNUSED(goep_cid);
    goep_client->obex_connection_id = connection_id;
}

uint8_t goep_client_get_request_opcode(uint16_t goep_cid){
    UNUSED(goep_cid);
    return goep_client->obex_opcode;
}

void goep_client_request_can_send_now(uint16_t goep_cid){
    UNUSED(goep_cid);
    rfcomm_request_can_send_now_event(goep_client->bearer_cid);
}

void goep_client_create_connect_request(uint16_t goep_cid, uint8_t obex_version_number, uint8_t flags, uint16_t maximum_obex_packet_length){
    UNUSED(goep_cid);
    goep_client_packet_init(goep_cid, OBEX_OPCODE_CONNECT);
    uint8_t fields[4];
    fields[0] = obex_version_number;
    fields[1] = flags;
    // workaround: limit OBEX packet len to RFCOMM MTU to avoid handling of fragemented packets
    maximum_obex_packet_length = btstack_min(maximum_obex_packet_length, goep_client->bearer_mtu);
    big_endian_store_16(fields, 2, maximum_obex_packet_length);
    goep_client_packet_append(&fields[0], sizeof(fields));
}

void goep_client_create_get_request(uint16_t goep_cid){
    UNUSED(goep_cid);
    goep_client_packet_init(goep_cid, OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK);
    goep_client_packet_add_connection_id(goep_cid);
}

void goep_client_create_set_path_request(uint16_t goep_cid, uint8_t flags){
    UNUSED(goep_cid);
    goep_client_packet_init(goep_cid, OBEX_OPCODE_SETPATH);
    uint8_t fields[2];
    fields[0] = flags;
    fields[1] = 0;  // reserved
    goep_client_packet_append(&fields[0], sizeof(fields));
    goep_client_packet_add_connection_id(goep_cid);
}

void goep_client_add_header_target(uint16_t goep_cid, uint16_t length, const uint8_t * target){
    UNUSED(goep_cid);
    uint8_t header[3];
    header[0] = OBEX_HEADER_TARGET;
    big_endian_store_16(header, 1, 1 + 2 + length);
    goep_client_packet_append(&header[0], sizeof(header));
    goep_client_packet_append(target, length);
}

void goep_client_add_header_name(uint16_t goep_cid, const char * name){
    UNUSED(goep_cid);
    int len_incl_zero = strlen(name) + 1;
    uint8_t * buffer = rfcomm_get_outgoing_buffer();
    uint16_t pos = big_endian_read_16(buffer, 1);
    buffer[pos++] = OBEX_HEADER_NAME;
    big_endian_store_16(buffer, pos, 1 + 2 + len_incl_zero*2);
    pos += 2;
    int i;
    // @note name[len] == 0 
    for (i = 0 ; i < len_incl_zero ; i++){
        buffer[pos++] = 0;
        buffer[pos++] = *name++;
    }
    big_endian_store_16(buffer, 1, pos);
 }

void goep_client_add_header_type(uint16_t goep_cid, const char * type){
    UNUSED(goep_cid);
    uint8_t header[3];
    header[0] = OBEX_HEADER_TYPE;
    int len_incl_zero = strlen(type) + 1;
    big_endian_store_16(header, 1, 1 + 2 + len_incl_zero);
    goep_client_packet_append(&header[0], sizeof(header));
    goep_client_packet_append((const uint8_t*)type, len_incl_zero);
}

int goep_client_execute(uint16_t goep_cid){
    UNUSED(goep_cid);
    uint8_t * buffer = rfcomm_get_outgoing_buffer();
    uint16_t pos = big_endian_read_16(buffer, 1);
    return rfcomm_send_prepared(goep_client->bearer_cid, pos);
}
