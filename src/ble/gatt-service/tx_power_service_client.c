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

#define BTSTACK_FILE__ "tx_power_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "ble/gatt-service/gatt_service_client_helper.h"
#include "ble/gatt-service/tx_power_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

// IAS Client
static gatt_service_client_helper_t txps_client;

static btstack_context_callback_registration_t txps_client_handle_can_send_now;

static void txps_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void txps_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void txps_client_run_for_connection(void * context);

// list of uuids
static const uint16_t txps_uuid16s[TX_POWER_SERVICE_CLIENT_NUM_CHARACTERISTICS] = {
    ORG_BLUETOOTH_CHARACTERISTIC_TX_POWER_LEVEL
};

typedef enum {
    TXPS_CLIENT_CHARACTERISTIC_INDEX_TX_POWER_LEVEL = 0,
    TXPS_CLIENT_CHARACTERISTIC_INDEX_RFU
} txps_client_characteristic_index_t;

#ifdef ENABLE_TESTING_SUPPORT
static char * txps_client_characteristic_name[] = {
    "TX_POWER_LEVEL",
    "RFU"
};
#endif

static void txps_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void txps_client_connected(txps_client_connection_t * connection, uint8_t status, uint8_t * packet, uint16_t size) {
    if (status == ERROR_CODE_SUCCESS){
        connection->state = TX_POWER_SERVICE_CLIENT_STATE_READY;
    } else {
        connection->state = TX_POWER_SERVICE_CLIENT_STATE_IDLE;
    }
    txps_client_replace_subevent_id_and_emit(connection->basic_connection.event_callback, packet, size,
                                             GATTSERVICE_SUBEVENT_TXPS_CLIENT_CONNECTED);
}

static void txps_client_emit_uint8(uint16_t cid, btstack_packet_handler_t event_callback, uint8_t subevent, const uint8_t * data, uint8_t data_size){
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

static uint16_t txps_client_value_handle_for_index(txps_client_connection_t * connection){
    return connection->basic_connection.characteristics[connection->characteristic_index].value_handle;
}


static void txps_client_emit_read_event(txps_client_connection_t * connection, uint8_t index, uint8_t status, const uint8_t * data, uint16_t data_size){
    UNUSED(status);

    if ((data_size > 0) && (data == NULL)){
        return;
    }

    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&txps_client, index);
    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_TX_POWER_LEVEL:
           txps_client_emit_uint8(connection->basic_connection.cid,  connection->basic_connection.event_callback, GATTSERVICE_SUBEVENT_TXPS_CLIENT_TX_POWER_LEVEL, data, (uint8_t)data_size);
           break;
        default:
            btstack_assert(false);
            break;
    }
}

static uint8_t txps_client_can_query_characteristic(txps_client_connection_t * connection, txps_client_characteristic_index_t characteristic_index){
    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    return connection->state == TX_POWER_SERVICE_CLIENT_STATE_READY ? ERROR_CODE_SUCCESS : ERROR_CODE_CONTROLLER_BUSY;
}

static uint8_t txps_client_request_send_gatt_query(txps_client_connection_t * connection, txps_client_characteristic_index_t characteristic_index){
    connection->characteristic_index = characteristic_index;

    txps_client_handle_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.con_handle;
    uint8_t status = gatt_client_request_to_send_gatt_query(&txps_client_handle_can_send_now, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = TX_POWER_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

static uint8_t txps_client_request_read_characteristic(uint16_t cid, txps_client_characteristic_index_t characteristic_index){
    txps_client_connection_t * connection = (txps_client_connection_t *) gatt_service_client_get_connection_for_cid(&txps_client, cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t status = txps_client_can_query_characteristic(connection, characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
   
    connection->state = TX_POWER_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    return txps_client_request_send_gatt_query(connection, characteristic_index);
}

static void txps_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
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
                    connection_helper = gatt_service_client_get_connection_for_cid(&txps_client, cid);
                    btstack_assert(connection_helper != NULL);

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = TXPS_CLIENT_CHARACTERISTIC_INDEX_TX_POWER_LEVEL;
                             i < TXPS_CLIENT_CHARACTERISTIC_INDEX_RFU; i++) {
                            printf("0x%04X %s\n", connection_helper->characteristics[i].value_handle,
                                   txps_client_characteristic_name[i]);
                        }
                    };
#endif
                    status = gattservice_subevent_client_connected_get_status(packet);
                    txps_client_connected((txps_client_connection_t *) connection_helper, status, packet, size);
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    // TODO reset client
                    cid = gattservice_subevent_client_disconnected_get_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&txps_client, cid);
                    btstack_assert(connection_helper != NULL);
                    txps_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size,
                                                            GATTSERVICE_SUBEVENT_TXPS_CLIENT_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }
}

