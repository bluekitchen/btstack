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

#define BTSTACK_FILE__ "object_transfer_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "le-audio/gatt-service/object_transfer_service_util.h"
#include "le-audio/gatt-service/object_transfer_service_client.h"

#include "btstack_memory.h"
#include "ble/core.h"
#include "ble/gatt_client.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"

#include "le-audio/gatt-service/object_transfer_service_util.h"
#include "le-audio/gatt-service/object_transfer_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "bluetooth_psm.h"

static gatt_service_client_helper_t ots_client;

static void ots_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void ots_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void ots_client_run_for_connection(void * context);

// list of uuids
static const uint16_t ots_uuid16s[OBJECT_TRANSFER_SERVICE_NUM_CHARACTERISTICS] = {
    ORG_BLUETOOTH_CHARACTERISTIC_OTS_FEATURE,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_NAME,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_TYPE,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_SIZE,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_FIRST_CREATED,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LAST_MODIFIED,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_PROPERTIES,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ACTION_CONTROL_POINT,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_CONTROL_POINT,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_FILTER,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_FILTER,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_FILTER,
    ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_CHANGED
};

typedef enum {
    OTS_CLIENT_CHARACTERISTIC_INDEX_OTS_FEATURE = 0,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_NAME,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_TYPE,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_SIZE,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_FIRST_CREATED,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LAST_MODIFIED,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_ID,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_PROPERTIES,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_ACTION_CONTROL_POINT,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_CONTROL_POINT,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_1,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_2,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_3,
    OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_CHANGED,
    OTS_CLIENT_CHARACTERISTIC_INDEX_INDEX_RFU
} ots_client_characteristic_index_t;

#ifdef ENABLE_TESTING_SUPPORT
static char * ots_client_characteristic_name[] = {
    "OTS_FEATURE",
    "OBJECT_NAME",
    "OBJECT_TYPE",
    "OBJECT_SIZE",
    "OBJECT_FIRST_CREATED",
    "OBJECT_LAST_MODIFIED",
    "OBJECT_ID",
    "OBJECT_PROPERTIES",
    "OBJECT_ACTION_CONTROL_POINT",
    "OBJECT_LIST_CONTROL_POINT",
    "OBJECT_LIST_FILTER_1",
    "OBJECT_LIST_FILTER_2",
    "OBJECT_LIST_FILTER_3",
    "OBJECT_CHANGED",
    "RFU"
};
#endif

static void ots_client_emit_timeout(gatt_service_client_connection_helper_t * connection_helper, uint16_t characteristic_uuid){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint8_t event[17];
    int pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_OTS_CLIENT_TIMEOUT;
    little_endian_store_16(event, pos, characteristic_uuid);
    pos += 2;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void ots_client_operations_timer_timeout_handler(btstack_timer_source_t * timer){
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t) btstack_run_loop_get_timer_context(timer);
    ots_client_connection_t * connection = (ots_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&ots_client, con_handle);

    if (connection == NULL){
        return;
    }
    ots_client_emit_timeout((gatt_service_client_connection_helper_t *) connection, gatt_service_client_characteristic_index2uuid16(&ots_client, connection->characteristic_index));
    l2cap_le_disconnect(connection->basic_connection.cid);
}

static void ots_client_operations_start_timer(ots_client_connection_t * connection){
    btstack_run_loop_set_timer_handler(&connection->operation_timer, ots_client_operations_timer_timeout_handler);
    btstack_run_loop_set_timer_context(&connection->operation_timer, (void *)(uintptr_t)connection->basic_connection.con_handle);

    btstack_run_loop_set_timer(&connection->operation_timer, 30000);
    btstack_run_loop_add_timer(&connection->operation_timer);
}

ots_client_connection_t *  ots_client_get_connection_for_cbm_local_cid(uint16_t cbm_local_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &ots_client.connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        ots_client_connection_t * connection = (ots_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->le_cbm_connection.cid != cbm_local_cid) continue;
        return connection;
    }
    return NULL;
}

static void ots_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static uint16_t ots_client_value_handle_for_index(ots_client_connection_t * connection){
    return connection->basic_connection.characteristics[connection->characteristic_index].value_handle;
}

