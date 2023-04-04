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

#define BTSTACK_FILE__ "media_control_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "le-audio/gatt-service/media_control_service_util.h"
#include "le-audio/gatt-service/media_control_service_client.h"

#include "btstack_memory.h"
#include "ble/core.h"
#include "ble/gatt_client.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"

// MSC Client
static gatt_service_client_helper_t mcs_client;
static btstack_context_callback_registration_t mcs_client_handle_can_send_now;

static void mcs_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void mcs_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// list of uuids
static const gatt_service_client_characteristic_desc16_t mcs_characteristics_desc16[] = {
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_NAME,
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_URL,
    ORG_BLUETOOTH_CHARACTERISTIC_TRACK_CHANGED,
    ORG_BLUETOOTH_CHARACTERISTIC_TRACK_TITLE,
    ORG_BLUETOOTH_CHARACTERISTIC_TRACK_DURATION,
    ORG_BLUETOOTH_CHARACTERISTIC_TRACK_POSITION,
    ORG_BLUETOOTH_CHARACTERISTIC_PLAYBACK_SPEED,
    ORG_BLUETOOTH_CHARACTERISTIC_SEEKING_SPEED,
    ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_SEGMENTS_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_NEXT_TRACK_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_PARENT_GROUP_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_GROUP_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDER,
    ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDERS_SUPPORTED,
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_STATE,
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT,
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED,
    ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_RESULTS_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_CONTROL_POINT,
    ORG_BLUETOOTH_CHARACTERISTIC_CONTENT_CONTROL_ID,
};

typedef enum {
    MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_PLAYER_NAME = 0,
    MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_PLAYER_ICON_OBJECT_ID,
    MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_PLAYER_ICON_URL,
    MCS_CLIENT_CHARACTERISTIC_INDEX_TRACK_CHANGED,
    MCS_CLIENT_CHARACTERISTIC_INDEX_TRACK_TITLE,
    MCS_CLIENT_CHARACTERISTIC_INDEX_TRACK_DURATION,
    MCS_CLIENT_CHARACTERISTIC_INDEX_TRACK_POSITION,
    MCS_CLIENT_CHARACTERISTIC_INDEX_PLAYBACK_SPEED,
    MCS_CLIENT_CHARACTERISTIC_INDEX_SEEKING_SPEED,
    MCS_CLIENT_CHARACTERISTIC_INDEX_CURRENT_TRACK_SEGMENTS_OBJECT_ID,
    MCS_CLIENT_CHARACTERISTIC_INDEX_CURRENT_TRACK_OBJECT_ID,
    MCS_CLIENT_CHARACTERISTIC_INDEX_NEXT_TRACK_OBJECT_ID,
    MCS_CLIENT_CHARACTERISTIC_INDEX_PARENT_GROUP_OBJECT_ID,
    MCS_CLIENT_CHARACTERISTIC_INDEX_CURRENT_GROUP_OBJECT_ID,
    MCS_CLIENT_CHARACTERISTIC_INDEX_PLAYING_ORDER,
    MCS_CLIENT_CHARACTERISTIC_INDEX_PLAYING_ORDERS_SUPPORTED,
    MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_STATE,
    MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_CONTROL_POINT,
    MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED,
    MCS_CLIENT_CHARACTERISTIC_INDEX_SEARCH_RESULTS_OBJECT_ID,
    MCS_CLIENT_CHARACTERISTIC_INDEX_SEARCH_CONTROL_POINT,
    MCS_CLIENT_CHARACTERISTIC_INDEX_CONTENT_CONTROL_ID,
} mcs_client_characteristic_index_t;

static void mcs_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void mcs_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    gatt_service_client_connection_helper_t * connection;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
            connection = gatt_service_client_get_connection_for_cid(&mcs_client, gattservice_subevent_client_connected_get_cid(packet));
            btstack_assert(connection != NULL);
            mcs_client_replace_subevent_id_and_emit(connection->event_callback, packet, size, GATTSERVICE_SUBEVENT_MCS_CLIENT_CONNECTED);
            break;

        case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
            connection = gatt_service_client_get_connection_for_cid(&mcs_client, gattservice_subevent_client_disconnected_get_cid(packet));
            btstack_assert(connection != NULL);
            mcs_client_replace_subevent_id_and_emit(connection->event_callback, packet, size, GATTSERVICE_SUBEVENT_MCS_CLIENT_DISCONNECTED);
            break;

        default:
            break;
    }
}

