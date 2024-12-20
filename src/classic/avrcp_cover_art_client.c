/*
 * Copyright (C) 2023 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "avrcp_cover_art_client.c"

#include <string.h>

#include "btstack_debug.h"
#include "btstack_event.h"
#include "classic/avrcp.h"
#include "classic/avrcp_cover_art_client.h"
#include "classic/obex.h"
#include "hci_event.h"

#ifdef ENABLE_AVRCP_COVER_ART

static const hci_event_t avrcp_cover_art_client_connected = {
    .event_code = HCI_EVENT_AVRCP_META,
    .subevent_code = AVRCP_SUBEVENT_COVER_ART_CONNECTION_ESTABLISHED,
    .format = "1B22"
};

static const hci_event_t avrcp_cover_art_client_disconnected = {
    .event_code = HCI_EVENT_AVRCP_META,
    .subevent_code = AVRCP_SUBEVENT_COVER_ART_CONNECTION_RELEASED,
    .format = "2"
};

static const hci_event_t avrcp_cover_art_client_operation_complete = {
        .event_code = HCI_EVENT_AVRCP_META,
        .subevent_code = AVRCP_SUBEVENT_COVER_ART_OPERATION_COMPLETE,
        .format = "21"
};

// 7163DD54-4A7E-11E2-B47C-0050C2490048
static const uint8_t avrcp_cover_art_uuid[] = { 0x71, 0x63, 0xDD, 0x54, 0x4A, 0x7E, 0x11, 0xE2, 0xB4, 0x7C, 0x00, 0x50, 0xC2, 0x49, 0x00, 0x48 };

// OBEX types
const char * avrcp_cover_art_image_properties_type     = "x-bt/img-properties";
const char * avrcp_cover_art_image_type               = "x-bt/img-img";
const char * avrcp_cover_art_linked_thumbnail_type     = "x-bt/img-thm";

static btstack_linked_list_t avrcp_cover_art_client_connections;

static uint16_t avrcp_cover_art_client_next_cid(void) {
    static uint16_t cid = 0;
    cid++;
    if (cid == 0){
        cid = 1;
    }
    return cid;
}

static avrcp_cover_art_client_t * avrcp_cover_art_client_for_goep_cid(uint16_t goep_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_cover_art_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_cover_art_client_t * connection = (avrcp_cover_art_client_t *)btstack_linked_list_iterator_next(&it);
        if (connection->goep_cid == goep_cid) {
            return connection;
        };
    }
    return NULL;
}

static avrcp_cover_art_client_t * avrcp_cover_art_client_for_avrcp_cid(uint16_t avrcp_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_cover_art_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_cover_art_client_t * connection = (avrcp_cover_art_client_t *)btstack_linked_list_iterator_next(&it);
        if (connection->avrcp_cid == avrcp_cid) {
            return connection;
        };
    }
    return NULL;
}

static avrcp_cover_art_client_t * avrcp_cover_art_client_for_cover_art_cid(uint16_t cover_art_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_cover_art_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_cover_art_client_t * connection = (avrcp_cover_art_client_t *)btstack_linked_list_iterator_next(&it);
        if (connection->cover_art_cid == cover_art_cid) {
            return connection;
        };
    }
    return NULL;
}

static void avrcp_cover_art_client_emit_connection_established(btstack_packet_handler_t packet_handler, uint8_t status,
                                                               bd_addr_t addr, uint16_t avrcp_cid,
                                                               uint16_t cover_art_cid) {
    uint8_t buffer[20];
    uint16_t size = hci_event_create_from_template_and_arguments(buffer, sizeof(buffer), &avrcp_cover_art_client_connected, status, addr, avrcp_cid, cover_art_cid);
    (*packet_handler)(HCI_EVENT_PACKET, 0, buffer, size);
}

static void cover_art_client_emit_operation_complete_event(avrcp_cover_art_client_t * cover_art_client, uint8_t status) {
    uint8_t buffer[20];
    uint16_t size = hci_event_create_from_template_and_arguments(buffer, sizeof(buffer), &avrcp_cover_art_client_operation_complete, cover_art_client->cover_art_cid, status);
    (*cover_art_client->packet_handler)(HCI_EVENT_PACKET, 0, buffer, size);
}

static void avrcp_cover_art_client_emit_connection_released(btstack_packet_handler_t packet_handler, uint16_t cover_art_cid) {
    uint8_t buffer[20];
    uint16_t size = hci_event_create_from_template_and_arguments(buffer, sizeof(buffer), &avrcp_cover_art_client_disconnected, cover_art_cid);
    (*packet_handler)(HCI_EVENT_PACKET, 0, buffer, size);
}

static void avrcp_cover_art_finalize_connection(avrcp_cover_art_client_t *cover_art_client) {
    btstack_assert(cover_art_client != NULL);
    memset(cover_art_client, 0, sizeof(avrcp_cover_art_client_t));
    btstack_linked_list_remove(&avrcp_cover_art_client_connections, (btstack_linked_item_t *) cover_art_client);
}

static void avrcp_cover_art_client_parser_callback_connect(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    avrcp_cover_art_client_t * client = (avrcp_cover_art_client_t *) user_data;
    switch (header_id){
        case OBEX_HEADER_CONNECTION_ID:
            if (obex_parser_header_store(client->obex_header_buffer, sizeof(client->obex_header_buffer), total_len, data_offset, data_buffer, data_len) == OBEX_PARSER_HEADER_COMPLETE){
                goep_client_set_connection_id(client->goep_cid, big_endian_read_32(client->obex_header_buffer, 0));
            }
            break;
        default:
            break;
    }
}

static void avrcp_cover_art_client_prepare_srm_header(avrcp_cover_art_client_t * cover_art_client){
    if (cover_art_client->flow_control_enabled == false){
        obex_srm_client_prepare_header(&cover_art_client->obex_srm, cover_art_client->goep_cid);
    }
}

static void avrcp_cover_art_client_parser_callback_get_operation(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    avrcp_cover_art_client_t *client = (avrcp_cover_art_client_t *) user_data;
    switch (header_id) {
        case OBEX_HEADER_SINGLE_RESPONSE_MODE:
            obex_parser_header_store(&client->obex_srm.srm_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
            obex_parser_header_store(&client->obex_srm.srmp_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_BODY:
        case OBEX_HEADER_END_OF_BODY:
            switch(client->state){
                case AVRCP_COVER_ART_W4_OBJECT:
                    client->packet_handler(BIP_DATA_PACKET, client->cover_art_cid, (uint8_t *) data_buffer, data_len);
                    if (data_offset + data_len == total_len){
                        client->flow_wait_for_user = true;
                    }
                    break;
                default:
                    btstack_unreachable();
                    break;
            }
            break;
        default:
            // ignore other headers
            break;
    }
}

static void avrcp_cover_art_client_prepare_get_operation(avrcp_cover_art_client_t * cover_art_client){
    obex_parser_init_for_response(&cover_art_client->obex_parser, OBEX_OPCODE_GET, avrcp_cover_art_client_parser_callback_get_operation, cover_art_client);
    obex_srm_client_reset_fields(&cover_art_client->obex_srm);
    cover_art_client->obex_parser_waiting_for_response = true;
}

static void avrcp_cover_art_client_handle_can_send_now(avrcp_cover_art_client_t * cover_art_client){
    switch (cover_art_client->state) {
        case AVRCP_COVER_ART_W2_SEND_CONNECT_REQUEST:
            // prepare request
            goep_client_request_create_connect(cover_art_client->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_client_header_add_target(cover_art_client->goep_cid, avrcp_cover_art_uuid, 16);
            // state
            cover_art_client->state = AVRCP_COVER_ART_W4_CONNECT_RESPONSE;
            // prepare response
            obex_parser_init_for_response(&cover_art_client->obex_parser, OBEX_OPCODE_CONNECT,
                                          avrcp_cover_art_client_parser_callback_connect, cover_art_client);
            obex_srm_client_init(&cover_art_client->obex_srm);
            cover_art_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(cover_art_client->goep_cid);
            break;
        case AVRCP_COVER_ART_W2_SEND_GET_OBJECT:
            goep_client_request_create_get(cover_art_client->goep_cid);
            if (cover_art_client->first_request){
                cover_art_client->first_request = false;
                obex_srm_client_init(&cover_art_client->obex_srm);
                avrcp_cover_art_client_prepare_srm_header(cover_art_client);
                goep_client_header_add_type(cover_art_client->goep_cid, cover_art_client->object_type);
                if (cover_art_client->image_descriptor != NULL){
                    goep_client_header_add_variable(cover_art_client->goep_cid, OBEX_HEADER_IMG_DESCRIPTOR, (const uint8_t *) cover_art_client->image_descriptor, (uint16_t) strlen(cover_art_client->image_descriptor));
                }
                uint8_t image_handle_len = btstack_max(7, (uint16_t) strlen(cover_art_client->image_handle));
                goep_client_header_add_unicode_prefix(cover_art_client->goep_cid, OBEX_HEADER_IMG_HANDLE, cover_art_client->image_handle, image_handle_len);
            }
            // state
            cover_art_client->state = AVRCP_COVER_ART_W4_OBJECT;
            cover_art_client->flow_next_triggered = 0;
            cover_art_client->flow_wait_for_user = 0;
            // prepare response
            avrcp_cover_art_client_prepare_get_operation(cover_art_client);
            // send packet
            goep_client_execute(cover_art_client->goep_cid);
            break;
        case AVRCP_COVER_ART_W2_SEND_DISCONNECT_REQUEST:
            // prepare request
            goep_client_request_create_disconnect(cover_art_client->goep_cid);
            // state
            cover_art_client->state = AVRCP_COVER_ART_W4_DISCONNECT_RESPONSE;
            // prepare response
            obex_parser_init_for_response(&cover_art_client->obex_parser, OBEX_OPCODE_DISCONNECT, NULL, cover_art_client);
            cover_art_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(cover_art_client->goep_cid);
            return;
        default:
            break;
    }
}

static void avrcp_cover_art_goep_event_handler(const uint8_t *packet, uint16_t size) {
    UNUSED(size);
    uint8_t status;
    avrcp_cover_art_client_t * cover_art_client;
    btstack_packet_handler_t packet_handler;
    uint16_t cover_art_cid;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_GOEP_META:
            switch (hci_event_goep_meta_get_subevent_code(packet)){
                case GOEP_SUBEVENT_CONNECTION_OPENED:
                    cover_art_client = avrcp_cover_art_client_for_goep_cid(goep_subevent_connection_opened_get_goep_cid(packet));
                    btstack_assert(cover_art_client != NULL);
                    status = goep_subevent_connection_opened_get_status(packet);
                    if (status){
                        log_info("connection failed %u", status);
                        avrcp_cover_art_finalize_connection(cover_art_client);
                        avrcp_cover_art_client_emit_connection_established(cover_art_client->packet_handler, status,
                                                                           cover_art_client->addr,
                                                                           cover_art_client->avrcp_cid,
                                                                           cover_art_client->cover_art_cid);
                    } else {
                        log_info("connection established");
                        cover_art_client->state = AVRCP_COVER_ART_W2_SEND_CONNECT_REQUEST;
                        goep_client_request_can_send_now(cover_art_client->goep_cid);
                    }
                    break;
                case GOEP_SUBEVENT_CONNECTION_CLOSED:
                    cover_art_client = avrcp_cover_art_client_for_goep_cid(goep_subevent_connection_opened_get_goep_cid(packet));
                    btstack_assert(cover_art_client != NULL);
                    if (cover_art_client->state > AVRCP_COVER_ART_CONNECTED){
                        cover_art_client_emit_operation_complete_event(cover_art_client, OBEX_DISCONNECTED);
                    }
                    cover_art_cid = cover_art_client->cover_art_cid;
                    packet_handler = cover_art_client->packet_handler;
                    avrcp_cover_art_finalize_connection(cover_art_client);
                    avrcp_cover_art_client_emit_connection_released(packet_handler, cover_art_cid);
                    break;
                case GOEP_SUBEVENT_CAN_SEND_NOW:
                    cover_art_client = avrcp_cover_art_client_for_goep_cid(goep_subevent_can_send_now_get_goep_cid(packet));
                    btstack_assert(cover_art_client != NULL);
                    avrcp_cover_art_client_handle_can_send_now(cover_art_client);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void avrcp_cover_art_client_goep_data_handler(avrcp_cover_art_client_t * cover_art_client, uint8_t *packet, uint16_t size){
    btstack_assert(cover_art_client->obex_parser_waiting_for_response);

    obex_parser_object_state_t parser_state;
    parser_state = obex_parser_process_data(&cover_art_client->obex_parser, packet, size);
    if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE){
        cover_art_client->obex_parser_waiting_for_response = false;
        obex_parser_operation_info_t op_info;
        obex_parser_get_operation_info(&cover_art_client->obex_parser, &op_info);
        switch (cover_art_client->state){
            case AVRCP_COVER_ART_W4_CONNECT_RESPONSE:
                switch (op_info.response_code) {
                    case OBEX_RESP_SUCCESS:
                        cover_art_client->state = AVRCP_COVER_ART_CONNECTED;
                        avrcp_cover_art_client_emit_connection_established(cover_art_client->packet_handler,
                                                                           ERROR_CODE_SUCCESS,
                                                                           cover_art_client->addr,
                                                                           cover_art_client->avrcp_cid,
                                                                           cover_art_client->cover_art_cid);
                        break;
                    default:
                        log_info("pbap: obex connect failed, result 0x%02x", packet[0]);
                        cover_art_client->state = AVRCP_COVER_ART_INIT;
                        avrcp_cover_art_client_emit_connection_established(cover_art_client->packet_handler,
                                                                           OBEX_CONNECT_FAILED,
                                                                           cover_art_client->addr,
                                                                           cover_art_client->avrcp_cid,
                                                                           cover_art_client->cover_art_cid);
                        break;
                }
                break;
            case AVRCP_COVER_ART_W4_OBJECT:
                switch (op_info.response_code) {
                    case OBEX_RESP_CONTINUE:
                        obex_srm_client_handle_headers(&cover_art_client->obex_srm);
                        if (obex_srm_client_is_srm_active(&cover_art_client->obex_srm)) {
                            // prepare response
                            avrcp_cover_art_client_prepare_get_operation(cover_art_client);
                            break;
                        }
                        cover_art_client->state = AVRCP_COVER_ART_W2_SEND_GET_OBJECT;
                        if (!cover_art_client->flow_control_enabled || !cover_art_client->flow_wait_for_user ||
                            cover_art_client->flow_next_triggered) {
                            goep_client_request_can_send_now(cover_art_client->goep_cid);
                        }
                        break;
                    case OBEX_RESP_SUCCESS:
                        cover_art_client->state = AVRCP_COVER_ART_CONNECTED;
                        cover_art_client_emit_operation_complete_event(cover_art_client, ERROR_CODE_SUCCESS);
                        break;
                    default:
                        log_info("unexpected response 0x%02x", packet[0]);
                        cover_art_client->state = AVRCP_COVER_ART_CONNECTED;
                        cover_art_client_emit_operation_complete_event(cover_art_client, OBEX_UNKNOWN_ERROR);
                        break;
                }
                break;
            case AVRCP_COVER_ART_W4_DISCONNECT_RESPONSE:
                goep_client_disconnect(cover_art_client->goep_cid);
                break;
            default:
                btstack_unreachable();
                break;
        }
    }
}

static void avrcp_cover_art_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel); // ok: there is no channel
    UNUSED(size);    // ok: handling own goep events
    avrcp_cover_art_client_t * cover_art_client;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            avrcp_cover_art_goep_event_handler(packet, size);
            break;
        case GOEP_DATA_PACKET:
            cover_art_client = avrcp_cover_art_client_for_goep_cid(channel);
            btstack_assert(cover_art_client != NULL);
            avrcp_cover_art_client_goep_data_handler(cover_art_client, packet, size);
            break;
        default:
            break;
    }
}

static uint8_t avrcp_cover_art_client_setup_connection(avrcp_cover_art_client_t * cover_art_client, uint16_t l2cap_psm){
    cover_art_client->state = AVRCP_COVER_ART_W4_GOEP_CONNECTION;
    return goep_client_connect_l2cap(&cover_art_client->goep_client,
                                     (l2cap_ertm_config_t *) cover_art_client->ertm_config,
                                     cover_art_client->ertm_buffer,
                                     cover_art_client->ertm_buffer_size,
                                     &avrcp_cover_art_packet_handler,
                                     cover_art_client->addr,
                                     l2cap_psm,
                                     &cover_art_client->goep_cid);
}

static void avrcp_cover_art_handle_sdp_query_complete(avrcp_connection_t * connection, uint8_t status){
    avrcp_cover_art_client_t * cover_art_client = avrcp_cover_art_client_for_avrcp_cid(connection->avrcp_cid);

    if (cover_art_client == NULL) {
        return;
    }
    if (cover_art_client->state != AVRCP_COVER_ART_W4_SDP_QUERY_COMPLETE){
        return;
    }

    // l2cap available?
    if (status == ERROR_CODE_SUCCESS){
        if (connection->cover_art_psm == 0){
            status = SDP_SERVICE_NOT_FOUND;
        }
    }

    if (status == ERROR_CODE_SUCCESS) {
        // ready to connect
        cover_art_client->state = AVRCP_COVER_ART_W2_GOEP_CONNECT;
        status = avrcp_cover_art_client_setup_connection(cover_art_client, connection->cover_art_psm);
    }

    if (status != ERROR_CODE_SUCCESS){
        btstack_packet_handler_t packet_handler = cover_art_client->packet_handler;
        uint16_t cover_art_cid = cover_art_client->cover_art_cid;
        uint16_t avrcp_cid =  cover_art_client->avrcp_cid;
        avrcp_cover_art_finalize_connection(cover_art_client);
        avrcp_cover_art_client_emit_connection_established(packet_handler, status, connection->remote_addr,
                                                           avrcp_cid, cover_art_cid);
    }
}

void avrcp_cover_art_client_init(void){
    avrcp_register_cover_art_sdp_query_complete_handler(&avrcp_cover_art_handle_sdp_query_complete);
}

uint8_t
avrcp_cover_art_client_connect(avrcp_cover_art_client_t *cover_art_client, btstack_packet_handler_t packet_handler,
                               bd_addr_t remote_addr, uint8_t *ertm_buffer, uint32_t ertm_buffer_size,
                               const l2cap_ertm_config_t *ertm_config, uint16_t *avrcp_cover_art_cid) {

    avrcp_connection_t * connection_controller = avrcp_get_connection_for_bd_addr_for_role(AVRCP_CONTROLLER, remote_addr);
    avrcp_connection_t * connection_target = avrcp_get_connection_for_bd_addr_for_role(AVRCP_TARGET, remote_addr);
    if ((connection_target == NULL) || (connection_controller == NULL)){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    cover_art_client->cover_art_cid = avrcp_cover_art_client_next_cid();
    memcpy(cover_art_client->addr, remote_addr, 6);
    cover_art_client->avrcp_cid = connection_controller->avrcp_cid;
    cover_art_client->packet_handler = packet_handler;
    cover_art_client->flow_control_enabled = false;

    // store ERTM config
    cover_art_client->ertm_config = ertm_config;
    cover_art_client->ertm_buffer = ertm_buffer;
    cover_art_client->ertm_buffer_size = ertm_buffer_size;

    if (avrcp_cover_art_cid != NULL){
        *avrcp_cover_art_cid = cover_art_client->cover_art_cid;
    }

    btstack_linked_list_add(&avrcp_cover_art_client_connections, (btstack_linked_item_t *) cover_art_client);

    if (connection_controller->cover_art_psm == 0){
        cover_art_client->state = AVRCP_COVER_ART_W4_SDP_QUERY_COMPLETE;
        avrcp_trigger_sdp_query(connection_controller, connection_controller);
        return ERROR_CODE_SUCCESS;
    } else {
        return avrcp_cover_art_client_setup_connection(cover_art_client, connection_controller->cover_art_psm);
    }
}

static uint8_t avrcp_cover_art_client_get_object(uint16_t avrcp_cover_art_cid, const char * object_type, const char * image_handle, const char * image_descriptor){
    avrcp_cover_art_client_t * cover_art_client = avrcp_cover_art_client_for_cover_art_cid(avrcp_cover_art_cid);
    if (cover_art_client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (cover_art_client->state != AVRCP_COVER_ART_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    cover_art_client->state = AVRCP_COVER_ART_W2_SEND_GET_OBJECT;
    cover_art_client->first_request = true;
    cover_art_client->image_handle = image_handle;
    cover_art_client->image_descriptor = image_descriptor;
    cover_art_client->object_type = object_type;
    goep_client_request_can_send_now(cover_art_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_cover_art_client_get_linked_thumbnail(uint16_t avrcp_cover_art_cid, const char * image_handle){
    return avrcp_cover_art_client_get_object(avrcp_cover_art_cid,
                                             avrcp_cover_art_linked_thumbnail_type,
                                             image_handle,
                                             NULL);
}

uint8_t avrcp_cover_art_client_get_image(uint16_t avrcp_cover_art_cid, const char * image_handle, const char * image_descriptor){
    return avrcp_cover_art_client_get_object(avrcp_cover_art_cid,
                                             avrcp_cover_art_image_type,
                                             image_handle,
                                             image_descriptor);
}

uint8_t avrcp_cover_art_client_get_image_properties(uint16_t avrcp_cover_art_cid, const char * image_handle){
    return avrcp_cover_art_client_get_object(avrcp_cover_art_cid,
                                             avrcp_cover_art_image_properties_type,
                                             image_handle,
                                             NULL);
}

uint8_t avrcp_cover_art_client_disconnect(uint16_t avrcp_cover_art_cid){
    avrcp_cover_art_client_t * cover_art_client = avrcp_cover_art_client_for_cover_art_cid(avrcp_cover_art_cid);
    if (cover_art_client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (cover_art_client->state < AVRCP_COVER_ART_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    cover_art_client->state = AVRCP_COVER_ART_W2_SEND_DISCONNECT_REQUEST;
    goep_client_request_can_send_now(cover_art_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

void avrcp_cover_art_client_deinit(void){
    avrcp_cover_art_client_connections = NULL;
}

#endif /* ENABLE_AVRCP_COVER_ART */
