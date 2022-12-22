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

#define BTSTACK_FILE__ "ublox_spp_service_server.c"

/**
 * Implementation of the ublox SPP-like profile
 *
 * To use with your application, add '#import <ublox_spp_service.gatt' to your .gatt file
 * and call all functions below. All strings and blobs need to stay valid after calling the functions.
 */

#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "hci.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "ble/gatt-service/ublox_spp_service_server.h"

#define UBLOX_SPP_MAX_CREDITS           16
#define UBLOX_SPP_CREDITS_THRESHOLD      8

//

typedef struct {
    hci_con_handle_t con_handle;
    
    // characteristic: FIFO 
    uint16_t fifo_value_handle;
    uint8_t  data[20];
    
    // characteristic descriptor: Client Characteristic Configuration
    uint16_t fifo_client_configuration_descriptor_handle;
    uint16_t fifo_client_configuration_descriptor_value; // none, notify or indicate;
    btstack_context_callback_registration_t fifo_callback;

    // characteristic: Flow control/credits 
    uint16_t credits_value_handle;
    uint16_t incoming_credits;
    uint16_t outgoing_credits;
    uint8_t  delta_credits;

    // characteristic descriptor: Client Characteristic Configuration
    uint16_t credits_client_configuration_descriptor_handle;
    uint16_t credits_client_configuration_descriptor_value;
    
    btstack_context_callback_registration_t credits_callback;

    btstack_packet_handler_t client_packet_handler;

    // flow control
    btstack_context_callback_registration_t * request;
} ublox_spp_service_t;

static att_service_handler_t  ublox_spp_service;
static ublox_spp_service_t    ublox_spp;

static void ublox_spp_service_emit_state(ublox_spp_service_t * instance, bool enabled){
    uint8_t event[5];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = enabled ? GATTSERVICE_SUBEVENT_SPP_SERVICE_CONNECTED : GATTSERVICE_SUBEVENT_SPP_SERVICE_DISCONNECTED;
    little_endian_store_16(event,pos, instance->con_handle);
    pos += 2;
    (*instance->client_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

static int ublox_spp_service_flow_control_enabled(ublox_spp_service_t * instance){
    return instance->credits_client_configuration_descriptor_value;
}

static ublox_spp_service_t * ublox_get_instance_for_con_handle(hci_con_handle_t con_handle){
    ublox_spp_service_t * instance = &ublox_spp;
    if (con_handle == HCI_CON_HANDLE_INVALID) return NULL;
    instance->con_handle = con_handle;
    return instance;
}

static inline void ublox_spp_service_init_credits(ublox_spp_service_t * instance){
    instance->incoming_credits = 0;
    instance->outgoing_credits = 0;
    instance->delta_credits = UBLOX_SPP_MAX_CREDITS;
}

static uint16_t ublox_spp_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(offset);
    UNUSED(buffer_size);
    ublox_spp_service_t * instance = ublox_get_instance_for_con_handle(con_handle);
    if (!instance) return 0; 

    if (attribute_handle == instance->fifo_client_configuration_descriptor_handle){
        if (buffer != NULL){
            little_endian_store_16(buffer, 0, instance->fifo_client_configuration_descriptor_value);
        }
        return 2;
    }

    if (attribute_handle == instance->credits_client_configuration_descriptor_handle){
        if (buffer != NULL){
            little_endian_store_16(buffer, 0, instance->credits_client_configuration_descriptor_value);
        }
        return 2;
    }
    return 0;
}


static int ublox_spp_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);
    ublox_spp_service_t * instance = ublox_get_instance_for_con_handle(con_handle);
    if (!instance) return 0; 

    if (attribute_handle == instance->fifo_value_handle){
        instance->client_packet_handler(RFCOMM_DATA_PACKET, (uint16_t) con_handle, &buffer[0], buffer_size);
        if (!ublox_spp_service_flow_control_enabled(instance)) return 0;
        if (!instance->incoming_credits) return 0;
        instance->incoming_credits--;
        if (instance->incoming_credits < UBLOX_SPP_CREDITS_THRESHOLD){
            att_server_request_to_send_notification(&instance->credits_callback, instance->con_handle);
        }
    }

    if (attribute_handle == instance->fifo_client_configuration_descriptor_handle){
        if (buffer_size < 2u){
            return ATT_ERROR_INVALID_OFFSET;
        }
        instance->fifo_client_configuration_descriptor_value = little_endian_read_16(buffer, 0);
        log_info("ublox spp service FIFO control: %d", instance->fifo_client_configuration_descriptor_value);
        ublox_spp_service_emit_state(instance,  instance->fifo_client_configuration_descriptor_value != 0);
    }

    if (attribute_handle == instance->credits_value_handle){
        if (!ublox_spp_service_flow_control_enabled(instance)) return 0;
        int8_t credits = (int8_t)buffer[0];
        if (credits <= 0) return 0;
        instance->outgoing_credits += credits;
        log_info("received outgoing credits, total %d", instance->outgoing_credits);
        // Provide credits
        att_server_request_to_send_notification(&instance->credits_callback, instance->con_handle);
        
        // handle user request
        if (instance->request){
            btstack_context_callback_registration_t * request = instance->request;
            instance->request = NULL;
            att_server_request_to_send_notification(request, instance->con_handle);
        }
    }

    if (attribute_handle == instance->credits_client_configuration_descriptor_handle){
        if (buffer_size < 2u){
            return ATT_ERROR_INVALID_OFFSET;
        }
        instance->credits_client_configuration_descriptor_value = little_endian_read_16(buffer, 0);
        log_info("ublox spp service Credits control: %d", instance->credits_client_configuration_descriptor_value);
        ublox_spp_service_init_credits(instance);
    }
    
    return 0;
}

