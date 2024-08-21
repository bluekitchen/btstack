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

#define BTSTACK_FILE__ "volume_control_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "ble/gatt-service/gatt_service_client_helper.h"
#include "le-audio/gatt-service/volume_control_service_client.h"
#include "le-audio/gatt-service/audio_input_control_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

// VCS Client
static gatt_service_client_t vcs_client;

static btstack_context_callback_registration_t vcs_client_handle_can_send_now;

static void vcs_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void vcs_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void vcs_client_run_for_connection(void * context);

// list of uuids
static const uint16_t vcs_uuid16s[VOLUME_CONTROL_SERVICE_NUM_CHARACTERISTICS] = {
    ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_STATE,       
    ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_CONTROL_POINT,
    ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_FLAGS
};

typedef enum {
    VCS_CLIENT_CHARACTERISTIC_INDEX_START_GROUP = 0,
    VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_STATE = 0,
    VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_CONTROL_POINT,
    VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_FLAGS,
    VCS_CLIENT_CHARACTERISTIC_INDEX_RFU
} vcs_client_characteristic_index_t;

#ifdef ENABLE_TESTING_SUPPORT
static char * vcs_client_characteristic_name[] = {
    "VOLUME_STATE",
    "VOLUME_CONTROL_POINT",
    "VOLUME_FLAGS",
    "RFU"
};
#endif

