/*
 * Copyright (C) 2022 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "coordinated_set_identification_service_client.c"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "ble/sm.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"
#include "btstack_crypto.h"

#include "le-audio/le_audio_util.h"
#include "le-audio/gatt-service/coordinated_set_identification_service_client.h"
#include "ad_parser.h"
#include "bluetooth_data_types.h"
#include "ble/gatt_service_client.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

typedef enum {
    CSIS_CHARACTERISTIC_INDEX_SIRK = 0,
    CSIS_CHARACTERISTIC_INDEX_SIZE, 
    CSIS_CHARACTERISTIC_INDEX_LOCK, 
    CSIS_CHARACTERISTIC_INDEX_RANK,
    CSIS_CHARACTERISTIC_INDEX_RFU                   
} csis_characteristic_index_t;

// SIRK Calculation
static btstack_crypto_aes128_cmac_t aes128_cmac_request;
static uint8_t  s1[16];
static uint8_t   T[16];
static uint8_t  k1[16];
static const uint8_t s1_string[] = { 'S', 'I', 'R', 'K', 'e', 'n', 'c'};
static uint8_t key_ltk[16];
static bool csis_client_sirk_decryption_ongoing;


// CSIS Client
static btstack_packet_handler_t csis_client_event_callback;
static btstack_linked_list_t    csis_connections;
static uint16_t                 csis_client_cid_counter = 0;

// RSI Calculation
static bool csis_client_rsi_calculation_ongoing;
static btstack_crypto_aes128_t aes128_request;
static uint8_t csis_hash[16];
// - Single check
static uint8_t csis_sirk[16];   // single check
static uint8_t csis_rsi[6];     // single check
// - Find members
static uint16_t csis_client_find_member_epoch;
static uint8_t const * csis_client_find_member_sirk;
static uint8_t csis_client_find_member_num_entries;
static csis_client_rsi_entry_t * csis_client_find_member_entries;
static uint8_t csis_client_find_member_candidates;
static uint8_t csis_client_find_member_next_read;
static uint8_t csis_client_find_member_next_write;
static uint8_t csis_client_find_member_prand_prime[16];

static void csis_client_find_member_next_check(void);

// prototypes
static void csis_client_trigger_next_sirk_calculation(void);
static void csis_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static btstack_packet_callback_registration_t csis_client_hci_event_callback_registration;

#ifdef ENABLE_TESTING_SUPPORT
static char * csis_characteristic_index_name[] = {
    "SIRK",
    "SIZE",
    "LOCK",
    "RANK",
    "RFU"
};

static char * characteristic_index2name(csis_characteristic_index_t index){
    if (index >= CSIS_CHARACTERISTIC_INDEX_RFU){
        return csis_characteristic_index_name[4];
    }
    return csis_characteristic_index_name[(uint8_t) index];
}
#endif

static uint16_t csis_client_get_next_cid(void){
    csis_client_cid_counter = btstack_next_cid_ignoring_zero(csis_client_cid_counter);
    return csis_client_cid_counter;
}

static csis_client_connection_t * csis_client_get_connection_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &csis_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        csis_client_connection_t * connection = (csis_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        return connection;
    }
    return NULL;
}

static csis_client_connection_t * csis_client_get_connection_for_cid(uint16_t csis_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &csis_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        csis_client_connection_t * connection = (csis_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->cid != csis_cid) continue;
        return connection;
    }
    return NULL;
}

static csis_characteristic_index_t csis_get_characteristic_index_for_uuid16(uint16_t uuid16){
    switch (uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_SET_IDENTITY_RESOLVING_KEY_CHARACTERISTIC:
            return CSIS_CHARACTERISTIC_INDEX_SIRK;

        case ORG_BLUETOOTH_CHARACTERISTIC_SIZE_CHARACTERISTIC:
            return CSIS_CHARACTERISTIC_INDEX_SIZE;

        case ORG_BLUETOOTH_CHARACTERISTIC_LOCK_CHARACTERISTIC:
            return CSIS_CHARACTERISTIC_INDEX_LOCK;

        case ORG_BLUETOOTH_CHARACTERISTIC_RANK_CHARACTERISTIC:
            return CSIS_CHARACTERISTIC_INDEX_RANK;

        default:
            return CSIS_CHARACTERISTIC_INDEX_RFU;
    }
}

static csis_characteristic_index_t csis_client_get_characteristic_index_for_value_handle(csis_client_connection_t * connection, uint16_t value_handle){
    uint8_t i;
    for (i = 0; i < (uint8_t)CSIS_CHARACTERISTIC_INDEX_RFU; i++){
        if (connection->characteristics[i].value_handle == value_handle){
            return (csis_characteristic_index_t)i;
        }
    }
    return CSIS_CHARACTERISTIC_INDEX_RFU;
}

static void csis_client_finalize_connection(csis_client_connection_t * connection){
    btstack_linked_list_remove(&csis_connections, (btstack_linked_item_t*) connection);
}

static void csis_client_emit_connection_established(csis_client_connection_t * connection, uint8_t status){
    btstack_assert(csis_client_event_callback != NULL);

    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_CSIS_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    (*csis_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_connected(csis_client_connection_t * connection, uint8_t status) {
    if (status == ERROR_CODE_SUCCESS){
        connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_READY;
        csis_client_emit_connection_established(connection, status);
    } else {
        connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_IDLE;
        csis_client_emit_connection_established(connection, status);
        csis_client_finalize_connection(connection);
    }
}

static void csis_client_emit_disconnect(uint16_t cid){
    btstack_assert(csis_client_event_callback != NULL);

    uint8_t event[5];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_CSIS_CLIENT_DISCONNECTED;
    little_endian_store_16(event, pos, cid);
    (*csis_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_write_lock_complete(csis_client_connection_t * connection, uint8_t status){
    btstack_assert(csis_client_event_callback != NULL);

    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_CSIS_CLIENT_LOCK_WRITE_COMPLETE;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = connection->coordinator_lock;
    (*csis_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_read_remote_lock(csis_client_connection_t * connection, uint8_t status, uint8_t data){
    btstack_assert(csis_client_event_callback != NULL);

    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_CSIS_CLIENT_LOCK;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = data;
    (*csis_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_read_remote_coordinated_set_size(csis_client_connection_t * connection, uint8_t status, uint8_t data){
    btstack_assert(csis_client_event_callback != NULL);

    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_CSIS_CLIENT_COORDINATED_SET_SIZE;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = data;
    (*csis_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_read_remote_rank(csis_client_connection_t * connection, uint8_t status, uint8_t data){
    btstack_assert(csis_client_event_callback != NULL);

    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_CSIS_CLIENT_RANK;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = data;
    (*csis_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_read_remote_sirk(csis_client_connection_t * connection, uint8_t status, csis_sirk_type_t remote_sirk_type, const uint8_t * sirk){
    btstack_assert(csis_client_event_callback != NULL);
    btstack_assert(sirk != NULL);

    uint8_t event[23];
    memset(event, 0, sizeof(event));

    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_CSIS_CLIENT_SIRK;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = (uint8_t)remote_sirk_type;
    reverse_128(sirk, &event[pos]);
    (*csis_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_handle_k1(void * context){
    // decryption complete
    csis_client_sirk_decryption_ongoing = false;
    uint16_t csis_cid = (uint16_t)(uintptr_t)context;
    csis_client_connection_t * connection = csis_client_get_connection_for_cid(csis_cid);
    if (connection != NULL) {
        uint8_t decrypted_sirk[16];
        uint8_t i;
        for (i = 0; i < sizeof(k1); i++) {
            decrypted_sirk[i] = k1[i] ^ connection->remote_sirk[i];
        }
        memcpy(connection->remote_sirk, decrypted_sirk, sizeof(decrypted_sirk));
        connection->remote_sirk_state = CSIS_SIRK_CALCULATION_STATE_READY;
        csis_client_emit_read_remote_sirk(connection, ATT_ERROR_SUCCESS, CSIS_SIRK_TYPE_ENCRYPTED,
                                          connection->remote_sirk);
    }
    csis_client_trigger_next_sirk_calculation();
}

static void csis_client_handle_T(void * context){
    static const uint8_t csis_string[] = { 'c', 's', 'i', 's'};
    btstack_crypto_aes128_cmac_message(&aes128_cmac_request, T, sizeof(csis_string), csis_string,
                                       k1, csis_client_handle_k1, context);
}

static void csis_client_handle_s1(void * context){
    uint16_t csis_cid = (uint16_t)(uintptr_t)context;
    csis_client_connection_t * connection = csis_client_get_connection_for_cid(csis_cid);
    if (connection == NULL){
        csis_client_sirk_decryption_ongoing = false;
    } else {
        // get connection LTK for decryption
        sm_get_ltk(connection->con_handle, key_ltk);
        btstack_crypto_aes128_cmac_message(&aes128_cmac_request, s1, sizeof(key_ltk), key_ltk, T,
                                           csis_client_handle_T, (void *)(uintptr_t) connection->cid);
    }
}

static void csis_client_trigger_next_sirk_calculation(void){
    if (csis_client_sirk_decryption_ongoing){
        return;
    }
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &csis_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        csis_client_connection_t * connection = (csis_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->remote_sirk_state == CSIS_SIRK_CALCULATION_W2_START){
            connection->remote_sirk_state = CSIS_SIRK_CALCULATION_ACTIVE;
            // start calculation
            csis_client_sirk_decryption_ongoing = true;
            btstack_crypto_aes128_cmac_zero(&aes128_cmac_request, sizeof(s1_string), s1_string,
                                            s1, &csis_client_handle_s1, (void *)(uintptr_t) connection->cid);
            break;
        }
    }
}

static void csis_client_handle_value_query_result(csis_client_connection_t * connection, csis_characteristic_index_t index, uint8_t status, const uint8_t * data, uint16_t data_size){
    connection->read_value_status = status;
    connection->read_value_data = 0;
    switch (index){
        case CSIS_CHARACTERISTIC_INDEX_SIRK:
            if (status == ATT_ERROR_SUCCESS){
                if (data_size == 17){
                    // store sirk type in read_value_data field
                    connection->read_value_data = data[0];
                    reverse_128((uint8_t *)&data[1], connection->remote_sirk);
                } else {
                    connection->read_value_status = ATT_ERROR_VALUE_NOT_ALLOWED;
                }
            }
            break;
        case CSIS_CHARACTERISTIC_INDEX_SIZE:
        case CSIS_CHARACTERISTIC_INDEX_LOCK:
        case CSIS_CHARACTERISTIC_INDEX_RANK:
            if (status == ERROR_CODE_SUCCESS){
                if (data_size == 1) {
                    connection->read_value_data = data[0];
                } else {
                    connection->read_value_status = ATT_ERROR_VALUE_NOT_ALLOWED;
                }
            }
            break;
        default:
            break;
    }
}

static void csis_client_report_value_query_result(csis_client_connection_t *connection, uint8_t index) {
    switch (index){
        case CSIS_CHARACTERISTIC_INDEX_SIZE:
            csis_client_emit_read_remote_coordinated_set_size(connection, connection->read_value_status,  connection->read_value_data);
            break;
        case CSIS_CHARACTERISTIC_INDEX_LOCK:
            csis_client_emit_read_remote_lock(connection, connection->read_value_status,  connection->read_value_data);
            break;
        case CSIS_CHARACTERISTIC_INDEX_RANK:
            csis_client_emit_read_remote_rank(connection, connection->read_value_status,  connection->read_value_data);
            break;
        case CSIS_CHARACTERISTIC_INDEX_SIRK:
            switch ((csis_sirk_type_t)connection->read_value_data){
                case CSIS_SIRK_TYPE_ENCRYPTED:
                    connection->remote_sirk_state = CSIS_SIRK_CALCULATION_W2_START;
                    csis_client_trigger_next_sirk_calculation();
                    break;
                case CSIS_SIRK_TYPE_PUBLIC:
                    csis_client_emit_read_remote_sirk(connection, ATT_ERROR_SUCCESS, connection->read_value_data, connection->remote_sirk);
                    break;
                default:
                    connection->read_value_data = CSIS_SIRK_TYPE_PROHIBITED;
                    connection->read_value_status = ATT_ERROR_VALUE_NOT_ALLOWED;
                    csis_client_emit_read_remote_sirk(connection, ATT_ERROR_SUCCESS, connection->read_value_data, connection->remote_sirk);
                    break;
            }
            break;
        default:
            break;
    }
}

static void csis_client_handle_gatt_server_notification(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION){
        return;
    }

    csis_client_connection_t * connection = csis_client_get_connection_for_con_handle(gatt_event_notification_get_handle(packet));
    if (connection == NULL){
        return;
    }

    uint16_t value_handle = gatt_event_notification_get_value_handle(packet);
    uint16_t value_length = gatt_event_notification_get_value_length(packet);
    uint8_t * value = (uint8_t *)gatt_event_notification_get_value(packet);

    // get index for value_handle
    csis_characteristic_index_t index = csis_client_get_characteristic_index_for_value_handle(connection, value_handle);
    csis_client_handle_value_query_result(connection, index, ATT_ERROR_SUCCESS, value, value_length);
    csis_client_report_value_query_result(connection, index);
}

static uint8_t csis_client_register_notification(csis_client_connection_t * connection){
    gatt_client_characteristic_t characteristic;
    if (connection->characteristics[connection->characteristic_index].client_configuration_handle == 0){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    characteristic.value_handle = connection->characteristics[connection->characteristic_index].value_handle;
    characteristic.end_handle = connection->characteristics[connection->characteristic_index].end_handle;
    characteristic.properties = connection->characteristics[connection->characteristic_index].properties;

    uint8_t status = gatt_client_write_client_characteristic_configuration(
                &csis_client_handle_gatt_client_event, 
                connection->con_handle, 
                &characteristic, 
                GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
  
    // notification supported, register for value updates
    if (status == ERROR_CODE_SUCCESS){
        gatt_client_listen_for_characteristic_value_updates(
            &connection->characteristics[connection->characteristic_index].notification_listener, 
            &csis_client_handle_gatt_server_notification, 
            connection->con_handle, &characteristic);
    } 
    return status;
}

static void csis_client_run_for_connection(csis_client_connection_t * connection){
    uint8_t status;
    gatt_client_characteristic_t characteristic;
    gatt_client_service_t service;
    uint8_t value[1];

    switch (connection->state){
        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT;
            gatt_client_discover_primary_services_by_uuid16(&csis_client_handle_gatt_client_event, connection->con_handle, ORG_BLUETOOTH_SERVICE_COORDINATED_SET_IDENTIFICATION_SERVICE);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;
            
            service.start_group_handle = connection->start_handle;
            service.end_group_handle = connection->end_handle;
            service.uuid16 = ORG_BLUETOOTH_SERVICE_COORDINATED_SET_IDENTIFICATION_SERVICE;

            gatt_client_discover_characteristics_for_service(
                &csis_client_handle_gatt_client_event, 
                connection->con_handle, 
                &service);

            break;
        
        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read client characteristic descriptors [index %d]:\n", connection->characteristic_index);
#endif
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT;
            characteristic.value_handle = connection->characteristics[connection->characteristic_index].value_handle;
            characteristic.properties   = connection->characteristics[connection->characteristic_index].properties;
            characteristic.end_handle   = connection->characteristics[connection->characteristic_index].end_handle;

            (void) gatt_client_discover_characteristic_descriptors(&csis_client_handle_gatt_client_event, connection->con_handle, &characteristic);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_CONFIGURATION:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read client characteristic value [index %d, handle 0x%04X]:\n", connection->characteristic_index, connection->characteristics[connection->characteristic_index].client_configuration_handle);
#endif
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT;

            // result in GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT
            (void) gatt_client_read_characteristic_descriptor_using_descriptor_handle(
                &csis_client_handle_gatt_client_event,
                connection->con_handle, 
                connection->characteristics[connection->characteristic_index].client_configuration_handle);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED;
    
            status = csis_client_register_notification(connection);
            switch (status) {
                case ERROR_CODE_SUCCESS:
#ifdef ENABLE_TESTING_SUPPORT
                    printf("Register notification [index %d, handle 0x%04X]: %s\n",
                           connection->characteristic_index,
                           connection->characteristics[connection->characteristic_index].client_configuration_handle,
                           characteristic_index2name((csis_characteristic_index_t) connection->characteristic_index));
#endif
                    return;
                case ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE:
#ifdef ENABLE_TESTING_SUPPORT
                    printf("Notification not supported [index %d, handle 0x%04X]: %s\n",
                           connection->characteristic_index,
                           connection->characteristics[connection->characteristic_index].client_configuration_handle,
                           characteristic_index2name((csis_characteristic_index_t) connection->characteristic_index));
#endif
                    return;
                default:
                    break;
            }

            if (connection->characteristics[connection->characteristic_index].client_configuration_handle != 0){
                connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_CONFIGURATION;
                break;
            }
            
            csis_client_connected(connection, ERROR_CODE_SUCCESS);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_READ;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &csis_client_handle_gatt_client_event, 
                connection->con_handle, 
                connection->characteristics[connection->characteristic_index].value_handle);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_WRITTEN;
            value[0] = (uint8_t)connection->coordinator_lock;

            (void)gatt_client_write_value_of_characteristic(
                &csis_client_handle_gatt_client_event,
                connection->con_handle, 
                connection->characteristics[connection->characteristic_index].value_handle,
                1, 
                value); 
            break;

        default:
            break;
    }
}

static bool csis_client_next_index_for_query(csis_client_connection_t *connection) {
    bool next_query_found = false;
    while (!next_query_found && (connection->characteristic_index < 3)) {
        connection->characteristic_index++;
        if (connection->characteristics[connection->characteristic_index].client_configuration_handle != 0) {
            next_query_found = true;
        }
    }
    return next_query_found;
}

static bool csis_client_handle_query_complete(csis_client_connection_t * connection, uint8_t status){
    if (status != ATT_ERROR_SUCCESS){
        switch (connection->state){
            case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
                csis_client_connected(connection, gatt_service_client_att_status_to_error_code(status));
                return false;
            default:
                break;
        }
    }

    switch (connection->state){
        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            if (connection->service_instances_num == 0){
                csis_client_connected(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                return false;
            }
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS;
            break;
        
        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
            connection->characteristic_index = 0;
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
            if (connection->characteristic_index < 3){
                connection->characteristic_index++;
                connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
                break;
            }

            connection->characteristic_index = 0;
            if (csis_client_next_index_for_query(connection)){
                connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
            } else {
                connection->characteristic_index = 0;
                csis_client_connected(connection, ERROR_CODE_SUCCESS);
            }
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED:
            if (connection->characteristic_index < 2){
                connection->characteristic_index++;
                connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
                break;
            }

            connection->characteristic_index = 0;
            csis_client_connected(connection, ERROR_CODE_SUCCESS);
            break;
      
        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT:
            csis_client_connected(connection, ERROR_CODE_SUCCESS);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_WRITTEN:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_READY;
            csis_client_emit_write_lock_complete(connection, status);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_READ:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_READY;
            if (status != ERROR_CODE_SUCCESS){
                // simulate failed query result
                csis_client_handle_value_query_result(connection, connection->characteristic_index, status, NULL, 0);
            }
            csis_client_report_value_query_result(connection, connection->characteristic_index);
            break;

        default:    
            break;

    }
    return true;
}

static void csis_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    csis_client_connection_t * connection = NULL;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    gatt_client_characteristic_descriptor_t characteristic_descriptor;
    uint8_t index;

    bool call_run = true;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_MTU:
            connection = csis_client_get_connection_for_con_handle(gatt_event_mtu_get_handle(packet));
            btstack_assert(connection != NULL);
            connection->mtu = gatt_event_mtu_get_MTU(packet);
            break;

        case GATT_EVENT_SERVICE_QUERY_RESULT:
            connection = csis_client_get_connection_for_con_handle(gatt_event_service_query_result_get_handle(packet));
            btstack_assert(connection != NULL);

            if (connection->service_instances_num < 1){
                gatt_event_service_query_result_get_service(packet, &service);
                connection->start_handle = service.start_group_handle;
                connection->end_handle   = service.end_group_handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("CSIS Service: start handle 0x%04X, end handle 0x%04X\n", connection->start_handle, connection->end_handle);
#endif          
                connection->service_instances_num++;
            } else {
                log_info("Found more then one CSIS Service instance. ");
            }
            break;
 
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            connection = csis_client_get_connection_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(connection != NULL);

            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
            
            switch (characteristic.uuid16){
                case ORG_BLUETOOTH_CHARACTERISTIC_SET_IDENTITY_RESOLVING_KEY_CHARACTERISTIC:
                case ORG_BLUETOOTH_CHARACTERISTIC_SIZE_CHARACTERISTIC:
                case ORG_BLUETOOTH_CHARACTERISTIC_LOCK_CHARACTERISTIC:
                case ORG_BLUETOOTH_CHARACTERISTIC_RANK_CHARACTERISTIC:
                    index = (uint8_t)csis_get_characteristic_index_for_uuid16(characteristic.uuid16);
                    break;

                default:
                    btstack_assert(false);
                    return;
            }

            connection->characteristics[index].value_handle = characteristic.value_handle;
            connection->characteristics[index].properties = characteristic.properties;
            connection->characteristics[index].end_handle = characteristic.end_handle;
            connection->characteristics[index].uuid16     = characteristic.uuid16;
            

#ifdef ENABLE_TESTING_SUPPORT
            printf("CSIS Characteristic:\n    Attribute Handle 0x%04X, Properties 0x%02X, Handle 0x%04X, UUID 0x%04X, %s\n",
                characteristic.start_handle, 
                characteristic.properties, 
                characteristic.value_handle, characteristic.uuid16, characteristic_index2name((csis_characteristic_index_t)index));
            
#endif
            break;

        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            connection = csis_client_get_connection_for_con_handle(gatt_event_all_characteristic_descriptors_query_result_get_handle(packet));
            btstack_assert(connection != NULL);
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &characteristic_descriptor);
            
            if (characteristic_descriptor.uuid16 != ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION){
                break;
            }
            
            switch (connection->state){
                case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
                    connection->characteristics[connection->characteristic_index].client_configuration_handle = characteristic_descriptor.handle;
                    break;

                default:
                    btstack_assert(false);
                    break;
            }

#ifdef ENABLE_TESTING_SUPPORT
            printf("    CSIS Characteristic Configuration Descriptor:  Handle 0x%04X, UUID 0x%04X\n", 
                characteristic_descriptor.handle,
                characteristic_descriptor.uuid16);
#endif
            break;

        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            connection = csis_client_get_connection_for_con_handle(gatt_event_characteristic_value_query_result_get_handle(packet));
            btstack_assert(connection != NULL);                

            if (connection->state == COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT){
#ifdef ENABLE_TESTING_SUPPORT
                printf("    Received CCC value: ");
                printf_hexdump(gatt_event_characteristic_value_query_result_get_value(packet),  gatt_event_characteristic_value_query_result_get_value_length(packet));
#endif  
                break;
            }

            csis_client_handle_value_query_result(connection, connection->characteristic_index, ATT_ERROR_SUCCESS,
                                                  gatt_event_characteristic_value_query_result_get_value(packet),
                                                  gatt_event_characteristic_value_query_result_get_value_length(packet));
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            connection = csis_client_get_connection_for_con_handle(gatt_event_query_complete_get_handle(packet));
            btstack_assert(connection != NULL);
            call_run = csis_client_handle_query_complete(connection, gatt_event_query_complete_get_att_status(packet));
            break;

        default:
            break;
    }

    if (call_run && (connection != NULL)){
        csis_client_run_for_connection(connection);
    }
}       

uint8_t coordinated_set_identification_service_client_connect(csis_client_connection_t * connection, 
    hci_con_handle_t con_handle, uint16_t * csis_cid){
    
    if (csis_client_get_connection_for_con_handle(con_handle) != NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint16_t cid = csis_client_get_next_cid();
    if (csis_cid != NULL) {
        *csis_cid = cid;
    }

    memset(connection, 0, sizeof(csis_client_connection_t));
    connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE;
    connection->cid = cid;
    connection->con_handle = con_handle;
    connection->mtu = ATT_DEFAULT_MTU;
    connection->service_instances_num = 0;
    btstack_linked_list_add(&csis_connections, (btstack_linked_item_t *) connection);

    csis_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

static void csis_client_hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    
    hci_con_handle_t con_handle;
    csis_client_connection_t * connection;
    
    switch (hci_event_packet_get_type(packet)){
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            connection = csis_client_get_connection_for_con_handle(con_handle);
            if (connection != NULL){
                csis_client_emit_disconnect(connection->cid);
                csis_client_finalize_connection(connection);
            }
            break;
        default:
            break;
    }
}

static uint8_t csis_client_read_characteristics_value(uint16_t ascs_cid, csis_characteristic_index_t index){
    csis_client_connection_t * connection = csis_client_get_connection_for_cid(ascs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    if (connection->characteristics[index].value_handle == 0){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    connection->characteristic_index = index;
    csis_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t coordinated_set_identification_service_client_read_sirk(uint16_t ascs_cid){
    return csis_client_read_characteristics_value(ascs_cid, CSIS_CHARACTERISTIC_INDEX_SIRK);
}

uint8_t coordinated_set_identification_service_client_read_coordinated_set_size(uint16_t ascs_cid){
    return csis_client_read_characteristics_value(ascs_cid, CSIS_CHARACTERISTIC_INDEX_SIZE);
}

uint8_t coordinated_set_identification_service_client_read_member_rank(uint16_t ascs_cid){
    return csis_client_read_characteristics_value(ascs_cid, CSIS_CHARACTERISTIC_INDEX_RANK);
}

uint8_t coordinated_set_identification_service_client_read_member_lock(uint16_t ascs_cid){
    return csis_client_read_characteristics_value(ascs_cid, CSIS_CHARACTERISTIC_INDEX_LOCK);
}

uint8_t coordinated_set_identification_service_client_write_member_lock(uint16_t ascs_cid, csis_member_lock_t lock){
    csis_client_connection_t * connection = csis_client_get_connection_for_cid(ascs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    if (connection->characteristics[CSIS_CHARACTERISTIC_INDEX_LOCK].value_handle == 0){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    connection->characteristic_index = CSIS_CHARACTERISTIC_INDEX_LOCK;
    connection->coordinator_lock = lock;
    csis_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}


void coordinated_set_identification_service_client_init(btstack_packet_handler_t packet_handler){
    btstack_assert(packet_handler != NULL);
    csis_client_event_callback = packet_handler;
    csis_client_rsi_calculation_ongoing = false;
    csis_client_sirk_decryption_ongoing = false;
    csis_client_hci_event_callback_registration.callback = &csis_client_hci_event_handler;
    hci_add_event_handler(&csis_client_hci_event_callback_registration);
}

void coordinated_set_identification_service_client_deinit(void){
    csis_client_event_callback = NULL;
    csis_connections = NULL;
}

static void csis_client_handle_csis_hash(void * arg){
    UNUSED(arg);
    btstack_assert(csis_client_event_callback != NULL);

    bool is_match = memcmp(&csis_rsi[3], &csis_hash[13], 3) == 0;
    csis_client_rsi_calculation_ongoing = false;

    uint8_t event[11];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_CSIS_CLIENT_RSI_MATCH;
    memset(&event[pos], 0, 7);
    pos += 7;
    event[pos++] = is_match ? 1u : 0u;
    
    (*csis_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

uint8_t coordinated_set_identification_service_client_check_rsi(const uint8_t * rsi, const uint8_t * sirk){
    btstack_assert(rsi != NULL);
    btstack_assert(sirk != NULL);
    btstack_assert(csis_client_event_callback != NULL);

    if (csis_client_rsi_calculation_ongoing){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    csis_client_rsi_calculation_ongoing = true;

    memcpy(csis_sirk, sirk, sizeof(csis_sirk));
    memcpy(csis_rsi,   rsi, sizeof(csis_rsi));
    
    uint8_t prand_prime[16];
    memset(prand_prime, 0, 16);
    memcpy(&prand_prime[13], &rsi[0], 3);

    btstack_crypto_aes128_encrypt(&aes128_request, csis_sirk, prand_prime, csis_hash, &csis_client_handle_csis_hash, NULL);
    return ERROR_CODE_SUCCESS;
}

bool coordinated_set_identification_service_client_get_adv_rsi(uint8_t const * const adv_data, uint8_t adv_len, uint8_t * const rsi){
    ad_context_t context;
    for (ad_iterator_init(&context, adv_len, adv_data); ad_iterator_has_more(&context); ad_iterator_next(&context)) {
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len  = ad_iterator_get_data_len(&context);
        const uint8_t *data = ad_iterator_get_data(&context);
        switch (data_type) {
            case BLUETOOTH_DATA_TYPE_RESOLVABLE_SET_IDENTIFIER:
                if (data_len == 6){
                    reverse_48(data, rsi);
                    return true;
                }
                break;
            default:
                break;
        }
    }
    return false;
}

static void csis_client_find_member_handle_csis_hash(void * arg){
    // check epoch to avoid use after (implicit) free
    uint16_t epoch = (uint16_t)(uintptr_t) arg;
    if (epoch != csis_client_find_member_epoch){
        return;
    }

    bool is_match = memcmp(&csis_client_find_member_entries[csis_client_find_member_next_read].rsi[3], &csis_hash[13], 3) == 0;
    log_info("is match %u", (int) is_match);

    uint8_t event[11];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_CSIS_CLIENT_RSI_MATCH;
    event[pos++] = csis_client_find_member_entries[csis_client_find_member_next_read].addr_type;
    reverse_bd_addr(csis_client_find_member_entries[csis_client_find_member_next_read].addr, &event[pos]);
    pos += 6;
    event[pos++] = is_match ? 1u : 0u;
    (*csis_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));

    csis_client_rsi_calculation_ongoing = false;

    csis_client_find_member_candidates--;
    csis_client_find_member_next_read = (csis_client_find_member_next_read + 1) % csis_client_find_member_num_entries;

    csis_client_find_member_next_check();
}

static void csis_client_find_member_next_check(void){
    // find next candidate
    if ((csis_client_rsi_calculation_ongoing == false) && (csis_client_find_member_candidates > 0)){
        csis_client_rsi_calculation_ongoing = true;

        log_info("check %u of %u", csis_client_find_member_next_read, csis_client_find_member_candidates);

        memset(csis_client_find_member_prand_prime, 0, 16);
        memcpy(&csis_client_find_member_prand_prime[13], csis_client_find_member_entries[csis_client_find_member_next_read].rsi, 3);

        btstack_crypto_aes128_encrypt(&aes128_request, csis_client_find_member_sirk, csis_client_find_member_prand_prime, csis_hash,
                                      &csis_client_find_member_handle_csis_hash, (void *)(uintptr_t) csis_client_find_member_epoch);
    }
}

static void coordinated_set_identification_service_client_add_entry(bd_addr_t addr, bd_addr_type_t addr_type, uint8_t const * const rsi){
    csis_client_find_member_candidates++;
    csis_client_find_member_entries[csis_client_find_member_next_write].addr_type = addr_type;
    memcpy(csis_client_find_member_entries[csis_client_find_member_next_write].addr, addr, 6);
    memcpy(csis_client_find_member_entries[csis_client_find_member_next_write].rsi, rsi, 6);
    csis_client_find_member_next_write = (csis_client_find_member_next_write + 1) % csis_client_find_member_num_entries;

    csis_client_find_member_next_check();
}

uint8_t coordinated_set_identification_service_client_find_members(csis_client_rsi_entry_t * entries,
                                                                   uint8_t num_entries,
                                                                   uint8_t const * const sirk){
    btstack_assert(csis_client_event_callback != NULL);

    csis_client_find_member_epoch++;
    csis_client_find_member_candidates = 0;
    csis_client_find_member_entries = entries;
    csis_client_find_member_num_entries = num_entries;
    csis_client_find_member_sirk = sirk;
    csis_client_find_member_next_read = 0;
    csis_client_find_member_next_write = 0;
    // clear addr type
    uint8_t i;
    for (i=0;i<csis_client_find_member_num_entries;i++){
        csis_client_find_member_entries[i].addr_type = BD_ADDR_TYPE_UNKNOWN;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t coordinated_set_identification_service_client_check_advertisement(bd_addr_t addr, bd_addr_type_t addr_type,
                                                                          uint8_t const * const adv_data, uint8_t adv_len){
    if (csis_client_find_member_candidates < csis_client_find_member_num_entries){
        uint8_t rsi[6];
        if (coordinated_set_identification_service_client_get_adv_rsi(adv_data, adv_len, rsi)){
            // if already in rsi list, skip
            uint8_t i;
            for (i=0; i < csis_client_find_member_num_entries; i++){
                if (csis_client_find_member_entries[i].addr_type != addr_type) continue;
                if (memcmp(csis_client_find_member_entries[i].addr, addr, 6) != 0) continue;
                return ERROR_CODE_SUCCESS;
            }
            coordinated_set_identification_service_client_add_entry(addr, addr_type, rsi);
        }
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t coordinated_set_identification_service_client_check_hci_event(uint8_t const * const packet, uint16_t size){
    UNUSED(size);

    bd_addr_t adv_addr;
    bd_addr_type_t adv_addr_type;
    uint8_t adv_size;
    const uint8_t *adv_data;
    switch (packet[0]) {
        case GAP_EVENT_ADVERTISING_REPORT:
            adv_size = gap_event_advertising_report_get_data_length(packet);
            adv_data = gap_event_advertising_report_get_data(packet);
            gap_event_advertising_report_get_address(packet, adv_addr);
            adv_addr_type = gap_event_advertising_report_get_address_type(packet);
            break;
        case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
            adv_size = gap_event_extended_advertising_report_get_data_length(packet);
            adv_data = gap_event_extended_advertising_report_get_data(packet);
            gap_event_extended_advertising_report_get_address(packet, adv_addr);
            adv_addr_type = gap_event_extended_advertising_report_get_address_type(packet) & 1;
            break;
        default:
            return ERROR_CODE_SUCCESS;
    }
    return coordinated_set_identification_service_client_check_advertisement(adv_addr, adv_addr_type, adv_data, adv_size);
}
