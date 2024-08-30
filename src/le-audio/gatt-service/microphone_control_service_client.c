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

#define BTSTACK_FILE__ "microphone_control_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "ble/gatt_service_client.h"
#include "le-audio/gatt-service/microphone_control_service_client.h"
#include "le-audio/gatt-service/audio_input_control_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

// MSC Client
static gatt_service_client_t mics_client;
static btstack_linked_list_t mics_connections;

static void mics_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void mics_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void mics_client_run_for_connection(void * context);

// list of uuids
static const uint16_t aics_uuid16s[MICROPHONE_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS] = {
    ORG_BLUETOOTH_CHARACTERISTIC_MUTE
};

typedef enum {
    MICS_CLIENT_CHARACTERISTIC_INDEX_MUTE = 0,
    MICS_CLIENT_CHARACTERISTIC_INDEX_RFU
} mics_client_characteristic_index_t;

#ifdef ENABLE_TESTING_SUPPORT
static char * mics_client_characteristic_name[] = {
    "MUTE",
    "RFU"
};
#endif


static mics_client_connection_t * mics_client_get_connection_for_cid(uint16_t connection_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it,  &mics_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        mics_client_connection_t * connection = (mics_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (gatt_service_client_get_connection_id(&connection->basic_connection) == connection_cid) {
            return connection;
        }
    }
    return NULL;
}

static void mics_client_add_connection(mics_client_connection_t * connection){
    btstack_linked_list_add(&mics_connections, (btstack_linked_item_t*) connection);
}

static void mics_client_finalize_connection(mics_client_connection_t * connection){
    btstack_linked_list_remove(&mics_connections, (btstack_linked_item_t*) connection);
}


