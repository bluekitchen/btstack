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
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_OTS_CLIENT_CONNECTED;
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
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_OTS_CLIENT_WRITE_DONE;

    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    event[pos++] = connection_helper->service_index;
    little_endian_store_16(event, pos, characteristic_uuid16);
    pos+= 2;
    event[pos++] = att_status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static uint16_t ots_client_serialize_characteristic_value_for_write(ots_client_connection_t * connection, uint8_t ** out_value){
    uint8_t value_length = 0;

    switch (connection->characteristic_index){
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_NAME:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_FIRST_CREATED:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LAST_MODIFIED:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_PROPERTIES:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_ACTION_CONTROL_POINT:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_CONTROL_POINT:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_1:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_2:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_3:
            break;

        default:
            btstack_assert(false);
            break;
    }
    return value_length;
}

static void ots_client_emit_read_event(gatt_service_client_connection_helper_t * connection_helper, uint8_t characteristic_index, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    // uint8_t expected_data_size = 0;

    switch (characteristic_index){
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OTS_FEATURE:
            // expected_data_size = 4;
            // connection->features = little_endian_read_32(packet, 0);
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_NAME:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_TYPE:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_SIZE:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_FIRST_CREATED:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LAST_MODIFIED:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_ID:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_PROPERTIES:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_1:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_2:
            break;
        case OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_FILTER_3:
            break;

        default:
            btstack_assert(false);
            break;
    }
}

static void ots_client_emit_notify_event(gatt_service_client_connection_helper_t * connection_helper, uint16_t value_handle, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    uint16_t characteristic_uuid16 = gatt_service_client_helper_characteristic_uuid16_for_value_handle(&ots_client, connection_helper, value_handle);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ACTION_CONTROL_POINT:
            break;
        
        case ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_CONTROL_POINT:
            break;

        case ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_CHANGED:
            break;

        default:
            btstack_assert(false);
            break;
    }
}

// static uint8_t ots_client_request_send_gatt_query(ots_client_connection_t * connection, ots_client_characteristic_index_t characteristic_index){
//     connection->characteristic_index = characteristic_index;

//     uint8_t status = gatt_client_request_to_send_gatt_query(&connection->gatt_query_can_send_now, connection->basic_connection.con_handle);
//     if (status != ERROR_CODE_SUCCESS){
//         connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;
//     } 
//     return status;
// }

// static uint8_t ots_client_request_read_characteristic(ots_client_connection_t * connection, ots_client_characteristic_index_t characteristic_index){
//     btstack_assert(connection != NULL);
//     if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
//         return ERROR_CODE_CONTROLLER_BUSY;
//     }

//     uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
//     if (status != ERROR_CODE_SUCCESS){
//         return status;
//     }
//     connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
//     return ots_client_request_send_gatt_query(connection, characteristic_index);
// }

// uint8_t object_transfer_control_service_client_write_audio_location(ots_client_connection_t * connection, uint32_t audio_location){
//     btstack_assert(connection != NULL);
//     if (connection->state != OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY){
//         return ERROR_CODE_CONTROLLER_BUSY;
//     }

//     little_endian_store_32(connection->data.data_bytes, 0, audio_location);
//     return ots_client_request_write_characteristic_without_response(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_AUDIO_LOCATION);
// }

// uint8_t object_transfer_control_service_client_read_offset_state(ots_client_connection_t * connection){
//     return ots_client_request_read_characteristic(connection, OTS_CLIENT_CHARACTERISTIC_INDEX_OFFSET_STATE);
// }


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
                    ots_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, GATTSERVICE_SUBEVENT_OTS_CLIENT_DISCONNECTED);
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

    switch(hci_event_packet_get_type(packet)){
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
                    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_UNINITIALISED;
                    ots_client_emit_connected(connection, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
                    break;

                case OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_OTS_FEATURES_RESULT:
                    if (gatt_event_query_complete_get_att_status(packet) == ERROR_CODE_SUCCESS){
                        connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;
                        ots_client_emit_connected(connection, ERROR_CODE_SUCCESS);
                        break;
                    }
                    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_UNINITIALISED;
                    ots_client_emit_connected(connection, gatt_event_query_complete_get_att_status(packet));
                    break;

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

    connection->state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_CONNECTED;
    memset(connection->characteristics_storage, 0, OBJECT_TRANSFER_SERVICE_NUM_CHARACTERISTICS * sizeof(gatt_service_client_characteristic_t));
    return gatt_service_client_connect(con_handle,
                                       &ots_client, &connection->basic_connection,
                                       ORG_BLUETOOTH_SERVICE_OBJECT_TRANSFER, service_index,
                                       connection->characteristics_storage, OBJECT_TRANSFER_SERVICE_NUM_CHARACTERISTICS,
                                       packet_handler, ots_cid);
}

static bool ots_client_feature_supported(ots_client_connection_t * connection, uint32_t feature){
    return true;
}

static uint32_t feature;
uint8_t object_transfer_service_client_read_ots_feature(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_read_object_name(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_read_object_type(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_read_object_size(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_read_object_first_created(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_read_object_last_modified(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_read_object_id(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_read_object_properties(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_read_object_list_filter_1(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_read_object_list_filter_2(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_read_object_list_filter_3(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}


uint8_t object_transfer_service_client_write_object_name(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_write_object_first_created(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_write_object_last_modified(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_write_object_properties(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_write_object_action_control_point(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_write_object_list_control_point(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_write_object_list_filter_1(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_write_object_list_filter_2(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_client_write_object_list_filter_3(ots_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_client_feature_supported(connection, feature)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    return ERROR_CODE_SUCCESS;
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