static void vcs_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void vcs_client_emit_connection_established(const gatt_service_client_connection_t *connection_helper, uint8_t num_aics_clients, uint8_t num_vocs_clients, uint8_t status) {
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint8_t event[10];
    int pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_VCS_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, connection_helper->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos += 2;
    event[pos++] = num_aics_clients; // num included services
    event[pos++] = num_vocs_clients; // num included services
    event[pos++] = status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void vcs_client_finalize_connection(vcs_client_connection_t * connection){
    // already finalized by GATT CLIENT HELPER
    if (connection == NULL){
        return;
    }
    gatt_service_client_finalize_connection(&vcs_client, &connection->basic_connection);
}

static void vcs_client_connected(vcs_client_connection_t * connection, uint8_t status) {
    if (status == ERROR_CODE_SUCCESS){
        connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_READY;
        vcs_client_emit_connection_established(&connection->basic_connection, connection->aics_connections_num, connection->vocs_connections_num, status);
    } else {
        connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_IDLE;
        vcs_client_emit_connection_established(&connection->basic_connection, 0, 0, status);
        vcs_client_finalize_connection(connection);
    }
}

static void vcs_client_emit_uint8_array(gatt_service_client_connection_t * connection_helper, uint8_t subevent, const uint8_t * data, uint8_t data_size, uint8_t att_status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);
    btstack_assert(data_size <= 4);

    uint8_t event[11];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = 3 + data_size;
    event[pos++] = subevent;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    event[pos++] = connection_helper->service_index;
    memcpy(&event[pos], data, data_size);
    pos += data_size;
    event[pos++] = att_status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void vcs_client_emit_done_event(vcs_client_connection_t * connection, uint8_t index, uint8_t status){
    btstack_packet_handler_t event_callback = connection->basic_connection.event_callback;
    btstack_assert(event_callback != NULL);

    uint16_t cid = connection->basic_connection.cid;
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&vcs_client, index);

    uint8_t event[9];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_VCS_CLIENT_WRITE_DONE;

    little_endian_store_16(event, pos, cid);
    pos+= 2;
    event[pos++] = index;

    little_endian_store_16(event, pos, characteristic_uuid16);
    pos+= 2;
    event[pos++] = status;
    (*event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}


static void vcs_client_emit_read_event(gatt_service_client_connection_t * connection_helper, uint8_t characteristic_index, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    uint16_t expected_data_size;
    uint8_t  subevent_id;
    uint8_t  null_data[3];
    memset(null_data, 0, sizeof(null_data));

    switch (characteristic_index){
        case VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_STATE:
            expected_data_size = 3;
            if (expected_data_size == data_size){
                vcs_client_connection_t * connection = (vcs_client_connection_t *)connection_helper;
                connection->change_counter = data[2];
            }
            subevent_id = LEAUDIO_SUBEVENT_VCS_CLIENT_VOLUME_STATE;
            break;
        case VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_FLAGS:
            expected_data_size = 1;
            subevent_id = LEAUDIO_SUBEVENT_VCS_CLIENT_VOLUME_FLAGS;
            break;
        default:
            btstack_assert(false);
            break;
    }

    if (att_status != ATT_ERROR_SUCCESS){
        vcs_client_emit_uint8_array(connection_helper, subevent_id, null_data, 0, att_status);
        return;
    }
    if (data_size != expected_data_size){
        vcs_client_emit_uint8_array(connection_helper, subevent_id, null_data, 0, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
    } else {
        vcs_client_emit_uint8_array(connection_helper, subevent_id, data, expected_data_size, ERROR_CODE_SUCCESS);
    }
}

static void vcs_client_emit_notify_event(gatt_service_client_connection_t * connection_helper, uint16_t value_handle, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    int16_t expected_data_size;
    uint8_t  subevent_id;
    uint8_t  null_data[3];
    memset(null_data, 0, sizeof(null_data));

    uint16_t characteristic_uuid16 = gatt_service_client_helper_characteristic_uuid16_for_value_handle(&vcs_client, connection_helper, value_handle);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_STATE:
            expected_data_size = 3;
            if (expected_data_size == data_size){
                vcs_client_connection_t * connection = (vcs_client_connection_t *)connection_helper;
                connection->change_counter = data[2];
            }
            subevent_id = LEAUDIO_SUBEVENT_VCS_CLIENT_VOLUME_STATE;
            break;
        case ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_FLAGS:
            expected_data_size = 1;
            subevent_id = LEAUDIO_SUBEVENT_VCS_CLIENT_VOLUME_FLAGS;
            break;
        default:
            btstack_assert(false);
            break;
    }

    if (att_status != ATT_ERROR_SUCCESS){
        vcs_client_emit_uint8_array(connection_helper, subevent_id, null_data, 0, att_status);
        return;
    }
    if (data_size != expected_data_size){
        vcs_client_emit_uint8_array(connection_helper, subevent_id, null_data, 0, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
    } else {
        vcs_client_emit_uint8_array(connection_helper, subevent_id, data, expected_data_size, ERROR_CODE_SUCCESS);
    }
}

static uint8_t vcs_client_can_query_characteristic(vcs_client_connection_t * connection, vcs_client_characteristic_index_t characteristic_index){
    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    return connection->state == VOLUME_CONTROL_SERVICE_CLIENT_STATE_READY ? ERROR_CODE_SUCCESS : ERROR_CODE_CONTROLLER_BUSY;
}

static uint8_t vcs_client_request_send_gatt_query(vcs_client_connection_t * connection, vcs_client_characteristic_index_t characteristic_index){
    connection->characteristic_index = characteristic_index;

    vcs_client_handle_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.con_handle;
    uint8_t status = gatt_client_request_to_send_gatt_query(&vcs_client_handle_can_send_now, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

static uint8_t vcs_client_request_read_characteristic(uint16_t aics_cid, vcs_client_characteristic_index_t characteristic_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, aics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t status = vcs_client_can_query_characteristic(connection, characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
   
    connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    return vcs_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t vcs_client_request_write_characteristic(vcs_client_connection_t * connection, vcs_client_characteristic_index_t characteristic_index){
    connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    return vcs_client_request_send_gatt_query(connection, characteristic_index);
}

static void vcs_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    gatt_service_client_connection_t * connection_helper;
    vcs_client_connection_t * connection;
    hci_con_handle_t con_handle;
    uint16_t cid;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    cid = gattservice_subevent_client_connected_get_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&vcs_client, cid);
                    btstack_assert(connection_helper != NULL);
                    connection = (vcs_client_connection_t *)connection_helper;

                    status = gattservice_subevent_client_connected_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS){
                        vcs_client_connected(connection, status);
                        break;
                    }

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = VCS_CLIENT_CHARACTERISTIC_INDEX_START_GROUP; i < VCS_CLIENT_CHARACTERISTIC_INDEX_RFU; i++){
                            printf("0x%04X %s\n", connection_helper->characteristics[i].value_handle, vcs_client_characteristic_name[i]);
                        }
                    };
#endif

                    if (connection->basic_connection.characteristics[VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_STATE].value_handle == 0){
                        connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_IDLE;
                        vcs_client_connected(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                        break;
                    }

                    connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHANGE_COUNTER;
                    vcs_client_request_send_gatt_query(connection, VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_STATE);
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    // TODO reset client
                    cid = gattservice_subevent_client_disconnected_get_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&vcs_client, cid);
                    btstack_assert(connection_helper != NULL);
                    vcs_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_VCS_CLIENT_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;

        case GATT_EVENT_NOTIFICATION:
                    con_handle = (hci_con_handle_t)gatt_event_notification_get_handle(packet);
                    connection_helper = gatt_service_client_get_connection_for_con_handle(&vcs_client, con_handle);

                    btstack_assert(connection != NULL);

                    vcs_client_emit_notify_event(connection_helper, gatt_event_notification_get_value_handle(packet), ATT_ERROR_SUCCESS,
                                                 gatt_event_notification_get_value(packet),
                                                 gatt_event_notification_get_value_length(packet));
                    break;
        
        case HCI_EVENT_LEAUDIO_META:
            switch (hci_event_leaudio_meta_get_subevent_code(packet)){
                
                case LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_INPUT_STATE:
                    cid = leaudio_subevent_aics_client_audio_input_state_get_aics_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&vcs_client, cid);
                    btstack_assert(connection_helper != NULL);
                    vcs_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_VCS_CLIENT_AUDIO_INPUT_STATE);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_GAIN_SETTINGS_PROPERTIES:
                    cid = leaudio_subevent_aics_client_gain_settings_properties_get_aics_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&vcs_client, cid);
                    btstack_assert(connection_helper != NULL);
                    vcs_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_VCS_CLIENT_GAIN_SETTINGS_PROPERTIES);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_INPUT_TYPE:
                    cid = leaudio_subevent_aics_client_audio_input_type_get_aics_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&vcs_client, cid);
                    btstack_assert(connection_helper != NULL);
                    vcs_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_VCS_CLIENT_AUDIO_INPUT_TYPE);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_INPUT_STATUS:
                    cid = leaudio_subevent_aics_client_audio_input_status_get_aics_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&vcs_client, cid);
                    btstack_assert(connection_helper != NULL);
                    vcs_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_VCS_CLIENT_AUDIO_INPUT_STATUS);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_DESCRIPTION:
                    cid = leaudio_subevent_aics_client_audio_description_get_aics_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&vcs_client, cid);
                    btstack_assert(connection_helper != NULL);
                    vcs_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_VCS_CLIENT_AUDIO_DESCRIPTION);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_WRITE_DONE:
                    cid = leaudio_subevent_aics_client_write_done_get_aics_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&vcs_client, cid);
                    btstack_assert(connection_helper != NULL);
                    vcs_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_VCS_CLIENT_WRITE_DONE);
                    break;

                case LEAUDIO_SUBEVENT_AICS_CLIENT_CONNECTED:
                    cid = leaudio_subevent_aics_client_connected_get_aics_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&vcs_client, cid);
                    btstack_assert(connection_helper != NULL);
                    connection = (vcs_client_connection_t *)connection_helper;

                    if (leaudio_subevent_aics_client_connected_get_att_status(packet) == ERROR_CODE_SUCCESS) {
                        connection->aics_connections_connected++;
                    } else {
                        log_info("VCS: AICS client connection failed, err 0x%02x, con_handle 0x%04x, cid 0x%04x",
                                 leaudio_subevent_aics_client_connected_get_att_status(packet),
                                 leaudio_subevent_aics_client_connected_get_con_handle(packet),
                                 leaudio_subevent_aics_client_connected_get_aics_cid(packet));
                    }

                    if (connection->aics_connections_index < (connection->aics_connections_num-1)) {
                        connection->aics_connections_index++;
                        (void) audio_input_control_service_client_connect(
                                connection->basic_connection.con_handle,
                                &vcs_client_packet_handler_internal,
                                connection->aics_connections_storage[connection->aics_connections_index].basic_connection.start_handle,
                                connection->aics_connections_storage[connection->aics_connections_index].basic_connection.end_handle,
                                connection->aics_connections_storage[connection->aics_connections_index].basic_connection.service_index,
                                &connection->aics_connections_storage[connection->aics_connections_index]);
                        break;
                    }

                    if (connection->vocs_connections_num != 0) {
                        volume_offset_control_service_client_init();
                        connection->vocs_connections_index = 0;
                        connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_VOCS_SERVICES_CONNECTED;

                        (void) volume_offset_control_service_client_connect(
                                connection->basic_connection.con_handle,
                                &vcs_client_packet_handler_internal,
                                connection->vocs_connections_storage[connection->vocs_connections_index].basic_connection.start_handle,
                                connection->vocs_connections_storage[connection->vocs_connections_index].basic_connection.end_handle,
                                connection->vocs_connections_storage[connection->vocs_connections_index].basic_connection.service_index,
                                &connection->vocs_connections_storage[connection->vocs_connections_index]);
                        break;
                    }

                    vcs_client_connected(connection, ERROR_CODE_SUCCESS);
                    break;

                case LEAUDIO_SUBEVENT_VOCS_CLIENT_CONNECTED:
                    cid = leaudio_subevent_vocs_client_connected_get_vocs_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&vcs_client, cid);
                    btstack_assert(connection_helper != NULL);
                    connection = (vcs_client_connection_t *)connection_helper;

                    if (leaudio_subevent_vocs_client_connected_get_att_status(packet) == ERROR_CODE_SUCCESS) {
                        connection->vocs_connections_connected++;
                    } else {
                        log_info("VCS: VOCS client connection failed, err 0x%02x, con_handle 0x%04x, cid 0x%04x",
                                 leaudio_subevent_vocs_client_connected_get_att_status(packet),
                                 leaudio_subevent_vocs_client_connected_get_con_handle(packet),
                                 leaudio_subevent_vocs_client_connected_get_vocs_cid(packet));
                    }

                    if (connection->vocs_connections_index < (connection->vocs_connections_num-1)) {
                        connection->vocs_connections_index++;
                        (void) volume_offset_control_service_client_connect(
                                connection->basic_connection.con_handle,
                                &vcs_client_packet_handler_internal,
                                connection->vocs_connections_storage[connection->vocs_connections_index].basic_connection.start_handle,
                                connection->vocs_connections_storage[connection->vocs_connections_index].basic_connection.end_handle,
                                connection->vocs_connections_storage[connection->vocs_connections_index].basic_connection.service_index,
                                &connection->vocs_connections_storage[connection->vocs_connections_index]);
                        return;
                    }

                    vcs_client_connected(connection, ERROR_CODE_SUCCESS);
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }
}

