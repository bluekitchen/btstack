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

#define BTSTACK_FILE__ "volume_offset_control_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "le-audio/gatt-service/volume_offset_control_service_util.h"
#include "le-audio/gatt-service/volume_offset_control_service_client.h"
#include "ble/gatt_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "le-audio/le_audio_util.h"

static gatt_service_client_t vocs_client;

static void vocs_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void vocs_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void vocs_client_run_for_connection(void * context);

// list of uuids
static const uint16_t vocs_uuid16s[VOLUME_OFFSET_CONTROL_SERVICE_NUM_CHARACTERISTICS] = {
    ORG_BLUETOOTH_CHARACTERISTIC_OFFSET_STATE,
    ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_LOCATION,
    ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_OFFSET_CONTROL_POINT,
    ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_OUTPUT_DESCRIPTION
};

typedef enum {
    VOCS_CLIENT_CHARACTERISTIC_INDEX_OFFSET_STATE = 0,
    VOCS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_LOCATION,
    VOCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_OFFSET_CONTROL_POINT,
    VOCS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_OUTPUT_DESCRIPTION,
    VOCS_CLIENT_CHARACTERISTIC_INDEX_RFU
} vocs_client_characteristic_index_t;

#ifdef ENABLE_TESTING_SUPPORT
static char * vocs_client_characteristic_name[] = {
    "OFFSET_STATE",
    "AUDIO_LOCATION",
    "VOLUME_OFFSET_CONTROL_POINT",
    "AUDIO_OUTPUT_DESCRIPTION",
    "RFU"
};
#endif

static void vocs_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static uint16_t vocs_client_value_handle_for_index(vocs_client_connection_t * connection){
    return connection->basic_connection.characteristics[connection->characteristic_index].value_handle;
}

static void vocs_client_emit_string_value(gatt_service_client_connection_t * connection_helper, uint8_t subevent, const uint8_t * data, uint16_t data_size, uint8_t att_status){
    UNUSED( data_size );

    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);
    
    uint8_t event[VOCS_MAX_AUDIO_OUTPUT_DESCRIPTION_LENGTH + 7];
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