static void mics_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void mics_client_emit_connection_established(const gatt_service_client_connection_t *connection_helper, uint8_t num_included_clients, uint8_t status) {
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint8_t event[9];
    int pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_MICS_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, connection_helper->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos += 2;
    event[pos++] = num_included_clients; // num included services
    event[pos++] = status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void mics_client_connected(mics_client_connection_t *connection, uint8_t status) {
    if (status == ERROR_CODE_SUCCESS){
        connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_READY;
        mics_client_emit_connection_established(&connection->basic_connection, connection->aics_connections_num, status);
    } else {
        connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_IDLE;
        mics_client_emit_connection_established(&connection->basic_connection, 0, status);
        mics_client_finalize_connection(connection);
    }
}

static void mics_client_emit_number(uint16_t cid, btstack_packet_handler_t event_callback, uint8_t subevent, const uint8_t * data, uint8_t data_size, uint8_t expected_data_size, uint8_t status){
    UNUSED(data_size);
    btstack_assert(event_callback != NULL);
    
    if (data_size != expected_data_size){
        return;
    }
    
    uint8_t event[10];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = 3 + data_size;
    event[pos++] = subevent;
    little_endian_store_16(event, pos, cid);
    pos+= 2;
    reverse_bytes(data, &event[pos], data_size);
    pos += data_size;
    event[pos++] = status;
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void mics_client_emit_done_event(mics_client_connection_t * connection, uint8_t index, uint8_t status){
    btstack_packet_handler_t event_callback = connection->basic_connection.event_callback;
    btstack_assert(event_callback != NULL);

    uint16_t cid = connection->basic_connection.cid;
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_index(&mics_client, index);

    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_MICS_CLIENT_WRITE_DONE;

    little_endian_store_16(event, pos, cid);
    pos+= 2;
    little_endian_store_16(event, pos, characteristic_uuid16);
    pos+= 2;
    event[pos++] = status;
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void mics_client_emit_read_event(mics_client_connection_t * connection, uint8_t index, uint8_t status, const uint8_t * data, uint16_t data_size){
    UNUSED(status);

    if ((data_size > 0) && (data == NULL)){
        return;
    }

    btstack_packet_handler_t event_callback = connection->basic_connection.event_callback; 
    btstack_assert(event_callback != NULL);

    uint16_t cid = connection->basic_connection.cid;
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_index(&mics_client, index);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_MUTE:
           mics_client_emit_number(cid, event_callback, LEAUDIO_SUBEVENT_MICS_CLIENT_MUTE, data, data_size, 1, ATT_ERROR_SUCCESS);
           break;
        default:
            btstack_assert(false);
            break;
    }
}

static void mics_client_emit_notify_event(mics_client_connection_t * connection, uint16_t value_handle, uint8_t status, const uint8_t * data, uint16_t data_size){
    UNUSED(status);

    if ((data_size > 0) && (data == NULL)){
        return;
    }

    btstack_packet_handler_t event_callback = connection->basic_connection.event_callback;
    btstack_assert(event_callback != NULL);

    uint16_t cid = connection->basic_connection.cid;
    uint16_t characteristic_index = gatt_service_client_characteristic_index_for_value_handle(&connection->basic_connection, value_handle);
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_index(&mics_client, characteristic_index);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_MUTE:
            mics_client_emit_number(cid, event_callback, LEAUDIO_SUBEVENT_MICS_CLIENT_MUTE, data, data_size, 1,  ATT_ERROR_SUCCESS);
           break;
        default:
            btstack_assert(false);
            break;
    }
}


static uint8_t mics_client_can_query_characteristic(mics_client_connection_t * connection, mics_client_characteristic_index_t characteristic_index){
    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    return connection->state == MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_READY ? ERROR_CODE_SUCCESS : ERROR_CODE_CONTROLLER_BUSY;
}

static uint8_t mics_client_request_send_gatt_query(mics_client_connection_t * connection, mics_client_characteristic_index_t characteristic_index){
    connection->characteristic_index = characteristic_index;

    connection->gatt_query_can_send_now_request.context = (void *)(uintptr_t)connection->basic_connection.cid;
    uint8_t status = gatt_client_request_to_send_gatt_query(&connection->gatt_query_can_send_now_request, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

static uint8_t mics_client_request_read_characteristic(uint16_t aics_cid, mics_client_characteristic_index_t characteristic_index){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(aics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t status = mics_client_can_query_characteristic(connection, characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
   
    connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    return mics_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t mics_client_request_write_characteristic(mics_client_connection_t * connection, mics_client_characteristic_index_t characteristic_index){
    connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    return mics_client_request_send_gatt_query(connection, characteristic_index);
}

static void mics_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    mics_client_connection_t * connection;
    uint16_t connection_id;
    uint16_t cid;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    cid = gattservice_subevent_client_connected_get_cid(packet);
                    connection = mics_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);
                    status = gattservice_subevent_client_connected_get_status(packet);

                    if (status != ERROR_CODE_SUCCESS){
                        mics_client_connected(connection, status);
                        break;
                    }

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = MICS_CLIENT_CHARACTERISTIC_INDEX_MUTE; i < MICS_CLIENT_CHARACTERISTIC_INDEX_RFU; i++){
                            printf("0x%04X %s\n", connection_helper->characteristics[i].value_handle, mics_client_characteristic_name[i]);
                        }
                    };
                    printf("\nMICS Client: Query AICS included services\n");
#endif
                    // only look for included services if we can use them
                    connection->aics_connections_index = 0;
                    connection->aics_connections_connected = 0;
                    
                    if ((connection->aics_connections_max_num > 0) && (connection->aics_connections_storage != NULL)){
                        connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_INCLUDED_SERVICES;

                        connection->gatt_query_can_send_now_request.context = (void *) (uintptr_t) connection->basic_connection.cid;
                        (void) gatt_client_request_to_send_gatt_query(&connection->gatt_query_can_send_now_request,
                                                                      connection->basic_connection.con_handle);
                    } else {
                        // cannot connect to included AICS services, MICS connected
                        mics_client_connected(connection, ERROR_CODE_SUCCESS);
                    }
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    cid = gattservice_subevent_client_disconnected_get_cid(packet);
                    connection = mics_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);
                    mics_client_finalize_connection(connection);
                    mics_client_replace_subevent_id_and_emit(gatt_service_client_get_packet_handler(&connection->basic_connection), packet, size, LEAUDIO_SUBEVENT_MICS_CLIENT_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;

        case HCI_EVENT_LEAUDIO_META:
            switch (hci_event_leaudio_meta_get_subevent_code(packet)){
                
                case LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_INPUT_STATE:
                    cid = leaudio_subevent_aics_client_audio_input_state_get_aics_cid(packet);
                    connection = mics_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);
                    mics_client_replace_subevent_id_and_emit(gatt_service_client_get_packet_handler(&connection->basic_connection), packet, size, LEAUDIO_SUBEVENT_MICS_CLIENT_AUDIO_INPUT_STATE);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_GAIN_SETTINGS_PROPERTIES:
                    cid = leaudio_subevent_aics_client_gain_settings_properties_get_aics_cid(packet);
                    connection = mics_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);
                    mics_client_replace_subevent_id_and_emit(gatt_service_client_get_packet_handler(&connection->basic_connection), packet, size, LEAUDIO_SUBEVENT_MICS_CLIENT_GAIN_SETTINGS_PROPERTIES);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_INPUT_TYPE:
                    cid = leaudio_subevent_aics_client_audio_input_type_get_aics_cid(packet);
                    connection = mics_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);
                    mics_client_replace_subevent_id_and_emit(gatt_service_client_get_packet_handler(&connection->basic_connection), packet, size, LEAUDIO_SUBEVENT_MICS_CLIENT_AUDIO_INPUT_TYPE);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_INPUT_STATUS:
                    cid = leaudio_subevent_aics_client_audio_input_status_get_aics_cid(packet);
                    connection = mics_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);
                    mics_client_replace_subevent_id_and_emit(gatt_service_client_get_packet_handler(&connection->basic_connection), packet, size, LEAUDIO_SUBEVENT_MICS_CLIENT_AUDIO_INPUT_STATUS);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_DESCRIPTION:
                    cid = leaudio_subevent_aics_client_audio_description_get_aics_cid(packet);
                    connection = mics_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);
                    mics_client_replace_subevent_id_and_emit(gatt_service_client_get_packet_handler(&connection->basic_connection), packet, size, LEAUDIO_SUBEVENT_MICS_CLIENT_AUDIO_DESCRIPTION);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_WRITE_DONE:
                    cid = leaudio_subevent_aics_client_write_done_get_aics_cid(packet);
                    connection = mics_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);
                    mics_client_replace_subevent_id_and_emit(gatt_service_client_get_packet_handler(&connection->basic_connection), packet, size, LEAUDIO_SUBEVENT_MICS_CLIENT_WRITE_DONE);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_CONNECTED:
                    cid = leaudio_subevent_aics_client_connected_get_aics_cid(packet);
                    connection = mics_client_get_connection_for_cid(cid);
                    btstack_assert(connection != NULL);

                    if (leaudio_subevent_aics_client_connected_get_att_status(packet) == ERROR_CODE_SUCCESS) {
                        connection->aics_connections_connected++;
                    } else {
                        log_info("MICS: AICS client connection failed, err 0x%02x, con_handle 0x%04x, cid 0x%04x",
                                 leaudio_subevent_aics_client_connected_get_att_status(packet),
                                 leaudio_subevent_aics_client_connected_get_con_handle(packet),
                                 leaudio_subevent_aics_client_connected_get_aics_cid(packet));
                    }

                    // connect to next one
                    if (connection->aics_connections_index < (connection->aics_connections_num-1)) {
                        connection->aics_connections_index++;
                        (void) audio_input_control_service_client_connect(
                                connection->basic_connection.con_handle,
                                &mics_client_packet_handler_internal,
                                connection->aics_connections_storage[connection->aics_connections_index].basic_connection.start_handle,
                                connection->aics_connections_storage[connection->aics_connections_index].basic_connection.end_handle,
                                connection->aics_connections_storage[connection->aics_connections_index].basic_connection.service_index,
                                &connection->aics_connections_storage[connection->aics_connections_index]
                        );
                    } else {
                        // no AICS service left to connect to
                        mics_client_connected(connection, ERROR_CODE_SUCCESS);
                    }
                    break;

                default:
                    break;
            }
            break;

        case GATT_EVENT_NOTIFICATION:
            connection_id = gatt_event_notification_get_connection_id(packet);
            connection = mics_client_get_connection_for_cid(connection_id);
            btstack_assert(connection != NULL);

            mics_client_emit_notify_event(connection, gatt_event_notification_get_value_handle(packet), ATT_ERROR_SUCCESS,
                                         gatt_event_notification_get_value(packet),
                                         gatt_event_notification_get_value_length(packet));
            break;
        default:
            break;
    }
}


