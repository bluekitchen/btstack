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

#define BTSTACK_FILE__ "goep_client.c"
 
#include "btstack_config.h"

#include <stdint.h>
#include <string.h>

#include "btstack_debug.h"
#include "hci_dump.h"
#include "bluetooth_sdp.h"
#include "btstack_event.h"
#include "classic/goep_client.h"
#include "classic/obex_message_builder.h"
#include "classic/obex.h"
#include "classic/obex_iterator.h"
#include "classic/rfcomm.h"
#include "classic/sdp_client.h"
#include "classic/sdp_util.h"
#include "l2cap.h"

//------------------------------------------------------------------------------------------------------------
// goep_client.c
//

#ifdef ENABLE_GOEP_L2CAP
#ifndef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
#error "ENABLE_GOEP_L2CAP requires ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE. Please enable ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE or disable ENABLE_GOEP_L2CAP"
#endif
#endif

static uint16_t goep_client_cid;
static btstack_linked_list_t goep_clients;

static goep_client_t *    goep_client_sdp_active;
static uint8_t            goep_client_sdp_query_attribute_value[30];
static const unsigned int goep_client_sdp_query_attribute_value_buffer_size = sizeof(goep_client_sdp_query_attribute_value);
static uint8_t goep_packet_buffer[150];

// singleton instance
static goep_client_t   goep_client_singleton;

#ifdef ENABLE_GOEP_L2CAP
// singleton instance
static uint8_t goep_client_singleton_ertm_buffer[1000];
static l2cap_ertm_config_t goep_client_singleton_ertm_config = {
    1,  // ertm mandatory
    2,  // max transmit, some tests require > 1
    2000,
    12000,
    512,    // l2cap ertm mtu
    2,
    2,
    1,      // 16-bit FCS
};
#endif

static inline void goep_client_emit_connected_event(goep_client_t * goep_client, uint8_t status){
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_GOEP_META;
    pos++;  // skip len
    event[pos++] = GOEP_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event, pos, goep_client->cid);
    pos+=2;
    event[pos++] = status;
    (void)memcpy(&event[pos], goep_client->bd_addr, 6);
    pos += 6;
    little_endian_store_16(event, pos, goep_client->con_handle);
    pos += 2;
    event[pos++] = goep_client->incoming;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("goep_client_emit_connected_event size %u", pos);
    goep_client->client_handler(HCI_EVENT_PACKET, goep_client->cid, &event[0], pos);
}   

static inline void goep_client_emit_connection_closed_event(goep_client_t * goep_client){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_GOEP_META;
    pos++;  // skip len
    event[pos++] = GOEP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event, pos, goep_client->cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("goep_client_emit_connection_closed_event size %u", pos);
    goep_client->client_handler(HCI_EVENT_PACKET, goep_client->cid, &event[0], pos);
}   

static inline void goep_client_emit_can_send_now_event(goep_client_t * goep_client){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_GOEP_META;
    pos++;  // skip len
    event[pos++] = GOEP_SUBEVENT_CAN_SEND_NOW;
    little_endian_store_16(event, pos, goep_client->cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("goep_client_emit_can_send_now_event size %u", pos);
    goep_client->client_handler(HCI_EVENT_PACKET, goep_client->cid, &event[0], pos);
}   

static void goep_client_handle_connection_opened(goep_client_t * goep_client, uint8_t status, uint16_t mtu){
    if (status) {
        goep_client->state = GOEP_CLIENT_INIT;
        log_info("goep_client: open failed, status %u", status);
    } else {
        goep_client->bearer_mtu = mtu;
        goep_client->state = GOEP_CLIENT_CONNECTED;
        log_info("goep_client: connection opened. cid %u, max frame size %u", goep_client->bearer_cid, goep_client->bearer_mtu);
    }
    goep_client_emit_connected_event(goep_client, status);
}

static void goep_client_handle_connection_close(goep_client_t * goep_client){
    goep_client->state = GOEP_CLIENT_INIT;
    btstack_linked_list_remove(&goep_clients, (btstack_linked_item_t *) goep_client);
    goep_client_emit_connection_closed_event(goep_client);
}

static goep_client_t * goep_client_for_cid(uint16_t cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &goep_clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        goep_client_t * goep_client = (goep_client_t *) btstack_linked_list_iterator_next(&it);
        if (goep_client->cid == cid){
            return goep_client;
        }
    }
    return NULL;
}

