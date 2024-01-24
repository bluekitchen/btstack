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

#define BTSTACK_FILE__ "audio_input_control_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "le-audio/gatt-service/audio_input_control_service_util.h"
#include "le-audio/gatt-service/audio_input_control_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

// MSC Client
static gatt_service_client_helper_t aics_client;
static btstack_context_callback_registration_t aics_client_handle_can_send_now;

static void aics_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void aics_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void aics_client_run_for_connection(void * context);

// list of uuids
static const uint16_t aics_uuid16s[AUDIO_INPUT_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS] = {
    ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATE,
    ORG_BLUETOOTH_CHARACTERISTIC_GAIN_SETTINGS_ATTRIBUTE,
    ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_TYPE,
    ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATUS,
    ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_CONTROL_POINT,
    ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_DESCRIPTION
};

typedef enum {
    AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_STATE = 0,
    AICS_CLIENT_CHARACTERISTIC_INDEX_GAIN_SETTINGS_ATTRIBUTE,
    AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_TYPE,
    AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_STATUS,
    AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_CONTROL_POINT,
    AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_DESCRIPTION,
    AICS_CLIENT_CHARACTERISTIC_INDEX_RFU
} aics_client_characteristic_index_t;

#ifdef ENABLE_TESTING_SUPPORT
static char * aics_client_characteristic_name[] = {
    "AUDIO_INPUT_STATE",
    "GAIN_SETTINGS_ATTRIBUTE",
    "AUDIO_INPUT_TYPE",
    "AUDIO_INPUT_STATUS",
    "AUDIO_INPUT_CONTROL_POINT",
    "AUDIO_INPUT_DESCRIPTION",
    "RFU"
};
#endif

static void aics_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}


static uint16_t aics_client_value_handle_for_index(aics_client_connection_t * connection){
    return connection->basic_connection.characteristics[connection->characteristic_index].value_handle;
}

static uint16_t gatt_service_client_characteristic_value_handle2uuid16(aics_client_connection_t * connection, uint16_t value_handle) {
    int i;
    for (i = 0; i < connection->basic_connection.characteristics_num; i++){
        if (connection->basic_connection.characteristics[i].value_handle == value_handle) {
            return gatt_service_client_characteristic_index2uuid16(&aics_client, i);
        }
    }
    return 0;
}


