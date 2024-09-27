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

#define BTSTACK_FILE__ "goep_server.c"
 
#include "btstack_config.h"

#include <stdint.h>
#include <string.h>

#include "hci_cmd.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "btstack_event.h"

#include "classic/goep_server.h"
#include "classic/obex.h"
#include "classic/obex_message_builder.h"

#ifdef ENABLE_GOEP_L2CAP
#include "l2cap.h"

static l2cap_ertm_config_t ertm_config = {
    1,  // ertm mandatory
    2,  // max transmit, some tests require > 1
    2000,
    12000,
    (GOEP_SERVER_ERTM_BUFFER / 2),    // l2cap ertm mtu
    4,
    4,
    1,      // 16-bit FCS
};

static uint8_t goep_server_l2cap_packet_buffer[GOEP_SERVER_ERTM_BUFFER];

#endif

static btstack_linked_list_t goep_server_connections = NULL;
static btstack_linked_list_t goep_server_services = NULL;
static uint16_t goep_server_cid_counter = 0;

static goep_server_service_t * goep_server_get_service_for_rfcomm_channel(uint8_t rfcomm_channel){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) goep_server_services; it ; it = it->next){
        goep_server_service_t * service = ((goep_server_service_t *) it);
        if (service->rfcomm_channel == rfcomm_channel){
            return service;
        };
    }
    return NULL;
}

#ifdef ENABLE_GOEP_L2CAP
static goep_server_service_t * goep_server_get_service_for_l2cap_psm(uint16_t l2cap_psm){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) goep_server_services; it ; it = it->next){
        goep_server_service_t * service = ((goep_server_service_t *) it);
        if (service->l2cap_psm == l2cap_psm){
            return service;
        };
    }
    return NULL;
}
#endif

static goep_server_connection_t * goep_server_get_connection_for_rfcomm_cid(uint16_t bearer_cid){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) goep_server_connections; it ; it = it->next){
        goep_server_connection_t * connection = ((goep_server_connection_t *) it);
        if (connection->type != GOEP_CONNECTION_RFCOMM) continue;
        if (connection->bearer_cid == bearer_cid){
            return connection;
        };
    }
    return NULL;
}

#ifdef ENABLE_GOEP_L2CAP
static goep_server_connection_t * goep_server_get_connection_for_l2cap_cid(uint16_t bearer_cid){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) goep_server_connections; it ; it = it->next){
        goep_server_connection_t * connection = ((goep_server_connection_t *) it);
        if (connection->type != GOEP_CONNECTION_L2CAP) continue;
        if (connection->bearer_cid == bearer_cid){
            return connection;
        };
    }
    return NULL;
}
#endif

static goep_server_connection_t * goep_server_get_connection_for_goep_cid(uint16_t goep_cid){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) goep_server_connections; it ; it = it->next){
        goep_server_connection_t * connection = ((goep_server_connection_t *) it);
        if (connection->goep_cid == goep_cid){
            return connection;
        };
    }
    return NULL;
}

static uint16_t goep_server_get_next_goep_cid(void){
    goep_server_cid_counter++;
    if (goep_server_cid_counter == 0){
        goep_server_cid_counter = 1;
    }
    return goep_server_cid_counter;
}

static inline void goep_server_emit_incoming_connection(btstack_packet_handler_t callback, uint16_t goep_cid, bd_addr_t bd_addr, hci_con_handle_t con_handle){
    uint8_t event[13];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GOEP_META;
    event[pos++] = 15 - 2;
    event[pos++] = GOEP_SUBEVENT_INCOMING_CONNECTION;
    little_endian_store_16(event, pos, goep_cid);
    pos+=2;
    reverse_bd_addr(bd_addr, &event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    btstack_assert(pos == sizeof(event));
    callback(HCI_EVENT_PACKET, goep_cid, &event[0], pos);
}

static inline void goep_server_emit_connection_opened(btstack_packet_handler_t callback, uint16_t goep_cid, bd_addr_t bd_addr, hci_con_handle_t con_handle, uint8_t status){
    uint8_t event[15];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GOEP_META;
    event[pos++] = 15 - 2;
    event[pos++] = GOEP_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event, pos, goep_cid);
    pos+=2;
    event[pos++] = status;
    reverse_bd_addr(bd_addr, &event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = 1;
    btstack_assert(pos == sizeof(event));
    callback(HCI_EVENT_PACKET, goep_cid, &event[0], pos);
}   

static inline void goep_server_emit_connection_closed(btstack_packet_handler_t callback, uint16_t goep_cid){
    uint8_t event[5];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GOEP_META;
    event[pos++] = 5 - 3;
    event[pos++] = GOEP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event, pos, goep_cid);
    pos += 2;
    btstack_assert(pos == sizeof(event));
    callback(HCI_EVENT_PACKET, goep_cid, &event[0], pos);
} 