static void vcs_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    gatt_service_client_connection_t * connection_helper;
    vcs_client_connection_t * connection = NULL;
    hci_con_handle_t con_handle;
    gatt_client_service_t service;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_characteristic_value_query_result_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&vcs_client, con_handle);
            btstack_assert(connection_helper != NULL);
            connection = (vcs_client_connection_t *)connection_helper;

            vcs_client_emit_read_event(connection_helper, connection->characteristic_index, ATT_ERROR_SUCCESS,
                                       gatt_event_characteristic_value_query_result_get_value(packet),
                                       gatt_event_characteristic_value_query_result_get_value_length(packet));


            switch (connection->state){
                case VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_CHANGE_COUNTER_RESULT:
                    // check wrong packet length
                    if (gatt_event_characteristic_value_query_result_get_value_length(packet) != 3){
                        connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_CHANGE_COUNTER_RESULT_READ_FAILED;
                        break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_included_service_query_result_get_handle(packet);
            connection = (vcs_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&vcs_client, con_handle);
            btstack_assert(connection != NULL);

            gatt_event_included_service_query_result_get_service(packet, &service);
            switch (service.uuid16){
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
                case ORG_BLUETOOTH_SERVICE_VOLUME_OFFSET_CONTROL:
                    if (connection->vocs_connections_num < connection->vocs_connections_max_num){
                        gatt_event_included_service_query_result_get_service(packet, &service);
                        vocs_client_connection_t * vocs_connection = &connection->vocs_connections_storage[connection->vocs_connections_num];

                        vocs_connection->basic_connection.service_index = connection->aics_connections_num;
                        vocs_connection->basic_connection.service_uuid16 = service.uuid16;
                        vocs_connection->basic_connection.start_handle = service.start_group_handle;
                        vocs_connection->basic_connection.end_handle = service.end_group_handle;
                        connection->vocs_connections_num++;
                    } else {
                        log_info("Num included VOCS services exceeded storage capacity, max num %d", connection->aics_connections_max_num);
                    }
                    break;
                default:
                    break;
            }

            break;

        case GATT_EVENT_QUERY_COMPLETE:
            con_handle = (hci_con_handle_t)gatt_event_query_complete_get_handle(packet);
            connection = (vcs_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&vcs_client, con_handle);
            btstack_assert(connection != NULL);

            status = gatt_event_query_complete_get_att_status(packet);
            switch (connection->state){
                case VOLUME_CONTROL_SERVICE_CLIENT_STATE_CHANGE_COUNTER_RESULT_READ_FAILED:
                    vcs_client_connected(connection, ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE);
                    break;

                case VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_CHANGE_COUNTER_RESULT:
                    connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_INCLUDED_SERVICES;
                    (void) gatt_client_request_to_send_gatt_query(&vcs_client_handle_can_send_now,
                                                                  connection->basic_connection.con_handle);
                    break;

                case VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT:
                    connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_READY;
                    vcs_client_emit_done_event(connection, connection->characteristic_index, status);
                    break;

                case VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_INCLUDED_SERVICES:
#ifdef ENABLE_TESTING_SUPPORT
                    printf("\nVCS Client: Trigger included services query\n");
#endif

                    // only look for included services if we can use them
                    status = ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
                    connection->aics_connections_index = 0;
                    connection->aics_connections_connected = 0;

                    if ((connection->aics_connections_max_num > 0) && (connection->aics_connections_storage != NULL)){
                        status = audio_input_control_service_client_ready_to_connect(
                                connection->basic_connection.con_handle,
                                &vcs_client_packet_handler_internal,
                                &connection->aics_connections_storage[0]
                        );
                    }
                    if (status == ERROR_CODE_SUCCESS) {
                        vcs_client_handle_can_send_now.context = (void *) (uintptr_t) connection->basic_connection.con_handle;
                        (void) gatt_client_request_to_send_gatt_query(&vcs_client_handle_can_send_now,
                                                                      connection->basic_connection.con_handle);
                    } else {
                        // cannot connect to included AICS services, check VOCS services
                        connection->vocs_connections_index = 0;
                        connection->vocs_connections_connected = 0;

                        if ((connection->vocs_connections_max_num > 0) && (connection->vocs_connections_storage != NULL)){
                            status = volume_offset_control_service_client_ready_to_connect(
                                    connection->basic_connection.con_handle,
                                    &vcs_client_packet_handler_internal,
                                    &connection->vocs_connections_storage[0]
                            );
                        }
                        if (status == ERROR_CODE_SUCCESS) {
                            vcs_client_handle_can_send_now.context = (void *) (uintptr_t) connection->basic_connection.con_handle;
                            (void) gatt_client_request_to_send_gatt_query(&vcs_client_handle_can_send_now,
                                                                          connection->basic_connection.con_handle);
                        } else {
                            // cannot connect to included VOCS services,
                            vcs_client_connected(connection, ERROR_CODE_SUCCESS);
                        }
                    }
                    break;

                case VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICES_RESULT:
                    if (connection->aics_connections_num != 0) {
                        connection->aics_connections_index = 0;

                        connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_AICS_SERVICES_CONNECTED;

                        (void) audio_input_control_service_client_connect(
                                connection->basic_connection.con_handle,
                                &vcs_client_packet_handler_internal,
                                connection->aics_connections_storage[connection->aics_connections_index].basic_connection.start_handle,
                                connection->aics_connections_storage[connection->aics_connections_index].basic_connection.end_handle,
                                connection->aics_connections_storage[connection->aics_connections_index].basic_connection.service_index,
                                &connection->aics_connections_storage[connection->aics_connections_index]);
                        break;
                    }

                    if (connection->vocs_connections_num != 0) {
                        volume_offset_control_service_client_init();
                        connection->vocs_connections_index = 0;
                        connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_VOCS_SERVICES_CONNECTED;

                        (void) volume_offset_control_service_client_connect(
                                connection->basic_connection.con_handle,
                                &vcs_client_packet_handler_internal,
                                connection->vocs_connections_storage[connection->vocs_connections_index].basic_connection.start_handle,
                                connection->vocs_connections_storage[connection->vocs_connections_index].basic_connection.end_handle,
                                connection->vocs_connections_storage[connection->vocs_connections_index].basic_connection.service_index,
                                &connection->vocs_connections_storage[connection->vocs_connections_index]);

                        break;
                    }
                    vcs_client_connected(connection, ERROR_CODE_SUCCESS);
                    break;

                default:
                    connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_READY;
                    break;
            }
            break;

        default:
            break;
    }
}