static void vocs_client_emit_connection_established(gatt_service_client_connection_t * connection_helper, uint8_t status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint8_t event[9];
    int pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_VOCS_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, connection_helper->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos += 2;
    event[pos++] = 0; // num included services
    event[pos++] = status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void vocs_client_finalize_connection(vocs_client_connection_t * connection){
    // already finalized by GATT CLIENT HELPER
    if (connection == NULL){
        return;
    }
    gatt_service_client_finalize_connection(&vocs_client, &connection->basic_connection);
}

static void vocs_client_connected(vocs_client_connection_t * connection, uint8_t status) {
    if (status == ERROR_CODE_SUCCESS){
        connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_READY;
        vocs_client_emit_connection_established(&connection->basic_connection, status);
    } else {
        connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_IDLE;
        vocs_client_emit_connection_established(&connection->basic_connection, status);
        vocs_client_finalize_connection(connection);
    }
}

static void vocs_client_emit_uint8_array(gatt_service_client_connection_t * connection_helper, uint8_t subevent, const uint8_t * data, uint8_t data_size, uint8_t att_status){
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

static void vocs_client_emit_done_event(gatt_service_client_connection_t * connection_helper, uint8_t index, uint8_t att_status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&vocs_client, index);

    uint8_t event[9];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_VCS_CLIENT_WRITE_DONE;

    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    event[pos++] = connection_helper->service_index;
    little_endian_store_16(event, pos, characteristic_uuid16);
    pos+= 2;
    event[pos++] = att_status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void vocs_client_emit_read_event(gatt_service_client_connection_t * connection_helper, uint8_t characteristic_index, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    uint8_t subevent_id;
    uint16_t expected_data_size;
    uint8_t null_data[4];
    memset(null_data, 0, sizeof(null_data));

    switch (characteristic_index){
        case VOCS_CLIENT_CHARACTERISTIC_INDEX_OFFSET_STATE:
            subevent_id = LEAUDIO_SUBEVENT_VOCS_CLIENT_OFFSET_STATE;
            expected_data_size = 3;
            // UPDATE change_counter
            if (data_size == expected_data_size){
                ((vocs_client_connection_t *)connection_helper)->change_counter = data[2];
            }
            break;
        
        case VOCS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_LOCATION:
            subevent_id = LEAUDIO_SUBEVENT_VOCS_CLIENT_AUDIO_LOCATION;
            expected_data_size = 4;
            break;

        case VOCS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_OUTPUT_DESCRIPTION:
            subevent_id = LEAUDIO_SUBEVENT_VOCS_CLIENT_AUDIO_OUTPUT_DESCRIPTION;
            if (att_status == ATT_ERROR_SUCCESS){
                vocs_client_emit_string_value(connection_helper, subevent_id, data, data_size, att_status);
                return;
            }
            break;

        default:
            btstack_assert(false);
            break;
    }

    if (att_status != ATT_ERROR_SUCCESS){
        vocs_client_emit_uint8_array(connection_helper, subevent_id, null_data, 0, att_status);
        return;
    }
    if (data_size != expected_data_size){
        vocs_client_emit_uint8_array(connection_helper, subevent_id, null_data, 0, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
    } else {
        vocs_client_emit_uint8_array(connection_helper, subevent_id, data, expected_data_size, ERROR_CODE_SUCCESS);
    }
}

static void vocs_client_emit_notify_event(gatt_service_client_connection_t * connection_helper, uint16_t value_handle, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    uint8_t subevent_id;
    uint16_t expected_data_size;
    uint8_t null_data[4];
    memset(null_data, 0, sizeof(null_data));

    uint16_t characteristic_uuid16 = gatt_service_client_helper_characteristic_uuid16_for_value_handle(&vocs_client, connection_helper, value_handle);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_OFFSET_STATE:
            subevent_id = LEAUDIO_SUBEVENT_VOCS_CLIENT_OFFSET_STATE;
            expected_data_size = 3;
            // UPDATE change_counter
            if (data_size == expected_data_size){
                ((vocs_client_connection_t *)connection_helper)->change_counter = data[2];
            }
            break;
        
        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_LOCATION:
            subevent_id = LEAUDIO_SUBEVENT_VOCS_CLIENT_AUDIO_LOCATION;
            expected_data_size = 4;
            break;

        case ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_OUTPUT_DESCRIPTION:
            subevent_id = LEAUDIO_SUBEVENT_VOCS_CLIENT_AUDIO_OUTPUT_DESCRIPTION;
            if (att_status == ATT_ERROR_SUCCESS){
                vocs_client_emit_string_value(connection_helper, subevent_id, data, data_size, att_status);
                return;
            }
            break;

        default:
            btstack_assert(false);
            break;
    }

    if (att_status != ATT_ERROR_SUCCESS){
        vocs_client_emit_uint8_array(connection_helper,subevent_id, null_data, 0, att_status);
        return;
    }
    if (data_size != expected_data_size){
        vocs_client_emit_uint8_array(connection_helper,subevent_id, null_data, 0, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
    } else {
        vocs_client_emit_uint8_array(connection_helper,subevent_id, data, expected_data_size, ERROR_CODE_SUCCESS);
    }
}

static uint8_t vocs_client_request_send_gatt_query(vocs_client_connection_t * connection, vocs_client_characteristic_index_t characteristic_index){
    connection->characteristic_index = characteristic_index;

    uint8_t status = gatt_client_request_to_send_gatt_query(&connection->gatt_query_can_send_now, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

static uint8_t vocs_client_request_read_characteristic(vocs_client_connection_t * connection, vocs_client_characteristic_index_t characteristic_index){
    btstack_assert(connection != NULL);
    if (connection->state != VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    return vocs_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t vocs_client_request_write_characteristic_without_response(vocs_client_connection_t * connection, vocs_client_characteristic_index_t characteristic_index){
    connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE;
    return vocs_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t vocs_client_request_write_characteristic(vocs_client_connection_t * connection, vocs_client_characteristic_index_t characteristic_index){
    connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    return vocs_client_request_send_gatt_query(connection, characteristic_index);
}


uint8_t volume_offset_control_service_client_write_volume_offset(vocs_client_connection_t * connection, int16_t volume_offset){
    btstack_assert(connection != NULL);
    if (connection->state != VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    connection->data.data_bytes[0] = (uint8_t)VOCS_OPCODE_SET_VOLUME_OFFSET;
    connection->data.data_bytes[1] = connection->change_counter;
    little_endian_store_16(connection->data.data_bytes, 2, (uint16_t)volume_offset);

    return vocs_client_request_write_characteristic(connection, VOCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_OFFSET_CONTROL_POINT);
}

uint8_t volume_offset_control_service_client_write_audio_location(vocs_client_connection_t * connection, uint32_t audio_location){
    btstack_assert(connection != NULL);
    if (connection->state != VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    little_endian_store_32(connection->data.data_bytes, 0, audio_location);
    return vocs_client_request_write_characteristic_without_response(connection, VOCS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_LOCATION);
}

uint8_t volume_offset_control_service_client_read_offset_state(vocs_client_connection_t * connection){
    return vocs_client_request_read_characteristic(connection, VOCS_CLIENT_CHARACTERISTIC_INDEX_OFFSET_STATE);
}

uint8_t volume_offset_control_service_client_read_audio_location(vocs_client_connection_t * connection){
    return vocs_client_request_read_characteristic(connection, VOCS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_LOCATION);
}

uint8_t volume_offset_control_service_client_read_audio_output_description(vocs_client_connection_t * connection){
    return vocs_client_request_read_characteristic(connection, VOCS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_OUTPUT_DESCRIPTION);
}

static void vocs_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    gatt_service_client_connection_t * connection_helper;
    vocs_client_connection_t * connection;
    hci_con_handle_t con_handle;;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&vocs_client, gattservice_subevent_client_connected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);
                    connection = (vocs_client_connection_t *) connection_helper;

                    status = gattservice_subevent_client_connected_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS){
                        vocs_client_connected(connection, status);
                        break;
                    }

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = VOCS_CLIENT_CHARACTERISTIC_INDEX_OFFSET_STATE; i < VOCS_CLIENT_CHARACTERISTIC_INDEX_RFU; i++){
                            printf("    0x%04X %s\n", connection_helper->characteristics[i].value_handle, vocs_client_characteristic_name[i]);

                        }
                    };
                    printf("VOCS Client: Query input state to retrieve and cache change counter\n");
#endif
                    
                    if (connection->basic_connection.characteristics[VOCS_CLIENT_CHARACTERISTIC_INDEX_OFFSET_STATE].value_handle == 0){
                        vocs_client_connected(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                        break;
                    }

                    connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHANGE_COUNTER;
                    vocs_client_request_send_gatt_query(connection, VOCS_CLIENT_CHARACTERISTIC_INDEX_OFFSET_STATE);
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&vocs_client, gattservice_subevent_client_disconnected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);
                    vocs_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_VOCS_CLIENT_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;

        case GATT_EVENT_NOTIFICATION:
            con_handle = (hci_con_handle_t)gatt_event_notification_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&vocs_client, con_handle);

            btstack_assert(connection_helper != NULL);

            vocs_client_emit_notify_event(connection_helper, gatt_event_notification_get_value_handle(packet), ATT_ERROR_SUCCESS,
                                         gatt_event_notification_get_value(packet),gatt_event_notification_get_value_length(packet));
            break;
        default:
            break;
    }
}

static void vocs_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    gatt_service_client_connection_t * connection_helper = NULL;
    hci_con_handle_t con_handle;
    vocs_client_connection_t * connection;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_characteristic_value_query_result_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&vocs_client, con_handle);

            btstack_assert(connection_helper != NULL);

            connection = (vocs_client_connection_t *)connection_helper;

            switch (connection->state){
                case VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_CHANGE_COUNTER_RESULT:
                    btstack_assert(connection->characteristic_index == VOCS_CLIENT_CHARACTERISTIC_INDEX_OFFSET_STATE);
                    
                    if (gatt_event_characteristic_value_query_result_get_value_length(packet) != 3) {
                        connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_CHANGE_COUNTER_RESULT_READ_FAILED;
                        break;
                    }

                    connection->change_counter = gatt_event_characteristic_value_query_result_get_value(packet)[2];
                    break;

                case VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT:
                    connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_READY;
                    vocs_client_emit_read_event(connection_helper, connection->characteristic_index, ATT_ERROR_SUCCESS,
                                                gatt_event_characteristic_value_query_result_get_value(packet),
                                                gatt_event_characteristic_value_query_result_get_value_length(packet));
                    break;

                default:
                    btstack_assert(false);
                    break;
            }


            break;

        case GATT_EVENT_QUERY_COMPLETE:
            con_handle = (hci_con_handle_t)gatt_event_query_complete_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&vocs_client, con_handle);
            btstack_assert(connection_helper != NULL);
            status = gatt_event_query_complete_get_att_status(packet);

            connection = (vocs_client_connection_t *)connection_helper;
            switch (connection->state){
                case VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT:
                    vocs_client_emit_done_event(connection_helper, connection->characteristic_index, status);
                    break;

                case VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_CHANGE_COUNTER_RESULT_READ_FAILED:
                    vocs_client_connected(connection, ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE);
                    break;

                case VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_CHANGE_COUNTER_RESULT:
#ifdef ENABLE_TESTING_SUPPORT
                    printf("VOCS Client: connected, change counter initialized to %d\n", connection->change_counter);
#endif              
                    vocs_client_connected(connection, gatt_service_client_att_status_to_error_code(status));
                    break;
                default:
                    break;
            }
            connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_READY;
            break;

        default:
            break;
    }
}

