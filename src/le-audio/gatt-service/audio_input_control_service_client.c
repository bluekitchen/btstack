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

static gatt_service_client_t aics_client;

static void aics_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void aics_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void aics_client_run_for_connection(void * context);

// list of uuids
static const uint16_t aics_uuid16s[AUDIO_INPUT_CONTROL_SERVICE_NUM_CHARACTERISTICS] = {
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
            return gatt_service_client_characteristic_uuid16_for_index(&aics_client, i);
        }
    }
    return 0;
}

static void aics_client_emit_string_value(gatt_service_client_connection_t *connection_helper, uint8_t subevent,
                                          const uint8_t *data, uint8_t att_status) {
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint8_t event[AICS_MAX_AUDIO_INPUT_DESCRIPTION_LENGTH + 7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    pos++;                      // reserve event[1] for subevent size 
    event[pos++] = subevent;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;

    pos++;                      // reserve event[5] for value size
    uint16_t data_length = btstack_strcpy((char *)&event[pos], sizeof(event) - pos, (const char *)data);
    pos += data_length;
    event[5] = data_length;     // store value size
    event[pos++] = att_status;

    event[1] = pos - 2;         // store subevent size
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void aics_client_emit_connection_established(gatt_service_client_connection_t * connection_helper, uint8_t status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint8_t event[9];
    int pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_AICS_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, connection_helper->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos += 2;
    event[pos++] = 0; // num included services
    event[pos++] = status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void aics_client_finalize_connection(aics_client_connection_t * connection){
    // already finalized by GATT CLIENT HELPER
    if (connection == NULL){
        return;
    }
    gatt_service_client_finalize_connection(&aics_client, &connection->basic_connection);
}

static void aics_client_connected(aics_client_connection_t * connection, uint8_t status) {
    if (status == ERROR_CODE_SUCCESS){
        connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY;
        aics_client_emit_connection_established(&connection->basic_connection, status);
    } else {
        connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_IDLE;
        aics_client_emit_connection_established(&connection->basic_connection, status);
        aics_client_finalize_connection(connection);
    }
}

static void aics_client_emit_uint8_array(gatt_service_client_connection_t * connection_helper, uint8_t subevent, const uint8_t * data, uint8_t data_size, uint8_t att_status){
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

static void aics_client_emit_done_event(gatt_service_client_connection_t * connection_helper, uint8_t index, uint8_t att_status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_index(&aics_client, index);

    uint8_t event[9];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_AICS_CLIENT_WRITE_DONE;

    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    event[pos++] = connection_helper->service_index;
    little_endian_store_16(event, pos, characteristic_uuid16);
    pos+= 2;
    event[pos++] = att_status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void aics_client_emit_read_event(gatt_service_client_connection_t * connection_helper, uint8_t characteristic_index, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    uint8_t subevent_id;
    uint16_t expected_data_size;
    uint8_t null_data[4];
    memset(null_data, 0, sizeof(null_data));

    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_index(&aics_client,
                                                                                         characteristic_index);
    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_DESCRIPTION:
            subevent_id = LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_DESCRIPTION;
            if (att_status == ATT_ERROR_SUCCESS){
                aics_client_emit_string_value(connection_helper, subevent_id, data, ATT_ERROR_SUCCESS);
                return;
            }
            break;

        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATE:
            subevent_id = LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_INPUT_STATE;
            expected_data_size = 4;
            // UPDATE change_counter
            if (data_size == expected_data_size){
                ((aics_client_connection_t *)connection_helper)->change_counter = data[3];
            }
            break;

        case ORG_BLUETOOTH_CHARACTERISTIC_GAIN_SETTINGS_ATTRIBUTE:
            subevent_id = LEAUDIO_SUBEVENT_AICS_CLIENT_GAIN_SETTINGS_PROPERTIES;
            expected_data_size = 3;
            break;

        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_TYPE:
            subevent_id = LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_INPUT_TYPE;
            expected_data_size = 1;
            break;

        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATUS:
            subevent_id = LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_INPUT_STATUS;
            expected_data_size = 1;
            break;

        default:
            btstack_assert(false);
            return;
    }

    if (att_status != ATT_ERROR_SUCCESS){
        aics_client_emit_uint8_array(connection_helper,subevent_id, null_data, 0, att_status);
        return;
    }
    if (data_size != expected_data_size){
        aics_client_emit_uint8_array(connection_helper,subevent_id, null_data, 0, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
    } else {
        aics_client_emit_uint8_array(connection_helper,subevent_id, data, expected_data_size, ERROR_CODE_SUCCESS);
    }
}

static void aics_client_emit_notify_event(gatt_service_client_connection_t * connection_helper, uint16_t value_handle, const uint8_t * data, uint16_t data_size){
    uint8_t subevent_id;
    uint16_t expected_data_size;
    uint8_t null_data[4];
    memset(null_data, 0, sizeof(null_data));

    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_value_handle2uuid16((aics_client_connection_t *)connection_helper, value_handle);
    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_DESCRIPTION:
            aics_client_emit_string_value(connection_helper, LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_DESCRIPTION, NULL,
                                          ATT_ERROR_SUCCESS);
            return;

        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATE:
            subevent_id = LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_INPUT_STATE;
            expected_data_size = 4;
            // UPDATE change_counter
            if (data_size == expected_data_size){
                ((aics_client_connection_t *)connection_helper)->change_counter = data[3];
            }
            break;

        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATUS:
            subevent_id = LEAUDIO_SUBEVENT_AICS_CLIENT_AUDIO_INPUT_STATUS;
            expected_data_size = 1;
            break;

        default:
            btstack_assert(false);
            break;
    }

    if (data_size != expected_data_size){
        aics_client_emit_uint8_array(connection_helper,subevent_id, null_data, 0, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
    } else {
        aics_client_emit_uint8_array(connection_helper,subevent_id, data, expected_data_size, ERROR_CODE_SUCCESS);
    }
}

static uint8_t aics_client_request_send_gatt_query(aics_client_connection_t * connection, aics_client_characteristic_index_t characteristic_index){
    connection->characteristic_index = characteristic_index;
    connection->gatt_query_can_send_now.context = (void *)(uintptr_t) connection->basic_connection.cid;
    uint8_t status = gatt_client_request_to_send_gatt_query(&connection->gatt_query_can_send_now, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

static uint8_t aics_client_request_read_characteristic(aics_client_connection_t * connection, aics_client_characteristic_index_t characteristic_index){
    btstack_assert(connection != NULL);
    if (connection->state != AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
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

static uint8_t aics_client_request_write_characteristic(aics_client_connection_t * connection, aics_client_characteristic_index_t characteristic_index){
    connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    return aics_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t aics_control_point_procedure_request(aics_client_connection_t * connection, aics_opcode_t opcode){
    btstack_assert(connection != NULL);
    if (connection->state != AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    connection->data.data_bytes[0] = (uint8_t)opcode;
    connection->data.data_bytes[1] = connection->change_counter;
    return aics_client_request_write_characteristic(connection, AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_CONTROL_POINT);
}

uint8_t audio_input_control_service_client_write_unmute(aics_client_connection_t * connection){
    return aics_control_point_procedure_request(connection, AICS_OPCODE_UMUTE);
}

uint8_t audio_input_control_service_client_write_mute(aics_client_connection_t * connection){
    return aics_control_point_procedure_request(connection, AICS_OPCODE_MUTE);
}

uint8_t audio_input_control_service_client_write_manual_gain_mode(aics_client_connection_t * connection){
    return aics_control_point_procedure_request(connection, AICS_OPCODE_SET_MANUAL_GAIN_MODE);
}

uint8_t audio_input_control_service_client_write_automatic_gain_mode(aics_client_connection_t * connection){
    return aics_control_point_procedure_request(connection, AICS_OPCODE_SET_AUTOMATIC_GAIN_MODE);
}

uint8_t audio_input_control_service_client_write_gain_setting(aics_client_connection_t * connection, int8_t gain_setting){
    btstack_assert(connection != NULL);
    if (connection->state != AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    connection->data.data_bytes[0] = (uint8_t)AICS_OPCODE_SET_GAIN_SETTING;
    connection->data.data_bytes[1] = connection->change_counter;
    connection->data.data_bytes[2] = (uint8_t)gain_setting;
    return aics_client_request_write_characteristic(connection, AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_CONTROL_POINT);
}

uint8_t audio_input_control_service_client_write_input_description(aics_client_connection_t * connection, const char * audio_input_description){
    btstack_assert(connection != NULL);
    if (connection->state != AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    if (audio_input_description == NULL){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    connection->data.data_string = audio_input_description;
    return aics_client_request_write_characteristic_without_response(connection, AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_DESCRIPTION);
}

uint8_t audio_input_control_service_client_read_input_state(aics_client_connection_t * connection){
    return aics_client_request_read_characteristic(connection, AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_STATE);
}

uint8_t audio_input_control_service_client_read_gain_setting_properties(aics_client_connection_t * connection){
    return aics_client_request_read_characteristic(connection, AICS_CLIENT_CHARACTERISTIC_INDEX_GAIN_SETTINGS_ATTRIBUTE);
}

uint8_t audio_input_control_service_client_read_input_type(aics_client_connection_t * connection){
    return aics_client_request_read_characteristic(connection, AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_TYPE);
}

uint8_t audio_input_control_service_client_read_input_status(aics_client_connection_t * connection){
    return aics_client_request_read_characteristic(connection, AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_STATUS);
}

uint8_t audio_input_control_service_client_read_input_description(aics_client_connection_t * connection){
    return aics_client_request_read_characteristic(connection, AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_DESCRIPTION);
}

static void aics_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    gatt_service_client_connection_t * connection_helper;
    aics_client_connection_t * connection;
    uint16_t connection_id;
    uint8_t status;
    uint16_t value_handle;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&aics_client, gattservice_subevent_client_connected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);
                    connection = (aics_client_connection_t *)connection_helper;

                    status = gattservice_subevent_client_connected_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS){
                        aics_client_connected(connection, status);
                        break;
                    }

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_STATE; i < AICS_CLIENT_CHARACTERISTIC_INDEX_RFU; i++){
                            printf("    0x%04X %s\n", connection_helper->characteristics[i].value_handle, aics_client_characteristic_name[i]);

                        }
                    };
                    printf("AICS Client: Query input state to retrieve and cache change counter\n");
#endif
                    connection = (aics_client_connection_t *) connection_helper;
                    if (connection->basic_connection.characteristics[AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_STATE].value_handle == 0){
                        aics_client_connected(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                        break;
                    }

                    connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHANGE_COUNTER;
                    aics_client_request_send_gatt_query(connection, AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_STATE);
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&aics_client, gattservice_subevent_client_disconnected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);
                    aics_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_AICS_CLIENT_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;

        case GATT_EVENT_NOTIFICATION:
            connection_id = gatt_event_notification_get_connection_id(packet);
            value_handle = gatt_event_notification_get_value_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_cid(&aics_client, connection_id);
            btstack_assert(connection_helper != NULL);

            aics_client_emit_notify_event(connection_helper, value_handle, gatt_event_notification_get_value(packet),gatt_event_notification_get_value_length(packet));
            break;
        default:
            break;
    }
}

static void aics_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    gatt_service_client_connection_t * connection_helper = NULL;
    uint16_t connection_id;
    aics_client_connection_t * connection;
    uint8_t status; 

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            connection_id = (uint16_t)gatt_event_characteristic_value_query_result_get_connection_id(packet);
            connection_helper = gatt_service_client_get_connection_for_cid(&aics_client, connection_id);

            btstack_assert(connection_helper != NULL);

            connection = (aics_client_connection_t *)connection_helper;
            switch (connection->state){
                case AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W4_CHANGE_COUNTER_RESULT:
                    btstack_assert(connection->characteristic_index == AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_STATE);
                    if (gatt_event_characteristic_value_query_result_get_value_length(packet) != 4) {
#ifdef ENABLE_TESTING_SUPPORT
                        printf("AICS Client: connection failed\n");
#endif
                        // wait for GATT_EVENT_QUERY_COMPLETE to call aics_client_emit_connected 
                        connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_CHANGE_COUNTER_READ_FAILED;
                        break;
                    }

                    connection->change_counter = gatt_event_characteristic_value_query_result_get_value(packet)[3];
                    break;

                case AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT:
                    connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY;
                    aics_client_emit_read_event(connection_helper, connection->characteristic_index, ATT_ERROR_SUCCESS,
                                                gatt_event_characteristic_value_query_result_get_value(packet),
                                                gatt_event_characteristic_value_query_result_get_value_length(packet));
                    break;

                default:
                    btstack_assert(false);
                    break;
            }


            break;

        case GATT_EVENT_QUERY_COMPLETE:
            connection_id = (uint16_t)gatt_event_query_complete_get_connection_id(packet);
            connection_helper = gatt_service_client_get_connection_for_cid(&aics_client, connection_id);
            btstack_assert(connection_helper != NULL);
            connection = (aics_client_connection_t *)connection_helper;

            status = gatt_event_query_complete_get_att_status(packet);
            switch (connection->state){
                case AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT:
                    connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY;
                    aics_client_emit_done_event(connection_helper, connection->characteristic_index, status);
                    break;

                case AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_CHANGE_COUNTER_READ_FAILED:
                    aics_client_connected(connection, ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE);
                    break;
                
                case AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W4_CHANGE_COUNTER_RESULT:
                    aics_client_connected(connection, gatt_service_client_att_status_to_error_code(status));
                    break;

                default:
                    connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY;
                    break;
            }
            break;

        default:
            break;
    }
}

static uint16_t aics_client_serialize_characteristic_value_for_write(aics_client_connection_t * connection, uint8_t ** out_value){
    uint8_t value_length = 0;
     switch (connection->characteristic_index){
        case AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_DESCRIPTION:
            *out_value = (uint8_t *) connection->data.data_string;
            value_length = strlen(connection->data.data_string);
            break;

        case AICS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_INPUT_CONTROL_POINT:
            switch ((aics_opcode_t)connection->data.data_bytes[0]){
                case AICS_OPCODE_SET_GAIN_SETTING:
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

static void aics_client_run_for_connection(void * context){
    uint16_t connection_id = (uint16_t)(uintptr_t)context;
    aics_client_connection_t * connection = (aics_client_connection_t *)gatt_service_client_get_connection_for_cid(&aics_client, connection_id);

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;

    switch (connection->state){
        case AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHANGE_COUNTER:
            connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W4_CHANGE_COUNTER_RESULT;
            (void) gatt_client_read_value_of_characteristic_using_value_handle_with_context(
                    &aics_client_handle_gatt_client_event, connection->basic_connection.con_handle,
                    aics_client_value_handle_for_index(connection), aics_client.service_id, connection->basic_connection.cid);
            break;

        case AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle_with_context(
                &aics_client_handle_gatt_client_event, connection->basic_connection.con_handle,
                aics_client_value_handle_for_index(connection), aics_client.service_id, connection->basic_connection.cid);
            break;

        case AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE:
            connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_READY;

            value_length = aics_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_value_of_characteristic_without_response(
                    connection->basic_connection.con_handle,
                aics_client_value_handle_for_index(connection),
                value_length, value);
            
            break;
        case AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;

            value_length = aics_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_value_of_characteristic_with_context(
                    &aics_client_handle_gatt_client_event, connection->basic_connection.con_handle,
                    aics_client_value_handle_for_index(connection),
                    value_length, value, aics_client.service_id, connection->basic_connection.cid);
            break;
        default:
            break;
    }
}

static void aics_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_trampoline_packet_handler(&aics_client, packet_type, channel, packet, size);
}

void audio_input_control_service_client_init(void){
    gatt_service_client_register_client(&aics_client, &aics_client_packet_handler_internal);

    aics_client.characteristics_desc16_num = sizeof(aics_uuid16s)/sizeof(uint16_t);
    aics_client.characteristics_desc16 = aics_uuid16s;
}

uint8_t audio_input_control_service_client_connect(
    hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    uint16_t service_start_handle, 
    uint16_t service_end_handle, 
    uint8_t service_index, 
    aics_client_connection_t * connection){

    connection->gatt_query_can_send_now.callback = &aics_client_run_for_connection;
    connection->change_counter = 0;
    connection->state = AUDIO_INPUT_CONTROL_SERVICE_CLIENT_STATE_W4_CONNECTED;

    return gatt_service_client_connect_secondary_service(con_handle,
        &aics_client, &connection->basic_connection,
        ORG_BLUETOOTH_SERVICE_AUDIO_INPUT_CONTROL, service_start_handle, service_end_handle, service_index,
        connection->characteristics_storage, AUDIO_INPUT_CONTROL_SERVICE_NUM_CHARACTERISTICS, packet_handler);
}

uint8_t audio_input_control_service_client_disconnect(aics_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_SUCCESS;
    }
    return gatt_service_client_disconnect(&aics_client, connection->basic_connection.cid);
}

void audio_input_control_service_client_deinit(void){
    gatt_service_client_unregister_client(&aics_client);
}