static void aics_client_emit_string_value(uint16_t cid, btstack_packet_handler_t event_callback, uint8_t subevent, const uint8_t * data, uint16_t data_size){
    UNUSED(data_size);
    btstack_assert(event_callback != NULL);
    
    uint8_t event[257];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    pos++;                      // reserve event[1] for subevent size 
    event[pos++] = subevent;
    little_endian_store_16(event, pos, cid);
    pos+= 2;
    pos++;                      // reserve event[5] for value size

    uint16_t data_length = btstack_strcpy((char *)&event[pos], sizeof(event) - pos, (const char *)data);
    pos += data_length;

    event[1] = pos - 2;         // store subevent size
    event[5] = data_length;     // store value size
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void aics_client_emit_number(uint16_t cid, btstack_packet_handler_t event_callback, uint8_t subevent, const uint8_t * data, uint8_t data_size, uint8_t expected_data_size){
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

static void aics_client_emit_done_event(aics_client_connection_t * connection, uint8_t index, uint8_t status){
    btstack_packet_handler_t event_callback = connection->basic_connection.event_callback;
    btstack_assert(event_callback != NULL);

    uint16_t cid = connection->basic_connection.cid;
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&aics_client, index);

    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    // event[pos++] = GATTSERVICE_SUBEVENT_AICS_CLIENT_WRITE_DONE;

    little_endian_store_16(event, pos, cid);
    pos+= 2;
    little_endian_store_16(event, pos, characteristic_uuid16);
    pos+= 2;
    event[pos++] = status;
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void aics_client_emit_media_control_point_notification_result_event(uint16_t cid, btstack_packet_handler_t event_callback, const uint8_t * data, uint8_t data_size){
    btstack_assert(event_callback != NULL);

    if (data_size != 2){
        return;
    }
    
    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    // event[pos++] = GATTSERVICE_SUBEVENT_AICS_CONTROL_POINT_NOTIFICATION_RESULT;

    little_endian_store_16(event, pos, cid);
    pos+= 2;
    event[pos++] = data[0]; // opcode
    event[pos++] = data[1]; // result code
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void aics_client_emit_read_event(aics_client_connection_t * connection, uint8_t index, uint8_t status, const uint8_t * data, uint16_t data_size){
    if ((data_size > 0) && (data == NULL)){
        return;
    }

    btstack_packet_handler_t event_callback = connection->basic_connection.event_callback; 
    btstack_assert(event_callback != NULL);

    //    uint16_t cid = connection->basic_connection.cid;
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&aics_client, index);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATE:
            // aics_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_AICS_CLIENT_MEDIA_PLAYER_NAME, data, data_size);
            // aics_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_AICS_CLIENT_TRACK_DURATION, data, data_size, 4);
           break;
        case ORG_BLUETOOTH_CHARACTERISTIC_GAIN_SETTINGS_ATTRIBUTE:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_TYPE:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATUS:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_CONTROL_POINT:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_DESCRIPTION:
            break;
        default:
            btstack_assert(false);
            break;
    }
}

static void aics_client_emit_notify_event(aics_client_connection_t * connection, uint16_t value_handle, uint8_t status, const uint8_t * data, uint16_t data_size){
    if ((data_size > 0) && (data == NULL)){
        return;
    }

    btstack_packet_handler_t event_callback = connection->basic_connection.event_callback;
    btstack_assert(event_callback != NULL);

    uint16_t cid = connection->basic_connection.cid;
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_value_handle2uuid16(connection, value_handle);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATE:
            // aics_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_AICS_CLIENT_MEDIA_PLAYER_NAME, data, data_size);
            // aics_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_AICS_CLIENT_TRACK_DURATION, data, data_size, 4);
           break;
        case ORG_BLUETOOTH_CHARACTERISTIC_GAIN_SETTINGS_ATTRIBUTE:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_TYPE:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATUS:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_CONTROL_POINT:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_DESCRIPTION:
            break;
        default:
            btstack_assert(false);
            break;
    }
}


static uint8_t aics_client_can_query_characteristic(aics_client_connection_t * connection, aics_client_characteristic_index_t characteristic_index){
    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    return connection->state == AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY ? ERROR_CODE_SUCCESS : ERROR_CODE_CONTROLLER_BUSY;
}

static uint8_t aics_client_request_send_gatt_query(aics_client_connection_t * connection, aics_client_characteristic_index_t characteristic_index){
    connection->characteristic_index = characteristic_index;

    aics_client_handle_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.con_handle;
    uint8_t status = gatt_client_request_to_send_gatt_query(&aics_client_handle_can_send_now, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

static uint8_t aics_client_request_read_characteristic(uint16_t aics_cid, aics_client_characteristic_index_t characteristic_index){
    aics_client_connection_t * connection = (aics_client_connection_t *) gatt_service_client_get_connection_for_cid(&aics_client, aics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t status = aics_client_can_query_characteristic(connection, characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
   
    connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    return aics_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t aics_client_request_write_characteristic_without_response(aics_client_connection_t * connection, aics_client_characteristic_index_t characteristic_index){
    connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE;
    return aics_client_request_send_gatt_query(connection, characteristic_index);
}

//uint8_t audio_input_control_service_client_get_media_player_name(uint16_t aics_cid){
//    return aics_client_request_read_characteristic(aics_cid, AICS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_PLAYER_NAME);
//}


//uint8_t audio_input_control_service_client_set_track_position(uint16_t aics_cid, uint32_t position_10ms){
//    aics_client_connection_t * connection = (aics_client_connection_t *) gatt_service_client_get_connection_for_cid(&aics_client, aics_cid);
//    if (connection == NULL){
//        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
//    }
//    aics_client_characteristic_index_t index = AICS_CLIENT_CHARACTERISTIC_INDEX_TRACK_POSITION;
//
//    uint8_t status = aics_client_can_query_characteristic(connection, index);
//    if (status != ERROR_CODE_SUCCESS){
//        return status;
//    }
//    connection->data.data_32 = position_10ms;
//    return aics_client_request_write_characteristic_without_response(connection, index);
//}


// ****************************

static void aics_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    gatt_service_client_connection_helper_t * connection_helper;
    aics_client_connection_t * connection;
    hci_con_handle_t con_handle;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&aics_client, gattservice_subevent_client_connected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_STATE; i < AICS_CLIENT_CHARACTERISTIC_INDEX_RFU; i++){
                            printf("0x%04X %s\n", connection_helper->characteristics[i].value_handle, aics_client_characteristic_name[i]);

                        }
                    };
#endif
                    aics_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, GATTSERVICE_SUBEVENT_AICS_CLIENT_CONNECTED);
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&aics_client, gattservice_subevent_client_disconnected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);
                    aics_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, GATTSERVICE_SUBEVENT_AICS_CLIENT_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;

        case GATT_EVENT_NOTIFICATION:
            con_handle = (hci_con_handle_t)gatt_event_notification_get_handle(packet);
            connection = (aics_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&aics_client, con_handle);

            btstack_assert(connection != NULL);

            aics_client_emit_notify_event(connection, gatt_event_notification_get_value_handle(packet), ATT_ERROR_SUCCESS,
                                         gatt_event_notification_get_value(packet),
                                         gatt_event_notification_get_value_length(packet));
            break;
        default:
            break;
    }
}