static uint16_t vcs_client_serialize_characteristic_value_for_write(vcs_client_connection_t * connection, uint8_t ** out_value){
    uint8_t value_length = 0;
    switch (connection->characteristic_index){
        case VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_CONTROL_POINT:
            switch ((aics_opcode_t)connection->data.data_bytes[0]){
                case VCS_OPCODE_SET_ABSOLUTE_VOLUME:
                    value_length = 3;
                    break;
                default:
                    value_length = 2;
                    break;
            }
            *out_value = (uint8_t *) connection->data.data_bytes;
            break;

        default:
            btstack_assert(false);
            break;
    }
    return value_length;
}

static void vcs_client_run_for_connection(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t)context;
    gatt_service_client_connection_t * connection_helper = gatt_service_client_get_connection_for_con_handle(&vcs_client, con_handle);
    vcs_client_connection_t * connection = (vcs_client_connection_t *)connection_helper;

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;
    gatt_client_service_t service;

    switch (connection->state){
        case VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHANGE_COUNTER:
            connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_CHANGE_COUNTER_RESULT;
            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                    &vcs_client_handle_gatt_client_event, con_handle,
                    gatt_service_client_helper_value_handle_for_index(connection_helper, connection->characteristic_index));
            break;

        case VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_INCLUDED_SERVICES:
            connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICES_RESULT;
            service.start_group_handle = connection->basic_connection.start_handle;
            service.end_group_handle = connection->basic_connection.end_handle;