static uint16_t vocs_client_serialize_characteristic_value_for_write(vocs_client_connection_t * connection, uint8_t ** out_value){
    uint8_t value_length = 0;
     switch (connection->characteristic_index){
        case VOCS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_OUTPUT_DESCRIPTION:
            *out_value = (uint8_t *) connection->data.data_string;
            value_length = strlen(connection->data.data_string);
            break;

        case VOCS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_LOCATION:
            *out_value = (uint8_t *) connection->data.data_bytes;
            value_length = 4;
            break;

        case VOCS_CLIENT_CHARACTERISTIC_INDEX_VOLUME_OFFSET_CONTROL_POINT:
            switch ((vocs_opcode_t)connection->data.data_bytes[0]){
                case VOCS_OPCODE_SET_VOLUME_OFFSET:
                    value_length = 4;
                    break;
                default:
                    btstack_assert(false);
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

static void vocs_client_run_for_connection(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t)context;
    vocs_client_connection_t * connection = (vocs_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&vocs_client, con_handle);

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;

    switch (connection->state){
        case VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHANGE_COUNTER:
            connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_CHANGE_COUNTER_RESULT;
            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                    &vocs_client_handle_gatt_client_event, con_handle,
                    vocs_client_value_handle_for_index(connection));
            break;

        case VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &vocs_client_handle_gatt_client_event, con_handle, 
                vocs_client_value_handle_for_index(connection));
            break;

        case VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE:
            connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_READY;

            value_length = vocs_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_value_of_characteristic_without_response(
                con_handle,
                vocs_client_value_handle_for_index(connection),
                value_length, value);
            
            break;
        case VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;

            value_length = vocs_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_value_of_characteristic(
                    &vocs_client_handle_gatt_client_event, con_handle,
                    vocs_client_value_handle_for_index(connection),
                    value_length, value);
            break;
        default:
            break;
    }
}