static inline void goep_server_emit_can_send_now_event(goep_server_connection_t * connection){
    uint8_t event[5];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GOEP_META;
    event[pos++] = 5 - 3;
    event[pos++] = GOEP_SUBEVENT_CAN_SEND_NOW;
    little_endian_store_16(event,pos,connection->goep_cid);
    pos += 2;
    btstack_assert(pos == sizeof(event));
    connection->callback(HCI_EVENT_PACKET, connection->goep_cid, &event[0], pos);
}

static void goep_server_handle_connection_opened(goep_server_connection_t * context, bd_addr_t addr, hci_con_handle_t con_handle, uint8_t status, uint16_t bearer_cid, uint16_t bearer_mtu){

    uint16_t goep_cid = context->goep_cid;
    btstack_packet_handler_t packet_handler = context->callback;

    if (status) {
        log_info("goep_client: open failed, status %u", status);
        btstack_linked_list_remove(&goep_server_connections, (btstack_linked_item_t *) context);
        btstack_memory_goep_server_connection_free(context);
    } else {
        // context->bearer_mtu = mtu;
        context->state = GOEP_SERVER_CONNECTED;
        context->bearer_cid = bearer_cid;
#ifdef ENABLE_GOEP_L2CAP
        if (context->type == GOEP_CONNECTION_L2CAP){
            bearer_mtu = btstack_min(bearer_mtu, sizeof(goep_server_l2cap_packet_buffer));
        }
#endif
        context->bearer_mtu = bearer_mtu;
        log_info("goep_server: connection opened. cid %u, max frame size %u", context->bearer_cid, bearer_mtu);
    }

    goep_server_emit_connection_opened(packet_handler, goep_cid, addr, con_handle, status);
}

static void goep_server_handle_connection_closed(goep_server_connection_t * goep_connection){
    uint16_t goep_cid = goep_connection->goep_cid;
    btstack_packet_handler_t packet_handler = goep_connection->callback;

    btstack_linked_list_remove(&goep_server_connections, (btstack_linked_item_t *) goep_connection);
    btstack_memory_goep_server_connection_free(goep_connection);

    goep_server_emit_connection_closed(packet_handler, goep_cid);
}