#ifdef ENABLE_TESTING_SUPPORT
            printf("\nVCS Client: Query included services\n");
#endif
            (void) gatt_client_find_included_services_for_service(
                    vcs_client_handle_gatt_client_event,
                    con_handle,
                    &service);
            break;

        case VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &vcs_client_handle_gatt_client_event, con_handle,
                gatt_service_client_helper_value_handle_for_index(connection_helper, connection->characteristic_index));
            break;

        case VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;

            value_length = vcs_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_value_of_characteristic(
                    &vcs_client_handle_gatt_client_event, con_handle,
                    gatt_service_client_helper_value_handle_for_index(connection_helper, connection->characteristic_index),
                value_length, value);
            
            break;

        default:
            break;
    }
}

static void vcs_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_hci_event_handler(&vcs_client, packet_type, channel, packet, size);
}

void volume_control_service_client_init(void){
    gatt_service_client_init(&vcs_client, &vcs_client_packet_handler_trampoline);
    gatt_service_client_register_packet_handler(&vcs_client, &vcs_client_packet_handler_internal);

    vcs_client.characteristics_desc16_num = sizeof(vcs_uuid16s)/sizeof(uint16_t);
    vcs_client.characteristics_desc16 = vcs_uuid16s;

    vcs_client_handle_can_send_now.callback = &vcs_client_run_for_connection;
}