static goep_client_t * goep_client_for_bearer_cid(uint16_t bearer_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &goep_clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        goep_client_t * goep_client = (goep_client_t *) btstack_linked_list_iterator_next(&it);
        if (goep_client->bearer_cid == bearer_cid){
            return goep_client;
        }
    }
    return NULL;
}

static void goep_client_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    goep_client_t * goep_client;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
#ifdef ENABLE_GOEP_L2CAP
                case L2CAP_EVENT_CHANNEL_OPENED:
                    goep_client = goep_client_for_bearer_cid(l2cap_event_channel_opened_get_local_cid(packet));
                    btstack_assert(goep_client != NULL);
                    goep_client_handle_connection_opened(goep_client, l2cap_event_channel_opened_get_status(packet),
                                                         btstack_min(l2cap_event_channel_opened_get_remote_mtu(packet), l2cap_event_channel_opened_get_local_mtu(packet)));
                    break;
                case L2CAP_EVENT_CAN_SEND_NOW:
                    goep_client = goep_client_for_bearer_cid(l2cap_event_can_send_now_get_local_cid(packet));
                    btstack_assert(goep_client != NULL);
                    goep_client_emit_can_send_now_event(goep_client);
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    goep_client = goep_client_for_bearer_cid(l2cap_event_channel_closed_get_local_cid(packet));
                    btstack_assert(goep_client != NULL);
                    goep_client_handle_connection_close(goep_client);
                    break;
#endif
                case RFCOMM_EVENT_CHANNEL_OPENED:
                    goep_client = goep_client_for_bearer_cid(rfcomm_event_channel_opened_get_rfcomm_cid(packet));
                    btstack_assert(goep_client != NULL);
                    goep_client_handle_connection_opened(goep_client, rfcomm_event_channel_opened_get_status(packet), rfcomm_event_channel_opened_get_max_frame_size(packet));
                    return;
                case RFCOMM_EVENT_CAN_SEND_NOW:
                    goep_client = goep_client_for_bearer_cid(rfcomm_event_can_send_now_get_rfcomm_cid(packet));
                    btstack_assert(goep_client != NULL);
                    goep_client_emit_can_send_now_event(goep_client);
                    break;
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    goep_client = goep_client_for_bearer_cid(rfcomm_event_channel_closed_get_rfcomm_cid(packet));
                    btstack_assert(goep_client != NULL);
                    goep_client_handle_connection_close(goep_client);
                    break;
                default:
                    break;
            }
            break;
        case L2CAP_DATA_PACKET:
        case RFCOMM_DATA_PACKET:
            goep_client = goep_client_for_bearer_cid(channel);
            btstack_assert(goep_client != NULL);
            goep_client->client_handler(GOEP_DATA_PACKET, goep_client->cid, packet, size);
            break;
        default:
            break;
    }
}

