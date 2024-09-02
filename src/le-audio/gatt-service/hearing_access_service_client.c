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

#define BTSTACK_FILE__ "hearing_access_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "ble/gatt_service_client.h"
#include "le-audio/gatt-service/hearing_access_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

// VCS Client
static gatt_service_client_t has_client;

static btstack_context_callback_registration_t has_client_handle_can_send_now;

static void has_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void has_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void has_client_run_for_connection(void * context);

// list of uuids
static const uint16_t has_uuid16s[HEARING_ACCESS_SERVICE_NUM_CHARACTERISTICS] = {
    ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_FEATURES,       
    ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_PRESET_CONTROL_POINT,
    ORG_BLUETOOTH_CHARACTERISTIC_ACTIVE_PRESET_INDEX
};

typedef enum {
    HAS_CLIENT_CHARACTERISTIC_INDEX_START_GROUP = 0,
    HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_FEATURES = 0,
    HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT,
    HAS_CLIENT_CHARACTERISTIC_INDEX_ACTIVE_PRESET_INDEX,
    HAS_CLIENT_CHARACTERISTIC_INDEX_RFU
} has_client_characteristic_index_t;

#ifdef ENABLE_TESTING_SUPPORT
static char * has_client_characteristic_name[] = {
    "HEARING_AID_FEATURES",
    "HEARING_AID_PRESET_CONTROL_POINT",
    "ACTIVE_PRESET_INDEX",
    "RFU"
};
#endif