static void ots_client_emit_connected(ots_client_connection_t * connection, uint8_t status){
    btstack_assert(connection != NULL);
    gatt_service_client_connection_helper_t * connection_helper = (gatt_service_client_connection_helper_t *) connection;
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint8_t event[17];
    int pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_OTS_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, connection_helper->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos += 2;
    event[pos++] = 0; // num included services
    little_endian_store_32(event, pos, connection->oacp_features);
    pos += 4;
    little_endian_store_32(event, pos, connection->olcp_features);
    pos += 4;
    event[pos++] = status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void ots_client_emit_done_event(gatt_service_client_connection_helper_t * connection_helper, uint8_t index, uint8_t att_status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_index2uuid16(&ots_client, index);

    uint8_t event[9];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_OTS_CLIENT_WRITE_DONE;

    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    event[pos++] = connection_helper->service_index;
    little_endian_store_16(event, pos, characteristic_uuid16);
    pos+= 2;
    event[pos++] = att_status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void ots_client_emit_features_event(gatt_service_client_connection_helper_t * connection_helper, uint8_t att_status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);
    ots_client_connection_t * connection = (ots_client_connection_t *)connection_helper;

    uint8_t event[14];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_OTS_CLIENT_FEATURES;

    little_endian_store_16(event, pos, connection_helper->cid);
    pos += 2;
    little_endian_store_32(event, pos, connection->oacp_features);
    pos += 4;
    little_endian_store_32(event, pos, connection->olcp_features);
    pos += 4;
    event[pos++] = att_status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void ots_client_emit_string_value(gatt_service_client_connection_helper_t * connection_helper, uint8_t subevent, const uint8_t * data, uint16_t data_size, uint8_t att_status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint8_t event[OTS_MAX_STRING_LENGHT + 7];
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

static void ots_client_emit_filter_value(gatt_service_client_connection_helper_t * connection_helper, uint8_t filter_index, ots_filter_type_t filter_type, const uint8_t * data, uint8_t data_size, uint8_t att_status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint8_t event[OTS_MAX_STRING_LENGHT + 9];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = 7 + data_size;
    event[pos++] = LEAUDIO_SUBEVENT_OTS_CLIENT_FILTER;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    event[pos++] = filter_index;
    event[pos++] = (uint8_t)filter_type;
    event[pos++] = data_size;
    memcpy(&event[pos], data, data_size);
    pos += data_size;
    event[pos++] = att_status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void ots_client_emit_variable_uint8_array(gatt_service_client_connection_helper_t * connection_helper, uint8_t subevent, const uint8_t * data, uint8_t data_size, uint8_t att_status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);
    btstack_assert(data_size <= 16);

    uint8_t event[23];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = 5 + data_size;
    event[pos++] = subevent;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    event[pos++] = data_size;
    memcpy(&event[pos], data, data_size);
    pos += data_size;
    event[pos++] = att_status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void ots_client_emit_uint8_array(gatt_service_client_connection_helper_t * connection_helper, uint8_t subevent, const uint8_t * data, uint8_t data_size, uint8_t att_status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);
    btstack_assert(data_size <= 8);

    uint8_t event[14];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = 5 + data_size;
    event[pos++] = subevent;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    memcpy(&event[pos], data, data_size);
    pos += data_size;
    event[pos++] = att_status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}


static void ots_client_emit_data_chunk(gatt_service_client_connection_helper_t * connection_helper, uint8_t state){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    ots_client_connection_t * connection = (ots_client_connection_t *) connection_helper;

    uint8_t event[18];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_OTS_CLIENT_DATA_CHUNK;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    event[pos++] = state;
    little_endian_store_32(event, pos, connection->cbm_data_chunk_length);
    pos += 4;
    little_endian_store_32(event, pos, connection->cbm_data_offset);
    pos += 4;
    little_endian_store_32(event, pos, connection->cbm_data_chunk_bytes_transferred);
    pos += 4;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static uint16_t ots_client_serialize_characteristic_value_for_write(ots_client_connection_t * connection, uint8_t ** out_value){
    uint8_t value_length = 0;

    switch (connection->characteristic_index){
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_NAME:
            value_length = connection->data_length;
            *out_value   = (uint8_t *)connection->data.data_string;
            break;

        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_FIRST_CREATED:
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LAST_MODIFIED:
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_PROPERTIES:
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_1:
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_2:
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_3:
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_ACTION_CONTROL_POINT:
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_CONTROL_POINT:
            value_length = connection->data_length;
            *out_value   = connection->data.data_bytes;
            break;

        default:
            btstack_assert(false);
            break;
    }
    return value_length;
}

static void ots_client_emit_read_event(gatt_service_client_connection_helper_t * connection_helper, uint8_t characteristic_index, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    uint8_t expected_data_size = 0;
    uint8_t  emit_bytes[16];

    switch (characteristic_index){
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OTS_FEATURE:
            if (data_size == 8){
                ots_client_connection_t * connection = (ots_client_connection_t *)connection_helper;
                connection->oacp_features = little_endian_read_32(data, 0);
                connection->olcp_features = little_endian_read_32(data, 4);
            }
            ots_client_emit_features_event(connection_helper, att_status);
            return;

        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_NAME:
            ots_client_emit_string_value(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_NAME, data, data_size, att_status);
            return;

        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_TYPE:
            if (data_size == 2){
                expected_data_size = 2;
            } else {
                expected_data_size = 16;
            }
            if (data_size == expected_data_size){
                reverse_bytes(data, emit_bytes, data_size);
            }
            ots_client_emit_variable_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_TYPE, emit_bytes, expected_data_size, att_status);
            return;

        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_SIZE:
            if (data_size == 8){
                // current_size(4) + allocated_size(4)
                expected_data_size = 8;
            }
            ots_client_emit_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_SIZE, data, expected_data_size, att_status);
            return;

        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_FIRST_CREATED:
            if (data_size == 7){
                expected_data_size = 7;
                reverse_bytes(data, emit_bytes, 2);
                memcpy(emit_bytes + 2, data + 2, 5);
            }
            ots_client_emit_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_FIRST_CREATED, emit_bytes, expected_data_size, att_status);
            break;

        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LAST_MODIFIED:
            if (data_size == 7){
                expected_data_size = 7;
                reverse_bytes(data, emit_bytes, 2);
                memcpy(emit_bytes + 2, data + 2, 5);
            }
            ots_client_emit_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_LAST_MODIFIED, emit_bytes, expected_data_size, att_status);
            break;

        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_ID:
            if (data_size == OTS_OBJECT_ID_LEN){
                expected_data_size = OTS_OBJECT_ID_LEN;
                reverse_bytes(data, emit_bytes, OTS_OBJECT_ID_LEN);
            }
            ots_client_emit_variable_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_ID, emit_bytes, expected_data_size, att_status);
            break;

        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_PROPERTIES:
            if (data_size == 4){
                expected_data_size = 4;
            }
            ots_client_emit_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_PROPERTIES, data, expected_data_size, att_status);
            break;

        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_1:
            if (data_size >= 1){
                uint8_t pos = 0;
                ots_filter_type_t type = (ots_filter_type_t)data[pos++];
                ots_client_emit_filter_value(connection_helper, 1, type, &data[pos], data_size - pos, att_status);
                break;
            }
            ots_client_emit_filter_value(connection_helper, 1, OTS_FILTER_TYPE_RFU, data, 0, att_status);
            break;

        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_2:
            if (data_size >= 1){
                uint8_t pos = 0;
                ots_filter_type_t type = (ots_filter_type_t)data[pos++];
                ots_client_emit_filter_value(connection_helper, 2, type, &data[pos], data_size - pos, att_status);
                break;
            }
            ots_client_emit_filter_value(connection_helper, 2, OTS_FILTER_TYPE_RFU, data, 0, att_status);
            break;

        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_3:
            if (data_size >= 1){
                uint8_t pos = 0;
                ots_filter_type_t type = (ots_filter_type_t)data[pos++];
                ots_client_emit_filter_value(connection_helper, 3, type, &data[pos], data_size - pos, att_status);
                break;
            }
            ots_client_emit_filter_value(connection_helper, 3, OTS_FILTER_TYPE_RFU, data, 0, att_status);
            break;

        default:
            btstack_assert(false);
            break;
    }
}