uint8_t volume_control_service_client_connect(hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    vcs_client_connection_t * vcs_connection,
    aics_client_connection_t * aics_connections, uint8_t aics_connections_num,
    vocs_client_connection_t * vocs_connections, uint8_t vocs_connections_num,
    uint16_t * out_vcs_cid
){

    btstack_assert(packet_handler != NULL);
    btstack_assert(vcs_connection != NULL);

    vcs_connection->events_packet_handler = packet_handler;

    vcs_connection->aics_connections_max_num = 0;
    vcs_connection->aics_connections_storage = NULL;
    vcs_connection->aics_connections_num = 0;

    if (aics_connections_num > 0){
        btstack_assert(aics_connections != NULL);
        vcs_connection->aics_connections_storage = aics_connections;
        vcs_connection->aics_connections_max_num = aics_connections_num;
    }

    vcs_connection->vocs_connections_max_num = 0;
    vcs_connection->vocs_connections_num = 0;
    vcs_connection->vocs_connections_storage = NULL;
    
    if (vocs_connections_num > 0){
        btstack_assert(vocs_connections != NULL);
        vcs_connection->vocs_connections_storage = vocs_connections;
        vcs_connection->vocs_connections_max_num = vocs_connections_num;
    }

    vcs_connection->state = VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_CONNECTION;

    return gatt_service_client_connect(con_handle,
                                       &vcs_client, &vcs_connection->basic_connection,
                                       ORG_BLUETOOTH_SERVICE_VOLUME_CONTROL, 0,
                                       vcs_connection->characteristics_storage, VOLUME_CONTROL_SERVICE_NUM_CHARACTERISTICS,
                                       packet_handler, out_vcs_cid);
}