static void has_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void has_client_emit_connection_established(const gatt_service_client_connection_t *connection_helper, uint8_t status) {
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_HAS_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, connection_helper->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos += 2;
    event[pos++] = status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void has_client_finalize_connection(has_client_connection_t * connection){
    // already finalized by GATT CLIENT HELPER
    if (connection == NULL){
        return;
    }
    gatt_service_client_finalize_connection(&has_client, &connection->basic_connection);
}

static void has_client_connected(has_client_connection_t * connection, uint8_t status) {
    if (status == ERROR_CODE_SUCCESS){
        connection->state = HEARING_ACCESS_SERVICE_CLIENT_STATE_READY;
        has_client_emit_connection_established(&connection->basic_connection, status);
    } else {
        connection->state = HEARING_ACCESS_SERVICE_CLIENT_STATE_IDLE;
        has_client_emit_connection_established(&connection->basic_connection, status);
        has_client_finalize_connection(connection);
    }
}

static void has_client_emit_uint8_value(gatt_service_client_connection_t * connection_helper, uint8_t subevent, uint8_t data, uint8_t att_status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);
    
    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = 5;
    event[pos++] = subevent;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    event[pos++] = data;
    event[pos++] = att_status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void has_client_emit_generic_update(gatt_service_client_connection_t * connection_helper, const uint8_t * data, uint8_t data_size){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);
    
    uint8_t event[12 + HAS_PRESET_RECORD_NAME_MAX_LENGTH];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = 0;
    event[pos++] = LEAUDIO_SUBEVENT_HAS_CLIENT_GENERIC_UPDATE;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    // opcode[0], changeid[1], is_last[2], prev_index[3], index[4], properties[5], name[6]
    event[pos++] = data[0]; // opcode: PresetChanged == 0x03
    event[pos++] = data[2]; // is_last
    event[pos++] = data[3]; // prev_index
    event[pos++] = data[4]; // newly_added_preset_index
    event[pos++] = data[5] & HEARING_AID_PRESET_PROPERTIES_MASK_WRITABLE;     // newly_added_preset_is_writable
    event[pos++] = data[5] & HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE; // newly_added_preset_is_available
    
    uint16_t name_length = 0;
    event[pos++] = name_length;

    if (data_size > 6){
        name_length = btstack_strcpy((char *)&event[pos], sizeof(event) - pos, (const char *) &data[6]);
        event[pos - 1] = (uint8_t) name_length;
    }
    pos += (uint8_t) name_length;

    // update packet len
    event[1] = pos - 2;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void has_client_emit_preset_record_changed(gatt_service_client_connection_t * connection_helper, uint8_t subevent_id, const uint8_t * data, uint8_t data_size){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);
    
    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = 3 + data_size;
    event[pos++] = subevent_id;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    // opcode[0], changeid[1], is_last[2], changed_preset_index[3]
    event[pos++] = data[0]; // opcode: PresetChanged == 0x03
    event[pos++] = data[2]; // is_last
    event[pos++] = data[3]; // changed_preset_index
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void has_client_emit_preset_record_deleted(gatt_service_client_connection_t * connection_helper, const uint8_t * data, uint8_t data_size){
    has_client_emit_preset_record_changed(connection_helper, LEAUDIO_SUBEVENT_HAS_CLIENT_PRESET_RECORD_DELETED, data, data_size);
}

static void has_client_emit_preset_record_available(gatt_service_client_connection_t * connection_helper, const uint8_t * data, uint8_t data_size){
    has_client_emit_preset_record_changed(connection_helper, LEAUDIO_SUBEVENT_HAS_CLIENT_PRESET_RECORD_AVAILABLE, data, data_size);
}

static void has_client_emit_preset_record_unavailable(gatt_service_client_connection_t * connection_helper, const uint8_t * data, uint8_t data_size){
    has_client_emit_preset_record_changed(connection_helper, LEAUDIO_SUBEVENT_HAS_CLIENT_PRESET_RECORD_UNAVAILABLE, data, data_size);
}
//
//static void has_client_emit_hearing_aid_features(gatt_service_client_connection_helper_t * connection_helper, const uint8_t * data, uint8_t data_size){
//    btstack_assert(connection_helper != NULL);
//    btstack_assert(connection_helper->event_callback != NULL);
//
//    uint8_t event[6];
//    uint16_t pos = 0;
//    event[pos++] = HCI_EVENT_LEAUDIO_META;
//    event[pos++] = 3 + data_size;
//    event[pos++] = LEAUDIO_SUBEVENT_HAS_CLIENT_HEARING_AID_FEATURES;
//    little_endian_store_16(event, pos, connection_helper->cid);
//    pos+= 2;
//    event[pos++] = data[0];
//    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
//}
//
//static void has_client_emit_active_preset_index(gatt_service_client_connection_helper_t * connection_helper, const uint8_t * data, uint8_t data_size){
//    btstack_assert(connection_helper != NULL);
//    btstack_assert(connection_helper->event_callback != NULL);
//
//    uint8_t event[6];
//    uint16_t pos = 0;
//    event[pos++] = HCI_EVENT_LEAUDIO_META;
//    event[pos++] = 3 + data_size;
//    event[pos++] = LEAUDIO_SUBEVENT_HAS_CLIENT_ACTIVE_PRESET_INDEX;
//    little_endian_store_16(event, pos, connection_helper->cid);
//    pos+= 2;
//    event[pos++] = data[0];
//    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
//}


static void has_client_emit_read_event(gatt_service_client_connection_t * connection_helper, uint16_t value_handle, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    uint16_t expected_data_size;
    uint8_t  subevent_id;
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_value_handle(&has_client, connection_helper, value_handle);

    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_FEATURES:
            expected_data_size = 1;
            subevent_id = LEAUDIO_SUBEVENT_HAS_CLIENT_HEARING_AID_FEATURES;
            break;
        
        case ORG_BLUETOOTH_CHARACTERISTIC_ACTIVE_PRESET_INDEX:
            expected_data_size = 1;
            subevent_id = LEAUDIO_SUBEVENT_HAS_CLIENT_ACTIVE_PRESET_INDEX;
            break;
        
        default:
            btstack_assert(false);
            break;
    }

    if (att_status != ATT_ERROR_SUCCESS){
        has_client_emit_uint8_value(connection_helper, subevent_id, 0, att_status);
        return;
    }
    if (data_size != expected_data_size){
        has_client_emit_uint8_value(connection_helper, subevent_id, 0, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
    } else {
        has_client_emit_uint8_value(connection_helper, subevent_id, data[0], ERROR_CODE_SUCCESS);
    }
}


static void has_client_emit_indicate_event(gatt_service_client_connection_t * connection_helper, uint16_t value_handle, const uint8_t * data, uint16_t data_size){
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_value_handle(&has_client, connection_helper, value_handle);

    if (characteristic_uuid16 != ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_PRESET_CONTROL_POINT){
        return;
    }
    
    if ((has_opcode_t)data[0] != HAS_OPCODE_PRESET_CHANGED){
        return;
    }

    switch ((has_changeid_t)data[1]){
        case HAS_CHANGEID_GENERIC_UPDATE:
            if (data_size < 6){
                break;
            }
            has_client_emit_generic_update(connection_helper, data, data_size);
            break;
        
        case HAS_CHANGEID_PRESET_RECORD_DELETED:
            if (data_size < 4){
                break;
            }
            has_client_emit_preset_record_deleted(connection_helper, data, data_size);
            break;
        
        case HAS_CHANGEID_PRESET_RECORD_AVAILABLE:
            if (data_size < 4){
                break;
            }
            has_client_emit_preset_record_available(connection_helper, data, data_size);
            break;
        
        case HAS_CHANGEID_PRESET_RECORD_UNAVAILABLE:
            if (data_size < 4){
                break;
            }
            has_client_emit_preset_record_unavailable(connection_helper, data, data_size);
            break;
            
        default:
            return;
    }
}

static void has_client_emit_notify_event(gatt_service_client_connection_t * connection_helper, uint16_t value_handle, const uint8_t * data, uint16_t data_size){
    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_value_handle(&has_client, connection_helper, value_handle);
    has_client_emit_read_event(connection_helper,  characteristic_uuid16,  ERROR_CODE_SUCCESS, data, data_size);
}

static uint8_t has_client_can_query_characteristic(has_client_connection_t * connection, has_client_characteristic_index_t characteristic_index){
    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    return connection->state == HEARING_ACCESS_SERVICE_CLIENT_STATE_READY ? ERROR_CODE_SUCCESS : ERROR_CODE_CONTROLLER_BUSY;
}

static uint8_t has_client_request_send_gatt_query(has_client_connection_t * connection, has_client_characteristic_index_t characteristic_index){
    connection->characteristic_index = characteristic_index;

    has_client_handle_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.con_handle;
    uint8_t status = gatt_client_request_to_send_gatt_query(&has_client_handle_can_send_now, connection->basic_connection.con_handle);
    if (status != ERROR_CODE_SUCCESS){
        connection->state = HEARING_ACCESS_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

static uint8_t has_client_request_read_characteristic(uint16_t aics_cid, has_client_characteristic_index_t characteristic_index){
    has_client_connection_t * connection = (has_client_connection_t *) gatt_service_client_get_connection_for_cid(&has_client, aics_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    uint8_t status = has_client_can_query_characteristic(connection, characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
   
    connection->state = HEARING_ACCESS_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    return has_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t has_client_request_write_characteristic(has_client_connection_t * connection, has_client_characteristic_index_t characteristic_index){
    connection->state = HEARING_ACCESS_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    return has_client_request_send_gatt_query(connection, characteristic_index);
}

static void has_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    gatt_service_client_connection_t * connection_helper;
    has_client_connection_t * connection;
    hci_con_handle_t con_handle;
    uint16_t cid;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    cid = gattservice_subevent_client_connected_get_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&has_client, cid);
                    btstack_assert(connection_helper != NULL);
                    connection = (has_client_connection_t *)connection_helper;

                    status = gattservice_subevent_client_connected_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS){
                        has_client_connected(connection, status);
                        break;
                    }

#ifdef ENABLE_TESTING_SUPPORT
                    {
                        uint8_t i;
                        for (i = HAS_CLIENT_CHARACTERISTIC_INDEX_START_GROUP; i < HAS_CLIENT_CHARACTERISTIC_INDEX_RFU; i++){
                            printf("0x%04X %s\n", connection_helper->characteristics[i].value_handle, has_client_characteristic_name[i]);
                        }
                    };
#endif

                    if (connection->basic_connection.characteristics[HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_FEATURES].value_handle == 0){
                        connection->state = HEARING_ACCESS_SERVICE_CLIENT_STATE_IDLE;
                        has_client_connected(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                        break;
                    }

                    connection->state = HEARING_ACCESS_SERVICE_CLIENT_STATE_READY;
                    has_client_connected(connection, ERROR_CODE_SUCCESS);
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    // TODO reset client
                    cid = gattservice_subevent_client_disconnected_get_cid(packet);
                    connection_helper = gatt_service_client_get_connection_for_cid(&has_client, cid);
                    btstack_assert(connection_helper != NULL);
                    has_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_HAS_CLIENT_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;

        case GATT_EVENT_NOTIFICATION:
            con_handle = (hci_con_handle_t)gatt_event_notification_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&has_client, con_handle);

            btstack_assert(connection != NULL);

            has_client_emit_notify_event(connection_helper, gatt_event_notification_get_value_handle(packet),
                                         gatt_event_notification_get_value(packet),
                                         gatt_event_notification_get_value_length(packet));
            break;
        
        case GATT_EVENT_INDICATION:
            con_handle = (hci_con_handle_t)gatt_event_indication_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&has_client, con_handle);

            btstack_assert(connection != NULL);

            has_client_emit_indicate_event(connection_helper, gatt_event_indication_get_value_handle(packet),
                                         gatt_event_indication_get_value(packet),
                                         gatt_event_indication_get_value_length(packet));
            break;

        default:
            break;
    }
}

static void has_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    gatt_service_client_connection_t * connection_helper;
    has_client_connection_t * connection = NULL;
    hci_con_handle_t con_handle;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_characteristic_value_query_result_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&has_client, con_handle);
            btstack_assert(connection_helper != NULL);
            connection = (has_client_connection_t *)connection_helper;

            has_client_emit_read_event(connection_helper,
                                       gatt_event_characteristic_value_query_result_get_value_handle(packet),
                                       ATT_ERROR_SUCCESS,
                                       gatt_event_characteristic_value_query_result_get_value(packet),
                                       gatt_event_characteristic_value_query_result_get_value_length(packet));

            break;

        case GATT_EVENT_QUERY_COMPLETE:
            con_handle = (hci_con_handle_t)gatt_event_query_complete_get_handle(packet);
            connection = (has_client_connection_t *)gatt_service_client_get_connection_for_con_handle(&has_client, con_handle);
            btstack_assert(connection != NULL);
            connection->state = HEARING_ACCESS_SERVICE_CLIENT_STATE_READY;
            break;

        default:
            break;
    }
}