static void txps_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    txps_client_connection_t * connection = NULL;
    hci_con_handle_t con_handle;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_characteristic_value_query_result_get_handle(packet);
            connection = (txps_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&txps_client, con_handle);
            btstack_assert(connection != NULL);                

            txps_client_emit_read_event(connection, connection->characteristic_index, ATT_ERROR_SUCCESS, 
                gatt_event_characteristic_value_query_result_get_value(packet), 
                gatt_event_characteristic_value_query_result_get_value_length(packet));
            
            connection->state = TX_POWER_SERVICE_CLIENT_STATE_READY;
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            con_handle = (hci_con_handle_t)gatt_event_query_complete_get_handle(packet);
            connection = (txps_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&txps_client, con_handle);
            btstack_assert(connection != NULL);

            connection->state = TX_POWER_SERVICE_CLIENT_STATE_READY;
            break;

        default:
            break;
    }
}

static void txps_client_run_for_connection(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t)context;
    txps_client_connection_t * connection = (txps_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&txps_client, con_handle);

    btstack_assert(connection != NULL);

    switch (connection->state){
        case TX_POWER_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = TX_POWER_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &txps_client_handle_gatt_client_event, con_handle, 
                txps_client_value_handle_for_index(connection));
            break;

        default:
            break;
    }
}

static void txps_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_hci_event_handler(&txps_client, packet_type, channel, packet, size);
}

void tx_power_service_client_init(void){
    gatt_service_client_init(&txps_client, &txps_client_packet_handler_trampoline);
    gatt_service_client_register_packet_handler(&txps_client, &txps_client_packet_handler_internal);

    txps_client.characteristics_desc16_num = sizeof(txps_uuid16s)/sizeof(uint16_t);
    txps_client.characteristics_desc16 = txps_uuid16s;

    txps_client_handle_can_send_now.callback = &txps_client_run_for_connection;
}

uint8_t tx_power_service_client_connect(hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    txps_client_connection_t * txps_connection,
    gatt_service_client_characteristic_t * txps_storage_for_characteristics, uint8_t txps_characteristics_num,
    uint16_t * txps_cid
){

    btstack_assert(packet_handler != NULL);
    btstack_assert(txps_connection != NULL);
    btstack_assert(txps_characteristics_num == TX_POWER_SERVICE_CLIENT_NUM_CHARACTERISTICS);

    txps_connection->state = TX_POWER_SERVICE_CLIENT_STATE_W4_CONNECTION;
    return gatt_service_client_connect(con_handle,
                                       &txps_client, &txps_connection->basic_connection,
                                       ORG_BLUETOOTH_SERVICE_TX_POWER, 0,
                                       txps_storage_for_characteristics, txps_characteristics_num, packet_handler, txps_cid);
}

uint8_t tx_power_service_client_read_tx_power_level(uint16_t txps_cid){
    return txps_client_request_read_characteristic(txps_cid, TXPS_CLIENT_CHARACTERISTIC_INDEX_TX_POWER_LEVEL);
}

void tx_power_service_client_deinit(void){
    gatt_service_client_deinit(&txps_client);
}