static void goep_client_handle_sdp_query_end_of_record(goep_client_t * goep_client){
    if (goep_client->uuid == BLUETOOTH_SERVICE_CLASS_MESSAGE_ACCESS_SERVER){
        if (goep_client->mas_info.instance_id == goep_client->map_mas_instance_id){
            // Requested MAS Instance found, accept info
            goep_client->rfcomm_port = goep_client->mas_info.rfcomm_port;
            goep_client->profile_supported_features = goep_client->mas_info.supported_features;
            goep_client->map_supported_message_types = goep_client->mas_info.supported_message_types;
#ifdef ENABLE_GOEP_L2CAP
            goep_client->l2cap_psm = goep_client->mas_info.l2cap_psm;
            log_info("MAS Instance #%u found, rfcomm #%u, l2cap 0x%04x", goep_client->map_mas_instance_id,
                     goep_client->rfcomm_port, goep_client->l2cap_psm);
#else
            log_info("MAS Instance #%u found, rfcomm #%u", goep_client->map_mas_instance_id,
                     goep_client->rfcomm_port);
#endif
        }
    }
}
static uint8_t goep_client_start_connect(goep_client_t * goep_client){
#ifdef ENABLE_GOEP_L2CAP
    if (goep_client->l2cap_psm && (goep_client->ertm_buffer != NULL)){
        log_info("Remote GOEP L2CAP PSM: %u", goep_client->l2cap_psm);
        return l2cap_ertm_create_channel(&goep_client_packet_handler, goep_client->bd_addr, goep_client->l2cap_psm,
        &goep_client->ertm_config, goep_client->ertm_buffer,
        goep_client->ertm_buffer_size, &goep_client->bearer_cid);
    }
#endif
    log_info("Remote GOEP RFCOMM Server Channel: %u", goep_client->rfcomm_port);
    return rfcomm_create_channel(&goep_client_packet_handler, goep_client->bd_addr, goep_client->rfcomm_port, &goep_client->bearer_cid);
}