static void ots_client_emit_notify_event(gatt_service_client_connection_helper_t * connection_helper, uint16_t value_handle, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    uint16_t characteristic_uuid16 = gatt_service_client_helper_characteristic_uuid16_for_value_handle(&ots_client, connection_helper, value_handle);
    uint8_t  emit_bytes[2 + OTS_OBJECT_ID_LEN];
    ots_client_connection_t * connection;

    uint8_t pos = 0;

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_CHANGED:
            if (data_size == (OTS_OBJECT_ID_LEN + 1)) {
                ots_object_changed_flag_t flag = (ots_object_changed_flag_t) data[0];
                if (flag < OTS_OBJECT_CHANGED_FLAG_RFU) {
                    emit_bytes[pos++] = data[0];
                    emit_bytes[pos++] = OTS_OBJECT_ID_LEN;
                    reverse_bytes(&data[1], emit_bytes + pos, OTS_OBJECT_ID_LEN);
                    pos += OTS_OBJECT_ID_LEN;
                    ots_client_emit_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_CHANGED, emit_bytes, pos, att_status);
                }
            }
            break;

        case ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ACTION_CONTROL_POINT:
            if (data_size < 3) {
                ots_client_emit_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OACP_RESPONSE, emit_bytes, 0, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
                break;
            }
            if ((oacp_opcode_t)data[0] != OACP_OPCODE_RESPONSE_CODE) {
                break;
            }
            if (att_status != ATT_ERROR_SUCCESS){
                ots_client_emit_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OACP_RESPONSE, emit_bytes, 2, att_status);
            }

            connection = (ots_client_connection_t *)connection_helper;
            // opcode
            emit_bytes[pos++] = data[1];
            // response code
            emit_bytes[pos++] = data[2];

            if ((oacp_result_code_t)emit_bytes[1] == OACP_RESULT_CODE_SUCCESS){
                switch ((oacp_opcode_t)emit_bytes[0]){
                    case OACP_OPCODE_READ:
                        connection->current_object_read_transfer_in_progress = true;
                        connection->cbm_data_chunk_bytes_transferred = 0;
                        break;

                    case OACP_OPCODE_CALCULATE_CHECKSUM:
                        reverse_bytes(&data[3], emit_bytes + pos, 4);
                        pos += 4;
                        break;

                    case OACP_OPCODE_WRITE:
                        connection->current_object_write_transfer_in_progress = true;
                        connection->cbm_data_chunk_bytes_transferred = 0;
                        l2cap_request_can_send_now_event(connection->le_cbm_connection.cid);
                        break;

                    case OACP_OPCODE_CREATE:
                    case OACP_OPCODE_DELETE:
                    case OACP_OPCODE_EXECUTE:
                    case OACP_OPCODE_ABORT:
                        break;
                    default:
                        att_status = ATT_ERROR_INVALID_PDU;
                        break;
                }
            }

            ots_client_emit_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OACP_RESPONSE, emit_bytes, 2, att_status);
            break;
        
        case ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_CONTROL_POINT:
            if (data_size < 3) {
                ots_client_emit_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OLCP_RESPONSE, emit_bytes, 0, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
                break;
            }
            if ((olcp_opcode_t)data[0] != OLCP_OPCODE_RESPONSE_CODE) {
                break;
            }

            // opcode
            emit_bytes[0] = data[1];
            // response code
            emit_bytes[1] = data[2];
            if ((olcp_opcode_t)data[0] == OLCP_OPCODE_REQUEST_NUMBER_OF_OBJECTS){
                if (data_size != 7){
                    ots_client_emit_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OLCP_RESPONSE, emit_bytes, 0, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
                    break;
                }
                reverse_bytes(data + 3, emit_bytes + 2, data_size - 3);
            }
            ots_client_emit_uint8_array(connection_helper, LEAUDIO_SUBEVENT_OTS_CLIENT_OLCP_RESPONSE, emit_bytes, data_size-1, att_status);
            break;

        default:
            btstack_assert(false);
            break;
    }
}

static uint8_t ots_client_request_send_gatt_query(ots_client_connection_t * connection, ots_client_characteristic_index_t characteristic_index){
     connection->characteristic_index = characteristic_index;

     uint8_t status = gatt_client_request_to_send_gatt_query(&connection->gatt_query_can_send_now, connection->basic_connection.con_handle);
     if (status != ERROR_CODE_SUCCESS){
         connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;
     }
     return status;
}

