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

#define BTSTACK_FILE__ "link_loss_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "ble/gatt_service_client.h"
#include "ble/gatt-service/link_loss_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

// LLS Client
static gatt_service_client_t lls_client;
static btstack_linked_list_t lls_connections;

static btstack_context_callback_registration_t lls_client_handle_can_send_now;

static void lls_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void lls_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void lls_client_run_for_connection(void * context);

// list of uuids
static const uint16_t lls_uuid16s[LINK_LOSS_SERVICE_CLIENT_NUM_CHARACTERISTICS] = {
    ORG_BLUETOOTH_CHARACTERISTIC_ALERT_LEVEL
};

typedef enum {
    LLS_CLIENT_CHARACTERISTIC_INDEX_ALERT_LEVEL = 0,
    LLS_CLIENT_CHARACTERISTIC_INDEX_RFU
} lls_client_characteristic_index_t;

#ifdef ENABLE_TESTING_SUPPORT
static char * lls_client_characteristic_name[] = {
    "ALERT_LEVEL",
    "RFU"
};
#endif

static lls_client_connection_t * lls_client_get_connection_for_cid(uint16_t connection_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it,  &lls_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        lls_client_connection_t * connection = (lls_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (gatt_service_client_get_connection_id(&connection->basic_connection) == connection_cid) {
            return connection;
        }
    }
    return NULL;
}

static void lls_client_add_connection(lls_client_connection_t * connection){
    btstack_linked_list_add(&lls_connections, (btstack_linked_item_t*) connection);
}

static void lls_client_finalize_connection(lls_client_connection_t * connection){
    btstack_linked_list_remove(&lls_connections, (btstack_linked_item_t*) connection);
}

static void lls_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void lls_client_connected(lls_client_connection_t * connection, uint8_t status, uint8_t * packet, uint16_t size) {
    if (status == ERROR_CODE_SUCCESS){
        connection->state = LINK_LOSS_SERVICE_CLIENT_STATE_READY;
    } else {
        connection->state = LINK_LOSS_SERVICE_CLIENT_STATE_IDLE;
    }
    lls_client_replace_subevent_id_and_emit(connection->packet_handler, packet, size,
                                            GATTSERVICE_SUBEVENT_LLS_CLIENT_CONNECTED);
}

static void lls_client_emit_uint8(uint16_t cid, btstack_packet_handler_t event_callback, uint8_t subevent, const uint8_t * data, uint16_t data_size){
    UNUSED(data_size);
    btstack_assert(event_callback != NULL);

    if (data_size != 1){
        return;
    }

    uint8_t event[6];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = 4;
    event[pos++] = subevent;
    little_endian_store_16(event, pos, cid);
    pos+= 2;
    event[pos++] = data[0];

    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}


static uint16_t lls_client_value_handle_for_index(lls_client_connection_t * connection){
    return connection->basic_connection.characteristics[connection->characteristic_index].value_handle;
}

static void lls_client_emit_done_event(lls_client_connection_t * connection, uint8_t index, uint8_t status){
    btstack_assert(connection->packet_handler != NULL);

    uint16_t cid = connection->basic_connection.cid;
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_index(&lls_client, index);

    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_LLS_CLIENT_WRITE_DONE;

    little_endian_store_16(event, pos, cid);
    pos+= 2;
    little_endian_store_16(event, pos, characteristic_uuid16);
    pos+= 2;
    event[pos++] = status;
    (*connection->packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}


static void lls_client_emit_read_event(lls_client_connection_t * connection, uint8_t index, uint8_t status, const uint8_t * data, uint16_t data_size){
    UNUSED(status);

    if ((data_size > 0) && (data == NULL)){
        return;
    }

    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_index(&lls_client, index);
    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_ALERT_LEVEL:
            lls_client_emit_uint8(connection->basic_connection.cid, connection->packet_handler, GATTSERVICE_SUBEVENT_LLS_CLIENT_ALERT_LEVEL, data, data_size);
            break;
        default:
            btstack_assert(false);
            break;
    }
}