static uint16_t mcs_client_value_handle_for_index(mcs_client_connection_t * connection){
    return connection->basic_connection.characteristics[connection->characteristic_index].value_handle;
}

static uint16_t mcs_client_serialize_characteristic_value_for_write(mcs_client_connection_t * connection, uint8_t * out_value){
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&mcs_client, connection->characteristic_index);
    out_value = NULL;
    
    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_OBJECT_ID:
        case ORG_BLUETOOTH_CHARACTERISTIC_NEXT_TRACK_OBJECT_ID:
        case ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_GROUP_OBJECT_ID:
            out_value = (uint8_t *) connection->data.data_string;
            return strlen(connection->data.data_string);
        
        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_POSITION:
            little_endian_store_32(connection->write_buffer, 0, connection->data.data_32);
            out_value = connection->write_buffer;
            return 4;

        case ORG_BLUETOOTH_CHARACTERISTIC_PLAYBACK_SPEED:
            little_endian_store_16(connection->write_buffer, 0, connection->data.data_16);
            out_value = connection->write_buffer;
            return 2;
        
        case ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDER:
            connection->write_buffer[0] = connection->data.data_8;
            out_value = connection->write_buffer;
            return 1;

        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT:
            connection->write_buffer[0] = connection->data.data_8;

            switch ((media_control_point_opcode_t)connection->data.data_8){
                case MEDIA_CONTROL_POINT_OPCODE_GOTO_TRACK:
                case MEDIA_CONTROL_POINT_OPCODE_GOTO_GROUP:
                case MEDIA_CONTROL_POINT_OPCODE_GOTO_SEGMENT:
                case MEDIA_CONTROL_POINT_OPCODE_MOVE_RELATIVE:
                    little_endian_store_16(connection->write_buffer, 1, connection->media_control_command_param);
                    return 5;
                default:
                    return 1;
            }
            break;

        case ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_CONTROL_POINT:
            return connection->write_buffer_length;

        default:
            btstack_assert(false);
            break;
    }
    return 0;
}

static void mcs_client_run_for_connection(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t)context;
    mcs_client_connection_t * connection = (mcs_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&mcs_client, con_handle);

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;

    switch (connection->state){
        case MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = MEDIA_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &mcs_client_handle_gatt_client_event, con_handle, 
                mcs_client_value_handle_for_index(connection));
            break;

        case MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = MEDIA_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;

            value_length = mcs_client_serialize_characteristic_value_for_write(connection, value);
            (void) gatt_client_write_value_of_characteristic(
                &mcs_client_handle_gatt_client_event, con_handle, 
                mcs_client_value_handle_for_index(connection),
                value_length, value);
            
            break;

        default:
            break;
    }
}