static void goep_client_handle_sdp_query_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    goep_client_t * goep_client = goep_client_sdp_active;
    btstack_assert(goep_client != NULL);

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t des_list_it;
    des_iterator_t prot_it;
    uint8_t status;
    uint16_t record_index;
    bool goep_server_found;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:

            // detect new record
            record_index = sdp_event_query_attribute_byte_get_record_id(packet);
            if (record_index != goep_client->record_index){
                goep_client->record_index = record_index;
                goep_client_handle_sdp_query_end_of_record(goep_client);
                memset(&goep_client->mas_info, 0, sizeof(goep_client->mas_info));
            }

            // check if relevant attribute
            switch(sdp_event_query_attribute_byte_get_attribute_id(packet)){
                case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
                case BLUETOOTH_ATTRIBUTE_PBAP_SUPPORTED_FEATURES:
                case BLUETOOTH_ATTRIBUTE_MAS_INSTANCE_ID:
                case BLUETOOTH_ATTRIBUTE_SUPPORTED_MESSAGE_TYPES:
#ifdef ENABLE_GOEP_L2CAP
                case BLUETOOTH_ATTRIBUTE_GOEP_L2CAP_PSM:
#endif
                    break;
                default:
                    return;
            }

            // warn if attribute too large to fit in our buffer
            if (sdp_event_query_attribute_byte_get_attribute_length(packet) > goep_client_sdp_query_attribute_value_buffer_size) {
                log_error("SDP attribute value size exceeded for attribute %x: available %d, required %d", sdp_event_query_attribute_byte_get_attribute_id(packet), goep_client_sdp_query_attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
                break;
            }

            // store single byte
            goep_client_sdp_query_attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);

            // wait until value fully received
            if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) != sdp_event_query_attribute_byte_get_attribute_length(packet)) break;

            // process attributes
            switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
                    for (des_iterator_init(&des_list_it, goep_client_sdp_query_attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {
                        uint8_t       *des_element;
                        uint8_t       *element;
                        uint32_t       uuid;

                        if (des_iterator_get_type(&des_list_it) != DE_DES) continue;

                        des_element = des_iterator_get_element(&des_list_it);
                        des_iterator_init(&prot_it, des_element);

                        // first element is UUID
                        element = des_iterator_get_element(&prot_it);
                        if (de_get_element_type(element) != DE_UUID) continue;

                        uuid = de_get_uuid32(element);
                        des_iterator_next(&prot_it);
                        if (!des_iterator_has_more(&prot_it)) continue;

                        // second element is RFCOMM server channel or L2CAP PSM
                        element = des_iterator_get_element(&prot_it);
                        if (uuid == BLUETOOTH_PROTOCOL_RFCOMM){
                            if (goep_client->uuid == BLUETOOTH_SERVICE_CLASS_MESSAGE_ACCESS_SERVER) {
                                goep_client->mas_info.rfcomm_port = element[de_get_header_size(element)];
                            } else {
                                goep_client->rfcomm_port = element[de_get_header_size(element)];
                            }
                        }
                    }
                    break;
#ifdef ENABLE_GOEP_L2CAP
                case BLUETOOTH_ATTRIBUTE_GOEP_L2CAP_PSM:
                    if (goep_client->uuid == BLUETOOTH_SERVICE_CLASS_MESSAGE_ACCESS_SERVER){
                        de_element_get_uint16(goep_client_sdp_query_attribute_value, &goep_client->mas_info.l2cap_psm);
                    } else {
                        de_element_get_uint16(goep_client_sdp_query_attribute_value, &goep_client->l2cap_psm);
                    }
                    break;
#endif
                // BLUETOOTH_ATTRIBUTE_PBAP_SUPPORTED_FEATURES == BLUETOOTH_ATTRIBUTE_MAP_SUPPORTED_FEATURES == 0x0317
                case BLUETOOTH_ATTRIBUTE_PBAP_SUPPORTED_FEATURES:
                    if (de_get_element_type(goep_client_sdp_query_attribute_value) != DE_UINT) break;
                    if (de_get_size_type(goep_client_sdp_query_attribute_value) != DE_SIZE_32) break;
                    if (goep_client->uuid == BLUETOOTH_SERVICE_CLASS_MESSAGE_ACCESS_SERVER) {
                        goep_client->mas_info.supported_features  = big_endian_read_32(goep_client_sdp_query_attribute_value, de_get_header_size(goep_client_sdp_query_attribute_value));
                    } else {
                        goep_client->profile_supported_features  = big_endian_read_32(goep_client_sdp_query_attribute_value, de_get_header_size(goep_client_sdp_query_attribute_value));
                    }
                    break;

                case BLUETOOTH_ATTRIBUTE_MAS_INSTANCE_ID:
                    if (de_get_element_type(goep_client_sdp_query_attribute_value) != DE_UINT) break;
                    if (de_get_size_type(goep_client_sdp_query_attribute_value) != DE_SIZE_8) break;
                    goep_client->mas_info.instance_id = goep_client_sdp_query_attribute_value[de_get_header_size(goep_client_sdp_query_attribute_value)];
                    break;

                case BLUETOOTH_ATTRIBUTE_SUPPORTED_MESSAGE_TYPES:
                    if (de_get_element_type(goep_client_sdp_query_attribute_value) != DE_UINT) break;
                    if (de_get_size_type(goep_client_sdp_query_attribute_value) != DE_SIZE_8) break;
                    goep_client->mas_info.supported_message_types = goep_client_sdp_query_attribute_value[de_get_header_size(goep_client_sdp_query_attribute_value)];
                    break;

                default:
                    break;
            }
            break;

        case SDP_EVENT_QUERY_COMPLETE:
            goep_client_sdp_active = NULL;
            goep_client_handle_sdp_query_end_of_record(goep_client);
            status = sdp_event_query_complete_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                log_info("GOEP client, SDP query failed 0x%02x", status);
                goep_client->state = GOEP_CLIENT_INIT;
                goep_client_emit_connected_event(goep_client, status);
                break;
            }
            goep_server_found = false;
            if (goep_client->rfcomm_port != 0){
                goep_server_found = true;
            }
#ifdef ENABLE_GOEP_L2CAP
            if (goep_client->l2cap_psm != 0){
                goep_server_found = true;
            }
#endif
            if (goep_server_found == false){
                log_info("No GOEP RFCOMM or L2CAP server found");
                goep_client->state = GOEP_CLIENT_INIT;
                goep_client_emit_connected_event(goep_client, SDP_SERVICE_NOT_FOUND);
                break;
            }
            (void) goep_client_start_connect(goep_client);
            break;

        default:
            break;
    }
}

static uint8_t * goep_client_get_outgoing_buffer(goep_client_t * goep_client){
    if (goep_client->l2cap_psm){
        return goep_packet_buffer;
    } else {
        return rfcomm_get_outgoing_buffer();
    }
}

