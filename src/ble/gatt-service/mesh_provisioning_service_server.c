/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "mesh_provisioning_service_server.c"

#include "bluetooth.h"
#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "l2cap.h"
#include "hci.h"
#include "btstack_event.h"

#include "ble/gatt-service/mesh_provisioning_service_server.h"
#include "provisioning.h"

typedef struct {
    hci_con_handle_t con_handle;

    uint16_t data_in_client_value_handle;
    uint8_t  data_in_proxy_pdu[MESH_PROV_MAX_PROXY_PDU];
    
    // Mesh Provisioning Data Out
    uint16_t data_out_client_value_handle;
    uint8_t  data_out_proxy_pdu[MESH_PROV_MAX_PROXY_PDU];
    uint16_t data_out_proxy_pdu_size;

    // Mesh Provisioning Data Out Notification
    uint16_t data_out_client_configuration_descriptor_handle;
    uint16_t data_out_client_configuration_descriptor_value;
    btstack_context_callback_registration_t data_out_notify_callback;

    btstack_context_callback_registration_t  pdu_response_callback;
} mesh_provisioning_t;

static btstack_packet_handler_t mesh_provisioning_service_packet_handler;
static att_service_handler_t mesh_provisioning_service;
static mesh_provisioning_t mesh_provisioning;

static mesh_provisioning_t * mesh_provisioning_service_get_instance_for_con_handle(hci_con_handle_t con_handle){
    mesh_provisioning_t * instance = &mesh_provisioning;
    if (con_handle == HCI_CON_HANDLE_INVALID) return NULL;
    instance->con_handle = con_handle;
    return instance;
}

static hci_con_handle_t get_con_handle(void){
    return mesh_provisioning.con_handle;
}

static void pb_adv_emit_link_open(uint8_t status, uint16_t pb_adv_cid){
    uint8_t event[6] = { HCI_EVENT_MESH_META, 6, MESH_PB_ADV_LINK_OPEN, status};
    little_endian_store_16(event, 4, pb_adv_cid);
    mesh_provisioning_service_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void pb_adv_emit_link_close(uint16_t pb_adv_cid, uint8_t reason){
    uint8_t event[5] = { HCI_EVENT_MESH_META, 6, MESH_PB_ADV_LINK_CLOSED};
    little_endian_store_16(event, 4, pb_adv_cid);
    mesh_provisioning_service_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static uint16_t mesh_provisioning_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(attribute_handle);
    UNUSED(offset);
    UNUSED(buffer_size);
    
    mesh_provisioning_t * instance = mesh_provisioning_service_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("mesh_provisioning_service_read_callback: instance is null");
        return 0;
    }
    printf("mesh_provisioning_service_read_callback: not handeled read on handle 0x%02x\n", attribute_handle);
    return 0;
}

static int mesh_provisioning_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer_size);
    
    mesh_provisioning_t * instance = mesh_provisioning_service_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("mesh_provisioning_service_write_callback: instance is null");
        return 0;
    }

    if (attribute_handle == instance->data_in_client_value_handle){
        printf("mesh_provisioning_service_write_callback: handle write on 0x%02x, len %u\n", attribute_handle, buffer_size);
        if (!mesh_provisioning_service_packet_handler) return 0;
        (*mesh_provisioning_service_packet_handler)(PROVISIONING_DATA_PACKET, 0, buffer, buffer_size);
        return 0;
    }

    if (attribute_handle == instance->data_out_client_configuration_descriptor_handle){
        if (buffer_size < 2){
            return ATT_ERROR_INVALID_OFFSET;
        }
        instance->data_out_client_configuration_descriptor_value = little_endian_read_16(buffer, 0);
        printf("mesh_provisioning_service_write_callback: data out notify enabled %d\n", instance->data_out_client_configuration_descriptor_value);
        if (instance->data_out_client_configuration_descriptor_value){
            pb_adv_emit_link_close(con_handle, 0);
        } else {
            pb_adv_emit_link_open(0, con_handle);
        }
        return 0;
    }
    printf("mesh_provisioning_service_write_callback: not handeled write on handle 0x%02x, buffer size %d\n", attribute_handle, buffer_size);
    return 0;
}

void mesh_provisioning_service_server_init(void){
    mesh_provisioning_t * instance = &mesh_provisioning;
    if (!instance){
        log_error("mesh_provisioning_service_server_init: instance is null");
        return;
    }

    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING, &start_handle, &end_handle);

    if (!service_found) return;

    instance->data_in_client_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROVISIONING_DATA_IN);
    instance->data_out_client_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROVISIONING_DATA_OUT);
    instance->data_out_client_configuration_descriptor_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROVISIONING_DATA_OUT);
    
    printf("DataIn     value handle 0x%02x\n", instance->data_in_client_value_handle);
    printf("DataOut    value handle 0x%02x\n", instance->data_out_client_value_handle);
    printf("DataOut CC value handle 0x%02x\n", instance->data_out_client_configuration_descriptor_handle);
    
    mesh_provisioning_service.start_handle   = start_handle;
    mesh_provisioning_service.end_handle     = end_handle;
    mesh_provisioning_service.read_callback  = &mesh_provisioning_service_read_callback;
    mesh_provisioning_service.write_callback = &mesh_provisioning_service_write_callback;
    
    att_server_register_service_handler(&mesh_provisioning_service);
}