static uint16_t has_client_serialize_characteristic_value_for_write(has_client_connection_t * connection, uint8_t ** out_value){
    uint8_t value_length = 0;
    switch (connection->characteristic_index){
        case HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT:
            switch ((has_opcode_t)connection->data.data_bytes[0]){
                case HAS_OPCODE_READ_PRESETS_REQUEST:
                    value_length = 3;
                    break;
                
                case HAS_OPCODE_WRITE_PRESET_NAME:
                    value_length = 2 + strlen(connection->data.data_string);
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

static void has_client_run_for_connection(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t)context;
    gatt_service_client_connection_t * connection_helper = gatt_service_client_get_connection_for_con_handle(&has_client, con_handle);
    has_client_connection_t * connection = (has_client_connection_t *)connection_helper;

    btstack_assert(connection != NULL);
    uint16_t value_length;
    uint8_t * value;

    switch (connection->state){
        case HEARING_ACCESS_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = HEARING_ACCESS_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &has_client_handle_gatt_client_event, con_handle,
                gatt_service_client_characteristic_value_handle_for_index(connection_helper, connection->characteristic_index));
            break;

        case HEARING_ACCESS_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = HEARING_ACCESS_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;

            value_length = has_client_serialize_characteristic_value_for_write(connection, &value);
            (void) gatt_client_write_value_of_characteristic(
                    &has_client_handle_gatt_client_event, con_handle,
                    gatt_service_client_characteristic_value_handle_for_index(connection_helper, connection->characteristic_index),
                value_length, value);
            
            break;

        default:
            break;
    }
}

static void has_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_trampoline_packet_handler(&has_client, packet_type, channel, packet, size);
}