static uint16_t goep_client_get_outgoing_buffer_len(goep_client_t * goep_client){
    if (goep_client->l2cap_psm){
        return sizeof(goep_packet_buffer);
    } else {
        return rfcomm_get_max_frame_size(goep_client->bearer_cid);
    }
}

static void goep_client_packet_init(goep_client_t *goep_client, uint8_t opcode) {
    if (goep_client->l2cap_psm != 0){
    } else {
        rfcomm_reserve_packet_buffer();
    }
    // store opcode for parsing of response
    goep_client->obex_opcode = opcode;
}

void goep_client_init(void){
    goep_client_singleton.state = GOEP_CLIENT_INIT;
}

void goep_client_deinit(void){
    goep_clients = NULL;
    goep_client_cid = 0;
    memset(&goep_client_singleton, 0, sizeof(goep_client_t));
    memset(goep_client_sdp_query_attribute_value, 0, sizeof(goep_client_sdp_query_attribute_value));
    memset(goep_packet_buffer, 0, sizeof(goep_packet_buffer));
}

static void geop_client_sdp_query_start(void * context){
    uint16_t goep_cid = (uint16_t)(uintptr_t) context;
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (context != NULL){
        goep_client_sdp_active = goep_client;
        sdp_client_query_uuid16(&goep_client_handle_sdp_query_event, goep_client->bd_addr,
                                goep_client->uuid);
    }
}

uint8_t
goep_client_connect(goep_client_t *goep_client, l2cap_ertm_config_t *l2cap_ertm_config, uint8_t *l2cap_ertm_buffer,
                    uint16_t l2cap_ertm_buffer_size, btstack_packet_handler_t handler, bd_addr_t addr, uint16_t uuid,
                    uint8_t instance_id, uint16_t *out_cid) {
    btstack_assert(goep_client != NULL);

    // get new goep cid, skip 0x0000
    goep_client_cid++;
    if (goep_client_cid == 0) {
        goep_client_cid = 1;
    }

    // add to list
    memset(goep_client, 0, sizeof(goep_client_t));
    goep_client->state = GOEP_CLIENT_W4_SDP;
    goep_client->cid = goep_client_cid;
    goep_client->client_handler = handler;
    goep_client->uuid = uuid;
    goep_client->map_mas_instance_id = instance_id;
    goep_client->profile_supported_features = PROFILE_FEATURES_NOT_PRESENT;
    (void)memcpy(goep_client->bd_addr, addr, 6);
    goep_client->sdp_query_request.callback = geop_client_sdp_query_start;
    goep_client->sdp_query_request.context = (void *)(uintptr_t) goep_client->cid;
    goep_client->obex_connection_id = OBEX_CONNECTION_ID_INVALID;
#ifdef ENABLE_GOEP_L2CAP
    if (l2cap_ertm_config != NULL){
        memcpy(&goep_client->ertm_config, l2cap_ertm_config, sizeof(l2cap_ertm_config_t));
    }
    goep_client->ertm_buffer_size = l2cap_ertm_buffer_size;
    goep_client->ertm_buffer = l2cap_ertm_buffer;
#endif
    btstack_linked_list_add(&goep_clients, (btstack_linked_item_t *) goep_client);

    // request sdp query
    sdp_client_register_query_callback(&goep_client->sdp_query_request);

    *out_cid = goep_client->cid;
    return ERROR_CODE_SUCCESS;
}