void mesh_provisioning_service_server_send_proxy_pdu(uint16_t con_handle, const uint8_t * proxy_pdu, uint16_t proxy_pdu_size){
    mesh_provisioning_t * instance = mesh_provisioning_service_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("mesh_provisioning_service_server_data_out_can_send_now: instance is null");
        return;
    }
    att_server_notify(instance->con_handle, instance->data_out_client_value_handle, proxy_pdu, proxy_pdu_size); 
}

static void mesh_provisioning_service_can_send_now(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t) (uintptr_t) context;
    // notify client
    mesh_provisioning_t * instance = mesh_provisioning_service_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return;
    }

    if (!mesh_provisioning_service_packet_handler) return;
    uint8_t buffer[5];
    buffer[0] = HCI_EVENT_MESH_META;
    buffer[1] = 3;
    buffer[2] = MESH_SUBEVENT_CAN_SEND_NOW;
    little_endian_store_16(buffer, 3, (uint16_t) con_handle);
    (*mesh_provisioning_service_packet_handler)(HCI_EVENT_PACKET, 0, buffer, sizeof(buffer));
}

void mesh_provisioning_service_server_request_can_send_now(hci_con_handle_t con_handle){
    // printf("mesh_provisioning_service_server_request_can_send_now, con handle 0x%02x\n", con_handle);
    mesh_provisioning_t * instance = mesh_provisioning_service_get_instance_for_con_handle(con_handle);
    if (!instance){
        printf("mesh_provisioning_service_server_request_can_send_now: instance is null, 0x%2x\n", con_handle);
        return;
    }

    instance->pdu_response_callback.callback = &mesh_provisioning_service_can_send_now;
    instance->pdu_response_callback.context  = (void*) (uintptr_t) con_handle;
    att_server_register_can_send_now_callback(&instance->pdu_response_callback, con_handle);
}

void mesh_provisioning_service_server_register_packet_handler(btstack_packet_handler_t callback){
    mesh_provisioning_service_packet_handler = callback;
}

/************** PB ADV Impl for GATT Mesh Provisioning ********************/
typedef enum {
    MESH_MSG_SAR_FIELD_COMPLETE_MSG = 0,
    MESH_MSG_SAR_FIELD_FIRST_SEGMENT,
    MESH_MSG_SAR_FIELD_CONTINUE,
    MESH_MSG_SAR_FIELD_LAST_SEGMENT
} mesh_msg_sar_field_t; // Message segmentation and reassembly information

typedef enum {
    MESH_MSG_TYPE_NETWORK_PDU = 0,
    MESH_MSG_TYPE_BEACON,
    MESH_MSG_TYPE_PROXY_CONFIGURATION,
    MESH_MSG_TYPE_PROVISIONING_PDU
} mesh_msg_type_t;

static const uint8_t * pb_adv_own_device_uuid;
static const uint8_t * proxy_pdu;
static uint16_t proxy_pdu_size;
static uint16_t con_handle;
static uint8_t  sar_buffer[MESH_PROV_MAX_PROXY_PDU];
static uint16_t sar_offset;

static btstack_packet_handler_t pb_adv_packet_handler;
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
/**
 * Initialize Provisioning Bearer using Advertisement Bearer
 * @param DeviceUUID
 */
void pb_adv_init(const uint8_t * device_uuid){
    pb_adv_own_device_uuid = device_uuid;
    // setup mesh provisioning service
    mesh_provisioning_service_server_init();
    mesh_provisioning_service_server_register_packet_handler(packet_handler);
}

