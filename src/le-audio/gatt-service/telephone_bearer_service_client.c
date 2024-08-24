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

#define BTSTACK_FILE__ "telephone_bearer_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "btstack_memory.h"
#include "ble/core.h"
#include "ble/gatt_client.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"

#include "le-audio/gatt-service/telephone_bearer_service_util.h"
#include "le-audio/gatt-service/telephone_bearer_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

static gatt_service_client_t tbs_client;

static void tbs_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void tbs_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void tbs_client_run_for_connection(void * context);

static uint16_t tbs_client_value_handle_for_index(tbs_client_connection_t * connection){
    return connection->basic_connection.characteristics[connection->characteristic_index].value_handle;
}

static void tbs_client_emit_connection_established(gatt_service_client_connection_t * connection_helper, uint8_t status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint8_t event[9];
    int pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_TBS_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, connection_helper->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection_helper->cid);
    pos += 2;
    event[pos++] = 0; // num included services
    event[pos++] = status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void tbs_client_finalize_connection(tbs_client_connection_t * connection){
    // already finalized by GATT CLIENT HELPER
    if (connection == NULL){
        return;
    }
    gatt_service_client_finalize_connection(&tbs_client, &connection->basic_connection);
}

static void tbs_client_connected(tbs_client_connection_t * connection, uint8_t status) {
    if (status == ERROR_CODE_SUCCESS){
        connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY;
        tbs_client_emit_connection_established(&connection->basic_connection, status);
    } else {
        connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_IDLE;
        tbs_client_emit_connection_established(&connection->basic_connection, status);
        tbs_client_finalize_connection(connection);
    }
}

static void tbs_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void tbs_client_emit_done_event(gatt_service_client_connection_t * connection_helper, uint8_t index, uint8_t att_status){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);

    uint16_t characteristic_uuid16 = gatt_service_client_characteristic_uuid16_for_index(&tbs_client, index);

    uint8_t event[9];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_TBS_CLIENT_WRITE_DONE;

    little_endian_store_16(event, pos, connection_helper->cid);
    pos+= 2;
    event[pos++] = connection_helper->service_index;
    little_endian_store_16(event, pos, characteristic_uuid16);
    pos+= 2;
    event[pos++] = att_status;
    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

typedef struct {
    btstack_packet_handler_t callback;
    const uint8_t *data;
    uint16_t data_size;
    uint16_t cid;
    uint8_t subevent;
} emitter_t;

static void tbs_client_emit_blob( const emitter_t * emit ){
    btstack_packet_handler_t event_callback = emit->callback;
    btstack_assert(event_callback != NULL);

    uint8_t event[257] = { 0 };
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    pos++;                      // reserve event[1] for subevent size
    event[pos++] = emit->subevent;
    little_endian_store_16(event, pos, emit->cid);
    pos+= 2;
    pos++;                      // reserve event[5] for value size

    uint8_t data_length = btstack_min(sizeof(event)-pos, emit->data_size);
    memcpy( &event[pos], emit->data, data_length );
    pos += data_length;

    event[1] = pos - 2;         // store subevent size
    event[5] = data_length;     // store value size
    event_callback(HCI_EVENT_PACKET, 0, event, pos);
}

static void tbs_client_emit_nbytes( const emitter_t * emit ) {
    btstack_packet_handler_t event_callback = emit->callback;
    btstack_assert(event_callback != NULL);

    uint8_t event[258] = { 0 }; // space for 252byte sub-event data + 5byte header + 1byte to guarantee null termination, just in case
    uint8_t data_length = btstack_min( 252, emit->data_size);
    event[0] = HCI_EVENT_LEAUDIO_META;
    event[1] = 3 + data_length;   // sub-event size
    event[2] = emit->subevent;
    little_endian_store_16(event, 3, emit->cid);

    memcpy( &event[5], emit->data, data_length );

    event_callback(HCI_EVENT_PACKET, 0, event, 5 + data_length);
}

