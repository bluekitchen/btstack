/*
 * Copyright (C) 2022 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "coordinated_set_identification_service_server.c"

#include <stdio.h>

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/gatt-service/coordinated_set_identification_service_server.h"
#include "le-audio/le_audio_util.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

static att_service_handler_t    coordinated_set_identification_service;
static csis_server_data_t       csis_server_data;
static btstack_packet_handler_t csis_event_callback;
static csis_remote_client_t   * csis_clients;

// csis server data
static uint16_t sirk_handle;
static uint16_t sirk_configuration_handle;
static uint16_t sirk_configuration;

static uint8_t  sirk[16];
static csis_sirk_type_t  sirk_type;

static uint16_t size_handle;
static uint16_t size_configuration_handle;
static uint16_t size_configuration;
    
static uint16_t lock_handle;
static uint16_t lock_configuration_handle;
static uint16_t lock_configuration;
    
static uint16_t rank_handle;


static csis_remote_client_t * csis_get_remote_client_for_con_handle(hci_con_handle_t con_handle){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return NULL;
    }
    uint8_t i;
    for (i = 0; i < csis_clients_num; i++){
        if (csis_clients[i].con_handle == con_handle){
            return &csis_clients[i];
        }
    }
    return NULL;
}

static csis_remote_client_t * csis_server_add_remote_client(hci_con_handle_t con_handle){
    uint8_t i;
    
    for (i = 0; i < csis_clients_num; i++){
        if (csis_clients[i].con_handle == HCI_CON_HANDLE_INVALID){
            csis_clients[i].con_handle = con_handle;
            log_info("added client 0x%02x, index %d", con_handle, i);
            return &csis_clients[i];
        } 
    }
    return NULL;
}

static void csis_server_emit_remote_client_disconnected(hci_con_handle_t con_handle){
    btstack_assert(csis_event_callback != NULL);

    uint8_t event[5];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_REMOTE_CLIENT_DISCONNECTED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}


static void csis_server_emit_remote_client_connected(hci_con_handle_t con_handle, uint8_t status){
    btstack_assert(csis_event_callback != NULL);

    uint8_t event[6];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_REMOTE_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = status;
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static uint16_t coordinated_set_identification_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    csis_remote_client_t * client = csis_get_remote_client_for_con_handle(con_handle);
    
    if (client == NULL){
        client = ascs_server_add_remote_client(con_handle);
        
        if (client == NULL){
            ascs_server_emit_remote_client_connected(con_handle, ERROR_CODE_CONNECTION_LIMIT_EXCEEDED);
            log_info("There are already %d clients connected. No memory for new client.", ascs_clients_num);
            return 0;
        } else {
            ascs_server_emit_remote_client_connected(con_handle, ERROR_CODE_SUCCESS);
        }    
    }

    

    return 0;
}


static int coordinated_set_identification_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    // printf("coordinated_set_identification_service_write_callback con_handle 0x%02x, attr_handle 0x%02x \n", con_handle, attribute_handle);
    if (attribute_handle != 0 && attribute_handle != bass_audio_scan_control_point_handle){
    
        bass_server_source_t * source = bass_find_receive_state_for_client_configuration_handle(attribute_handle);
        if (source){
            source->bass_receive_state_client_configuration = little_endian_read_16(buffer, 0);
            bass_register_con_handle(con_handle, source->bass_receive_state_client_configuration);
        }
        return 0;
    }     
    
    return 0;
}

static void csis_client_reset(csis_remote_client_t * client){
    memset(client, 0, sizeof(csis_remote_client_t));
    client->con_handle = HCI_CON_HANDLE_INVALID;
}

static void coordinated_set_identification_service_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(packet);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }

    hci_con_handle_t con_handle;
    csis_remote_client_t * client;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            
            client = csis_find_client_for_con_handle(con_handle);
            if (client == NULL){
                break;
            }

            csis_client_reset(client);
            csis_server_emit_remote_client_disconnected(con_handle);
            break;
        default:
            break;
    }
}

void coordinated_set_identification_service_server_init(const uint8_t clients_num, csis_remote_client_t * clients){
    btstack_assert(clients_num != 0);
    btstack_assert(clients     != NULL);

     // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_COORDINATED_SET_IDENTIFICATION_SERVICE, &start_handle, &end_handle);
    btstack_assert(service_found != 0);
    UNUSED(service_found);

    // get characteristic value handle and client configuration handle
    sirk_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SET_IDENTITY_RESOLVING_KEY_CHARACTERISTIC);
    sirk_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SET_IDENTITY_RESOLVING_KEY_CHARACTERISTIC);

    size_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SIZE_CHARACTERISTIC);
    size_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SIZE_CHARACTERISTIC);
    
    lock_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_LOCK_CHARACTERISTIC);
    lock_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_LOCK_CHARACTERISTIC);

    rank_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_RANK_CHARACTERISTIC);
    
    log_info("Found SIRK service 0x%02x-0x%02x", start_handle, end_handle);

    csis_clients_num = clients_num;
    csis_clients = clients;
    memset(csis_clients, 0, sizeof(csis_remote_client_t) * csis_clients_num);

    // register service with ATT Server
    coordinated_set_identification_service.start_handle   = start_handle;
    coordinated_set_identification_service.end_handle     = end_handle;
    coordinated_set_identification_service.read_callback  = &coordinated_set_identification_service_read_callback;
    coordinated_set_identification_service.write_callback = &coordinated_set_identification_service_write_callback;
    coordinated_set_identification_service.packet_handler = coordinated_set_identification_service_packet_handler;
    att_server_register_service_handler(&coordinated_set_identification_service);
}

void coordinated_set_identification_service_server_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    csis_event_callback = callback;
}

void coordinated_set_identification_service_server_deinit(void){
}