#ifdef ENABLE_GOEP_L2CAP
static void goep_server_packet_handler_l2cap(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel); 
    UNUSED(size);

    bd_addr_t event_addr;
    uint16_t l2cap_psm;
    uint16_t l2cap_cid;
    goep_server_connection_t * goep_connection;
    goep_server_service_t *    goep_service;

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_psm = l2cap_event_incoming_connection_get_psm(packet);
                    l2cap_cid  = l2cap_event_incoming_connection_get_local_cid(packet);
                    goep_service = goep_server_get_service_for_l2cap_psm(l2cap_psm);
                    if (!goep_service){
                        l2cap_decline_connection(l2cap_cid);
                        break;
                    }

                    // alloc structure
                    goep_connection = btstack_memory_goep_server_connection_get();
                    if (!goep_connection){
                        l2cap_decline_connection(l2cap_cid);
                        break;
                    }

                    // setup connection
                    goep_connection->goep_cid   = goep_server_get_next_goep_cid();
                    goep_connection->bearer_cid = l2cap_cid;
                    goep_connection->callback   = goep_service->callback;
                    goep_connection->type       = GOEP_CONNECTION_L2CAP;
                    goep_connection->state      = GOEP_SERVER_W4_ACCEPT_REJECT;
                    btstack_linked_list_add(&goep_server_connections, (btstack_linked_item_t *) goep_connection);

                    // notify user
                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    goep_server_emit_incoming_connection(goep_service->callback, goep_connection->goep_cid, event_addr,
                                                         l2cap_event_incoming_connection_get_handle(packet));
                    break;

                case L2CAP_EVENT_CHANNEL_OPENED:
                    l2cap_cid = l2cap_event_channel_opened_get_local_cid(packet);
                    goep_connection = goep_server_get_connection_for_l2cap_cid(l2cap_cid);
                    btstack_assert(goep_connection != NULL);
                    btstack_assert(goep_connection->state == GOEP_SERVER_W4_CONNECTED);
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    goep_server_handle_connection_opened(goep_connection, event_addr,
                                                         l2cap_event_channel_opened_get_handle(packet),
                                                         l2cap_event_channel_opened_get_status(packet),
                                                         l2cap_cid,
                                                         l2cap_event_channel_opened_get_remote_mtu(packet) );
                    return;
                case L2CAP_EVENT_CAN_SEND_NOW:
                    l2cap_cid = l2cap_event_can_send_now_get_local_cid(packet);
                    goep_connection = goep_server_get_connection_for_l2cap_cid(l2cap_cid);
                    btstack_assert(goep_connection != NULL);
                    goep_server_emit_can_send_now_event(goep_connection);
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    l2cap_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    goep_connection = goep_server_get_connection_for_l2cap_cid(l2cap_cid);
                    btstack_assert(goep_connection != NULL);
                    goep_server_handle_connection_closed(goep_connection);
                    break;
                default:
                    break;
            }
            break;
        case L2CAP_DATA_PACKET:
            goep_connection = goep_server_get_connection_for_l2cap_cid(channel);
            btstack_assert(goep_connection != NULL);
            goep_connection->callback(GOEP_DATA_PACKET, goep_connection->goep_cid, packet, size);
            break;
        default:
            break;
    }
}
#endif

static void goep_server_packet_handler_rfcomm(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel); 
    UNUSED(size);    

    bd_addr_t event_addr;
    uint8_t  rfcomm_channel;
    uint16_t rfcomm_cid;
    goep_server_service_t    * goep_service;
    goep_server_connection_t * goep_connection;

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    rfcomm_channel = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_cid = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);

                    goep_service = goep_server_get_service_for_rfcomm_channel(rfcomm_channel);
                    if (!goep_service){
                        rfcomm_decline_connection(rfcomm_cid);
                        break;
                    }

                    // alloc structure
                    goep_connection = btstack_memory_goep_server_connection_get();
                    if (!goep_connection){
                        rfcomm_decline_connection(rfcomm_cid);
                        break;
                    }

                    // setup connection
                    goep_connection->goep_cid   = goep_server_get_next_goep_cid();
                    goep_connection->bearer_cid = rfcomm_cid;
                    goep_connection->callback   = goep_service->callback;
                    goep_connection->type       = GOEP_CONNECTION_RFCOMM;
                    goep_connection->state      = GOEP_SERVER_W4_ACCEPT_REJECT;
                    btstack_linked_list_add(&goep_server_connections, (btstack_linked_item_t *) goep_connection);

                    // notify user
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
                    goep_server_emit_incoming_connection(goep_service->callback, goep_connection->goep_cid, event_addr,
                                                         rfcomm_event_incoming_connection_get_con_handle(packet));
                    break;

                case RFCOMM_EVENT_CHANNEL_OPENED:
                    rfcomm_cid = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                    goep_connection = goep_server_get_connection_for_rfcomm_cid(rfcomm_cid);
                    btstack_assert(goep_connection != NULL);
                    btstack_assert(goep_connection->state == GOEP_SERVER_W4_CONNECTED);
                    rfcomm_event_channel_opened_get_bd_addr(packet, event_addr);
                    goep_server_handle_connection_opened(goep_connection, event_addr,
                                                         rfcomm_event_channel_opened_get_con_handle(packet),
                                                         rfcomm_event_channel_opened_get_status(packet),
                                                         rfcomm_cid,
                                                         rfcomm_event_channel_opened_get_max_frame_size(packet) );
                    break;

                case RFCOMM_EVENT_CAN_SEND_NOW:
                    rfcomm_cid = rfcomm_event_can_send_now_get_rfcomm_cid(packet); 
                    goep_connection = goep_server_get_connection_for_rfcomm_cid(rfcomm_cid);
                    btstack_assert(goep_connection != NULL);
                    goep_server_emit_can_send_now_event(goep_connection);
                    break;

                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    rfcomm_cid = rfcomm_event_channel_closed_get_rfcomm_cid(packet);
                    goep_connection = goep_server_get_connection_for_rfcomm_cid(rfcomm_cid);
                    btstack_assert(goep_connection != NULL);
                    goep_server_handle_connection_closed(goep_connection);
                    break;

                default:
                    break;
            }
            break;

        case RFCOMM_DATA_PACKET:
            goep_connection = goep_server_get_connection_for_rfcomm_cid(channel);
            btstack_assert(goep_connection != NULL);
            goep_connection->callback(GOEP_DATA_PACKET, goep_connection->goep_cid, packet, size);
            break;

        default:
            break;
    }
}