static void mcs_client_emit_string_value(uint16_t cid, btstack_packet_handler_t event_callback, uint8_t subevent, const uint8_t * data, uint16_t data_size){
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
    
    event[1] = pos - 2;         // store subevent size
    event[5] = data_length;     // store value size
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void mcs_client_emit_number(uint16_t cid, btstack_packet_handler_t event_callback, uint8_t subevent, const uint8_t * data, uint8_t data_size, uint8_t expected_data_size){
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

static void mcs_client_emit_done_event(mcs_client_connection_t * connection, uint8_t index, uint8_t status){
    btstack_packet_handler_t event_callback = gatt_service_client_get_event_callback_for_connection(&connection->basic_connection);
    btstack_assert(event_callback != NULL);

    uint16_t cid = gatt_service_client_get_cid_for_connection(&connection->basic_connection);
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&mcs_client, index);

    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_MCS_CLIENT_WRITE_DONE;

    little_endian_store_16(event, pos, cid);
    pos+= 2;
    little_endian_store_16(event, pos, characteristic_uuid16);
    pos+= 2;
    event[pos++] = status;
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void mcs_client_emit_media_control_point_notification_result_event(uint16_t cid, btstack_packet_handler_t event_callback, const uint8_t * data, uint8_t data_size){
    btstack_assert(event_callback != NULL);

    if (data_size != 2){
        return;
    }
    
    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_MCS_CLIENT_MEDIA_CONTROL_POINT_NOTIFICATION_RESULT;

    little_endian_store_16(event, pos, cid);
    pos+= 2;
    event[pos++] = data[0]; // opcode
    event[pos++] = data[1]; // result code
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void mcs_client_emit_search_control_point_notification_result_event(uint16_t cid, btstack_packet_handler_t event_callback, const uint8_t * data, uint8_t data_size){
    btstack_assert(event_callback != NULL);

    if (data_size != 1){
        return;
    }
    
    uint8_t event[6];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_MCS_CLIENT_SEARCH_CONTROL_POINT_NOTIFICATION_RESULT;

    little_endian_store_16(event, pos, cid);
    pos+= 2;
    event[pos++] = data[0]; // result code
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void mcs_client_emit_read_event(mcs_client_connection_t * connection, uint8_t index, uint8_t status, const uint8_t * data, uint16_t data_size){
    if ((data_size > 0) && (data == NULL)){
        return;
    }

    btstack_packet_handler_t event_callback = gatt_service_client_get_event_callback_for_connection(&connection->basic_connection);
    btstack_assert(event_callback != NULL);

    uint16_t cid = gatt_service_client_get_cid_for_connection(&connection->basic_connection);
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&mcs_client, index);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_NAME:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_NAME, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_ICON_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_URL:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_ICON_OBJECT_URL, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_TITLE:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_TRACK_TITLE, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_DURATION:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_TRACK_DURATION, data, data_size, 4);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_POSITION:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_TRACK_POSITION, data, data_size, 4);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_PLAYBACK_SPEED:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MSC_CLIENT_PLAYBACK_SPEED, data, data_size, 2);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_SEEKING_SPEED:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MSC_CLIENT_SEEKING_SPEED, data, data_size, 1);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_SEGMENTS_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_CURRENT_TRACK_SEGMENTS_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_CURRENT_TRACK_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_NEXT_TRACK_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_NEXT_TRACK_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_PARENT_GROUP_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_PARENT_GROUP_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_GROUP_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_CURRENT_GROUP_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDER:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_PLAYING_ORDER, data, data_size, 1);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDERS_SUPPORTED:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_PLAYING_ORDER_SUPPORTED, data, data_size, 2);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_STATE:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_MEDIA_STATE, data, data_size, 1);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_CONTROL_POINT_OPCODES_SUPPORTED, data, data_size, 4);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_RESULTS_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_SEARCH_RESULT_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_CONTENT_CONTROL_ID:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_CONTENT_CONTROL_ID, data, data_size, 1);
            break;  
         default:
            break;
    }
}

static void mcs_client_emit_notify_event(mcs_client_connection_t * connection, uint8_t index, uint8_t status, const uint8_t * data, uint16_t data_size){
    if ((data_size > 0) && (data == NULL)){
        return;
    }

    btstack_packet_handler_t event_callback = gatt_service_client_get_event_callback_for_connection(&connection->basic_connection);
    btstack_assert(event_callback != NULL);

    uint16_t cid = gatt_service_client_get_cid_for_connection(&connection->basic_connection);
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&mcs_client, index);

    switch (characteristic_uuid16){

        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_TITLE:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_TRACK_TITLE, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_DURATION:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_TRACK_DURATION, data, data_size, 4);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_POSITION:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_TRACK_POSITION, data, data_size, 4);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_PLAYBACK_SPEED:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MSC_CLIENT_PLAYBACK_SPEED, data, data_size, 2);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_SEEKING_SPEED:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MSC_CLIENT_SEEKING_SPEED, data, data_size, 1);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_CURRENT_TRACK_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_NEXT_TRACK_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_NEXT_TRACK_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_PARENT_GROUP_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_PARENT_GROUP_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_GROUP_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_CURRENT_GROUP_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDER:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_PLAYING_ORDER, data, data_size, 1);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_STATE:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_MEDIA_STATE, data, data_size, 1);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_CONTROL_POINT_OPCODES_SUPPORTED, data, data_size, 4);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_RESULTS_OBJECT_ID:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_SEARCH_RESULT_OBJECT_ID, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_CHANGED:
            mcs_client_emit_number(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_TRACK_CHANGED, data, data_size, 1);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT:
            mcs_client_emit_media_control_point_notification_result_event(cid, event_callback, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_CONTROL_POINT:
            mcs_client_emit_search_control_point_notification_result_event(cid, event_callback, data, data_size);
            break;

        default:
            break;
    }
}