uint8_t volume_control_service_client_read_volume_state(uint16_t vcs_cid){
    return vcs_client_request_read_characteristic(vcs_cid, VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_STATE);
}

uint8_t volume_control_service_client_read_volume_flags(uint16_t vcs_cid){
    return vcs_client_request_read_characteristic(vcs_cid, VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_FLAGS);
}

static uint8_t vcs_control_point_procedure_request(uint16_t vcs_cid, vcs_opcode_t opcode){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t status = vcs_client_can_query_characteristic(connection, VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_CONTROL_POINT);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    btstack_assert(connection != NULL);
    if (connection->state != VOLUME_CONTROL_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    connection->data.data_bytes[0] = (uint8_t)opcode;
    connection->data.data_bytes[1] = connection->change_counter;
    return vcs_client_request_write_characteristic(connection, VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_CONTROL_POINT);
}

uint8_t volume_control_service_client_mute(uint16_t vcs_cid){
    return vcs_control_point_procedure_request(vcs_cid, VCS_OPCODE_MUTE);
}

uint8_t volume_control_service_client_unmute(uint16_t vcs_cid){
    return vcs_control_point_procedure_request(vcs_cid, VCS_OPCODE_UNMUTE);
}

uint8_t volume_control_service_client_relative_volume_up(uint16_t vcs_cid){
    return vcs_control_point_procedure_request(vcs_cid, VCS_OPCODE_RELATIVE_VOLUME_UP);
}
uint8_t volume_control_service_client_relative_volume_down(uint16_t vcs_cid){
    return vcs_control_point_procedure_request(vcs_cid, VCS_OPCODE_RELATIVE_VOLUME_DOWN);
}
uint8_t volume_control_service_client_unmute_relative_volume_up(uint16_t vcs_cid){
    return vcs_control_point_procedure_request(vcs_cid, VCS_OPCODE_UNMUTE_RELATIVE_VOLUME_UP);
}
uint8_t volume_control_service_client_unmute_relative_volume_down(uint16_t vcs_cid){
    return vcs_control_point_procedure_request(vcs_cid, VCS_OPCODE_UNMUTE_RELATIVE_VOLUME_DOWN);
}

uint8_t volume_control_service_client_set_absolute_volume(uint16_t vcs_cid, uint8_t abs_volume){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t status = vcs_client_can_query_characteristic(connection, VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_CONTROL_POINT);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    btstack_assert(connection != NULL);
    if (connection->state != VOLUME_CONTROL_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    connection->data.data_bytes[0] = (uint8_t)VCS_OPCODE_SET_ABSOLUTE_VOLUME;
    connection->data.data_bytes[1] = connection->change_counter;
    connection->data.data_bytes[2] = abs_volume;

    return vcs_client_request_write_characteristic(connection, VCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_CONTROL_POINT);
}


uint8_t volume_control_service_client_write_unmute(uint16_t vcs_cid, uint8_t aics_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_unmute(&connection->aics_connections_storage[aics_index]);
}

uint8_t volume_control_service_client_write_mute(uint16_t vcs_cid, uint8_t aics_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_mute(&connection->aics_connections_storage[aics_index]);
}

uint8_t volume_control_service_client_write_gain_setting(uint16_t vcs_cid, uint8_t aics_index, int8_t gain_setting){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_gain_setting(&connection->aics_connections_storage[aics_index],
                                                                 gain_setting);
}

uint8_t volume_control_service_client_write_manual_gain_mode(uint16_t vcs_cid, uint8_t aics_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_manual_gain_mode(&connection->aics_connections_storage[aics_index]);
}

uint8_t volume_control_service_client_write_automatic_gain_mode(uint16_t vcs_cid, uint8_t aics_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_automatic_gain_mode(&connection->aics_connections_storage[aics_index]);
}

uint8_t volume_control_service_client_write_input_description(uint16_t vcs_cid, uint8_t aics_index, const char * audio_input_description){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_write_input_description(&connection->aics_connections_storage[aics_index], audio_input_description);
}

uint8_t volume_control_service_client_read_input_description(uint16_t vcs_cid, uint8_t aics_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_read_input_description(&connection->aics_connections_storage[aics_index]);
}

uint8_t volume_control_service_client_read_input_state(uint16_t vcs_cid, uint8_t aics_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_read_input_state(&connection->aics_connections_storage[aics_index]);
}

uint8_t volume_control_service_client_read_gain_setting_properties(uint16_t vcs_cid, uint8_t aics_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_read_gain_setting_properties(&connection->aics_connections_storage[aics_index]);
}

uint8_t volume_control_service_client_read_input_type(uint16_t vcs_cid, uint8_t aics_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_read_input_type(&connection->aics_connections_storage[aics_index]);
}

uint8_t volume_control_service_client_read_input_status(uint16_t vcs_cid, uint8_t aics_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (aics_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return audio_input_control_service_client_read_input_status(&connection->aics_connections_storage[aics_index]);
}

uint8_t volume_control_service_client_write_volume_offset(uint16_t vcs_cid, uint8_t vocs_index, int16_t volume_offset){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (vocs_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return volume_offset_control_service_client_write_volume_offset(&connection->vocs_connections_storage[vocs_index], volume_offset);
}

uint8_t volume_control_service_client_write_audio_location(uint16_t vcs_cid, uint8_t vocs_index, uint32_t audio_location){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (vocs_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return volume_offset_control_service_client_write_audio_location(&connection->vocs_connections_storage[vocs_index], audio_location);
}

uint8_t volume_control_service_client_read_offset_state(uint16_t vcs_cid, uint8_t vocs_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (vocs_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return volume_offset_control_service_client_read_offset_state(&connection->vocs_connections_storage[vocs_index]);
}

uint8_t volume_control_service_client_read_audio_location(uint16_t vcs_cid, uint8_t vocs_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (vocs_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return volume_offset_control_service_client_read_audio_location(&connection->vocs_connections_storage[vocs_index]);
}

uint8_t volume_control_service_client_read_audio_output_description(uint16_t vcs_cid, uint8_t vocs_index){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (vocs_index >= connection->aics_connections_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return volume_offset_control_service_client_read_audio_output_description(&connection->vocs_connections_storage[vocs_index]);
}


uint8_t volume_control_service_client_disconnect(uint16_t vcs_cid){
    vcs_client_connection_t * connection = (vcs_client_connection_t *) gatt_service_client_get_connection_for_cid(&vcs_client, vcs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t i;
    for (i = 0; i < connection->aics_connections_num; i++){
        audio_input_control_service_client_disconnect(&connection->aics_connections_storage[i]);
    }
    for (i = 0; i < connection->vocs_connections_num; i++){
        volume_offset_control_service_client_disconnect(&connection->vocs_connections_storage[i]);
    }
    return gatt_service_client_disconnect(&vcs_client, vcs_cid);
}

void volume_control_service_client_deinit(void){
    gatt_service_client_deinit(&vcs_client);
}