static void ublox_spp_credits_callback(void * context){
    ublox_spp_service_t * instance = (ublox_spp_service_t *) context;
    if (!instance) return;

    instance->delta_credits = UBLOX_SPP_MAX_CREDITS - instance->incoming_credits;
    if (instance->delta_credits){
        instance->incoming_credits = UBLOX_SPP_MAX_CREDITS;
        att_server_notify(instance->con_handle, instance->credits_value_handle, &instance->delta_credits, 1);
    }
}
/**
 * @brief Init ublox SPP Service Server with ATT DB
 * @param callback for tx data from peer
 */
void ublox_spp_service_server_init(btstack_packet_handler_t packet_handler){

    static const uint8_t ublox_spp_profile_uuid128[] = { 0x24, 0x56, 0xE1, 0xB9, 0x26, 0xE2, 0x8F, 0x83, 0xE7, 0x44, 0xF3, 0x4F, 0x01, 0xE9, 0xD7, 0x01 };
    static const uint8_t ublox_spp_fifo_uuid128[]    = { 0x24, 0x56, 0xE1, 0xB9, 0x26, 0xE2, 0x8F, 0x83, 0xE7, 0x44, 0xF3, 0x4F, 0x01, 0xE9, 0xD7, 0x03 };
    static const uint8_t ublox_spp_credits_uuid128[] = { 0x24, 0x56, 0xE1, 0xB9, 0x26, 0xE2, 0x8F, 0x83, 0xE7, 0x44, 0xF3, 0x4F, 0x01, 0xE9, 0xD7, 0x04 };

    ublox_spp_service_t * instance = &ublox_spp;
    instance->client_packet_handler = packet_handler;

    instance->credits_callback.callback = ublox_spp_credits_callback;
    instance->credits_callback.context = instance;
    ublox_spp_service_init_credits(instance);


    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid128(ublox_spp_profile_uuid128, &start_handle, &end_handle);
	btstack_assert(service_found != 0);
	UNUSED(service_found);

    // get characteristic value handle and client configuration handle
    // FIFO
    instance->fifo_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(start_handle, end_handle, ublox_spp_fifo_uuid128);
    instance->fifo_client_configuration_descriptor_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(start_handle, end_handle, ublox_spp_fifo_uuid128);
    // Credits
    instance->credits_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(start_handle, end_handle, ublox_spp_credits_uuid128);
    instance->credits_client_configuration_descriptor_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(start_handle, end_handle, ublox_spp_credits_uuid128);
    
    log_info("FIFO        value handle 0x%02x", instance->fifo_value_handle);
    log_info("FIFO CCC    value handle 0x%02x", instance->fifo_client_configuration_descriptor_handle);
    log_info("Credits     value handle 0x%02x", instance->credits_value_handle);
    log_info("Credits CCC value handle 0x%02x", instance->credits_client_configuration_descriptor_handle);
    
    // register service with ATT Server
    ublox_spp_service.start_handle   = start_handle;
    ublox_spp_service.end_handle     = end_handle;
    ublox_spp_service.read_callback  = &ublox_spp_service_read_callback;
    ublox_spp_service.write_callback = &ublox_spp_service_write_callback;
    att_server_register_service_handler(&ublox_spp_service);
}

/** 
 * @brief Queue send request. When called, one packet can be send via ublox_spp_service_send below
 * @param request
 * @param con_handle
 */
void ublox_spp_service_server_request_can_send_now(btstack_context_callback_registration_t * request, hci_con_handle_t con_handle){
    ublox_spp_service_t * instance = &ublox_spp;
    if (!ublox_spp_service_flow_control_enabled(instance) || instance->outgoing_credits) {
        att_server_request_to_send_notification(request, con_handle);
        return;
    }
    instance->request = request;
}

/**
 * @brief Send data
 * @param con_handle
 * @param data
 * @param size
 */
int ublox_spp_service_server_send(hci_con_handle_t con_handle, const uint8_t * data, uint16_t size){
    ublox_spp_service_t * instance = &ublox_spp;
    if (ublox_spp_service_flow_control_enabled(instance)){
        if (instance->outgoing_credits > 0u){
            instance->outgoing_credits--;
        }
    }
    return att_server_notify(con_handle, instance->fifo_value_handle, &data[0], size);
}