static void mics_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    mics_client_connection_t * connection = NULL;
    uint16_t connection_id;
    gatt_client_service_t service;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            connection_id = gatt_event_characteristic_value_query_result_get_connection_id(packet);
            connection = mics_client_get_connection_for_cid(connection_id);
            btstack_assert(connection != NULL);                

            mics_client_emit_read_event(connection, connection->characteristic_index, ATT_ERROR_SUCCESS, 
                gatt_event_characteristic_value_query_result_get_value(packet), 
                gatt_event_characteristic_value_query_result_get_value_length(packet));
            
            connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_READY;
            break;

        case GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT:
            connection_id = gatt_event_included_service_query_result_get_connection_id(packet);
            connection = mics_client_get_connection_for_cid(connection_id);
            btstack_assert(connection != NULL);

            gatt_event_included_service_query_result_get_service(packet, &service);
            switch (service.uuid16) {
                case ORG_BLUETOOTH_SERVICE_AUDIO_INPUT_CONTROL:
                    if (connection->aics_connections_num < connection->aics_connections_max_num){
                        gatt_event_included_service_query_result_get_service(packet, &service);
                        aics_client_connection_t * aics_connection = &connection->aics_connections_storage[connection->aics_connections_num];

                        aics_connection->basic_connection.service_index = connection->aics_connections_num;
                        aics_connection->basic_connection.service_uuid16 = service.uuid16;
                        aics_connection->basic_connection.start_handle = service.start_group_handle;
                        aics_connection->basic_connection.end_handle = service.end_group_handle;
                        connection->aics_connections_num++;
                    } else {
                        log_info("Num included AICS services exceeded storage capacity, max num %d", connection->aics_connections_max_num);
                    }
                    break;
                default:
                    break;
            }

            break;

        case GATT_EVENT_QUERY_COMPLETE:
            connection_id = gatt_event_query_complete_get_connection_id(packet);
            connection = mics_client_get_connection_for_cid(connection_id);
            btstack_assert(connection != NULL);

            switch (connection->state){
                case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICES_RESULT:
                    if (connection->aics_connections_num == 0) {
                        // no included AICS services, MICS connected
                        mics_client_connected(connection, ERROR_CODE_SUCCESS);
                        break;
                    }
                    connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICE_CONNECTED;
                    connection->aics_connections_index = 0;
                    connection->aics_connections_connected = 0;

                    (void) audio_input_control_service_client_connect(
                            connection->basic_connection.con_handle,
                            &mics_client_packet_handler_internal,
                            connection->aics_connections_storage[connection->aics_connections_index].basic_connection.start_handle,
                            connection->aics_connections_storage[connection->aics_connections_index].basic_connection.end_handle,
                            connection->aics_connections_storage[connection->aics_connections_index].basic_connection.service_index,
                            &connection->aics_connections_storage[connection->aics_connections_index]
                    );
                    break;

                default:
                    break;
            }

            connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_READY;
            mics_client_emit_done_event(connection, connection->characteristic_index, gatt_event_query_complete_get_att_status(packet));
            break;

        default:
            break;
    }
}

