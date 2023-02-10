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

static void mcs_client_run_for_connection(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t)context;
    mcs_client_connection_t * connection = (mcs_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&mcs_client, con_handle);

    btstack_assert(connection != NULL);

    switch (connection->state){
        case MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = MEDIA_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &mcs_client_handle_gatt_client_event, con_handle, 
                mcs_client_value_handle_for_index(connection));
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
    event[5] = data_length;   // store value size
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void mcs_client_emit_read_event(mcs_client_connection_t * connection, uint8_t index, uint8_t status, const uint8_t * data, uint16_t data_size){
    btstack_packet_handler_t event_callback = gatt_service_client_get_event_callback_for_connection(&connection->basic_connection);
    uint16_t cid = gatt_service_client_get_cid_for_connection(&connection->basic_connection);
    
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&mcs_client, index);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_NAME:
            mcs_client_emit_string_value(cid, event_callback, GATTSERVICE_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_NAME, data, data_size);
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_OBJECT_ID:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_URL:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_CHANGED:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_TITLE:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_DURATION:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_TRACK_POSITION:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_PLAYBACK_SPEED:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_SEEKING_SPEED:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_SEGMENTS_OBJECT_ID:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_OBJECT_ID:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_NEXT_TRACK_OBJECT_ID:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_PARENT_GROUP_OBJECT_ID:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_GROUP_OBJECT_ID:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDER:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDERS_SUPPORTED:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_STATE:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_RESULTS_OBJECT_ID:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_CONTROL_POINT:
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_CONTENT_CONTROL_ID:
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

uint8_t media_control_service_client_get_media_player_name(uint16_t mcs_cid){
    gatt_service_client_connection_helper_t * connection = gatt_service_client_get_connection_for_cid(&mcs_client, mcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != GATT_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    
    mcs_client_handle_can_send_now.context = (void *)(uintptr_t)connection->con_handle;
    uint8_t status = gatt_client_request_to_send_gatt_query(&mcs_client_handle_can_send_now, connection->con_handle);
    
    uint8_t index = (uint8_t)MCS_CLIENT_CHARACTERISTIC_INDEX_MEDIA_PLAYER_NAME;
    
    if (status == ERROR_CODE_SUCCESS){
        if (connection->characteristics[index].value_handle == 0){
            return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
        }
        mcs_client_connection_t * mcs_connection = (mcs_client_connection_t *)connection;
        mcs_connection->state = MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
        mcs_connection->characteristic_index = index;
    }
    return status;
}

uint8_t media_control_service_client_disconnect(uint16_t mcs_cid){
    return gatt_service_client_disconnect(&mcs_client, mcs_cid);
}

void media_control_service_client_deinit(void){
    gatt_service_client_deinit(&mcs_client);
}