static void mcs_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    mcs_client_connection_t * connection = NULL;
    hci_con_handle_t con_handle;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_characteristic_value_query_result_get_handle(packet);
            connection = (mcs_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&mcs_client, con_handle);
            btstack_assert(connection != NULL);                

            mcs_client_emit_read_event(connection, connection->characteristic_index, ATT_ERROR_SUCCESS, 
                gatt_event_characteristic_value_query_result_get_value(packet), 
                gatt_event_characteristic_value_query_result_get_value_length(packet));
            
            connection->state = MEDIA_CONTROL_SERVICE_CLIENT_STATE_READY;
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            con_handle = (hci_con_handle_t)gatt_event_query_complete_get_handle(packet);
            connection = (mcs_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&mcs_client, con_handle);
            btstack_assert(connection != NULL);
            
            connection->state = MEDIA_CONTROL_SERVICE_CLIENT_STATE_READY;
            mcs_client_emit_done_event(connection, connection->characteristic_index, gatt_event_query_complete_get_att_status(packet));
            break;

        case GATT_EVENT_NOTIFICATION:
            con_handle = (hci_con_handle_t)gatt_event_notification_get_handle(packet);
            connection = (mcs_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&mcs_client, con_handle);
           
            btstack_assert(connection != NULL);

            mcs_client_emit_notify_event(connection, connection->characteristic_index, ATT_ERROR_SUCCESS, 
                gatt_event_notification_get_value(packet), 
                gatt_event_notification_get_value_length(packet));
            break;

        default:
            break;
    }
}      

static void mcs_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_hci_event_handler(&mcs_client, packet_type, channel, packet, size);
}

void media_control_service_client_init(void){
    gatt_service_client_init(&mcs_client, &mcs_client_packet_handler_trampoline);
    gatt_service_client_register_packet_handler(&mcs_client, &mcs_client_packet_handler_internal);

    mcs_client.service_uuid16      = ORG_BLUETOOTH_SERVICE_MEDIA_CONTROL_SERVICE;

    mcs_client.characteristics_desc16_num = sizeof(mcs_characteristics_desc16)/sizeof(gatt_service_client_characteristic_desc16_t);
    mcs_client.characteristics_desc16 = mcs_characteristics_desc16;

    mcs_client_handle_can_send_now.callback = &mcs_client_run_for_connection;
}

uint8_t media_control_service_client_connect(hci_con_handle_t con_handle, mcs_client_connection_t * connection, 
    gatt_service_client_characteristic_t * characteristics, uint8_t characteristics_num,
    btstack_packet_handler_t packet_handler, uint16_t * mcs_cid){

    btstack_assert(mcs_client.characteristics_desc16_num > 0);
    return gatt_service_client_connect(con_handle,
        &mcs_client, &connection->basic_connection, 
        characteristics, characteristics_num, packet_handler, mcs_cid);
}

static bool mcs_client_can_query_characteristic(mcs_client_connection_t * connection, mcs_client_characteristic_index_t characteristic_index){
    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    return connection->state == MEDIA_CONTROL_SERVICE_CLIENT_STATE_READY ? ERROR_CODE_SUCCESS : ERROR_CODE_CONTROLLER_BUSY;
}

static uint8_t mcs_client_request_send_gatt_query(mcs_client_connection_t * connection, mcs_client_characteristic_index_t characteristic_index){
    connection->characteristic_index = characteristic_index;

    mcs_client_handle_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.con_handle;
    uint8_t status = gatt_client_request_to_send_gatt_query(&mcs_client_handle_can_send_now, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = MEDIA_CONTROL_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

static uint8_t mcs_client_request_read_characteristic(uint16_t mcs_cid, mcs_client_characteristic_index_t characteristic_index){
    mcs_client_connection_t * connection = (mcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t status = mcs_client_can_query_characteristic(connection, characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
   
    connection->state = MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    return mcs_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t mcs_client_request_write_characteristic(mcs_client_connection_t * connection, mcs_client_characteristic_index_t characteristic_index){
    connection->state = MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    return mcs_client_request_send_gatt_query(connection, characteristic_index);
}

uint8_t media_control_service_client_get_media_player_name(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_PLAYER_NAME); 
}

uint8_t media_control_service_client_get_media_player_icon_object_id(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_PLAYER_ICON_OBJECT_ID); 
}

uint8_t media_control_service_client_get_media_player_icon_url(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_PLAYER_ICON_URL); 
}

uint8_t media_control_service_client_get_track_title(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_TRACK_TITLE); 
}

uint8_t media_control_service_client_get_track_duration(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_TRACK_DURATION); 
}