static uint16_t mics_client_serialize_characteristic_value_for_write(mics_client_connection_t * connection, uint8_t ** out_value){
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_index(&mics_client,
                                                                                         connection->characteristic_index);
    *out_value = connection->write_buffer;

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_MUTE:
            connection->write_buffer[0] = connection->data.data_8;
            return 1;
        default:
            btstack_assert(false);
            break;
    }
    return 0;
}

static void mics_client_run_for_connection(void * context){
    uint16_t connection_id = (uint16_t)(uintptr_t)context;
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(connection_id);

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;
    gatt_client_service_t service;

    switch (connection->state){
        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_INCLUDED_SERVICES:
            connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICES_RESULT;
            service.start_group_handle = connection->basic_connection.start_handle;
            service.end_group_handle = connection->basic_connection.end_handle;

            (void) gatt_client_find_included_services_for_service_with_context(
                    mics_client_handle_gatt_client_event,
                    connection->basic_connection.con_handle,
                    &service, mics_client.service_id, connection->basic_connection.cid);
            break;

        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle_with_context(
                &mics_client_handle_gatt_client_event, connection->basic_connection.con_handle,
                gatt_service_client_characteristic_value_handle_for_index(&connection->basic_connection, connection->characteristic_index),
                          mics_client.service_id, connection->basic_connection.cid);
            break;

        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;

            value_length = mics_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_value_of_characteristic_with_context(
                    &mics_client_handle_gatt_client_event, connection->basic_connection.con_handle,
                    gatt_service_client_characteristic_value_handle_for_index(&connection->basic_connection, connection->characteristic_index),
                value_length, value, mics_client.service_id, connection->basic_connection.cid);
            
            break;

        default:
            break;
    }
}

