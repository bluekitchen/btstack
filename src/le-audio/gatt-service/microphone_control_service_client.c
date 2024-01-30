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

#include "le-audio/gatt-service/gatt_service_client_helper.h"
#include "le-audio/gatt-service/microphone_control_service_client.h"
#include "le-audio/gatt-service/audio_input_control_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

// MSC Client
static gatt_service_client_helper_t mics_client;

static btstack_context_callback_registration_t mics_client_handle_can_send_now;

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

static void mics_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void mics_client_emit_connected(const gatt_service_client_connection_helper_t *connection_helper, uint8_t num_included_clients,  uint8_t *packet, uint16_t size) {
    little_endian_store_16(packet, 3, connection_helper->cid);
    packet[5] = num_included_clients;
    mics_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size,
                                             GATTSERVICE_SUBEVENT_MICS_CLIENT_CONNECTED);
}

static uint16_t mics_client_value_handle_for_index(mics_client_connection_t * connection){
    return connection->basic_connection.characteristics[connection->characteristic_index].value_handle;
}

static uint16_t gatt_service_client_characteristic_value_handle2uuid16(mics_client_connection_t * connection, uint16_t value_handle) {
    int i;
    for (i = 0; i < connection->basic_connection.characteristics_num; i++){
        if (connection->basic_connection.characteristics[i].value_handle == value_handle) {
            return gatt_service_client_characteristic_index2uuid16(&mics_client, i);
        }
    }
    return 0;
}

static void mics_client_emit_number(uint16_t cid, btstack_packet_handler_t event_callback, uint8_t subevent, const uint8_t * data, uint8_t data_size, uint8_t expected_data_size){
    UNUSED(data_size);
    btstack_assert(event_callback != NULL);
    
    if (data_size != expected_data_size){
        return;
    }
    
    uint8_t event[9];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = 3 + data_size;
    event[pos++] = subevent;
    little_endian_store_16(event, pos, cid);
    pos+= 2;
    reverse_bytes(data, &event[pos], data_size);
    pos += data_size;

    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void mics_client_emit_done_event(mics_client_connection_t * connection, uint8_t index, uint8_t status){
    btstack_packet_handler_t event_callback = connection->basic_connection.event_callback;
    btstack_assert(event_callback != NULL);

    uint16_t cid = connection->basic_connection.cid;
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&mics_client, index);

    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_MICS_CLIENT_WRITE_DONE;

    little_endian_store_16(event, pos, cid);
    pos+= 2;
    little_endian_store_16(event, pos, characteristic_uuid16);
    pos+= 2;
    event[pos++] = status;
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}


static void mics_client_emit_read_event(mics_client_connection_t * connection, uint8_t index, uint8_t status, const uint8_t * data, uint16_t data_size){
    if ((data_size > 0) && (data == NULL)){
        return;
    }

    btstack_packet_handler_t event_callback = connection->basic_connection.event_callback; 
    btstack_assert(event_callback != NULL);

    uint16_t cid = connection->basic_connection.cid;
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&mics_client, index);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_MUTE:
           mics_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MICS_CLIENT_MUTE, data, data_size, 4);
           break;
        default:
            btstack_assert(false);
            break;
    }
}

static void mics_client_emit_notify_event(mics_client_connection_t * connection, uint16_t value_handle, uint8_t status, const uint8_t * data, uint16_t data_size){
    if ((data_size > 0) && (data == NULL)){
        return;
    }

    btstack_packet_handler_t event_callback = connection->basic_connection.event_callback;
    btstack_assert(event_callback != NULL);

    uint16_t cid = connection->basic_connection.cid;
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_value_handle2uuid16(connection, value_handle);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_MUTE:
            mics_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MICS_CLIENT_MUTE, data, data_size, 1);
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

    mics_client_handle_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.con_handle;
    uint8_t status = gatt_client_request_to_send_gatt_query(&mics_client_handle_can_send_now, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

static uint8_t mics_client_request_read_characteristic(uint16_t aics_cid, mics_client_characteristic_index_t characteristic_index){
    mics_client_connection_t * connection = (mics_client_connection_t *) gatt_service_client_get_connection_for_cid(&mics_client, aics_cid);
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
    gatt_service_client_connection_helper_t * connection_helper;
    mics_client_connection_t * connection;
    hci_con_handle_t con_handle;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&mics_client, gattservice_subevent_client_connected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);
                    connection = (mics_client_connection_t *)connection_helper;

                    if (connection->state != MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_CONNECTION){
                        return;
                    }

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = MICS_CLIENT_CHARACTERISTIC_INDEX_MUTE; i < MICS_CLIENT_CHARACTERISTIC_INDEX_RFU; i++){
                            printf("0x%04X %s\n", connection_helper->characteristics[i].value_handle, mics_client_characteristic_name[i]);
                        }
                    };