static uint8_t ots_client_request_read_characteristic(ots_client_connection_t * connection, ots_client_characteristic_index_t characteristic_index){
     btstack_assert(connection != NULL);
     if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
         return ERROR_CODE_CONTROLLER_BUSY;
     }

     uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
     if (status != ERROR_CODE_SUCCESS){
         return status;
     }
     connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
     return ots_client_request_send_gatt_query(connection, characteristic_index);
}


static uint8_t ots_client_request_read_long_characteristic(ots_client_connection_t * connection, ots_client_characteristic_index_t characteristic_index){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_READ_LONG_CHARACTERISTIC_VALUE;
    return ots_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t ots_client_request_write_characteristic(ots_client_connection_t * connection, ots_client_characteristic_index_t characteristic_index){
    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    return ots_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t ots_client_start_oacp_procedure(ots_client_connection_t * connection){
    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    ots_client_operations_start_timer(connection);
    return ots_client_request_send_gatt_query(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_ACTION_CONTROL_POINT);
}

static uint8_t ots_client_start_olcp_procedure(ots_client_connection_t * connection){
    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    ots_client_operations_start_timer(connection);
    return ots_client_request_send_gatt_query(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_CONTROL_POINT);
}

static uint8_t ots_client_request_write_long_characteristic(ots_client_connection_t * connection, ots_client_characteristic_index_t characteristic_index){
    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_WRITE_LONG_CHARACTERISTIC_VALUE;
    return ots_client_request_send_gatt_query(connection, characteristic_index);
}

static void ots_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    gatt_service_client_connection_helper_t * connection_helper;
    ots_client_connection_t * connection;
    hci_con_handle_t con_handle;;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&ots_client, gattservice_subevent_client_connected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = OTS_CLIENT_CHARACTERISTIC_INDEX_OTS_FEATURE; i < OTS_CLIENT_CHARACTERISTIC_INDEX_INDEX_RFU; i++){
                            printf("    0x%04X %s\n", connection_helper->characteristics[i].value_handle, ots_client_characteristic_name[i]);
                        }
                    };
                    printf("OTS Client: Query input state to retrieve and cache change counter\n");
#endif
                    connection = (ots_client_connection_t *) connection_helper;

                    if (gattservice_subevent_client_connected_get_status(packet) == ATT_ERROR_SUCCESS){
                        connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;
                    } else {
                        connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_UNINITIALISED;
                    }
                    ots_client_emit_connected(connection, gattservice_subevent_client_connected_get_status(packet));
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&ots_client, gattservice_subevent_client_disconnected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);
                    ots_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_OTS_CLIENT_DISCONNECTED);
                    connection_helper->con_handle = HCI_CON_HANDLE_INVALID;
                    break;

                default:
                    break;
            }
            break;

        case GATT_EVENT_NOTIFICATION:
            con_handle = (hci_con_handle_t)gatt_event_notification_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&ots_client, con_handle);

            btstack_assert(connection_helper != NULL);

            ots_client_emit_notify_event(connection_helper, gatt_event_notification_get_value_handle(packet), ATT_ERROR_SUCCESS,
                                         gatt_event_notification_get_value(packet),gatt_event_notification_get_value_length(packet));
            break;

        case GATT_EVENT_INDICATION:
            con_handle = (hci_con_handle_t)gatt_event_indication_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&ots_client, con_handle);

            btstack_assert(connection_helper != NULL);

            ots_client_emit_notify_event(connection_helper, gatt_event_indication_get_value_handle(packet), ATT_ERROR_SUCCESS,
                                         gatt_event_indication_get_value(packet),gatt_event_indication_get_value_length(packet));
            break;

        default:
            break;
    }
}

static void ots_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    gatt_service_client_connection_helper_t * connection_helper = NULL;
    hci_con_handle_t con_handle;
    ots_client_connection_t * connection;
    uint8_t offset;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_long_characteristic_value_query_result_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&ots_client, con_handle);
            btstack_assert(connection_helper != NULL);
            connection = (ots_client_connection_t *)connection_helper;
            switch (connection->state) {
                case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_READ_LONG_CHARACTERISTIC_VALUE_RESULT:
                    offset = gatt_event_long_characteristic_value_query_result_get_value_offset(packet);
                    if (offset == 0){
                        connection->long_value_buffer_length = 0;
                    }

                    if (offset < OTS_MAX_STRING_LENGHT){
                        uint8_t value_length = (uint8_t)gatt_event_long_characteristic_value_query_result_get_value_length(packet);
                        uint8_t  dst_size = OTS_MAX_STRING_LENGHT - offset;
                        uint16_t bytes_to_copy = (uint16_t) btstack_min(dst_size, value_length);
                        (void) memcpy(&connection->long_value_buffer[offset], gatt_event_long_characteristic_value_query_result_get_value(packet), bytes_to_copy);
                        connection->long_value_buffer_length = offset + bytes_to_copy;
                    }
                    break;
                default:
                    btstack_assert(false);
                    break;
            }
            break;

        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_characteristic_value_query_result_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&ots_client, con_handle);
            btstack_assert(connection_helper != NULL);
            connection = (ots_client_connection_t *)connection_helper;

            switch (connection->state){
                case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT:
                    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;
                    break;

                case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_OTS_FEATURES_RESULT:
                    if (gatt_event_characteristic_value_query_result_get_value_length(packet) != 4){
                        connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_OTS_FEATURES_READING_FAILED;
                        return;
                    }
                    break;

                default:
                    btstack_assert(false);
                    break;
            }
            ots_client_emit_read_event(connection_helper, connection->characteristic_index, ATT_ERROR_SUCCESS,
                                gatt_event_characteristic_value_query_result_get_value(packet),
                                gatt_event_characteristic_value_query_result_get_value_length(packet));
                    
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            con_handle = (hci_con_handle_t)gatt_event_query_complete_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&ots_client, con_handle);
            btstack_assert(connection_helper != NULL);

            connection = (ots_client_connection_t *)connection_helper;
            switch (connection->state){
                case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT:
                    ots_client_emit_done_event(connection_helper, connection->characteristic_index, gatt_event_query_complete_get_att_status(packet));
                    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;
                    break;

                case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_UNINITIALISED:
                    ots_client_emit_connected(connection, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
                    break;
                
                case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_OTS_FEATURES_READING_FAILED:
                    ots_client_emit_connected(connection, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
                    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_UNINITIALISED;
                    break;

                case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_OTS_FEATURES_RESULT:
                    if (gatt_event_query_complete_get_att_status(packet) == ERROR_CODE_SUCCESS){
                        connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;
                        ots_client_emit_connected(connection, ERROR_CODE_SUCCESS);
                        break;
                    }
                    ots_client_emit_connected(connection, gatt_event_query_complete_get_att_status(packet));
                    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_UNINITIALISED;
                    break;

                case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_READ_LONG_CHARACTERISTIC_VALUE_RESULT:
                    ots_client_emit_read_event(connection_helper, connection->characteristic_index, ATT_ERROR_SUCCESS,
                                               connection->long_value_buffer, connection->long_value_buffer_length);
                    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;

                default:
                    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;
                    break;
            }
            break;

        default:
            break;
    }
}