void microphone_control_service_client_init(void){
    gatt_service_client_register_client(&mics_client, &mics_client_packet_handler_internal);

    mics_client.characteristics_desc16_num = sizeof(aics_uuid16s)/sizeof(uint16_t);
    mics_client.characteristics_desc16 = aics_uuid16s;
}

uint8_t microphone_control_service_client_connect(hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    mics_client_connection_t * mics_connection,
    gatt_service_client_characteristic_t * mics_storage_for_characteristics, uint8_t mics_characteristics_num,
    aics_client_connection_t * aics_connections, uint8_t aics_connections_num,
    gatt_service_client_characteristic_t * aics_storage_for_characteristics, uint8_t aics_characteristics_num, // for all aics connections
    uint16_t * mics_cid
){

    btstack_assert(packet_handler != NULL);
    btstack_assert(mics_connection != NULL);
    btstack_assert(mics_characteristics_num == MICROPHONE_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS);

    mics_connection->aics_connections_max_num = 0;
    mics_connection->aics_characteristics_max_num = 0;
    mics_connection->aics_connections_storage = NULL;
    mics_connection->aics_characteristics_storage = NULL;
    mics_connection->gatt_query_can_send_now_request.callback = &mics_client_run_for_connection;

    if (aics_connections_num > 0){
        btstack_assert(aics_storage_for_characteristics != NULL);
        btstack_assert(aics_connections != NULL);
        btstack_assert(aics_characteristics_num == aics_connections_num * AUDIO_INPUT_CONTROL_SERVICE_NUM_CHARACTERISTICS);

        mics_connection->aics_connections_storage = aics_connections;
        mics_connection->aics_connections_max_num = aics_connections_num;
        mics_connection->aics_characteristics_storage = aics_storage_for_characteristics;
        mics_connection->aics_characteristics_max_num = aics_characteristics_num;
    }
    mics_connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_CONNECTION;
    mics_connection->aics_events_packet_handler = packet_handler;
    uint8_t status = gatt_service_client_connect_primary_service_with_uuid16(con_handle,
                                                                             &mics_client,
                                                                             &mics_connection->basic_connection,
                                                                             ORG_BLUETOOTH_SERVICE_MICROPHONE_CONTROL,
                                                                             0,
                                                                             mics_storage_for_characteristics,
                                                                             mics_characteristics_num,
                                                                             packet_handler);
    if (status == ERROR_CODE_SUCCESS){
        mics_client_add_connection(mics_connection);
    }

    return status;
}