static void tbs_client_emit_read_event(gatt_service_client_connection_t * connection_helper, uint8_t characteristic_index, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    UNUSED(att_status);

    emitter_t emitter = {
            .callback = connection_helper->event_callback,
            .cid = connection_helper->cid,
            .data = data,
            .data_size = data_size,
            .subevent = tbs_characteristic_index_to_subevent(characteristic_index),
    };

    switch (characteristic_index){
        case TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL:
        case TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH:
        case TBS_CHARACTERISTIC_INDEX_BEARER_TECHNOLOGY:
        case TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT_OPTIONAL_OPCODES:
        case TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT:
        case TBS_CHARACTERISTIC_INDEX_CONTENT_CONTROL_ID:
        case TBS_CHARACTERISTIC_INDEX_STATUS_FLAGS:
        case TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON:
            tbs_client_emit_nbytes( &emitter );
            break;
        case TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS:
        case TBS_CHARACTERISTIC_INDEX_BEARER_PROVIDER_NAME:
        case TBS_CHARACTERISTIC_INDEX_BEARER_UCI:
        case TBS_CHARACTERISTIC_INDEX_BEARER_URI_SCHEMES_SUPPORTED_LIST:
        case TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME:
        case TBS_CHARACTERISTIC_INDEX_CALL_STATE:
        case TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI:
        case TBS_CHARACTERISTIC_INDEX_INCOMING_CALL:
            tbs_client_emit_blob( &emitter );
            break;
        default:
            btstack_assert( false );
            break;
    }
}

static tbs_characteristic_index_t gatt_service_client_characteristic_value_handle_to_index(tbs_client_connection_t * connection, uint16_t value_handle) {
    for (int i = 0; i < connection->basic_connection.characteristics_num; i++){
        if (connection->basic_connection.characteristics[i].value_handle == value_handle) {
            return i;
        }
    }
    return 0;
}

static void tbs_client_emit_notify_event(gatt_service_client_connection_t * connection_helper, uint16_t value_handle, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    tbs_characteristic_index_t index = gatt_service_client_characteristic_value_handle_to_index( (tbs_client_connection_t *)connection_helper, value_handle );
    tbs_client_emit_read_event( connection_helper, index, att_status, data, data_size );
}

static uint8_t tbs_client_request_send_gatt_query(tbs_client_connection_t * connection, tbs_characteristic_index_t characteristic_index){
     connection->characteristic_index = characteristic_index;

     uint8_t status = gatt_client_request_to_send_gatt_query(&connection->gatt_query_can_send_now, connection->basic_connection.con_handle);
     if (status != ERROR_CODE_SUCCESS){
         connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY;
     }
     return status;
}

