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

#include "ble/gatt-service/gatt_service_client_helper.h"
#include "ble/gatt-service/immediate_alert_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

// IAS Client
static gatt_service_client_helper_t ias_client;

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
    ias_client_replace_subevent_id_and_emit(connection->basic_connection.event_callback, packet, size,
                                            GATTSERVICE_SUBEVENT_LLS_CLIENT_CONNECTED);
}


static uint16_t ias_client_value_handle_for_index(ias_client_connection_t * connection){
    return connection->basic_connection.characteristics[connection->characteristic_index].value_handle;
}

static uint8_t ias_client_can_query_characteristic(ias_client_connection_t * connection, ias_client_characteristic_index_t characteristic_index){
    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    return connection->state == IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_READY ? ERROR_CODE_SUCCESS : ERROR_CODE_CONTROLLER_BUSY;
}

static uint8_t ias_client_request_send_gatt_query(ias_client_connection_t * connection, ias_client_characteristic_index_t characteristic_index){
    connection->characteristic_index = characteristic_index;

    ias_client_handle_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.con_handle;
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
    gatt_service_client_connection_helper_t * connection_helper;
    uint16_t cid;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    cid = gattservice_subevent_client_connected_get_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&ias_client, cid);
                    btstack_assert(connection_helper != NULL);

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = IAS_CLIENT_CHARACTERISTIC_INDEX_ALERT_LEVEL;
                             i < IAS_CLIENT_CHARACTERISTIC_INDEX_RFU; i++) {
                            printf("0x%04X %s\n", connection_helper->characteristics[i].value_handle,
                                   ias_client_characteristic_name[i]);
                        }
                    };
#endif
                    status = gattservice_subevent_client_connected_get_status(packet);
                    ias_client_connected((ias_client_connection_t *) connection_helper, status, packet, size);
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    // TODO reset client
                    cid = gattservice_subevent_client_disconnected_get_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&ias_client, cid);
                    btstack_assert(connection_helper != NULL);
                    ias_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size,
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
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&ias_client, connection->characteristic_index);
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
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t)context;
    ias_client_connection_t * connection = (ias_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&ias_client, con_handle);

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;

    switch (connection->state){
        case IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_W2_WRITE_WITHOUT_RESPONSE_CHARACTERISTIC_VALUE:
            connection->state = IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_READY;

            value_length = ias_client_serialize_characteristic_value_for_write(connection, &value);
            gatt_client_write_value_of_characteristic_without_response(
                     con_handle,ias_client_value_handle_for_index(connection),
                     value_length, value);
            
            break;

        default:
            break;
    }
}

static void ias_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_hci_event_handler(&ias_client, packet_type, channel, packet, size);
}

void immediate_alert_service_client_init(void){
    gatt_service_client_init(&ias_client, &ias_client_packet_handler_trampoline);
    gatt_service_client_register_packet_handler(&ias_client, &ias_client_packet_handler_internal);

    ias_client.characteristics_desc16_num = sizeof(ias_uuid16s)/sizeof(uint16_t);
    ias_client.characteristics_desc16 = ias_uuid16s;

    ias_client_handle_can_send_now.callback = &ias_client_run_for_connection;
}

uint8_t immediate_alert_service_client_connect(hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    ias_client_connection_t * ias_connection,
    gatt_service_client_characteristic_t * ias_storage_for_characteristics, uint8_t ias_characteristics_num,
    uint16_t * ias_cid
){

    btstack_assert(packet_handler != NULL);
    btstack_assert(ias_connection != NULL);
    btstack_assert(ias_characteristics_num == IMMEDIATE_ALERT_SERVICE_CLIENT_NUM_CHARACTERISTICS);

    ias_connection->state = IMMEDIATE_ALERT_SERVICE_CLIENT_STATE_W4_CONNECTION;
    return gatt_service_client_connect(con_handle,
                                       &ias_client, &ias_connection->basic_connection,
                                       ORG_BLUETOOTH_SERVICE_IMMEDIATE_ALERT, 0,
                                       ias_storage_for_characteristics, ias_characteristics_num, packet_handler, ias_cid);
}

uint8_t immediate_alert_service_client_write_alert_level(uint16_t ias_cid, ias_alert_level_t alert_level){
    ias_client_connection_t * connection = (ias_client_connection_t *) gatt_service_client_get_connection_for_cid(&ias_client, ias_cid);
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
    return gatt_service_client_disconnect(&ias_client, ias_cid);
}

void immediate_alert_service_client_deinit(void){
    gatt_service_client_deinit(&ias_client);
}