/**
 * @brief Read mute state. The mute state is received via LEAUDIO_SUBEVENT_MICS_CLIENT_MUTE event.
 * @param mics_cid
 * @return status
 */
uint8_t microphone_control_service_client_read_mute_state(uint16_t mics_cid){
    return mics_client_request_read_characteristic(mics_cid, MICS_CLIENT_CHARACTERISTIC_INDEX_MUTE);
}

uint8_t microphone_control_service_client_mute(uint16_t mics_cid){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    mics_client_characteristic_index_t index = MICS_CLIENT_CHARACTERISTIC_INDEX_MUTE;

    uint8_t status = mics_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->data.data_8 = 1;
    return mics_client_request_write_characteristic(connection, index);
}

uint8_t microphone_control_service_client_unmute(uint16_t mics_cid){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    mics_client_characteristic_index_t index = MICS_CLIENT_CHARACTERISTIC_INDEX_MUTE;

    uint8_t status = mics_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->data.data_8 = 0;
    return mics_client_request_write_characteristic(connection, index);
}

uint8_t microphone_control_service_client_write_unmute(uint16_t mics_cid, uint8_t aics_index){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_unmute(&connection->aics_connections_storage[aics_index]);
}

uint8_t microphone_control_service_client_write_mute(uint16_t mics_cid, uint8_t aics_index){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_mute(&connection->aics_connections_storage[aics_index]);
}

uint8_t microphone_control_service_client_write_gain_setting(uint16_t mics_cid, uint8_t aics_index, int8_t gain_setting){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_gain_setting(&connection->aics_connections_storage[aics_index],
                                                                 gain_setting);
}

uint8_t microphone_control_service_client_write_manual_gain_mode(uint16_t mics_cid, uint8_t aics_index){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_manual_gain_mode(&connection->aics_connections_storage[aics_index]);
}

uint8_t microphone_control_service_client_write_automatic_gain_mode(uint16_t mics_cid, uint8_t aics_index){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_automatic_gain_mode(&connection->aics_connections_storage[aics_index]);
}

uint8_t microphone_control_service_client_write_input_description(uint16_t mics_cid, uint8_t aics_index, const char * audio_input_description){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_input_description(&connection->aics_connections_storage[aics_index], audio_input_description);
}

uint8_t microphone_control_service_client_read_input_description(uint16_t mics_cid, uint8_t aics_index){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_read_input_description(&connection->aics_connections_storage[aics_index]);
}

uint8_t microphone_control_service_client_read_input_state(uint16_t mics_cid, uint8_t aics_index){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_read_input_state(&connection->aics_connections_storage[aics_index]);
}

uint8_t microphone_control_service_client_read_gain_setting_properties(uint16_t mics_cid, uint8_t aics_index){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_read_gain_setting_properties(&connection->aics_connections_storage[aics_index]);
}

uint8_t microphone_control_service_client_read_input_type(uint16_t mics_cid, uint8_t aics_index){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_read_input_type(&connection->aics_connections_storage[aics_index]);
}

uint8_t microphone_control_service_client_read_input_status(uint16_t mics_cid, uint8_t aics_index){
    mics_client_connection_t * connection = mics_client_get_connection_for_cid(mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_read_input_status(&connection->aics_connections_storage[aics_index]);
}


uint8_t microphone_control_service_client_disconnect(uint16_t aics_cid){
    return gatt_service_client_disconnect(&mics_client, aics_cid);
}

void microphone_control_service_client_deinit(void){
    gatt_service_client_unregister_client(&mics_client);
}