static void ots_client_run_for_connection(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t)context;
    ots_client_connection_t * connection = (ots_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&ots_client, con_handle);

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;

    switch (connection->state){
        case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_QUERY_OTS_FEATURES:
            connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_OTS_FEATURES_RESULT;
            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                    &ots_client_handle_gatt_client_event, con_handle,
                    ots_client_value_handle_for_index(connection));
            break;

        case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &ots_client_handle_gatt_client_event, con_handle, 
                ots_client_value_handle_for_index(connection));
            break;

        case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_READ_LONG_CHARACTERISTIC_VALUE:
            connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_READ_LONG_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_long_value_of_characteristic_using_value_handle(
                    &ots_client_handle_gatt_client_event, con_handle,
                    ots_client_value_handle_for_index(connection));
            break;

        case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE:
            connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;

            value_length = ots_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_value_of_characteristic_without_response(
                con_handle,
                ots_client_value_handle_for_index(connection),
                value_length, value);
            break;

        case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;

            value_length = ots_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_value_of_characteristic(
                    &ots_client_handle_gatt_client_event, con_handle,
                    ots_client_value_handle_for_index(connection),
                    value_length, value);
            break;

        case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_WRITE_LONG_CHARACTERISTIC_VALUE:
            connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;

            value_length = ots_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_long_value_of_characteristic(
                    &ots_client_handle_gatt_client_event, con_handle,
                    ots_client_value_handle_for_index(connection),
                    value_length, value);
            break;

        default:
            break;
    }
}

static void ots_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_hci_event_handler(&ots_client, packet_type, channel, packet, size);
}

void object_transfer_service_client_init(void){
    gatt_service_client_init(&ots_client, &ots_client_packet_handler_trampoline);
    gatt_service_client_register_packet_handler(&ots_client, &ots_client_packet_handler_internal);

    ots_client.characteristics_desc16_num = sizeof(ots_uuid16s)/sizeof(uint16_t);
    ots_client.characteristics_desc16 = ots_uuid16s;
}

uint8_t object_transfer_service_client_connect(
    hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    ots_client_connection_t * connection,
    uint16_t service_start_handle, uint16_t service_end_handle, 
    uint8_t  service_index,
    uint16_t * ots_cid){

    btstack_assert(packet_handler != NULL);
    btstack_assert(connection != NULL);

    connection->gatt_query_can_send_now.callback = &ots_client_run_for_connection;
    connection->gatt_query_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.con_handle;
    connection->le_cbm_connection.connection_handle = HCI_CON_HANDLE_INVALID;
    connection->le_cbm_connection.cid = 0;

    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_CONNECTED;
    memset(connection->characteristics_storage, 0, OBJECT_TRANSFER_SERVICE_NUM_CHARACTERISTICS * sizeof(gatt_service_client_characteristic_t));
    return gatt_service_client_connect(con_handle,
                                       &ots_client, &connection->basic_connection,
                                       ORG_BLUETOOTH_SERVICE_OBJECT_TRANSFER, service_index,
                                       connection->characteristics_storage, OBJECT_TRANSFER_SERVICE_NUM_CHARACTERISTICS,
                                       packet_handler, ots_cid);
}


uint8_t object_transfer_service_client_connect_secondary_service(
        hci_con_handle_t con_handle,
        btstack_packet_handler_t packet_handler,
        uint16_t service_start_handle, uint16_t service_end_handle, uint8_t service_index, 
        ots_client_connection_t * connection){

    btstack_assert(packet_handler != NULL);
    btstack_assert(connection != NULL);

    connection->gatt_query_can_send_now.callback = &ots_client_run_for_connection;
    connection->gatt_query_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.con_handle;
    connection->le_cbm_connection.connection_handle = HCI_CON_HANDLE_INVALID;
    connection->le_cbm_connection.cid = 0;

    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_CONNECTED;
    memset(connection->characteristics_storage, 0, OBJECT_TRANSFER_SERVICE_NUM_CHARACTERISTICS * sizeof(gatt_service_client_characteristic_t));

    return gatt_service_client_connect_secondary_service(con_handle,
        &ots_client, &connection->basic_connection,
        ORG_BLUETOOTH_SERVICE_OBJECT_TRANSFER, service_start_handle, service_end_handle, service_index,
        connection->characteristics_storage, OBJECT_TRANSFER_SERVICE_NUM_CHARACTERISTICS, packet_handler);
}