static void vocs_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_trampoline_packet_handler(&vocs_client, packet_type, channel, packet, size);
}

void volume_offset_control_service_client_init(void){
    gatt_service_client_init(&vocs_client, &vocs_client_packet_handler_trampoline);
    gatt_service_client_register_packet_handler(&vocs_client, &vocs_client_packet_handler_internal);

    vocs_client.characteristics_desc16_num = sizeof(vocs_uuid16s)/sizeof(uint16_t);
    vocs_client.characteristics_desc16 = vocs_uuid16s;
}

uint8_t volume_offset_control_service_client_ready_to_connect(
        hci_con_handle_t con_handle,
        btstack_packet_handler_t packet_handler,
        vocs_client_connection_t * connection){

    return gatt_service_client_connect_secondary_service_ready_to_connect(con_handle,
                                                                          &vocs_client, &connection->basic_connection,
                                                                          connection->characteristics_storage,
                                                                          VOLUME_OFFSET_CONTROL_SERVICE_NUM_CHARACTERISTICS,
                                                                          packet_handler);
}

uint8_t volume_offset_control_service_client_connect(
    hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    uint16_t service_start_handle, 
    uint16_t service_end_handle, 
    uint8_t service_index, 
    vocs_client_connection_t * connection){

    connection->gatt_query_can_send_now.callback = &vocs_client_run_for_connection;
    connection->gatt_query_can_send_now.context = (void *)(uintptr_t) con_handle;
    connection->change_counter = 0;
    connection->state = VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_CONNECTED;
    return gatt_service_client_connect_secondary_service(con_handle,
        &vocs_client, &connection->basic_connection,
        ORG_BLUETOOTH_SERVICE_VOLUME_OFFSET_CONTROL, service_start_handle, service_end_handle, service_index,
         connection->characteristics_storage, VOLUME_OFFSET_CONTROL_SERVICE_NUM_CHARACTERISTICS, packet_handler);
}

uint8_t volume_offset_control_service_client_disconnect(vocs_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_SUCCESS;
    }
    return gatt_service_client_disconnect(&vocs_client, connection->basic_connection.cid);
}

void volume_offset_control_service_client_deinit(void){
    gatt_service_client_deinit(&vocs_client);
}