void goep_server_init(void){
}

uint8_t goep_server_register_service(btstack_packet_handler_t callback, uint8_t rfcomm_channel, uint16_t rfcomm_max_frame_size,
                uint16_t l2cap_psm, uint16_t l2cap_mtu, gap_security_level_t security_level){

    log_info("rfcomm_channel 0x%02x rfcomm_max_frame_size %u l2cap_psm 0x%02x l2cap_mtu %u",
             rfcomm_channel, rfcomm_max_frame_size, l2cap_psm, l2cap_mtu);

    // check if service is already registered
    goep_server_service_t * service;
    service = goep_server_get_service_for_rfcomm_channel(rfcomm_channel);
    if (service != NULL) {
        return RFCOMM_CHANNEL_ALREADY_REGISTERED;
    }

#ifdef ENABLE_GOEP_L2CAP
    if (l2cap_psm != 0){
        service = goep_server_get_service_for_l2cap_psm(l2cap_psm);
        if (service != NULL) {
            return L2CAP_SERVICE_ALREADY_REGISTERED;
        }
    }
#else
    UNUSED(l2cap_mtu);
    UNUSED(l2cap_psm);
    UNUSED(security_level);
#endif

    // alloc structure
    service = btstack_memory_goep_server_service_get();
    if (service == NULL) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    // fill in 
    service->callback = callback;
    service->rfcomm_channel = rfcomm_channel;
    service->l2cap_psm = l2cap_psm;

    uint8_t status = ERROR_CODE_SUCCESS;
    bool rfcomm_registered = false;

#ifdef ENABLE_GOEP_L2CAP
    bool l2cap_registered = false;
    // register with L2CAP
    if (l2cap_psm != 0){
        status = l2cap_register_service(goep_server_packet_handler_l2cap, l2cap_psm, l2cap_mtu, security_level);
        if (status == ERROR_CODE_SUCCESS){
            l2cap_registered = true;
        }
    }
#endif

    // register with RFCOMM
    if (status == ERROR_CODE_SUCCESS){
        status = rfcomm_register_service(goep_server_packet_handler_rfcomm, rfcomm_channel, rfcomm_max_frame_size);
        if (status == ERROR_CODE_SUCCESS){
            rfcomm_registered = true;
        }
    }

    // add service on success
    if (status == ERROR_CODE_SUCCESS){
        btstack_linked_list_add(&goep_server_services, (btstack_linked_item_t *) service);
        return ERROR_CODE_SUCCESS;
    }

    // unrestore otherwise
    btstack_memory_goep_server_service_free(service);
#ifdef ENABLE_GOEP_L2CAP
    if (l2cap_registered){
        l2cap_unregister_service(l2cap_psm);
    }
#endif
    if (rfcomm_registered){
        rfcomm_unregister_service(rfcomm_channel);
    }
    return status;
}