void hearing_access_service_client_init(void){
    gatt_service_client_register_client(&has_client, &has_client_packet_handler_internal, &has_client_packet_handler_trampoline);

    has_client.characteristics_desc16_num = sizeof(has_uuid16s)/sizeof(uint16_t);
    has_client.characteristics_desc16 = has_uuid16s;

    has_client_handle_can_send_now.callback = &has_client_run_for_connection;
}

uint8_t hearing_access_service_client_connect(hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    has_client_connection_t * has_connection,
    uint16_t * out_has_cid
){

    btstack_assert(packet_handler != NULL);
    btstack_assert(has_connection != NULL);

    has_connection->events_packet_handler = packet_handler;
    has_connection->state = HEARING_ACCESS_SERVICE_CLIENT_STATE_W4_CONNECTION;

    return gatt_service_client_connect_primary_service(con_handle,
                                       &has_client, &has_connection->basic_connection,
                                       ORG_BLUETOOTH_SERVICE_HEARING_ACCESS, 0,
                                       has_connection->characteristics_storage, HEARING_ACCESS_SERVICE_NUM_CHARACTERISTICS,
                                       packet_handler, out_has_cid);
}


uint8_t hearing_access_service_client_read_hearing_aid_features(uint16_t has_cid){
    return has_client_request_read_characteristic(has_cid, HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_FEATURES);
}