static uint8_t lls_client_can_query_characteristic(lls_client_connection_t * connection, lls_client_characteristic_index_t characteristic_index){
    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection,
                                                                  (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    return connection->state == LINK_LOSS_SERVICE_CLIENT_STATE_READY ? ERROR_CODE_SUCCESS : ERROR_CODE_CONTROLLER_BUSY;
}

static uint8_t lls_client_request_send_gatt_query(lls_client_connection_t * connection, lls_client_characteristic_index_t characteristic_index){
    connection->characteristic_index = characteristic_index;

    lls_client_handle_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.cid;
    uint8_t status = gatt_client_request_to_send_gatt_query(&lls_client_handle_can_send_now, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = LINK_LOSS_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

static uint8_t lls_client_request_read_characteristic(uint16_t lls_cid, lls_client_characteristic_index_t characteristic_index){
    lls_client_connection_t * connection = lls_client_get_connection_for_cid(lls_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t status = lls_client_can_query_characteristic(connection, characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
   
    connection->state = LINK_LOSS_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    return lls_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t lls_client_request_write_characteristic(lls_client_connection_t * connection, lls_client_characteristic_index_t characteristic_index){
    connection->state = LINK_LOSS_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    return lls_client_request_send_gatt_query(connection, characteristic_index);
}

static void lls_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    lls_client_connection_t * connection;
    uint16_t cid;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    cid = gattservice_subevent_client_connected_get_cid(packet);
                    connection = lls_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = LLS_CLIENT_CHARACTERISTIC_INDEX_ALERT_LEVEL;
                             i < LLS_CLIENT_CHARACTERISTIC_INDEX_RFU; i++) {
                            printf("0x%04X %s\n", connection->basic_connection.characteristics[i].value_handle,
                                   lls_client_characteristic_name[i]);
                        }
                    };
#endif
                    status = gattservice_subevent_client_connected_get_status(packet);
                    lls_client_connected(connection, status, packet, size);
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    cid = gattservice_subevent_client_disconnected_get_cid(packet);
                    connection = lls_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);
                    lls_client_finalize_connection(connection);
                    lls_client_replace_subevent_id_and_emit(connection->packet_handler, packet, size,
                                                            GATTSERVICE_SUBEVENT_LLS_CLIENT_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }
}

static void lls_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    lls_client_connection_t * connection = NULL;
    uint16_t connection_id;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            connection_id = gatt_event_characteristic_value_query_result_get_connection_id(packet);
            connection = lls_client_get_connection_for_cid(connection_id);
            btstack_assert(connection != NULL);

            lls_client_emit_read_event(connection, connection->characteristic_index, ATT_ERROR_SUCCESS, 
                gatt_event_characteristic_value_query_result_get_value(packet), 
                gatt_event_characteristic_value_query_result_get_value_length(packet));
            
            connection->state = LINK_LOSS_SERVICE_CLIENT_STATE_READY;
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            connection_id = gatt_event_query_complete_get_connection_id(packet);
            connection = lls_client_get_connection_for_cid(connection_id);
            btstack_assert(connection != NULL);

            connection->state = LINK_LOSS_SERVICE_CLIENT_STATE_READY;
            lls_client_emit_done_event(connection, connection->characteristic_index, gatt_event_query_complete_get_att_status(packet));
            break;

        default:
            break;
    }
}

static uint16_t lls_client_serialize_characteristic_value_for_write(lls_client_connection_t * connection, uint8_t ** out_value){
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_index(&lls_client,
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

static void lls_client_run_for_connection(void * context){
    uint16_t connection_id = (uint16_t)(uintptr_t)context;
    lls_client_connection_t * connection = lls_client_get_connection_for_cid(connection_id);

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;

    switch (connection->state){
        case LINK_LOSS_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = LINK_LOSS_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle_with_context(
                &lls_client_handle_gatt_client_event, connection->basic_connection.cid,
                lls_client_value_handle_for_index(connection), lls_client.service_id, connection->basic_connection.cid);
            break;

        case LINK_LOSS_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = LINK_LOSS_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;

            value_length = lls_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_value_of_characteristic_with_context(
                    &lls_client_handle_gatt_client_event, connection->basic_connection.cid,
                lls_client_value_handle_for_index(connection),
                value_length, value, lls_client.service_id, connection->basic_connection.cid);
            
            break;

        default:
            break;
    }
}

void link_loss_service_client_init(void){
    gatt_service_client_register_client(&lls_client, &lls_client_packet_handler_internal, lls_uuid16s, sizeof(lls_uuid16s)/sizeof(uint16_t));

    lls_client_handle_can_send_now.callback = &lls_client_run_for_connection;
}

uint8_t link_loss_service_client_connect(hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    lls_client_connection_t * lls_connection,
    gatt_service_client_characteristic_t * lls_storage_for_characteristics, uint8_t lls_characteristics_num,
    uint16_t * lls_cid
){

    btstack_assert(packet_handler != NULL);
    btstack_assert(lls_connection != NULL);
    btstack_assert(lls_characteristics_num == LINK_LOSS_SERVICE_CLIENT_NUM_CHARACTERISTICS);

    *lls_cid = 0;

    lls_connection->state = LINK_LOSS_SERVICE_CLIENT_STATE_W4_CONNECTION;
    lls_connection->packet_handler = packet_handler;

    uint8_t status = gatt_service_client_connect_primary_service_with_uuid16(con_handle,
                                                                             &lls_client,
                                                                             &lls_connection->basic_connection,
                                                                             ORG_BLUETOOTH_SERVICE_LINK_LOSS, 0,
                                                                             lls_storage_for_characteristics,
                                                                             lls_characteristics_num);

    if (status == ERROR_CODE_SUCCESS){
        lls_client_add_connection(lls_connection);
        *lls_cid = lls_connection->basic_connection.cid;
    }

    return status;
}

uint8_t link_loss_service_client_read_alert_level(uint16_t lls_cid){
    return lls_client_request_read_characteristic(lls_cid, LLS_CLIENT_CHARACTERISTIC_INDEX_ALERT_LEVEL);
}

uint8_t link_loss_service_client_write_alert_level(uint16_t lls_cid, lls_alert_level_t alert_level){
    lls_client_connection_t * connection = lls_client_get_connection_for_cid(lls_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (alert_level >= LLS_ALERT_LEVEL_RFU){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    lls_client_characteristic_index_t index = LLS_CLIENT_CHARACTERISTIC_INDEX_ALERT_LEVEL;

    uint8_t status = lls_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    connection->write_buffer[0] = (uint8_t)alert_level;
    return lls_client_request_write_characteristic(connection, index);
}

uint8_t link_loss_service_client_disconnect(uint16_t lls_cid){
    lls_client_connection_t * connection = lls_client_get_connection_for_cid(lls_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    return gatt_service_client_disconnect(&connection->basic_connection);
}

void link_loss_service_client_deinit(void){
    gatt_service_client_unregister_client(&lls_client);
}