uint8_t object_transfer_service_client_read_ots_feature(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OTS_FEATURE);
}

uint8_t object_transfer_service_client_read_object_name(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_NAME);
}

uint8_t object_transfer_service_client_read_long_object_name(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_long_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_NAME);
}

uint8_t object_transfer_service_client_read_object_type(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_TYPE);
}

uint8_t object_transfer_service_client_read_object_size(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_SIZE);
}

uint8_t object_transfer_service_client_read_object_first_created(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_FIRST_CREATED);
}

uint8_t object_transfer_service_client_read_object_last_modified(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LAST_MODIFIED);
}

uint8_t object_transfer_service_client_read_object_id(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_ID);
}

uint8_t object_transfer_service_client_read_object_properties(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_PROPERTIES);
}

uint8_t object_transfer_service_client_read_object_list_filter_1(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_1);
}

uint8_t object_transfer_service_client_read_object_list_filter_2(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_2);
}

uint8_t object_transfer_service_client_read_object_list_filter_3(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_3);
}

uint8_t object_transfer_service_client_read_object_long_list_filter_1(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_long_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_1);
}

uint8_t object_transfer_service_client_read_object_long_list_filter_2(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_long_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_2);
}

uint8_t object_transfer_service_client_read_object_long_list_filter_3(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_request_read_long_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_3);
}

uint8_t object_transfer_service_client_write_object_name(ots_client_connection_t * connection, const char * name){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    connection->data.data_string = name;
    connection->data_length = strlen(name);

    if (connection->basic_connection.mtu >= connection->data_length + 3){
        return ots_client_request_write_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_NAME);
    }
    return ots_client_request_write_long_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_NAME);
}

uint8_t ots_client_write_utc_date(ots_client_connection_t * connection, ots_client_characteristic_index_t characteristic_index, btstack_utc_t * date){
    btstack_assert(connection != NULL);
    btstack_assert(date != NULL);

    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    uint8_t pos = 0;
    little_endian_store_16(connection->data.data_bytes, pos, date->year);
    pos += 2;
    connection->data.data_bytes[pos++] = date->month;
    connection->data.data_bytes[pos++] = date->day;
    connection->data.data_bytes[pos++] = date->hours;
    connection->data.data_bytes[pos++] = date->minutes;
    connection->data.data_bytes[pos++] = date->seconds;
    connection->data_length = pos;
    return ots_client_request_write_characteristic(connection, characteristic_index);
}

uint8_t object_transfer_service_client_write_object_first_created(ots_client_connection_t * connection, btstack_utc_t * date){
    return ots_client_write_utc_date(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_FIRST_CREATED, date);
}

uint8_t object_transfer_service_client_write_object_last_modified(ots_client_connection_t * connection, btstack_utc_t * date){
    return ots_client_write_utc_date(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LAST_MODIFIED, date);
}

uint8_t object_transfer_service_client_write_object_properties(ots_client_connection_t * connection, uint32_t properties){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    little_endian_store_32(connection->data.data_bytes, 0, properties);
    connection->data_length = 4;
    return ots_client_request_write_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_PROPERTIES);
}