static uint8_t tbs_client_request_read_characteristic(uint16_t tbs_cid, tbs_characteristic_index_t characteristic_index){
    tbs_client_connection_t * connection = (tbs_client_connection_t *) gatt_service_client_get_connection_for_cid(&tbs_client, tbs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (connection->state != TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    uint8_t status = gatt_service_client_can_query_characteristic(&connection->basic_connection, (uint8_t) characteristic_index);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    return tbs_client_request_send_gatt_query(connection, characteristic_index);
}

static uint8_t tbs_client_request_write_characteristic_without_response(tbs_client_connection_t * connection, tbs_characteristic_index_t characteristic_index){
    connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE;
    return tbs_client_request_send_gatt_query(connection, characteristic_index);
}

uint8_t telephone_bearer_service_client_set_strength_reporting_interval(uint16_t tbs_cid, uint8_t reporting_interval_s){
    tbs_client_connection_t * connection = (tbs_client_connection_t *) gatt_service_client_get_connection_for_cid(&tbs_client, tbs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    uint8_t *out = connection->serialisation_buffer;
    out[0] = reporting_interval_s;
    connection->data_size = 1;

    return tbs_client_request_write_characteristic_without_response(connection, TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL);
}

uint8_t telephone_bearer_service_client_call_accept(uint16_t tbs_cid, uint8_t call_id){
    tbs_client_connection_t * connection = (tbs_client_connection_t *) gatt_service_client_get_connection_for_cid(&tbs_client, tbs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    uint8_t *out = connection->serialisation_buffer;
    out[0] = TBS_CONTROL_POINT_OPCODE_ACCEPT;
    out[1] = call_id;
    connection->data_size = 2;

    return tbs_client_request_write_characteristic_without_response(connection, TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT);
}

uint8_t telephone_bearer_service_client_call_terminate(uint16_t tbs_cid, uint8_t call_id){
    tbs_client_connection_t * connection = (tbs_client_connection_t *) gatt_service_client_get_connection_for_cid(&tbs_client, tbs_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY) {
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    uint8_t *out = connection->serialisation_buffer;
    out[0] = TBS_CONTROL_POINT_OPCODE_TERMINATE;
    out[1] = call_id;
    connection->data_size = 2;

    return tbs_client_request_write_characteristic_without_response(connection, TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT);
}

uint8_t telephone_bearer_service_client_call_hold(uint16_t tbs_cid, uint8_t call_id) {
    tbs_client_connection_t *connection = (tbs_client_connection_t*) gatt_service_client_get_connection_for_cid(&tbs_client, tbs_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY) {
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    uint8_t *out = connection->serialisation_buffer;
    out[0] = TBS_CONTROL_POINT_OPCODE_LOCAL_HOLD;
    out[1] = call_id;
    connection->data_size = 2;

    return tbs_client_request_write_characteristic_without_response(connection, TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT);
}

uint8_t telephone_bearer_service_client_call_retrieve(uint16_t tbs_cid, uint8_t call_id){
    tbs_client_connection_t * connection = (tbs_client_connection_t *) gatt_service_client_get_connection_for_cid(&tbs_client, tbs_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY) {
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    uint8_t *out = connection->serialisation_buffer;
    out[0] = TBS_CONTROL_POINT_OPCODE_LOCAL_RETRIEVE;
    out[1] = call_id;
    connection->data_size = 2;

    return tbs_client_request_write_characteristic_without_response(connection, TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT);
}

uint8_t telephone_bearer_service_client_call_originate(uint16_t tbs_cid, uint8_t call_id, const char *uri){
    UNUSED(call_id);

    tbs_client_connection_t * connection = (tbs_client_connection_t *) gatt_service_client_get_connection_for_cid(&tbs_client, tbs_cid);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY) {
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    uint16_t length = btstack_min(TBS_CLIENT_SERIALISATION_BUFFER_SIZE, strlen(uri));
    uint8_t *out = connection->serialisation_buffer;
    out[0] = TBS_CONTROL_POINT_OPCODE_ORIGINATE;
    memcpy(&out[1], uri, length);
    connection->data_size = length + 1;

    return tbs_client_request_write_characteristic_without_response(connection, TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT);
}

uint8_t telephone_bearer_service_client_call_join(uint16_t tbs_cid, const uint8_t *call_index_list, uint16_t size) {
    tbs_client_connection_t *connection = (tbs_client_connection_t*) gatt_service_client_get_connection_for_cid(&tbs_client, tbs_cid);

    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY) {
        return ERROR_CODE_CONTROLLER_BUSY;
    }

    uint16_t length = btstack_min(TBS_CLIENT_SERIALISATION_BUFFER_SIZE, size);
    uint8_t *out = connection->serialisation_buffer;
    out[0] = TBS_CONTROL_POINT_OPCODE_JOIN;
    memcpy(&out[1], call_index_list, length);
    connection->data_size = length + 1;

    return tbs_client_request_write_characteristic_without_response(connection, TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT);
}

uint8_t telephone_bearer_service_client_get_provider_name(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_BEARER_PROVIDER_NAME);
}

uint8_t telephone_bearer_service_client_get_list_current_calls(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS);
}

uint8_t telephone_bearer_service_client_get_signal_strength(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH);
}

uint8_t telephone_bearer_service_client_get_signal_strength_reporting_interval(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL);
}

uint8_t telephone_bearer_service_client_get_technology(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_BEARER_TECHNOLOGY);
}

uint8_t telephone_bearer_service_client_get_uci(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_BEARER_UCI);
}

