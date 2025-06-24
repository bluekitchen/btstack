/*
 * Copyright (C) 2021 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "scan_parameters_service_client.c"

#include "btstack_config.h"

#include <stdint.h>
#include <string.h>

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#include "scan_parameters_service_client.h"

#include "btstack_memory.h"
#include "ble/core.h"
#include "ble/gatt_client.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"

#include "ble/gatt_service_client.h"

static gatt_service_client_t sps_client;
static btstack_linked_list_t sps_connections;

static btstack_context_callback_registration_t sps_client_handle_can_send_now;

static void sps_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void sps_client_run_for_connection(void * context);

static uint16_t scan_parameters_service_scan_window = 0;
static uint16_t scan_parameters_service_scan_interval = 0;

// list of uuids
static const uint16_t sps_uuid16s[SCAN_PARAMETERS_SERVICE_CLIENT_NUM_CHARACTERISTICS] = {
        ORG_BLUETOOTH_CHARACTERISTIC_SCAN_INTERVAL_WINDOW,
        ORG_BLUETOOTH_CHARACTERISTIC_SCAN_REFRESH
};

typedef enum {
    SPS_CLIENT_CHARACTERISTIC_INDEX_SCAN_INTERVAL_WINDOW = 0,
    SPS_CLIENT_CHARACTERISTIC_INDEX_SCAN_REFRESH,
    SPS_CLIENT_CHARACTERISTIC_INDEX_RFU
} sps_client_characteristic_index_t;

static void sps_client_add_connection(sps_client_connection_t * connection){
    btstack_linked_list_add(&sps_connections, (btstack_linked_item_t*) connection);
}

static void sps_client_finalize_connection(sps_client_connection_t * connection){
    btstack_linked_list_remove(&sps_connections, (btstack_linked_item_t*) connection);
}

static void sps_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void sps_client_connected(sps_client_connection_t * connection, uint8_t status, uint8_t * packet, uint16_t size) {
    if (status == ERROR_CODE_SUCCESS){
        connection->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_READY;
    } else {
        connection->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_IDLE;
    }
    sps_client_replace_subevent_id_and_emit(connection->packet_handler, packet, size,
                                            GATTSERVICE_SUBEVENT_SCAN_PARAMETERS_SERVICE_CONNECTED);
}

static uint8_t scan_parameters_client_request_send_gatt_query(sps_client_connection_t * connection){
    sps_client_handle_can_send_now.context = (void *) (uintptr_t)connection->basic_connection.cid;
    uint8_t status = gatt_client_request_to_send_gatt_query(&sps_client_handle_can_send_now, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_READY;
    }
    return status;
}

static sps_client_connection_t * sps_client_get_connection_for_cid(uint16_t scan_parameters_service_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &sps_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        sps_client_connection_t * connection = (sps_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->basic_connection.cid != scan_parameters_service_cid) continue;
        return connection;
    }
    return NULL;
}

static void sps_client_run_for_connection(void * context){
    uint16_t connection_id = (hci_con_handle_t)(uintptr_t)context;
    sps_client_connection_t * connection = sps_client_get_connection_for_cid(connection_id);

    btstack_assert(connection != NULL);

    uint8_t  value[4];

    switch (connection->state){
        case SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_WRITE_WITHOUT_RESPONSE:
            connection->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_READY;

            little_endian_store_16(value, 0, scan_parameters_service_scan_interval);
            little_endian_store_16(value, 2, scan_parameters_service_scan_window);

            gatt_client_write_value_of_characteristic_without_response(
                    gatt_service_client_get_con_handle(&connection->basic_connection),
                    gatt_service_client_characteristic_value_handle_for_index(&connection->basic_connection, SPS_CLIENT_CHARACTERISTIC_INDEX_SCAN_INTERVAL_WINDOW),
                    4, value);

            break;

        default:
            break;
    }
}

static void sps_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    sps_client_connection_t * connection;
    uint16_t cid;
    uint8_t status;
    uint16_t value_handle;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    cid = gattservice_subevent_client_connected_get_cid(packet);
                    connection = sps_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);

#ifdef ENABLE_TESTING_SUPPORT
                    gatt_service_client_dump_characteristic_value_handles(&connection->basic_connection,
                                                                          sps_characteristic_names);
#endif
                    status = gattservice_subevent_client_connected_get_status(packet);
                    sps_client_connected(connection, status, packet, size);
                    scan_parameters_service_client_set(scan_parameters_service_scan_interval, scan_parameters_service_scan_window);
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    cid = gattservice_subevent_client_disconnected_get_cid(packet);
                    connection = sps_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);
                    sps_client_finalize_connection(connection);
                    sps_client_replace_subevent_id_and_emit(connection->packet_handler, packet, size,
                                                            GATTSERVICE_SUBEVENT_SCAN_PARAMETERS_SERVICE_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;

        case GATT_EVENT_NOTIFICATION:
            cid = gatt_event_notification_get_connection_id(packet);
            connection = sps_client_get_connection_for_cid(cid);
            btstack_assert(connection != NULL);
            value_handle = gatt_event_notification_get_value_handle(packet);
            if (gatt_service_client_characteristic_index_for_value_handle(&connection->basic_connection, value_handle) != SPS_CLIENT_CHARACTERISTIC_INDEX_SCAN_REFRESH){
                return;
            }
            connection->state = true;
            scan_parameters_service_client_set(scan_parameters_service_scan_interval, scan_parameters_service_scan_window);
            break;
        default:
            break;
    }
}

void scan_parameters_service_client_set(uint16_t scan_interval, uint16_t scan_window){
    scan_parameters_service_scan_interval = scan_interval; 
    scan_parameters_service_scan_window = scan_window; 

    btstack_linked_list_iterator_t it;  
    btstack_linked_list_iterator_init(&it, &sps_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        sps_client_connection_t * connection = (sps_client_connection_t*) btstack_linked_list_iterator_next(&it);
        if (connection->state == SCAN_PARAMETERS_SERVICE_CLIENT_STATE_READY){
            connection->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_WRITE_WITHOUT_RESPONSE;
            scan_parameters_client_request_send_gatt_query(connection);
        }
    }
}

uint8_t scan_parameters_service_client_connect(
        hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler,
        sps_client_connection_t * sps_connection,
        uint16_t * sps_cid){
    
    btstack_assert(packet_handler != NULL);
    btstack_assert(sps_connection != NULL);

    *sps_cid = 0;

    sps_connection->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_CONNECTION;
    sps_connection->packet_handler = packet_handler;

    uint8_t status = gatt_service_client_connect_primary_service_with_uuid16(con_handle,
                                                                             &sps_client,
                                                                             &sps_connection->basic_connection,
                                                                             ORG_BLUETOOTH_SERVICE_SCAN_PARAMETERS,
                                                                             &sps_connection->characteristics_storage[0],
                                                                             SCAN_PARAMETERS_SERVICE_CLIENT_NUM_CHARACTERISTICS);
    if (status == ERROR_CODE_SUCCESS){
        sps_client_add_connection(sps_connection);
        *sps_cid = sps_connection->basic_connection.cid;
    }

    return status;
}

uint8_t scan_parameters_service_client_disconnect(uint16_t scan_parameters_service_cid){
    sps_client_connection_t * connection = sps_client_get_connection_for_cid(scan_parameters_service_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // finalize connections
    sps_client_finalize_connection(connection);
    return ERROR_CODE_SUCCESS;
}

void scan_parameters_service_client_init(void){
    gatt_service_client_register_client(&sps_client, &sps_client_packet_handler_internal, sps_uuid16s, sizeof(sps_uuid16s)/sizeof(uint16_t));
    sps_client_handle_can_send_now.callback = &sps_client_run_for_connection;
}

void scan_parameters_service_client_deinit(void){
}