#ifdef ENABLE_GOEP_L2CAP
uint8_t goep_client_connect_l2cap(goep_client_t *goep_client, l2cap_ertm_config_t *l2cap_ertm_config, uint8_t *l2cap_ertm_buffer,
                          uint16_t l2cap_ertm_buffer_size, btstack_packet_handler_t handler, bd_addr_t addr, uint16_t l2cap_psm,
                          uint16_t *out_cid){
    btstack_assert(goep_client != NULL);
    btstack_assert(l2cap_ertm_config != NULL);
    btstack_assert(l2cap_ertm_buffer != NULL);

    // get new goep cid, skip 0x0000
    goep_client_cid++;
    if (goep_client_cid == 0) {
        goep_client_cid = 1;
    }

    // add to list
    memset(goep_client, 0, sizeof(goep_client_t));
    goep_client->state = GOEP_CLIENT_W4_SDP;
    goep_client->cid = goep_client_cid;
    goep_client->client_handler = handler;
    goep_client->profile_supported_features = PROFILE_FEATURES_NOT_PRESENT;
    (void)memcpy(goep_client->bd_addr, addr, 6);
    goep_client->obex_connection_id = OBEX_CONNECTION_ID_INVALID;
    memcpy(&goep_client->ertm_config, l2cap_ertm_config, sizeof(l2cap_ertm_config_t));
    goep_client->ertm_buffer_size = l2cap_ertm_buffer_size;
    goep_client->ertm_buffer = l2cap_ertm_buffer;
    btstack_linked_list_add_tail(&goep_clients, (btstack_linked_item_t *) goep_client);

    goep_client->l2cap_psm = l2cap_psm;

    *out_cid = goep_client->cid;

    return goep_client_start_connect(goep_client);
}
#endif

uint8_t goep_client_create_connection(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t uuid, uint16_t * out_cid){
    goep_client_t * goep_client = &goep_client_singleton;
    if (goep_client->state != GOEP_CLIENT_INIT) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
#ifdef ENABLE_GOEP_L2CAP
    return goep_client_connect(goep_client, &goep_client_singleton_ertm_config, goep_client_singleton_ertm_buffer,
                               sizeof(goep_client_singleton_ertm_buffer), handler, addr, uuid, 0, out_cid);
#else
    return goep_client_connect(goep_client,NULL, NULL, 0, handler, addr, uuid, 0, out_cid);
#endif
}

uint32_t goep_client_get_pbap_supported_features(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return PBAP_FEATURES_NOT_PRESENT;
    }
    return goep_client->profile_supported_features;
}

uint32_t goep_client_get_map_supported_features(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return PROFILE_FEATURES_NOT_PRESENT;
    }
    return goep_client->profile_supported_features;
}

uint8_t goep_client_get_map_mas_instance_id(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return 0;
    }
    return goep_client->map_mas_instance_id;
}

uint8_t goep_client_get_map_supported_message_types(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return 0;
    }
    return goep_client->map_supported_message_types;
}


bool goep_client_version_20_or_higher(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return false;
    }
    return goep_client->l2cap_psm != 0;
}

void goep_client_request_can_send_now(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    if (goep_client->l2cap_psm){
        l2cap_request_can_send_now_event(goep_client->bearer_cid);
    } else {
        rfcomm_request_can_send_now_event(goep_client->bearer_cid);
    }
}

uint8_t goep_client_disconnect(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (goep_client->l2cap_psm){
        l2cap_disconnect(goep_client->bearer_cid);
    } else {
        rfcomm_disconnect(goep_client->bearer_cid);
    }
    return ERROR_CODE_SUCCESS;
}

void goep_client_set_connection_id(uint16_t goep_cid, uint32_t connection_id){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    goep_client->obex_connection_id = connection_id;
}

uint8_t goep_client_get_request_opcode(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return 0;
    }
    return goep_client->obex_opcode;
}

void goep_client_request_create_connect(uint16_t goep_cid, uint8_t obex_version_number, uint8_t flags, uint16_t maximum_obex_packet_length){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    goep_client_packet_init(goep_client, OBEX_OPCODE_CONNECT);
    // workaround: limit OBEX packet len to L2CAP/RFCOMM MTU to avoid handling of fragemented packets
    maximum_obex_packet_length = btstack_min(maximum_obex_packet_length, goep_client->bearer_mtu);
    
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_request_create_connect(buffer, buffer_len, obex_version_number, flags, maximum_obex_packet_length);
}

void goep_client_request_create_get(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    goep_client_packet_init(goep_client, OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK);
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_request_create_get(buffer, buffer_len, goep_client->obex_connection_id);
}

void goep_client_request_create_put(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    goep_client_packet_init(goep_client, OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK);
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_request_create_put(buffer, buffer_len, goep_client->obex_connection_id);
}

