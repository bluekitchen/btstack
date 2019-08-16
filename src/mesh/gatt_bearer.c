/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "gatt_bearer.c"

#include <string.h>

#include "ble/gatt-service/mesh_proxy_service_server.h"
#include "ble/att_server.h"
#include "mesh/gatt_bearer.h"
#include "ble/core.h"
#include "bluetooth.h"
#include "bluetooth_data_types.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_run_loop.h"
#include "btstack_event.h"
#include "gap.h"
#include "btstack_event.h"
#include "provisioning.h"

#define NUM_TYPES 3

static btstack_packet_handler_t client_callbacks[NUM_TYPES];
static int request_can_send_now[NUM_TYPES];
static int last_sender;

// share buffer for reassembly and segmentation - protocol is half-duplex
static union {
    uint8_t  reassembly_buffer[MESH_PROV_MAX_PROXY_PDU];
    uint8_t  segmentation_buffer[MESH_PROV_MAX_PROXY_PDU];
} sar_buffer;

static const uint8_t * proxy_pdu;
static uint16_t proxy_pdu_size;
static uint8_t outgoing_ready;
static uint16_t reassembly_offset;
static uint16_t segmentation_offset;
static mesh_msg_sar_field_t segmentation_state;
static mesh_msg_type_t msg_type;
static uint16_t gatt_bearer_mtu;
static hci_con_handle_t gatt_bearer_con_handle;

// round-robin
static void gatt_bearer_emit_can_send_now(void){
    // if (gatt_active) return;
    int countdown = NUM_TYPES;
    while (countdown--) {
        last_sender++;
        if (last_sender == NUM_TYPES) {
            last_sender = 0;
        }
        if (request_can_send_now[last_sender]){
            request_can_send_now[last_sender] = 0;
            // emit can send now
            uint8_t event[5];
            int pos = 0;
            event[pos++] = HCI_EVENT_MESH_META;
            event[pos++] = 1;
            event[pos++] = MESH_SUBEVENT_CAN_SEND_NOW;
            little_endian_store_16(event, pos, gatt_bearer_con_handle);
            pos += 2;
            (*client_callbacks[last_sender])(HCI_EVENT_PACKET, 0, &event[0], pos);
            return;
        }
    }
}