uint8_t telephone_bearer_service_client_get_schemes_supported_list(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_BEARER_URI_SCHEMES_SUPPORTED_LIST);
}

uint8_t telephone_bearer_service_client_get_call_control_point_optional_opcodes(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT_OPTIONAL_OPCODES);
}

uint8_t telephone_bearer_service_client_get_call_friendly_name(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME);
}

uint8_t telephone_bearer_service_client_get_call_state(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_CALL_STATE);
}

uint8_t telephone_bearer_service_client_get_content_control_id(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_CONTENT_CONTROL_ID);
}

uint8_t telephone_bearer_service_client_get_incoming_call(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_INCOMING_CALL);
}

uint8_t telephone_bearer_service_client_get_incoming_call_target_bearer_uri(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI);
}

uint8_t telephone_bearer_service_client_get_status_flags(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_STATUS_FLAGS);
}

uint8_t telephone_bearer_service_client_get_termination_reason(uint16_t cid) {
    return tbs_client_request_read_characteristic(cid, TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON);
}

static void tbs_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    gatt_service_client_connection_t * connection_helper;
    tbs_client_connection_t * connection;
    hci_con_handle_t con_handle;;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&tbs_client, gattservice_subevent_client_connected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);
                    connection = (tbs_client_connection_t *) connection_helper;

                    status = gattservice_subevent_client_connected_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS){
                        tbs_client_connected(connection, status);
                        break;
                    }
                    
#ifdef ENABLE_TESTING_SUPPORT
                    {
                        for (int i = 0; i < TBS_CHARACTERISTICS_NUM; i++){
                            printf("    %#06x %s\n", connection_helper->characteristics[i].value_handle, tbs_characteristic_index_to_name(i));

                        }
                    };
                    printf("TBS Client: connected\n");
#endif                   
                    tbs_client_connected(connection, ERROR_CODE_SUCCESS);
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    connection_helper = gatt_service_client_get_connection_for_cid(&tbs_client, gattservice_subevent_client_disconnected_get_cid(packet));
                    btstack_assert(connection_helper != NULL);
                    tbs_client_replace_subevent_id_and_emit(connection_helper->event_callback, packet, size, LEAUDIO_SUBEVENT_TBS_CLIENT_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;

        case GATT_EVENT_NOTIFICATION:
            con_handle = (hci_con_handle_t)gatt_event_notification_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&tbs_client, con_handle);

            btstack_assert(connection_helper != NULL);

            tbs_client_emit_notify_event(connection_helper, gatt_event_notification_get_value_handle(packet), ATT_ERROR_SUCCESS,
                    gatt_event_notification_get_value(packet),gatt_event_notification_get_value_length(packet));
            break;
        default:
            break;
    }
}

static void tbs_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
    gatt_service_client_connection_t * connection_helper = NULL;
    tbs_client_connection_t * connection = NULL;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            con_handle = (hci_con_handle_t)gatt_event_characteristic_value_query_result_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&tbs_client, con_handle);
            connection = (tbs_client_connection_t *)connection_helper;
            btstack_assert(connection != NULL);
            switch (connection->state){
                case TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT:
                    connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY;
                    tbs_client_emit_read_event(connection_helper, connection->characteristic_index, ATT_ERROR_SUCCESS,
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
            connection_helper = gatt_service_client_get_connection_for_con_handle(&tbs_client, con_handle);
            connection = (tbs_client_connection_t *)connection_helper;
            btstack_assert(connection != NULL);

            switch (connection->state){
                case TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT:
                    tbs_client_emit_done_event(connection_helper, connection->characteristic_index, gatt_event_query_complete_get_att_status(packet));
                    connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY;
                    break;

                default:
                    connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY;
                    break;
            }
            break;

        default:
            break;
    }
}