uint8_t goep_server_accept_connection(uint16_t goep_cid){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != GOEP_SERVER_W4_ACCEPT_REJECT){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    connection->state = GOEP_SERVER_W4_CONNECTED;
#ifdef ENABLE_GOEP_L2CAP
    if (connection->type == GOEP_CONNECTION_L2CAP){
        return l2cap_ertm_accept_connection(connection->bearer_cid, &ertm_config, connection->ertm_buffer, GOEP_SERVER_ERTM_BUFFER);
    }
#endif
    return rfcomm_accept_connection(connection->bearer_cid);
}

uint8_t goep_server_decline_connection(uint16_t goep_cid){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    connection->state = GOEP_SERVER_W4_CONNECTED;
    if (connection->state != GOEP_SERVER_W4_ACCEPT_REJECT){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
#ifdef ENABLE_GOEP_L2CAP
    if (connection->type == GOEP_CONNECTION_L2CAP){
        l2cap_decline_connection(connection->bearer_cid);
        return ERROR_CODE_SUCCESS;
    }
#endif
    return rfcomm_decline_connection(connection->bearer_cid);
}

uint8_t goep_server_request_can_send_now(uint16_t goep_cid){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    btstack_assert(connection->state == GOEP_SERVER_CONNECTED);

    switch (connection->type){
        case GOEP_CONNECTION_RFCOMM:
            rfcomm_request_can_send_now_event(connection->bearer_cid);
            break;
#ifdef ENABLE_GOEP_L2CAP
        case GOEP_CONNECTION_L2CAP:
            l2cap_request_can_send_now_event(connection->bearer_cid);
            break;
#endif
        default:
            btstack_unreachable();
            break;
    }
    return ERROR_CODE_SUCCESS;
}

static uint8_t * goep_server_get_outgoing_buffer(goep_server_connection_t * connection){
    switch (connection->type){
#ifdef ENABLE_GOEP_L2CAP
        case GOEP_CONNECTION_L2CAP:
            return goep_server_l2cap_packet_buffer;
#endif
        case GOEP_CONNECTION_RFCOMM:
            return rfcomm_get_outgoing_buffer();
        default:
            btstack_unreachable();
            return NULL;
    }
}

static uint16_t goep_server_get_outgoing_buffer_len(goep_server_connection_t * connection){
    return connection->bearer_mtu;
}

static void goep_server_packet_init(goep_server_connection_t * connection){
    btstack_assert(connection->state == GOEP_SERVER_CONNECTED);
    switch (connection->type){
#ifdef ENABLE_GOEP_L2CAP
        case GOEP_CONNECTION_L2CAP:
            break;
#endif
        case GOEP_CONNECTION_RFCOMM:
            rfcomm_reserve_packet_buffer();
            break;
        default:
            btstack_unreachable();
            break;
    }
    connection->state = GOEP_SERVER_OUTGOING_BUFFER_RESERVED;
}

uint8_t goep_server_response_create_connect(uint16_t goep_cid, uint8_t obex_version_number, uint8_t flags, uint16_t maximum_obex_packet_length){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    goep_server_packet_init(connection);

    // workaround: limit OBEX packet len to L2CAP/RFCOMM MTU
    maximum_obex_packet_length = btstack_min(maximum_obex_packet_length, connection->bearer_mtu);

    uint8_t * buffer = goep_server_get_outgoing_buffer(connection);
    uint16_t buffer_len = goep_server_get_outgoing_buffer_len(connection);
    return obex_message_builder_response_create_connect(buffer, buffer_len, obex_version_number, flags, maximum_obex_packet_length, (uint32_t) goep_cid);
}

uint8_t goep_server_response_create_general(uint16_t goep_cid){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    goep_server_packet_init(connection);
    uint8_t * buffer = goep_server_get_outgoing_buffer(connection);
    uint16_t buffer_len = goep_server_get_outgoing_buffer_len(connection);
    return obex_message_builder_response_create_general(buffer, buffer_len, OBEX_RESP_SUCCESS);
}

uint16_t goep_server_response_get_max_message_size(uint16_t goep_cid){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return 0;
    }
    return goep_server_get_outgoing_buffer_len(connection);
}

uint8_t goep_server_header_add_end_of_body(uint16_t goep_cid, const uint8_t * end_of_body, uint16_t length){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint8_t * buffer = goep_server_get_outgoing_buffer(connection);
    uint16_t buffer_len = goep_server_get_outgoing_buffer_len(connection);
    return obex_message_builder_body_add_static(buffer, buffer_len, end_of_body, length);
}