static void gatt_bearer_emit_message_sent(mesh_msg_type_t type_id){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_MESH_META;
    event[pos++] = 1;
    event[pos++] = MESH_SUBEVENT_MESSAGE_SENT;
    little_endian_store_16(event, pos, gatt_bearer_con_handle);
    pos += 2;
    (*client_callbacks[type_id])(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}

static void gatt_bearer_emit_event_for_all(uint8_t * packet, uint16_t size){
    unsigned int i;
    for (i=0; i < NUM_TYPES; i++){
        if ( client_callbacks[i] == NULL) continue;
        (*client_callbacks[last_sender])(HCI_EVENT_PACKET, 0, packet, size);
    }
}

static void gatt_bearer_request(mesh_msg_type_t type_id){
    request_can_send_now[type_id] = 1;
    mesh_proxy_service_server_request_can_send_now(gatt_bearer_con_handle);
}


static void gatt_bearer_start_sending(hci_con_handle_t con_handle){
    uint16_t pdu_segment_len = btstack_min(proxy_pdu_size - segmentation_offset, gatt_bearer_mtu - 1 - 3);
    sar_buffer.segmentation_buffer[0] = (segmentation_state << 6) | msg_type;
    memcpy(&sar_buffer.segmentation_buffer[1], &proxy_pdu[segmentation_offset], pdu_segment_len);
    segmentation_offset += pdu_segment_len;
    mesh_proxy_service_server_send_proxy_pdu(con_handle, sar_buffer.segmentation_buffer, pdu_segment_len + 1);
    
    switch (segmentation_state){
        case MESH_MSG_SAR_FIELD_COMPLETE_MSG:
        case MESH_MSG_SAR_FIELD_LAST_SEGMENT:
            // gatt_bearer_emit_pdu_sent(0);
            outgoing_ready = 0;
            gatt_bearer_emit_message_sent(msg_type);
            break;
        case MESH_MSG_SAR_FIELD_CONTINUE:
        case MESH_MSG_SAR_FIELD_FIRST_SEGMENT:
            if ((proxy_pdu_size - segmentation_offset) > (gatt_bearer_mtu - 1 - 3)){
                segmentation_state = MESH_MSG_SAR_FIELD_CONTINUE;
            } else {
                segmentation_state = MESH_MSG_SAR_FIELD_LAST_SEGMENT;
            }
            mesh_proxy_service_server_request_can_send_now(con_handle);
            break;
        default:
            break;
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    mesh_msg_sar_field_t msg_sar_field;
    
    uint16_t pdu_segment_len;
    uint16_t pos;
    hci_con_handle_t con_handle;
    int send_to_mesh_network;

    switch (packet_type) {
        case MESH_PROXY_DATA_PACKET:
            pos = 0;
            // on provisioning PDU call packet handler with PROVISIONG_DATA type
            msg_sar_field = packet[pos] >> 6;
            msg_type = packet[pos] & 0x3F;
            pos++;

            switch (msg_type){
                case MESH_MSG_TYPE_NETWORK_PDU:
                case MESH_MSG_TYPE_BEACON:
                case MESH_MSG_TYPE_PROXY_CONFIGURATION:
                    if (!client_callbacks[msg_type]){
                        log_error("client callback not defined");
                        return;
                    } 
                    break;
                default:
                    log_info("gatt bearer: message type %d not supported", msg_type);
                    return;
            }
            pdu_segment_len = size - pos;

            if (sizeof(sar_buffer.reassembly_buffer) - reassembly_offset < pdu_segment_len) {
                log_error("sar buffer too small left %d, new to store %d", MESH_PROV_MAX_PROXY_PDU - reassembly_offset, pdu_segment_len);
                break;
            }

            // update mtu if incoming packet is larger than default
            if (size > (ATT_DEFAULT_MTU - 1)){
                log_info("Remote uses larger MTU, enable long PDUs");
                gatt_bearer_mtu = att_server_get_mtu(channel);
            }

            switch (msg_sar_field){
                case MESH_MSG_SAR_FIELD_COMPLETE_MSG:
                case MESH_MSG_SAR_FIELD_FIRST_SEGMENT:
                    memset(sar_buffer.reassembly_buffer, 0, sizeof(sar_buffer.reassembly_buffer));
                    if (sizeof(sar_buffer.reassembly_buffer) < pdu_segment_len) return;
                    memcpy(sar_buffer.reassembly_buffer, packet+pos, pdu_segment_len);
                    reassembly_offset = pdu_segment_len;
                    break;
                case MESH_MSG_SAR_FIELD_CONTINUE:
                    if ((sizeof(sar_buffer.reassembly_buffer) - reassembly_offset) < pdu_segment_len) return;
                    memcpy(sar_buffer.reassembly_buffer + reassembly_offset, packet+pos, pdu_segment_len);
                    reassembly_offset += pdu_segment_len;
                    return;
                case MESH_MSG_SAR_FIELD_LAST_SEGMENT:
                    if ((sizeof(sar_buffer.reassembly_buffer) - reassembly_offset) < pdu_segment_len) return;
                    memcpy(sar_buffer.reassembly_buffer + reassembly_offset, packet+pos, pdu_segment_len);
                    reassembly_offset += pdu_segment_len;
                    break;
                default:
                    break;
            }
            
            send_to_mesh_network = (msg_sar_field == MESH_MSG_SAR_FIELD_COMPLETE_MSG) || (msg_sar_field == MESH_MSG_SAR_FIELD_LAST_SEGMENT);
                    
            if (!send_to_mesh_network) break;
            switch (msg_type){
                case MESH_MSG_TYPE_NETWORK_PDU:
                case MESH_MSG_TYPE_BEACON:
                case MESH_MSG_TYPE_PROXY_CONFIGURATION:
                    if ((*client_callbacks[msg_type])){
                        (*client_callbacks[msg_type])(MESH_PROXY_DATA_PACKET, 0, sar_buffer.reassembly_buffer, reassembly_offset);
                    }
                    reassembly_offset = 0;
                    break;
                default:
                    log_info("gatt bearer: message type %d not supported", msg_type);
                    return;
            }
            
            break;
 
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_MESH_META:
                    switch (hci_event_mesh_meta_get_subevent_code(packet)){
                        case MESH_SUBEVENT_PROXY_CONNECTED:
                            gatt_bearer_mtu = ATT_DEFAULT_MTU;
                            gatt_bearer_con_handle = mesh_subevent_proxy_connected_get_con_handle(packet);
                            gatt_bearer_emit_event_for_all(packet, size);
                            break;
                        case MESH_SUBEVENT_PROXY_DISCONNECTED:
                            gatt_bearer_con_handle = HCI_CON_HANDLE_INVALID;
                            gatt_bearer_emit_event_for_all(packet, size);
                            break;
                        case MESH_SUBEVENT_CAN_SEND_NOW:
                            con_handle = little_endian_read_16(packet, 3); 
                            if (con_handle == HCI_CON_HANDLE_INVALID) return;

                            if (!outgoing_ready){
                                gatt_bearer_emit_can_send_now();
                                return;
                            }
                            gatt_bearer_start_sending(con_handle);
                            return;
                        default:
                            break;
                    }
            }
            break;
        default:
            break;
    }
}

void gatt_bearer_init(void){
    mesh_proxy_service_server_init();
    mesh_proxy_service_server_register_packet_handler(packet_handler);
}

void gatt_bearer_register_for_network_pdu(btstack_packet_handler_t _packet_handler){
    client_callbacks[MESH_MSG_TYPE_NETWORK_PDU] = _packet_handler;
}
void gatt_bearer_register_for_beacon(btstack_packet_handler_t _packet_handler){
    client_callbacks[MESH_MSG_TYPE_BEACON] = _packet_handler;
}
void gatt_bearer_register_for_mesh_proxy_configuration(btstack_packet_handler_t _packet_handler){
    client_callbacks[MESH_MSG_TYPE_PROXY_CONFIGURATION] = _packet_handler;
}

void gatt_bearer_request_can_send_now_for_network_pdu(void){
    gatt_bearer_request(MESH_MSG_TYPE_NETWORK_PDU);
}
void gatt_bearer_request_can_send_now_for_beacon(void){
    gatt_bearer_request(MESH_MSG_TYPE_BEACON);
}
void gatt_bearer_request_can_send_now_for_mesh_proxy_configuration(void){
    gatt_bearer_request(MESH_MSG_TYPE_PROXY_CONFIGURATION);
}

static void gatt_bearer_send_pdu(uint16_t con_handle, const uint8_t * pdu, uint16_t size){
    if (!pdu || size <= 0) return; 
    if (con_handle == HCI_CON_HANDLE_INVALID) return;
    // store pdu, request to send
    proxy_pdu = pdu;
    proxy_pdu_size = size;
    segmentation_offset = 0;

    // check if segmentation is necessary
    if (proxy_pdu_size > (gatt_bearer_mtu - 1 - 3)){
        segmentation_state = MESH_MSG_SAR_FIELD_FIRST_SEGMENT;
    } else {
        segmentation_state = MESH_MSG_SAR_FIELD_COMPLETE_MSG;
    }
    outgoing_ready = 1;
    gatt_bearer_start_sending(con_handle);
}

void gatt_bearer_send_network_pdu(const uint8_t * data, uint16_t data_len){
    msg_type = MESH_MSG_TYPE_NETWORK_PDU;
    gatt_bearer_send_pdu(gatt_bearer_con_handle, data, data_len);
}

void gatt_bearer_send_beacon(const uint8_t * data, uint16_t data_len){
    msg_type = MESH_MSG_TYPE_BEACON;
    gatt_bearer_send_pdu(gatt_bearer_con_handle, data, data_len);
}

void gatt_bearer_send_mesh_proxy_configuration(const uint8_t * data, uint16_t data_len){
    msg_type = MESH_MSG_TYPE_PROXY_CONFIGURATION;
    gatt_bearer_send_pdu(gatt_bearer_con_handle, data, data_len);
}

