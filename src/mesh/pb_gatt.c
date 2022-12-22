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

#define BTSTACK_FILE__ "pb_gatt.c"

#include "mesh/pb_gatt.h"

#include <stdlib.h>
#include <string.h>

#include "ble/att_server.h"
#include "mesh/gatt-service/mesh_provisioning_service_server.h"

#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "mesh/provisioning.h"

/************** PB GATT Mesh Provisioning ********************/

// share buffer for reassembly and segmentation - protocol is half-duplex
static union {
    uint8_t  reassembly_buffer[MESH_PROV_MAX_PROXY_PDU];
    uint8_t  segmentation_buffer[MESH_PROV_MAX_PROXY_PDU];
} sar_buffer;

static const uint8_t * proxy_pdu;
static uint16_t proxy_pdu_size;

static uint16_t reassembly_offset;
static uint16_t segmentation_offset;
static mesh_msg_sar_field_t segmentation_state;
static uint16_t pb_gatt_mtu;

static btstack_packet_handler_t pb_gatt_packet_handler;
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void pb_gatt_emit_pdu_sent(uint8_t status){
    uint8_t event[] = {HCI_EVENT_MESH_META, 2, MESH_SUBEVENT_PB_TRANSPORT_PDU_SENT, status};
    pb_gatt_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    mesh_msg_sar_field_t msg_sar_field;
    mesh_msg_type_t msg_type;
    uint16_t pdu_segment_len;
    uint16_t pos;
    hci_con_handle_t con_handle;

    switch (packet_type) {
        case PROVISIONING_DATA_PACKET:
            pos = 0;
            // on provisioning PDU call packet handler with PROVISIONG_DATA type
            msg_sar_field = (mesh_msg_sar_field_t) (packet[pos] >> 6);
            msg_type = (mesh_msg_type_t) (packet[pos] & 0x3F);
            pos++;
            if (msg_type != MESH_MSG_TYPE_PROVISIONING_PDU) return;
            if (!pb_gatt_packet_handler) return;
            
            pdu_segment_len = size - pos;

            if (sizeof(sar_buffer.reassembly_buffer) - reassembly_offset < pdu_segment_len) {
                log_error("sar buffer too small left %d, new to store %d", MESH_PROV_MAX_PROXY_PDU - reassembly_offset, pdu_segment_len);
                break;
            }

            // update mtu if incoming packet is larger than default
            if (size > (ATT_DEFAULT_MTU - 1)){
                log_info("Remote uses larger MTU, enable long PDUs");
                pb_gatt_mtu = att_server_get_mtu(channel);
            }
            
            switch (msg_sar_field){
                case MESH_MSG_SAR_FIELD_FIRST_SEGMENT:
                    memset(sar_buffer.reassembly_buffer, 0, sizeof(sar_buffer.reassembly_buffer));
                    (void)memcpy(sar_buffer.reassembly_buffer, packet + pos,
                                 pdu_segment_len);
                    reassembly_offset = pdu_segment_len;
                    return;
                case MESH_MSG_SAR_FIELD_CONTINUE:
                    (void)memcpy(sar_buffer.reassembly_buffer + reassembly_offset,
                                 packet + pos, pdu_segment_len);
                    reassembly_offset += pdu_segment_len;
                    return;
                case MESH_MSG_SAR_FIELD_LAST_SEGMENT:
                    (void)memcpy(sar_buffer.reassembly_buffer + reassembly_offset,
                                 packet + pos, pdu_segment_len);
                    reassembly_offset += pdu_segment_len;
                    // send to provisioning device
                    pb_gatt_packet_handler(PROVISIONING_DATA_PACKET, 0, sar_buffer.reassembly_buffer, reassembly_offset); 
                    reassembly_offset = 0;
                    break; 
                case MESH_MSG_SAR_FIELD_COMPLETE_MSG:
                    // send to provisioning device
                    pb_gatt_packet_handler(PROVISIONING_DATA_PACKET, 0, packet+pos, pdu_segment_len); 
                    break;
                default:
                    btstack_assert(false);
                    break;
            }
            break;

        case HCI_EVENT_PACKET:
            if (hci_event_packet_get_type(packet) != HCI_EVENT_MESH_META) break;
            
            switch (hci_event_mesh_meta_get_subevent_code(packet)){
                case MESH_SUBEVENT_PB_TRANSPORT_LINK_OPEN:
                case MESH_SUBEVENT_PB_TRANSPORT_LINK_CLOSED:
                    // Forward link open/close
                    pb_gatt_mtu = ATT_DEFAULT_MTU;
                    pb_gatt_packet_handler(HCI_EVENT_PACKET, 0, packet, size);
                    break; 
                case MESH_SUBEVENT_CAN_SEND_NOW:
                    con_handle = little_endian_read_16(packet, 3); 
                    if (con_handle == HCI_CON_HANDLE_INVALID) return;

                    sar_buffer.segmentation_buffer[0] = (segmentation_state << 6) | MESH_MSG_TYPE_PROVISIONING_PDU;
                    pdu_segment_len = btstack_min(proxy_pdu_size - segmentation_offset, pb_gatt_mtu - 1);
                    (void)memcpy(&sar_buffer.segmentation_buffer[1],
                                 &proxy_pdu[segmentation_offset],
                                 pdu_segment_len);
                    segmentation_offset += pdu_segment_len;

                    mesh_provisioning_service_server_send_proxy_pdu(con_handle, sar_buffer.segmentation_buffer, pdu_segment_len + 1);
                    
                    switch (segmentation_state){
                        case MESH_MSG_SAR_FIELD_COMPLETE_MSG:
                        case MESH_MSG_SAR_FIELD_LAST_SEGMENT:
                            pb_gatt_emit_pdu_sent(0);
                            break;
                        case MESH_MSG_SAR_FIELD_CONTINUE:
                        case MESH_MSG_SAR_FIELD_FIRST_SEGMENT:
                            if ((proxy_pdu_size - segmentation_offset) > (pb_gatt_mtu - 1)){
                                segmentation_state = MESH_MSG_SAR_FIELD_CONTINUE;
                            } else {
                                segmentation_state = MESH_MSG_SAR_FIELD_LAST_SEGMENT;
                            }
                            mesh_provisioning_service_server_request_can_send_now(con_handle);
                            break;
                        default:
                            btstack_assert(false);
                            break;
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

/**
 * Setup mesh provisioning service
 * @param device_uuid
 */
void pb_gatt_init(void){
    // setup mesh provisioning service
    mesh_provisioning_service_server_init();
    mesh_provisioning_service_server_register_packet_handler(packet_handler);
}

/**
 * Register listener for Provisioning PDUs 
 */
void pb_gatt_register_packet_handler(btstack_packet_handler_t _packet_handler){
    pb_gatt_packet_handler = _packet_handler;
}

/**
 * Send pdu
 * @param con_handle
 * @param pdu
 * @param pdu_size
 */
void pb_gatt_send_pdu(uint16_t con_handle, const uint8_t * pdu, uint16_t size){
    if (!pdu || size <= 0) return; 
    if (con_handle == HCI_CON_HANDLE_INVALID) return;
    // store pdu, request to send
    proxy_pdu = pdu;
    proxy_pdu_size = size;
    segmentation_offset = 0;

    // check if segmentation is necessary
    if (proxy_pdu_size > (pb_gatt_mtu - 1)){
        segmentation_state = MESH_MSG_SAR_FIELD_FIRST_SEGMENT;
    } else {
        segmentation_state = MESH_MSG_SAR_FIELD_COMPLETE_MSG;
    }
    mesh_provisioning_service_server_request_can_send_now(con_handle);
}
/**
 * Close Link
 * @param con_handle
 * @param reason 0 = success, 1 = timeout, 2 = fail
 */
void pb_gatt_close_link(hci_con_handle_t con_handle, uint8_t reason){
    UNUSED(con_handle);
    UNUSED(reason);
}

/**
 * Setup Link with unprovisioned device
 * @param device_uuid
 * @return con_handle or HCI_CON_HANDLE_INVALID
 */
uint16_t pb_gatt_create_link(const uint8_t * device_uuid){
    UNUSED(device_uuid);
    return HCI_CON_HANDLE_INVALID;
}