uint8_t goep_server_header_add_who(uint16_t goep_cid, const uint8_t * target){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint8_t * buffer = goep_server_get_outgoing_buffer(connection);
    uint16_t buffer_len = goep_server_get_outgoing_buffer_len(connection);
    return obex_message_builder_header_add_who(buffer, buffer_len, target);
}

uint8_t goep_server_header_add_srm_enable(uint16_t goep_cid){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint8_t * buffer = goep_server_get_outgoing_buffer(connection);
    uint16_t buffer_len = goep_server_get_outgoing_buffer_len(connection);
    return obex_message_builder_header_add_srm_enable(buffer, buffer_len);
}

uint8_t goep_server_header_add_srm_enable_wait(uint16_t goep_cid) {
    goep_server_connection_t* connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint8_t* buffer = goep_server_get_outgoing_buffer(connection);
    uint16_t buffer_len = goep_server_get_outgoing_buffer_len(connection);
    return obex_message_builder_header_add_srm_enable(buffer, buffer_len)
        || obex_message_builder_header_add_srmp_wait(buffer, buffer_len);
}

uint8_t goep_server_header_add_name(uint16_t goep_cid, const char* name) {

    if (name == NULL) {
        return OBEX_UNKNOWN_ERROR;
    }

    goep_server_connection_t* connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint8_t* buffer = goep_server_get_outgoing_buffer(connection);
    uint16_t buffer_len = goep_server_get_outgoing_buffer_len(connection);
    return obex_message_builder_header_add_name(buffer, buffer_len, name);
}

uint8_t goep_server_header_add_type(uint16_t goep_cid, const char* type) {

    if (type == NULL) {
        return OBEX_UNKNOWN_ERROR;
    }

    goep_server_connection_t* connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint8_t* buffer = goep_server_get_outgoing_buffer(connection);
    uint16_t buffer_len = goep_server_get_outgoing_buffer_len(connection);
    return obex_message_builder_header_add_type(buffer, buffer_len, type);
}

uint8_t goep_server_header_add_application_parameters(uint16_t goep_cid, const uint8_t * data, uint16_t length){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint8_t * buffer = goep_server_get_outgoing_buffer(connection);
    uint16_t buffer_len = goep_server_get_outgoing_buffer_len(connection);
    return obex_message_builder_header_add_application_parameters(buffer, buffer_len, data, length);
}

uint8_t goep_server_execute(uint16_t goep_cid, uint8_t response_code){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    btstack_assert(connection->state == GOEP_SERVER_OUTGOING_BUFFER_RESERVED);

    connection->state = GOEP_SERVER_CONNECTED;

    uint8_t * buffer = goep_server_get_outgoing_buffer(connection);
    // set response code
    buffer[0] = response_code;
    uint16_t pos = big_endian_read_16(buffer, 1);
    switch (connection->type) {
#ifdef ENABLE_GOEP_L2CAP
        case GOEP_CONNECTION_L2CAP:
            return l2cap_send(connection->bearer_cid, buffer, pos);
            break;
#endif
        case GOEP_CONNECTION_RFCOMM:
            return rfcomm_send_prepared(connection->bearer_cid, pos);
        default:
            btstack_unreachable();
            return ERROR_CODE_SUCCESS;
    }
}

uint8_t goep_server_disconnect(uint16_t goep_cid){
    goep_server_connection_t * connection = goep_server_get_connection_for_goep_cid(goep_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (connection->state < GOEP_SERVER_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    switch (connection->type) {
#ifdef ENABLE_GOEP_L2CAP
        case GOEP_CONNECTION_L2CAP:
            return l2cap_disconnect(connection->bearer_cid);
#endif
        case GOEP_CONNECTION_RFCOMM:
            return rfcomm_disconnect(connection->bearer_cid);
        default:
            btstack_unreachable();
            break;
    }
    return ERROR_CODE_SUCCESS;
}

void goep_server_deinit(void){
    goep_server_cid_counter = 0;
    goep_server_services = NULL;
}