static void pb_adv_emit_pdu_sent(uint8_t status){
    uint8_t event[] = {HCI_EVENT_MESH_META, 2, MESH_PB_ADV_PDU_SENT, status};
    pb_adv_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

/**
 * Register listener for Provisioning PDUs and MESH_PBV_ADV_SEND_COMPLETE
 */
void pb_adv_register_packet_handler(btstack_packet_handler_t _packet_handler){
    pb_adv_packet_handler = _packet_handler;
}

/** 
 * Send Provisioning PDU
 */
static uint8_t  buffer[100];
static uint16_t buffer_offset;
static mesh_msg_sar_field_t buffer_state;
static uint16_t pb_gatt_mtu;

void pb_adv_send_pdu(const uint8_t * pdu, uint16_t size){
    if (!pdu || size <= 0) return; 
    // store pdu, request to send
    printf_hexdump(pdu, size);
    proxy_pdu = pdu;
    proxy_pdu_size = size;
    buffer_offset = 0;


    // check if segmentation is necessary
    if (proxy_pdu_size > (pb_gatt_mtu - 1)){
        buffer_state = MESH_MSG_SAR_FIELD_FIRST_SEGMENT;
    } else {
        buffer_state = MESH_MSG_SAR_FIELD_COMPLETE_MSG;
    }
    mesh_provisioning_service_server_request_can_send_now(get_con_handle());
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    mesh_msg_sar_field_t msg_sar_field;
    mesh_msg_type_t msg_type;
    int pdu_segment_len;
    int pos;

    switch (packet_type) {
        case PROVISIONING_DATA_PACKET:
            pos = 0;
            // on provisioning PDU call packet handler with PROVISIONG_DATA type
            msg_sar_field = packet[pos] >> 6;
            msg_type = packet[pos] & 0x3F;
            pos++;
            if (msg_type != MESH_MSG_TYPE_PROVISIONING_PDU) return;
            if (!pb_adv_packet_handler) return;
            
            pdu_segment_len = size - pos;

            if (sizeof(sar_buffer) - sar_offset < pdu_segment_len) {
                printf("sar buffer too small left %d, new to store %d\n", MESH_PROV_MAX_PROXY_PDU - sar_offset, pdu_segment_len);
                break;
            }

            // update mtu if incoming packet is larger than default
            if (size > (ATT_DEFAULT_MTU - 1)){
                log_info("Remote uses larger MTU, enable long PDUs");
                pb_gatt_mtu = att_server_get_mtu(con_handle);
            }
            
            switch (msg_sar_field){
                case MESH_MSG_SAR_FIELD_FIRST_SEGMENT:
                    memset(sar_buffer, 0, sizeof(sar_buffer));
                    memcpy(sar_buffer, packet+pos, pdu_segment_len);
                    sar_offset = pdu_segment_len;
                    return;
                case MESH_MSG_SAR_FIELD_CONTINUE:
                    memcpy(sar_buffer + sar_offset, packet+pos, pdu_segment_len);
                    sar_offset += pdu_segment_len;
                    return;
                case MESH_MSG_SAR_FIELD_LAST_SEGMENT:
                    memcpy(sar_buffer + sar_offset, packet+pos, pdu_segment_len);
                    sar_offset += pdu_segment_len;
                    // send to provisioning device
                    pb_adv_packet_handler(PROVISIONING_DATA_PACKET, 0, sar_buffer, sar_offset); 
                    sar_offset = 0;
                    break; 
                case MESH_MSG_SAR_FIELD_COMPLETE_MSG:
                    // send to provisioning device
                    pb_adv_packet_handler(PROVISIONING_DATA_PACKET, 0, packet+pos, pdu_segment_len); 
                    break;
            }
            break;

        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_MESH_META:
                    switch (hci_event_mesh_meta_get_subevent_code(packet)){
                        case MESH_PB_ADV_LINK_OPEN:
                        case MESH_PB_ADV_LINK_CLOSED:
                            // Forward link open/close
                            pb_gatt_mtu = ATT_DEFAULT_MTU;
                            pb_adv_packet_handler(HCI_EVENT_PACKET, 0, packet, size);
                            break; 
                        case MESH_SUBEVENT_CAN_SEND_NOW:
                            con_handle = little_endian_read_16(packet, 3); 
                            if (con_handle == HCI_CON_HANDLE_INVALID) return;

                            buffer[0] = (buffer_state << 6) | MESH_MSG_TYPE_PROVISIONING_PDU;
                            pdu_segment_len = btstack_min(proxy_pdu_size - buffer_offset, pb_gatt_mtu - 1);
                            memcpy(&buffer[1], &proxy_pdu[buffer_offset], pdu_segment_len);
                            buffer_offset += pdu_segment_len;

                            mesh_provisioning_service_server_send_proxy_pdu(con_handle, buffer, pdu_segment_len + 1);
                            
                            switch (buffer_state){
                                case MESH_MSG_SAR_FIELD_COMPLETE_MSG:
                                case MESH_MSG_SAR_FIELD_LAST_SEGMENT:
                                    pb_adv_emit_pdu_sent(0);
                                    break;
                                case MESH_MSG_SAR_FIELD_CONTINUE:
                                case MESH_MSG_SAR_FIELD_FIRST_SEGMENT:
                                    if ((proxy_pdu_size - buffer_offset) > (pb_gatt_mtu - 1)){
                                        buffer_state = MESH_MSG_SAR_FIELD_CONTINUE;
                                    } else {
                                        buffer_state = MESH_MSG_SAR_FIELD_LAST_SEGMENT;
                                    }
                                    mesh_provisioning_service_server_request_can_send_now(con_handle);
                                    break;
                            }

                            // printf("sending packet, size %d, MTU %d: ", proxy_pdu_size, pb_gatt_mtu);
                            // printf_hexdump(proxy_pdu, proxy_pdu_size);
                            // printf("\n");
                            // mesh_provisioning_service_server_request_can_send_now(con_handle);

                            break;
                        default:
                            break;
                    }
                    printf("\n");
            }
            break;
        default:
            break;
    }
}


/**
 * Close Link
 * @param pb_adv_cid
 * @param reason 0 = success, 1 = timeout, 2 = fail
 */
void pb_adv_close_link(uint16_t pb_adv_cid, uint8_t reason){
}

/**
 * Setup Link with unprovisioned device
 * @param DeviceUUID
 * @returns pb_adv_cid or 0
 */
uint16_t pb_adv_create_link(const uint8_t * device_uuid){
    return 0;
}