uint8_t ots_client_write_filter(ots_client_connection_t * connection, ots_client_characteristic_index_t characteristic_index, ots_filter_type_t filter_type, uint8_t data_length, const uint8_t * data){
    btstack_assert(connection != NULL);

    if (data_length + 1 > sizeof(connection->data.data_bytes)){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    connection->data.data_bytes[0] = (uint8_t) filter_type;
    memcpy(connection->data.data_bytes + 1, data, data_length);
    connection->data_length = 1 + data_length;

    if (connection->basic_connection.mtu >= connection->data_length + 3){
        return ots_client_request_write_characteristic(connection, characteristic_index);
    }
    return ots_client_request_write_long_characteristic(connection, characteristic_index);
}

uint8_t object_transfer_service_client_write_object_list_filter_1(ots_client_connection_t * connection, ots_filter_type_t filter_type, uint8_t data_length, const uint8_t * data){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_write_filter(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_1, filter_type, data_length, data);
}

uint8_t object_transfer_service_client_write_object_list_filter_2(ots_client_connection_t * connection, ots_filter_type_t filter_type, uint8_t data_length, const uint8_t * data){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_write_filter(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_2, filter_type, data_length, data);
}

uint8_t object_transfer_service_client_write_object_list_filter_3(ots_client_connection_t * connection, ots_filter_type_t filter_type, uint8_t data_length, const uint8_t * data){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ots_client_write_filter(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_3, filter_type, data_length, data);
}

uint8_t ots_write_object_list_control_point(ots_client_connection_t * connection, olcp_opcode_t opcode, uint8_t * data, uint8_t data_length){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    connection->data_length = 0;
    connection->data.data_bytes[connection->data_length++] = opcode;
    if (data_length > 0){
        reverse_bytes(data, connection->data.data_bytes + connection->data_length,data_length);
        connection->data_length += data_length;
    }
    return ots_client_start_olcp_procedure(connection);
}

uint8_t object_transfer_service_client_command_first(ots_client_connection_t * connection){
    return ots_write_object_list_control_point(connection, OLCP_OPCODE_FIRST, NULL, 0);
}

uint8_t object_transfer_service_client_command_last(ots_client_connection_t * connection){
    return ots_write_object_list_control_point(connection, OLCP_OPCODE_LAST, NULL, 0);
}

uint8_t object_transfer_service_client_command_previous(ots_client_connection_t * connection){
    return ots_write_object_list_control_point(connection, OLCP_OPCODE_PREVIOUS, NULL, 0);
}

uint8_t object_transfer_service_client_command_next(ots_client_connection_t * connection){
    return ots_write_object_list_control_point(connection, OLCP_OPCODE_NEXT, NULL, 0);
}

uint8_t object_transfer_service_client_command_goto(ots_client_connection_t * connection, ots_object_id_t * object_id){
    return ots_write_object_list_control_point(connection, OLCP_OPCODE_GOTO, (uint8_t *) object_id, OTS_OBJECT_ID_LEN);
}

uint8_t object_transfer_service_client_command_order(ots_client_connection_t * connection, olcp_list_sort_order_t order){
    uint8_t data[1];
    data[0] = (uint8_t) order;
    return ots_write_object_list_control_point(connection, OLCP_OPCODE_ORDER, data, sizeof(data));
}

uint8_t object_transfer_service_client_command_request_number_of_objects(ots_client_connection_t * connection){
    return ots_write_object_list_control_point(connection, OLCP_OPCODE_REQUEST_NUMBER_OF_OBJECTS, NULL, 0);
}

uint8_t object_transfer_service_client_command_request_clear_marking(ots_client_connection_t * connection){
    return ots_write_object_list_control_point(connection, OLCP_OPCODE_CLEAR_MARKING, NULL, 0);
}

static void ots_client_l2cap_cbm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    bd_addr_t event_address;
    uint16_t psm;
    uint16_t cid;
    hci_con_handle_t handle;
    uint8_t status;
    ots_client_connection_t * connection;

    if (packet_type == L2CAP_DATA_PACKET){
        connection = ots_client_get_connection_for_cbm_local_cid(channel);
        if (!connection->current_object_read_transfer_in_progress){
            return;
        }

        uint16_t offset = connection->cbm_data_chunk_bytes_transferred ;
        connection->cbm_data_chunk_bytes_transferred += size;
        uint8_t state = 2;

        if (connection->cbm_data_chunk_bytes_transferred < connection->cbm_data_chunk_length){
           state = offset == 0 ? 0 : 1;
        }
        // TODO -> memcpy to connection->cbm_data;
        ots_client_emit_data_chunk((gatt_service_client_connection_helper_t *) connection, state);
        // printf("emit data[%d] offset %d, chunk length %d, transferred %d \n", state, connection->cbm_data_offset, connection->cbm_data_chunk_length,  connection->cbm_data_chunk_bytes_transferred);
        return;
    }

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }
    uint16_t  bytes_to_read;

    switch (hci_event_packet_get_type(packet)) {

        case L2CAP_EVENT_CBM_CHANNEL_OPENED:
            // inform about new l2cap connection
            l2cap_event_cbm_channel_opened_get_address(packet, event_address);
            psm = l2cap_event_cbm_channel_opened_get_psm(packet);
            cid = l2cap_event_cbm_channel_opened_get_local_cid(packet);
            handle = l2cap_event_cbm_channel_opened_get_handle(packet);
            connection = (ots_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&ots_client, handle);
            btstack_assert(connection != NULL);

            status = l2cap_event_cbm_channel_opened_get_status(packet);
            if (status == ERROR_CODE_SUCCESS) {
                log_info("L2CAP: CBM Channel successfully opened: %s, handle 0x%04x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x",
                       bd_addr_to_str(event_address), handle, psm, cid,  little_endian_read_16(packet, 15));

                connection->le_cbm_connection.cid = cid;
                connection->le_cbm_connection.connection_handle = handle;
                connection->le_cbm_connection.mtu = l2cap_event_cbm_channel_opened_get_remote_mtu(packet);
            } else {
                log_info("L2CAP: Connection to device %s failed. status code 0x%02x", bd_addr_to_str(event_address), status);
            }
            connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;
            break;

        case L2CAP_EVENT_CAN_SEND_NOW:
            cid = l2cap_event_can_send_now_get_local_cid(packet);
            connection = ots_client_get_connection_for_cbm_local_cid(cid);
            if (connection == NULL){
                break;
            }

            bytes_to_read = btstack_min(connection->le_cbm_connection.mtu, connection->cbm_data_chunk_length - connection->cbm_data_chunk_bytes_transferred);

            if (bytes_to_read > 0){

                l2cap_send(connection->le_cbm_connection.cid, &connection->cbm_data[connection->cbm_data_offset + connection->cbm_data_chunk_bytes_transferred], bytes_to_read);
                connection->cbm_data_chunk_bytes_transferred += bytes_to_read;
                l2cap_request_can_send_now_event(connection->le_cbm_connection.cid);
                break;
            }

            connection->cbm_data_offset = 0;
            connection->cbm_data_chunk_length = 0;
            connection->current_object_write_transfer_in_progress = false;
            break;

        case L2CAP_EVENT_CHANNEL_CLOSED:
            cid = l2cap_event_channel_closed_get_local_cid(packet);
            connection = (ots_client_connection_t *)gatt_service_client_get_connection_for_cid(&ots_client, cid);
            if (connection != NULL){
                log_info("L2CAP: Channel closed 0x%02x", cid);
                connection->le_cbm_connection.cid = 0;
                connection->le_cbm_connection.connection_handle = HCI_CON_HANDLE_INVALID;
                connection->le_cbm_connection.mtu = 0;
                btstack_run_loop_remove_timer(&connection->operation_timer);
            }
            break;
        default:
            break;
    }
}