void goep_client_request_create_set_path(uint16_t goep_cid, uint8_t flags){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    goep_client_packet_init(goep_client, OBEX_OPCODE_SETPATH);
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_request_create_set_path(buffer, buffer_len, flags, goep_client->obex_connection_id);
}

void goep_client_request_create_abort(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    goep_client_packet_init(goep_client, OBEX_OPCODE_ABORT);

    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_request_create_abort(buffer, buffer_len, goep_client->obex_connection_id);
}

void goep_client_request_create_disconnect(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    goep_client_packet_init(goep_client, OBEX_OPCODE_DISCONNECT);
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_request_create_disconnect(buffer, buffer_len, goep_client->obex_connection_id);
}

void goep_client_header_add_byte(uint16_t goep_cid, uint8_t header_type, uint8_t value){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_byte(buffer, buffer_len, header_type, value);
}

void goep_client_header_add_word(uint16_t goep_cid, uint8_t header_type, uint32_t value){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_word(buffer, buffer_len, header_type, value);
}

void goep_client_header_add_variable(uint16_t goep_cid, uint8_t header_type, const uint8_t * header_data, uint16_t header_data_length){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_variable(buffer, buffer_len, header_type, header_data, header_data_length);
}

void goep_client_header_add_srm_enable(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_srm_enable(buffer, buffer_len);
}

void goep_client_header_add_target(uint16_t goep_cid, const uint8_t * target, uint16_t length){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_target(buffer, buffer_len, target, length);
}

void goep_client_header_add_application_parameters(uint16_t goep_cid, const uint8_t * data, uint16_t length){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_application_parameters(buffer, buffer_len, data, length);
}

void goep_client_header_add_challenge_response(uint16_t goep_cid, const uint8_t * data, uint16_t length){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_challenge_response(buffer, buffer_len, data, length);
}

void goep_client_body_add_static(uint16_t goep_cid, const uint8_t * data, uint32_t length){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_body_add_static(buffer, buffer_len, data, length);
}

uint16_t goep_client_body_get_outgoing_buffer_len(uint16_t goep_cid) {
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return 0;
    }
    return goep_client_get_outgoing_buffer_len(goep_client);
};

void goep_client_body_fillup_static(uint16_t goep_cid, const uint8_t * data, uint32_t length, uint32_t * ret_length){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_body_fillup_static(buffer, buffer_len, data, length, ret_length);
}

void goep_client_header_add_name(uint16_t goep_cid, const char * name){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_name(buffer, buffer_len, name);
}

void goep_client_header_add_name_prefix(uint16_t goep_cid, const char * name, uint16_t name_len){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_name_prefix(buffer, buffer_len, name, name_len);
}

void goep_client_header_add_unicode_prefix(uint16_t goep_cid, uint8_t header_id, const char * name, uint16_t name_len){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_unicode_prefix(buffer, buffer_len, header_id, name, name_len);
}

void goep_client_header_add_type(uint16_t goep_cid, const char * type){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_type(buffer, buffer_len, type);
}

void goep_client_header_add_length(uint16_t goep_cid, uint32_t length){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    obex_message_builder_header_add_length(buffer, buffer_len, length);
}

uint16_t goep_client_request_get_max_body_size(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return 0;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);
    uint16_t pos = big_endian_read_16(buffer, 1);
    return buffer_len - pos;
}

int goep_client_execute(uint16_t goep_cid){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    return goep_client_execute_with_final_bit (goep_cid, true);
}

int goep_client_execute_with_final_bit(uint16_t goep_cid, bool final){
    goep_client_t * goep_client = goep_client_for_cid(goep_cid);
    if (goep_client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t * buffer = goep_client_get_outgoing_buffer(goep_client);
    uint16_t buffer_len = goep_client_get_outgoing_buffer_len(goep_client);

    obex_message_builder_set_final_bit (buffer, buffer_len, final);

    uint16_t pos = big_endian_read_16(buffer, 1);
    if (goep_client->l2cap_psm){
        return l2cap_send(goep_client->bearer_cid, buffer, pos);
    } else {
        return rfcomm_send_prepared(goep_client->bearer_cid, pos);
    }
}