uint8_t media_control_service_client_get_track_position(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_TRACK_POSITION); 
}

uint8_t media_control_service_client_get_playback_speed(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_PLAYBACK_SPEED); 
}

uint8_t media_control_service_client_get_seeking_speed(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_SEEKING_SPEED); 
}

uint8_t media_control_service_client_get_current_track_segments_object_id(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_CURRENT_TRACK_SEGMENTS_OBJECT_ID); 
}

uint8_t media_control_service_client_get_current_track_object_id(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_CURRENT_TRACK_OBJECT_ID); 
}

uint8_t media_control_service_client_get_next_track_object_id(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_NEXT_TRACK_OBJECT_ID); 
}

uint8_t media_control_service_client_get_parent_group_object_id(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_PARENT_GROUP_OBJECT_ID); 
}

uint8_t media_control_service_client_get_current_group_object_id(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_CURRENT_GROUP_OBJECT_ID); 
}

uint8_t media_control_service_client_get_playing_order(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_PLAYING_ORDER); 
}

uint8_t media_control_service_client_get_playing_orders_supported(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_PLAYING_ORDERS_SUPPORTED); 
}

uint8_t media_control_service_client_get_media_state(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_STATE); 
}

uint8_t media_control_service_client_get_media_control_point_opcodes_supported(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED); 
}

uint8_t media_control_service_client_get_search_results_object_id(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_SEARCH_RESULTS_OBJECT_ID); 
}

uint8_t media_control_service_client_get_content_control_id(uint16_t mcs_cid){
    return mcs_client_request_read_characteristic(mcs_cid, MCS_CLIENT_CHARACTERISTIC_INDEX_CONTENT_CONTROL_ID); 
}

uint8_t media_control_service_client_set_track_position(uint16_t mcs_cid, uint32_t position_10ms){
    mcs_client_connection_t * connection = (mcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    mcs_client_characteristic_index_t index = MCS_CLIENT_CHARACTERISTIC_INDEX_TRACK_POSITION;
    
    uint8_t status = mcs_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->data.data_32 = position_10ms;
    return mcs_client_request_write_characteristic(connection, index);
}

uint8_t media_control_service_client_set_playback_speed(uint16_t mcs_cid, uint16_t playback_speed){
    mcs_client_connection_t * connection = (mcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    mcs_client_characteristic_index_t index = MCS_CLIENT_CHARACTERISTIC_INDEX_PLAYBACK_SPEED;
    
    uint8_t status = mcs_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->data.data_16 = playback_speed;
    return mcs_client_request_write_characteristic(connection, index);
}

uint8_t media_control_service_client_set_current_track_object_id(uint16_t mcs_cid, const char * object_id){
    mcs_client_connection_t * connection = (mcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    mcs_client_characteristic_index_t index = MCS_CLIENT_CHARACTERISTIC_INDEX_CURRENT_TRACK_OBJECT_ID;
    
    uint8_t status = mcs_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->data.data_string = object_id;
    return mcs_client_request_write_characteristic(connection, index);
}

uint8_t media_control_service_client_set_next_track_object_id(uint16_t mcs_cid, const char * object_id){
    mcs_client_connection_t * connection = (mcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    mcs_client_characteristic_index_t index = MCS_CLIENT_CHARACTERISTIC_INDEX_NEXT_TRACK_OBJECT_ID;
    
    uint8_t status = mcs_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->data.data_string = object_id;
    return mcs_client_request_write_characteristic(connection, index);
}

uint8_t media_control_service_client_set_current_group_object_id(uint16_t mcs_cid, const char * object_id){
    mcs_client_connection_t * connection = (mcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    mcs_client_characteristic_index_t index = MCS_CLIENT_CHARACTERISTIC_INDEX_CURRENT_GROUP_OBJECT_ID;
    
    uint8_t status = mcs_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->data.data_string = object_id;
    return mcs_client_request_write_characteristic(connection, index);
}

uint8_t media_control_service_client_set_playing_order(uint16_t mcs_cid, uint8_t playing_order){
    mcs_client_connection_t * connection = (mcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    mcs_client_characteristic_index_t index = MCS_CLIENT_CHARACTERISTIC_INDEX_PLAYING_ORDER;
    
    uint8_t status = mcs_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->data.data_8 = playing_order;
    return mcs_client_request_write_characteristic(connection, index);

}

static uint8_t media_control_service_client_media_control_command(uint16_t mcs_cid, uint8_t media_control_opcode, int32_t media_control_command_param){
    mcs_client_connection_t * connection = (mcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    mcs_client_characteristic_index_t index = MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_CONTROL_POINT;
    
    uint8_t status = mcs_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->data.data_8 = media_control_opcode;
    connection->media_control_command_param = media_control_command_param;
    return mcs_client_request_write_characteristic(connection, index);
}

uint8_t media_control_service_client_command_play(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_PLAY, 0);
}

uint8_t media_control_service_client_command_pause(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_PAUSE, 0);
}

uint8_t media_control_service_client_command_fast_rewind(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_FAST_REWIND, 0);
}

uint8_t media_control_service_client_command_fast_forward(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_FAST_FORWARD, 0);
}

uint8_t media_control_service_client_command_stop(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_STOP, 0);
}

uint8_t media_control_service_client_command_move_relative(uint16_t mcs_cid, int32_t track_position_offset_10ms){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_MOVE_RELATIVE, track_position_offset_10ms);
}

uint8_t media_control_service_client_command_previous_segment(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_SEGMENT, 0);
}