#endif
                    if (connection->scheduled_task_query_included_services){
#ifdef ENABLE_TESTING_SUPPORT

                        printf("\nQuery AICS included services\n");
#endif
                        connection->scheduled_task_query_included_services = false;
                        connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_INCLUDED_SERVICES;
                        connection->aics_connections_num = 0;

                        mics_client_handle_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.con_handle;
                        uint8_t status = gatt_client_request_to_send_gatt_query(&mics_client_handle_can_send_now, connection->basic_connection.con_handle);

                        if (status != ERROR_CODE_SUCCESS){
                            connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_READY;
                            mics_client_emit_connected(connection_helper, connection->aics_connections_num, packet, size);
                        }
                    } else {
                        connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_READY;
                        mics_client_emit_connected(connection_helper, connection->aics_connections_num, packet, size);
                    }
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    // TODO reset client
                    connection_helper = gatt_service_client_get_connection_for_cid(&mics_client, gattservice_subevent_client_disconnected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);
                    mics_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, GATTSERVICE_SUBEVENT_MICS_CLIENT_DISCONNECTED);
                    break;

                case GATTSERVICE_SUBEVENT_AICS_CLIENT_CONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&mics_client, gattservice_subevent_aics_client_connected_get_aics_cid(packet));
                    btstack_assert(connection_helper != NULL);
                    connection = (mics_client_connection_t *)connection_helper;

                    switch (gattservice_subevent_aics_client_connected_get_att_status(packet)) {
                        case ERROR_CODE_SUCCESS:
                            printf("MICS: Audio Input Control service client connected\n");
                            break;
                        default:
                            printf("MICS: Audio Input Control service client connection failed, err 0x%02x.\n", gattservice_subevent_aics_client_connected_get_att_status(packet));
                            break;
                    }

                    switch (connection->state){
                        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICES_RESULT:
                            if (connection->aics_connections_index < (connection->aics_connections_num-1)) {
                                connection->aics_connections_index++;
                                (void) audio_input_control_service_client_connect(
                                        connection->basic_connection.con_handle,
                                        &connection->aics_connections_storage[connection->aics_connections_index],
                                        connection->aics_connections_storage[connection->aics_connections_index].basic_connection.start_handle,
                                        connection->aics_connections_storage[connection->aics_connections_index].basic_connection.end_handle,
                                        connection->aics_connections_storage[connection->aics_connections_index].basic_connection.service_index,
                                        &connection->aics_characteristics_storage[connection->aics_connections_index *
                                                                                  AUDIO_INPUT_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS],
                                        AUDIO_INPUT_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS,
                                        &mics_client_packet_handler_internal);
                                return;
                            }
                            connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_READY;
                            mics_client_emit_connected(&connection->basic_connection, connection->aics_connections_num, packet, size);
                            break;
                        default:
                            break;
                    }
                    break;

                default:
                    break;
            }
            break;

        case GATT_EVENT_NOTIFICATION:
            con_handle = (hci_con_handle_t)gatt_event_notification_get_handle(packet);
            connection = (mics_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&mics_client, con_handle);

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
    hci_con_handle_t con_handle;
    gatt_client_service_t service;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_characteristic_value_query_result_get_handle(packet);
            connection = (mics_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&mics_client, con_handle);
            btstack_assert(connection != NULL);                

            mics_client_emit_read_event(connection, connection->characteristic_index, ATT_ERROR_SUCCESS, 
                gatt_event_characteristic_value_query_result_get_value(packet), 
                gatt_event_characteristic_value_query_result_get_value_length(packet));
            
            connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_READY;
            break;

        case GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_included_service_query_result_get_handle(packet);
            connection = (mics_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&mics_client, con_handle);
            btstack_assert(connection != NULL);

            if (connection->aics_connections_num < (connection->aics_connections_max_num - 1) ){
                gatt_event_included_service_query_result_get_service(packet, &service);
                connection->aics_connections_storage[connection->aics_connections_num].basic_connection.service_index = connection->aics_connections_num;
                connection->aics_connections_storage[connection->aics_connections_num].basic_connection.service_uuid16 = service.uuid16;
                connection->aics_connections_storage[connection->aics_connections_num].basic_connection.start_handle = service.start_group_handle;
                connection->aics_connections_storage[connection->aics_connections_num].basic_connection.end_handle = service.end_group_handle;
                connection->aics_connections_num++;
            } else {
                log_info("Num included AICS services exceeded storage capacity, max num %d", connection->aics_connections_max_num);
            }
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            con_handle = (hci_con_handle_t)gatt_event_query_complete_get_handle(packet);
            connection = (mics_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&mics_client, con_handle);
            btstack_assert(connection != NULL);

            switch (connection->state){
                case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICES_RESULT:
                    if (connection->aics_connections_num == 0) {
                        break;
                    }
                    connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICE_CONNECTED;
                    connection->aics_connections_index = 0;

                    audio_input_control_service_client_init();
                    status = audio_input_control_service_client_connect(
                            connection->basic_connection.con_handle,
                            &connection->aics_connections_storage[connection->aics_connections_index],
                            connection->aics_connections_storage[connection->aics_connections_index].basic_connection.start_handle,
                            connection->aics_connections_storage[connection->aics_connections_index].basic_connection.end_handle,
                            connection->aics_connections_storage[connection->aics_connections_index].basic_connection.service_index,
                            &connection->aics_characteristics_storage[connection->aics_connections_index * AUDIO_INPUT_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS],
                            AUDIO_INPUT_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS,
                            &mics_client_packet_handler_internal);

                    if (status == ERROR_CODE_SUCCESS){
                        return;
                    }
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
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&mics_client, connection->characteristic_index);
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
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t)context;
    mics_client_connection_t * connection = (mics_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&mics_client, con_handle);

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;
    gatt_client_service_t service;

    switch (connection->state){
        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_INCLUDED_SERVICES:
            connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICES_RESULT;
            service.start_group_handle = connection->basic_connection.start_handle;
            service.end_group_handle = connection->basic_connection.end_handle;

            (void) gatt_client_find_included_services_for_service(
                    mics_client_handle_gatt_client_event,
                    con_handle,
                    &service);
            break;

        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &mics_client_handle_gatt_client_event, con_handle, 
                mics_client_value_handle_for_index(connection));
            break;

        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;

            value_length = mics_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_value_of_characteristic(
                    &mics_client_handle_gatt_client_event, con_handle,
                mics_client_value_handle_for_index(connection),
                value_length, value);
            
            break;

        default:
            break;
    }
}

