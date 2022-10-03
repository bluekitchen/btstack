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

#define BTSTACK_FILE__ "mesh_provisioning_service_server.c"

#include "mesh/gatt-service/mesh_provisioning_service_server.h"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"
#include "hci.h"

#include "mesh/provisioning.h"

typedef struct {
    hci_con_handle_t con_handle;

    uint16_t data_in_client_value_handle;
    uint8_t  data_in_proxy_pdu[MESH_PROV_MAX_PROXY_PDU];
    uint8_t  link_open;
    
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


static void mesh_provisioning_service_emit_link_open(hci_con_handle_t con_handle, uint8_t status){
    uint8_t event[7] = { HCI_EVENT_MESH_META, 5, MESH_SUBEVENT_PB_TRANSPORT_LINK_OPEN, status};
    little_endian_store_16(event, 4, con_handle);
    event[6] = MESH_PB_TYPE_GATT;
    mesh_provisioning_service_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void mesh_provisioning_service_emit_link_close(hci_con_handle_t con_handle, uint8_t reason){
    uint8_t event[6] = { HCI_EVENT_MESH_META, 3, MESH_SUBEVENT_PB_TRANSPORT_LINK_CLOSED};
    little_endian_store_16(event, 3, con_handle);
    event[5] = reason;
    mesh_provisioning_service_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static mesh_provisioning_t * mesh_provisioning_service_get_instance_for_con_handle(hci_con_handle_t con_handle){
    mesh_provisioning_t * instance = &mesh_provisioning;
    log_debug("mesh_provisioning_service_get_instance_for_con_handle, handle %x (instance %x)", con_handle, instance->con_handle);
    if (instance->con_handle != HCI_CON_HANDLE_INVALID && instance->con_handle != con_handle) return NULL;
    instance->con_handle = con_handle;
    return instance;
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
    if (attribute_handle == instance->data_out_client_configuration_descriptor_handle){
        if (buffer && buffer_size >= 2){
            little_endian_store_16(buffer, 0, instance->data_out_client_configuration_descriptor_value);
        }
        return 2;
    }
    log_info("mesh_provisioning_service_read_callback: not handled read on handle 0x%02x", attribute_handle);
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
        if (!mesh_provisioning_service_packet_handler) return 0;
        mesh_msg_type_t msg_type = buffer[0] & 0x3f;
        switch (msg_type){
            case MESH_MSG_TYPE_PROXY_CONFIGURATION:
                log_info("data_in_client_value_handle not handled, msg_type %d", msg_type);
                break;
            case MESH_MSG_TYPE_PROVISIONING_PDU:
                (*mesh_provisioning_service_packet_handler)(PROVISIONING_DATA_PACKET, con_handle, buffer, buffer_size);
                break;
            default:
                log_info("data_in_client_value_handle not written, wrong msg_type %d", msg_type);
                return ATT_ERROR_REQUEST_NOT_SUPPORTED; 
        }  
        return 0;
    }

    if (attribute_handle == instance->data_out_client_configuration_descriptor_handle){
        if (buffer_size < 2){
            return ATT_ERROR_INVALID_OFFSET;
        }
        uint16_t enable_data_out_notify = little_endian_read_16(buffer, 0);
        if (enable_data_out_notify){
            if (instance->data_out_client_configuration_descriptor_value) return 0;
            instance->data_out_client_configuration_descriptor_value = enable_data_out_notify;
            instance->link_open = 1;
            mesh_provisioning_service_emit_link_open(con_handle, 0);
        } else {
            if (!instance->data_out_client_configuration_descriptor_value) return 0;
            instance->data_out_client_configuration_descriptor_value = enable_data_out_notify;
            instance->link_open = 1;
            mesh_provisioning_service_emit_link_open(con_handle, 0);
        }
        log_info("mesh_provisioning_service_write_callback: enable_data_out_notify %d, con handle 0x%02x", enable_data_out_notify, con_handle);
        return 0;
    }
    log_info("mesh_provisioning_service_write_callback: not handled write on handle 0x%02x, buffer size %d", attribute_handle, buffer_size);
    return 0;
}

static void mesh_provisioning_service_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_DISCONNECTION_COMPLETE) return;
    hci_con_handle_t con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
    mesh_provisioning_t * instance = mesh_provisioning_service_get_instance_for_con_handle(con_handle);
    if (!instance) return;
    if (instance->link_open){
        // emit close, free instance
        instance->link_open = 0;
        instance->con_handle = HCI_CON_HANDLE_INVALID;
        instance->data_out_client_configuration_descriptor_value = 0;
        mesh_provisioning_service_emit_link_close(con_handle, 0);
    }
}

void mesh_provisioning_service_server_init(void){
    mesh_provisioning_t * instance = &mesh_provisioning;
    instance->con_handle = HCI_CON_HANDLE_INVALID;

    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING, &start_handle, &end_handle);
	btstack_assert(service_found != 0);
	UNUSED(service_found);

    instance->data_in_client_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROVISIONING_DATA_IN);
    instance->data_out_client_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROVISIONING_DATA_OUT);
    instance->data_out_client_configuration_descriptor_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROVISIONING_DATA_OUT);
    
    log_info("DataIn     value handle 0x%02x", instance->data_in_client_value_handle);
    log_info("DataOut    value handle 0x%02x", instance->data_out_client_value_handle);
    log_info("DataOut CC value handle 0x%02x", instance->data_out_client_configuration_descriptor_handle);
    
    mesh_provisioning_service.start_handle   = start_handle;
    mesh_provisioning_service.end_handle     = end_handle;
    mesh_provisioning_service.read_callback  = &mesh_provisioning_service_read_callback;
    mesh_provisioning_service.write_callback = &mesh_provisioning_service_write_callback;
    mesh_provisioning_service.packet_handler = &mesh_provisioning_service_server_packet_handler;
    
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
    mesh_provisioning_t * instance = mesh_provisioning_service_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("mesh_provisioning_service_server_request_can_send_now: instance is null, 0x%2x", con_handle);
        return;
    }
    instance->pdu_response_callback.callback = &mesh_provisioning_service_can_send_now;
    instance->pdu_response_callback.context  = (void*) (uintptr_t) con_handle;
    att_server_register_can_send_now_callback(&instance->pdu_response_callback, con_handle);
}

void mesh_provisioning_service_server_register_packet_handler(btstack_packet_handler_t callback){
    mesh_provisioning_service_packet_handler = callback;
}
