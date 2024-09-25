/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "immediate_alert_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "ble/gatt_service_client.h"
#include "ble/gatt-service/immediate_alert_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

// IAS Client
static gatt_service_client_t ias_client;
static btstack_linked_list_t ias_connections;

static btstack_context_callback_registration_t ias_client_handle_can_send_now;

static void ias_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void ias_client_run_for_connection(void * context);

// list of uuids
static const uint16_t ias_uuid16s[IMMEDIATE_ALERT_SERVICE_CLIENT_NUM_CHARACTERISTICS] = {
    ORG_BLUETOOTH_CHARACTERISTIC_ALERT_LEVEL
};

typedef enum {
    IAS_CLIENT_CHARACTERISTIC_INDEX_ALERT_LEVEL = 0,
    IAS_CLIENT_CHARACTERISTIC_INDEX_RFU
} ias_client_characteristic_index_t;

#ifdef ENABLE_TESTING_SUPPORT
static char * ias_client_characteristic_name[] = {
    "ALERT_LEVEL",
    "RFU"
};
#endif

static ias_client_connection_t * ias_client_get_connection_for_cid(uint16_t connection_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it,  &ias_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        ias_client_connection_t * connection = (ias_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (gatt_service_client_get_connection_id(&connection->basic_connection) == connection_cid) {
            return connection;
        }
    }
    return NULL;
}

static void ias_client_add_connection(ias_client_connection_t * connection){
    btstack_linked_list_add(&ias_connections, (btstack_linked_item_t*) connection);
}

static void ias_client_finalize_connection(ias_client_connection_t * connection){
    btstack_linked_list_remove(&ias_connections, (btstack_linked_item_t*) connection);
}

static void ias_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void ias_client_connected(ias_client_connection_t * connection, uint8_t status, uint8_t * packet, uint16_t size) {
    if (status == ERROR_CODE_SUCCESS){
        connection->state = IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_READY;
    } else {
        connection->state = IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_IDLE;
    }
    ias_client_replace_subevent_id_and_emit(connection->packet_handler, packet, size,
                                            GATTSERVICE_SUBEVENT_IAS_CLIENT_CONNECTED);
}


static uint16_t ias_client_value_handle_for_index(ias_client_connection_t * connection){
    return connection->basic_connection.characteristics[connection->characteristic_index].value_handle;
}

static uint8_t ias_client_can_query_characteristic(ias_client_connection_t * connection, ias_client_characteristic_index_t characteristic_index){
    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection,
                                                                  (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    return connection->state == IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_READY ? ERROR_CODE_SUCCESS : ERROR_CODE_CONTROLLER_BUSY;
}

static uint8_t ias_client_request_send_gatt_query(ias_client_connection_t * connection, ias_client_characteristic_index_t characteristic_index){
    connection->characteristic_index = characteristic_index;

    ias_client_handle_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.cid;
    uint8_t status = gatt_client_request_to_send_gatt_query(&ias_client_handle_can_send_now, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

static uint8_t ias_client_request_write_without_response_characteristic(ias_client_connection_t * connection, ias_client_characteristic_index_t characteristic_index){
    connection->state = IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_W2_WRITE_WITHOUT_RESPONSE_CHARACTERISTIC_VALUE;
    return ias_client_request_send_gatt_query(connection, characteristic_index);
}

static void ias_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    ias_client_connection_t * connection;
    uint16_t cid;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    cid = gattservice_subevent_client_connected_get_cid(packet);
                    connection = ias_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = IAS_CLIENT_CHARACTERISTIC_INDEX_ALERT_LEVEL;
                             i < IAS_CLIENT_CHARACTERISTIC_INDEX_RFU; i++) {
                            printf("0x%04X %s\n", connection->basic_connection.characteristics[i].value_handle,
                                   ias_client_characteristic_name[i]);
                        }
                    };
#endif
                    status = gattservice_subevent_client_connected_get_status(packet);
                    ias_client_connected(connection, status, packet, size);
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    cid = gattservice_subevent_client_disconnected_get_cid(packet);
                    connection = ias_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);
                    ias_client_finalize_connection(connection);
                    ias_client_replace_subevent_id_and_emit(connection->packet_handler,
                                                            packet, size,
                                                            GATTSERVICE_SUBEVENT_IAS_CLIENT_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }
}

static uint16_t ias_client_serialize_characteristic_value_for_write(ias_client_connection_t * connection, uint8_t ** out_value){
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_index(&ias_client,
                                                                                         connection->characteristic_index);
    *out_value = (uint8_t *)connection->write_buffer;

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_ALERT_LEVEL:
            return 1;
        default:
            btstack_assert(false);
            break;
    }
    return 0;
}

static void ias_client_run_for_connection(void * context){
    uint16_t connection_id = (uint16_t)(uintptr_t)context;
    ias_client_connection_t * connection = ias_client_get_connection_for_cid(connection_id);

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;

    switch (connection->state){
        case IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_W2_WRITE_WITHOUT_RESPONSE_CHARACTERISTIC_VALUE:
            connection->state = IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_READY;

            value_length = ias_client_serialize_characteristic_value_for_write(connection, &value);
            gatt_client_write_value_of_characteristic_without_response(
                    gatt_service_client_get_con_handle(&connection->basic_connection),
                    ias_client_value_handle_for_index(connection),
                     value_length, value);
            
            break;

        default:
            break;
    }
}

void immediate_alert_service_client_init(void){
    gatt_service_client_register_client(&ias_client, &ias_client_packet_handler_internal, ias_uuid16s,  sizeof(ias_uuid16s)/sizeof(uint16_t));

    ias_client_handle_can_send_now.callback = &ias_client_run_for_connection;
}

uint8_t immediate_alert_service_client_connect(hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    ias_client_connection_t * ias_connection,
    uint16_t * ias_cid
){

    btstack_assert(packet_handler != NULL);
    btstack_assert(ias_connection != NULL);

    *ias_cid = 0;

    ias_connection->state = IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_W4_CONNECTION;
    ias_connection->packet_handler = packet_handler;

    uint8_t status = gatt_service_client_connect_primary_service_with_uuid16(con_handle,
                                                                             &ias_client,
                                                                             &ias_connection->basic_connection,
                                                                             ORG_BLUETOOTH_SERVICE_IMMEDIATE_ALERT, 0,
                                                                             ias_connection->characteristics_storage,
                                                                             IMMEDIATE_ALERT_SERVICE_CLIENT_NUM_CHARACTERISTICS);

    if (status == ERROR_CODE_SUCCESS){
        ias_client_add_connection(ias_connection);
        *ias_cid = ias_connection->basic_connection.cid;
    }

    return status;
}

uint8_t immediate_alert_service_client_write_alert_level(uint16_t ias_cid, ias_alert_level_t alert_level){
    ias_client_connection_t * connection = ias_client_get_connection_for_cid(ias_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (alert_level >= IAS_ALERT_LEVEL_RFU){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    ias_client_characteristic_index_t index = IAS_CLIENT_CHARACTERISTIC_INDEX_ALERT_LEVEL;

    uint8_t status = ias_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    connection->write_buffer[0] = (uint8_t)alert_level;
    return ias_client_request_write_without_response_characteristic(connection, index);
}

uint8_t immediate_alert_service_client_disconnect(uint16_t ias_cid){
    ias_client_connection_t * connection = ias_client_get_connection_for_cid(ias_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    return gatt_service_client_disconnect(&connection->basic_connection);
}

void immediate_alert_service_client_deinit(void){
    gatt_service_client_unregister_client(&ias_client);
}