static void mics_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_hci_event_handler(&mics_client, packet_type, channel, packet, size);
}

void microphone_control_service_client_init(void){
    gatt_service_client_init(&mics_client, &mics_client_packet_handler_trampoline);
    gatt_service_client_register_packet_handler(&mics_client, &mics_client_packet_handler_internal);

    mics_client.characteristics_desc16_num = sizeof(aics_uuid16s)/sizeof(uint16_t);
    mics_client.characteristics_desc16 = aics_uuid16s;

    mics_client_handle_can_send_now.callback = &mics_client_run_for_connection;
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

    mics_connection->scheduled_task_query_included_services = false;
    mics_connection->aics_connections_max_num = 0;
    mics_connection->aics_characteristics_max_num = 0;
    mics_connection->aics_connections_storage = NULL;
    mics_connection->aics_characteristics_storage = NULL;

    if (aics_connections_num > 0){
        btstack_assert(aics_storage_for_characteristics != NULL);
        btstack_assert(aics_connections != NULL);
        btstack_assert(aics_characteristics_num == aics_connections_num * AUDIO_INPUT_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS);

        mics_connection->aics_connections_storage = aics_connections;
        mics_connection->aics_connections_max_num = aics_connections_num;
        mics_connection->aics_characteristics_storage = aics_storage_for_characteristics;
        mics_connection->aics_characteristics_max_num = aics_characteristics_num;

//        audio_input_control_service_client_init();
        mics_connection->scheduled_task_query_included_services = true;
    }
    mics_connection->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_CONNECTION;
    mics_connection->aics_events_packet_handler = packet_handler;
    return gatt_service_client_connect(con_handle,
                                       &mics_client, &mics_connection->basic_connection,
                                       ORG_BLUETOOTH_SERVICE_MICROPHONE_CONTROL, 0,
                                       mics_storage_for_characteristics, mics_characteristics_num, packet_handler, mics_cid);
}

/**
 * @brief Read mute state. The mute state is received via GATTSERVICE_SUBEVENT_MICS_CLIENT_MUTE event.
 * @param mics_cid
 * @return status
 */
uint8_t microphone_control_service_client_read_mute_state(uint16_t mics_cid){
    return mics_client_request_read_characteristic(mics_cid, MICS_CLIENT_CHARACTERISTIC_INDEX_MUTE);
}

uint8_t microphone_control_service_client_mute(uint16_t mics_cid){
    mics_client_connection_t * connection = (mics_client_connection_t *) gatt_service_client_get_connection_for_cid(&mics_client, mics_cid);
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
    mics_client_connection_t * connection = (mics_client_connection_t *) gatt_service_client_get_connection_for_cid(&mics_client, mics_cid);
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

uint8_t microphone_control_service_client_set_gain_setting(uint16_t mics_cid, uint8_t aics_index, int8_t gain_setting){
    mics_client_connection_t * connection = (mics_client_connection_t *) gatt_service_client_get_connection_for_cid(&mics_client, mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_set_gain_setting(&connection->aics_connections_storage[aics_index], gain_setting);
}

uint8_t microphone_control_service_client_set_manual_gain_mode(uint16_t mics_cid, uint8_t aics_index){
    mics_client_connection_t * connection = (mics_client_connection_t *) gatt_service_client_get_connection_for_cid(&mics_client, mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_set_manual_gain_mode(&connection->aics_connections_storage[aics_index]);
}

uint8_t microphone_control_service_client_set_automatic_gain_mode(uint16_t mics_cid, uint8_t aics_index){
    mics_client_connection_t * connection = (mics_client_connection_t *) gatt_service_client_get_connection_for_cid(&mics_client, mics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_set_automatic_gain_mode(&connection->aics_connections_storage[aics_index]);
}

uint8_t microphone_control_service_client_disconnect(uint16_t aics_cid){
    return gatt_service_client_disconnect(&mics_client, aics_cid);
}

void microphone_control_service_client_deinit(void){
    gatt_service_client_deinit(&mics_client);
}