uint8_t object_transfer_service_client_open_object_channel(ots_client_connection_t * connection, uint8_t * cbm_receive_buffer, uint16_t cbm_receive_buffer_len){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    uint8_t status = ERROR_CODE_SUCCESS;

    if (connection->le_cbm_connection.connection_handle == HCI_CON_HANDLE_INVALID){
        connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_L2CAP_CBM_CHANNEL_OPENED;
        status = l2cap_cbm_create_channel(&ots_client_l2cap_cbm_packet_handler, connection->basic_connection.con_handle,
                                          BLUETOOTH_PSM_OTS, cbm_receive_buffer,
                                          cbm_receive_buffer_len, L2CAP_LE_AUTOMATIC_CREDITS, LEVEL_0, &connection->le_cbm_connection.cid);

        if (status != ERROR_CODE_SUCCESS){
            connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;
            return status;
        }
    }
    return status;
}

uint8_t object_transfer_service_client_close_object_channel(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    return l2cap_le_disconnect(connection->le_cbm_connection.cid);
}


uint8_t object_transfer_service_client_read(ots_client_connection_t * connection, uint32_t data_offset, uint32_t data_len){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    if (connection->le_cbm_connection.connection_handle == HCI_CON_HANDLE_INVALID){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    connection->cbm_data_offset = data_offset;
    connection->cbm_data_chunk_length = data_len;
    connection->cbm_data_chunk_bytes_transferred = 0;

    connection->data_length = 0;
    connection->data.data_bytes[connection->data_length++] = OACP_OPCODE_READ;
    little_endian_store_32(connection->data.data_bytes, connection->data_length, connection->cbm_data_offset);
    connection->data_length += 4;
    little_endian_store_32(connection->data.data_bytes, connection->data_length, connection->cbm_data_chunk_length);
    connection->data_length += 4;
    return ots_client_start_oacp_procedure(connection);
}

uint8_t object_transfer_service_client_calculate_checksum(ots_client_connection_t * connection, uint32_t data_offset, uint32_t data_len){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    if (connection->le_cbm_connection.connection_handle == HCI_CON_HANDLE_INVALID){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    connection->cbm_data_offset = data_offset;
    connection->cbm_data_chunk_length = data_len;
    connection->cbm_data_chunk_bytes_transferred = 0;

    connection->data_length = 0;
    connection->data.data_bytes[connection->data_length++] = OACP_OPCODE_CALCULATE_CHECKSUM;
    little_endian_store_32(connection->data.data_bytes, connection->data_length, connection->cbm_data_offset);
    connection->data_length += 4;
    little_endian_store_32(connection->data.data_bytes, connection->data_length, connection->cbm_data_chunk_length);
    connection->data_length += 4;
    return ots_client_start_oacp_procedure(connection);
}

uint8_t object_transfer_service_client_write(ots_client_connection_t * connection, bool truncated, uint32_t data_offset, uint8_t *data, uint16_t data_len){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    if (connection->le_cbm_connection.connection_handle == HCI_CON_HANDLE_INVALID){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    connection->cbm_data_offset = data_offset;
    connection->cbm_data_chunk_length = data_len;
    connection->cbm_data_chunk_bytes_transferred = 0;
    connection->cbm_data = data;

    connection->data_length = 0;
    connection->data.data_bytes[connection->data_length++] = OACP_OPCODE_WRITE;
    little_endian_store_32(connection->data.data_bytes, connection->data_length, connection->cbm_data_offset);
    connection->data_length += 4;
    little_endian_store_32(connection->data.data_bytes, connection->data_length, connection->cbm_data_chunk_length);
    connection->data_length += 4;
    connection->data.data_bytes[connection->data_length++] = truncated ? 2 : 0;

    return ots_client_start_oacp_procedure(connection);
}

uint8_t object_transfer_service_client_execute(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    connection->data_length = 0;
    connection->data.data_bytes[connection->data_length++] = OACP_OPCODE_EXECUTE;
    return ots_client_start_oacp_procedure(connection);
}


uint8_t object_transfer_service_client_delete_object(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    connection->data_length = 0;
    connection->data.data_bytes[connection->data_length++] = OACP_OPCODE_DELETE;
    return ots_client_start_oacp_procedure(connection);
}

uint8_t object_transfer_service_client_abort(ots_client_connection_t * connection){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    connection->data_length = 0;
    connection->data.data_bytes[connection->data_length++] = OACP_OPCODE_ABORT;
    return ots_client_start_oacp_procedure(connection);
}

uint8_t object_transfer_service_client_create_object(ots_client_connection_t * connection, uint32_t object_size, ots_object_type_t type_uuid16){
    btstack_assert(connection != NULL);
    if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    connection->data_length = 0;
    connection->data.data_bytes[connection->data_length++] = OACP_OPCODE_CREATE;

    little_endian_store_32(connection->data.data_bytes, connection->data_length, object_size);
    connection->data_length += 4;
    little_endian_store_16(connection->data.data_bytes, connection->data_length, type_uuid16);
    connection->data_length += 2;

    return ots_client_start_oacp_procedure(connection);
}

uint8_t object_transfer_service_client_disconnect(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_SUCCESS;
    }
    return gatt_service_client_disconnect(&ots_client, connection->basic_connection.cid);
}

void object_transfer_control_service_client_deinit(void){
    gatt_service_client_deinit(&ots_client);
}