static void tbs_client_run_for_connection(void * context){
    tbs_client_connection_t * connection = (tbs_client_connection_t *)context;
    hci_con_handle_t con_handle = connection->basic_connection.con_handle;

    uint16_t value_length = connection->data_size;
    uint8_t *value = connection->serialisation_buffer;

    switch (connection->state){
        case TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &tbs_client_handle_gatt_client_event, con_handle, 
                tbs_client_value_handle_for_index(connection));
            break;

        case TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE:
            connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY;

            (void) gatt_client_write_value_of_characteristic_without_response(
                con_handle,
                tbs_client_value_handle_for_index(connection),
                value_length, value);
            
            break;
        case TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;

            (void) gatt_client_write_value_of_characteristic(
                    &tbs_client_handle_gatt_client_event, con_handle,
                    tbs_client_value_handle_for_index(connection),
                    value_length, value);
            break;
        default:
            break;
    }
}

static void tbs_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_trampoline_packet_handler(&tbs_client, packet_type, channel, packet, size);
}

void telephone_bearer_service_client_init(void){
    gatt_service_client_register_client(&tbs_client, &tbs_client_packet_handler_trampoline);
    gatt_service_client_register_packet_handler(&tbs_client, &tbs_client_packet_handler_internal);

    tbs_client.characteristics_desc16_num = TBS_CHARACTERISTICS_NUM;
    tbs_client.characteristics_desc16 = tbs_characteristic_uuids;
}

uint8_t telephone_bearer_service_client_connect(
    hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    tbs_client_connection_t * connection,
    uint8_t  service_index,
    uint16_t * tbs_cid){

    btstack_assert(packet_handler != NULL);
    btstack_assert(connection != NULL);

    connection->gatt_query_can_send_now.callback = &tbs_client_run_for_connection;
    connection->gatt_query_can_send_now.context = (void *)connection;

    connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W4_CONNECTED;
    return gatt_service_client_connect_primary_service(con_handle,
                                                       &tbs_client, &connection->basic_connection,
                                                       ORG_BLUETOOTH_SERVICE_TELEPHONE_BEARER_SERVICE, service_index,
                                                       connection->characteristics_storage, TBS_CHARACTERISTICS_NUM,
                                                       packet_handler, tbs_cid);
}

uint8_t telephone_generic_bearer_service_client_connect(
    hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    tbs_client_connection_t * connection,
    uint8_t  service_index,
    uint16_t * tbs_cid){

    btstack_assert(packet_handler != NULL);
    btstack_assert(connection != NULL);

    connection->gatt_query_can_send_now.callback = &tbs_client_run_for_connection;
    connection->gatt_query_can_send_now.context = (void *)connection;

    connection->state = TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W4_CONNECTED;
    return gatt_service_client_connect_primary_service(con_handle,
                                                       &tbs_client, &connection->basic_connection,
                                                       ORG_BLUETOOTH_SERVICE_GENERIC_TELEPHONE_BEARER_SERVICE,
                                                       service_index,
                                                       connection->characteristics_storage, TBS_CHARACTERISTICS_NUM,
                                                       packet_handler, tbs_cid);
}


uint8_t telephone_bearer_service_client_disconnect(uint16_t tbs_cid){
    tbs_client_connection_t * connection = (tbs_client_connection_t *) gatt_service_client_get_connection_for_cid(&tbs_client, tbs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    gatt_client_remove_gatt_query(&connection->gatt_query_can_send_now, connection->basic_connection.con_handle);
    return gatt_service_client_disconnect(&tbs_client, connection->basic_connection.cid);
}

void telephone_bearer_service_client_deinit(void){
    gatt_service_client_deinit(&tbs_client);
}