uint8_t media_control_service_client_command_next_segment(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_NEXT_SEGMENT, 0);
}

uint8_t media_control_service_client_command_first_segment(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_FIRST_SEGMENT, 0);
}

uint8_t media_control_service_client_command_last_segment(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_LAST_SEGMENT, 0);
}

uint8_t media_control_service_client_command_goto_segment(uint16_t mcs_cid, int32_t segment_nr){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_GOTO_SEGMENT, segment_nr);
}

uint8_t media_control_service_client_command_previous_track(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_TRACK, 0);
}

uint8_t media_control_service_client_command_next_track(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_NEXT_TRACK, 0);
}

uint8_t media_control_service_client_command_first_track(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_FIRST_TRACK, 0);
}

uint8_t media_control_service_client_command_last_track(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_LAST_TRACK, 0);
}

uint8_t media_control_service_client_command_goto_track(uint16_t mcs_cid, int32_t track_nr){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_GOTO_TRACK, track_nr);
}

uint8_t media_control_service_client_command_previous_group(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_GROUP, 0);
}

uint8_t media_control_service_client_command_next_group(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_NEXT_GROUP, 0);
}

uint8_t media_control_service_client_command_first_group(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_FIRST_GROUP, 0);
}

uint8_t media_control_service_client_command_last_group(uint16_t mcs_cid){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_LAST_GROUP, 0);
}

uint8_t media_control_service_client_command_goto_group(uint16_t mcs_cid, int32_t group_nr){
    return media_control_service_client_media_control_command(mcs_cid, MEDIA_CONTROL_POINT_OPCODE_GOTO_GROUP, group_nr);
}

uint8_t media_control_service_client_search_control_command_init(uint16_t mcs_cid){
    mcs_client_connection_t * connection = (mcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    memset(connection->write_buffer, 0, sizeof(connection->write_buffer));
    connection->write_buffer_length = 0;
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_client_search_control_command_add(uint16_t mcs_cid, search_control_point_opcode_t opcode, const char * data){
    mcs_client_connection_t * connection = (mcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t remaining_bytes = MCS_SEARCH_CONTROL_POINT_COMMAND_MAX_LENGTH  - connection->write_buffer_length;
    if (remaining_bytes < 2){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }


    if (data == NULL){
        connection->write_buffer[connection->write_buffer_length++] = 1;
        connection->write_buffer[connection->write_buffer_length++] = (uint8_t) opcode;
    } else {
        if ((remaining_bytes - 2) < strlen(data)){
            return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
        } else {
            connection->write_buffer[connection->write_buffer_length++] = 1 + strlen(data);
            connection->write_buffer[connection->write_buffer_length++] = (uint8_t) opcode;
        }
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_client_search_control_command_execute(uint16_t mcs_cid){
    mcs_client_connection_t * connection = (mcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    mcs_client_characteristic_index_t index = MCS_CLIENT_CHARACTERISTIC_INDEX_SEARCH_CONTROL_POINT;
    uint8_t status = mcs_client_can_query_characteristic(connection, index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    return mcs_client_request_write_characteristic(connection, index);
}


uint8_t media_control_service_client_disconnect(uint16_t mcs_cid){
    return gatt_service_client_disconnect(&mcs_client, mcs_cid);
}

void media_control_service_client_deinit(void){
    gatt_service_client_deinit(&mcs_client);
}