static void aics_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    aics_client_connection_t * connection = NULL;
    hci_con_handle_t con_handle;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_characteristic_value_query_result_get_handle(packet);
            connection = (aics_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&aics_client, con_handle);
            btstack_assert(connection != NULL);                

            aics_client_emit_read_event(connection, connection->characteristic_index, ATT_ERROR_SUCCESS, 
                gatt_event_characteristic_value_query_result_get_value(packet), 
                gatt_event_characteristic_value_query_result_get_value_length(packet));
            
            connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY;
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            con_handle = (hci_con_handle_t)gatt_event_query_complete_get_handle(packet);
            connection = (aics_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&aics_client, con_handle);
            btstack_assert(connection != NULL);
            
            connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY;
            aics_client_emit_done_event(connection, connection->characteristic_index, gatt_event_query_complete_get_att_status(packet));
            break;

        default:
            break;
    }
}      


static void aics_client_run_for_connection(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t)context;
    aics_client_connection_t * connection = (aics_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&aics_client, con_handle);

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;

    switch (connection->state){
        case AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &aics_client_handle_gatt_client_event, con_handle, 
                aics_client_value_handle_for_index(connection));
            break;

        case AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE:
            connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY;
// TODO
//            value_length = aics_client_serialize_characteristic_value_for_write(connection, &value);
//            (void) gatt_client_write_value_of_characteristic_without_response(
//                con_handle,
//                aics_client_value_handle_for_index(connection),
//                value_length, value);
            
            break;

        default:
            break;
    }
}

static void aics_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_hci_event_handler(&aics_client, packet_type, channel, packet, size);
}

void audio_input_control_service_client_init(void){
    gatt_service_client_init(&aics_client, &aics_client_packet_handler_trampoline);
    gatt_service_client_register_packet_handler(&aics_client, &aics_client_packet_handler_internal);

    aics_client.characteristics_desc16_num = sizeof(aics_uuid16s)/sizeof(uint16_t);
    aics_client.characteristics_desc16 = aics_uuid16s;

    aics_client_handle_can_send_now.callback = &aics_client_run_for_connection;
}

uint8_t audio_input_control_service_client_connect(hci_con_handle_t con_handle,
    aics_client_connection_t * connection, uint16_t service_start_handle, uint16_t service_end_handle, uint8_t service_index,
    gatt_service_client_characteristic_t * characteristics, uint8_t characteristics_num, btstack_packet_handler_t packet_handler){

    btstack_assert(characteristics_num > 0);
    return gatt_service_client_connect_secondary_service(con_handle,
        &aics_client, &connection->basic_connection,
        ORG_BLUETOOTH_SERVICE_AUDIO_INPUT_CONTROL, service_start_handle, service_end_handle, service_index,
        characteristics, characteristics_num, packet_handler);
}

uint8_t audio_input_control_service_client_disconnect(uint16_t aics_cid){
    return gatt_service_client_disconnect(&aics_client, aics_cid);
}

void audio_input_control_service_client_deinit(void){
    gatt_service_client_deinit(&aics_client);
}