uint8_t hearing_access_service_client_read_active_preset_index(uint16_t has_cid){
   return has_client_request_read_characteristic(has_cid, HAS_CLIENT_CHARACTERISTIC_INDEX_ACTIVE_PRESET_INDEX);
}


uint8_t hearing_access_service_client_read_presets_request(uint16_t has_cid, uint8_t start_index, uint8_t num_presets){
    has_client_connection_t * connection = (has_client_connection_t *) gatt_service_client_get_connection_for_cid(&has_client, has_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    uint8_t status = has_client_can_query_characteristic(connection, HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (connection->state != HEARING_ACCESS_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    connection->data.data_bytes[0] = (uint8_t)HAS_OPCODE_READ_PRESETS_REQUEST;
    connection->data.data_bytes[1] = start_index;
    connection->data.data_bytes[2] = num_presets;
    return has_client_request_write_characteristic(connection, HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT);
}


uint8_t hearing_access_service_client_write_preset_name(uint16_t has_cid, uint8_t index, const char * name){
    has_client_connection_t * connection = (has_client_connection_t *) gatt_service_client_get_connection_for_cid(&has_client, has_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    uint8_t status = has_client_can_query_characteristic(connection, HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (connection->state != HEARING_ACCESS_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    if (name == NULL){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    connection->data.data_bytes[0] = (uint8_t)HAS_OPCODE_WRITE_PRESET_NAME;
    connection->data.data_bytes[1] = index;
    connection->data.data_string = name;
    return has_client_request_write_characteristic(connection, HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT);
}

uint8_t hearing_access_service_client_set_active_preset(uint16_t has_cid, uint8_t index, bool synchronized_locally){
    has_client_connection_t * connection = (has_client_connection_t *) gatt_service_client_get_connection_for_cid(&has_client, has_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    uint8_t status = has_client_can_query_characteristic(connection, HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (connection->state != HEARING_ACCESS_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    connection->data.data_bytes[0] = synchronized_locally ? (uint8_t)HAS_OPCODE_SET_ACTIVE_PRESET_SYNCHRONIZED_LOCALLY : (uint8_t)HAS_OPCODE_SET_ACTIVE_PRESET;
    connection->data.data_bytes[1] = index;
    return has_client_request_write_characteristic(connection, HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT);
}

uint8_t hearing_access_service_client_set_next_preset(uint16_t has_cid, uint8_t index, bool synchronized_locally){
    has_client_connection_t * connection = (has_client_connection_t *) gatt_service_client_get_connection_for_cid(&has_client, has_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    uint8_t status = has_client_can_query_characteristic(connection, HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (connection->state != HEARING_ACCESS_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    connection->data.data_bytes[0] = synchronized_locally ? (uint8_t)HAS_OPCODE_SET_NEXT_PRESET_SYNCHRONIZED_LOCALLY : (uint8_t)HAS_OPCODE_SET_NEXT_PRESET;
    connection->data.data_bytes[1] = index;
    return has_client_request_write_characteristic(connection, HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT);
}

uint8_t hearing_access_service_client_set_previous_preset(uint16_t has_cid, uint8_t index, bool synchronized_locally){
    has_client_connection_t * connection = (has_client_connection_t *) gatt_service_client_get_connection_for_cid(&has_client, has_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    uint8_t status = has_client_can_query_characteristic(connection, HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (connection->state != HEARING_ACCESS_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    connection->data.data_bytes[0] = synchronized_locally ? (uint8_t)HAS_OPCODE_SET_PREVIOUS_PRESET_SYNCHRONIZED_LOCALLY : (uint8_t)HAS_OPCODE_SET_PREVIOUS_PRESET;
    connection->data.data_bytes[1] = index;
    return has_client_request_write_characteristic(connection, HAS_CLIENT_CHARACTERISTIC_INDEX_HEARING_AID_PRESET_CONTROL_POINT);
}

uint8_t hearing_access_service_client_disconnect(uint16_t has_cid){
    has_client_connection_t * connection = (has_client_connection_t *) gatt_service_client_get_connection_for_cid(&has_client, has_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    return gatt_service_client_disconnect(&has_client, has_cid);
}

void hearing_access_service_client_deinit(void){
}

