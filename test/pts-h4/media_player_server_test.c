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

#define BTSTACK_FILE__ "media_player_server_test.c"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "media_player_server_test.h"
#include "btstack.h"

#define BAP_SERVER_APP_MAX_NUM_CONNECTIONS 2

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

typedef enum {
    BAP_APP_SERVER_STATE_IDLE = 0,
    BAP_APP_SERVER_STATE_CONNECTED
} bap_app_server_state_t;

typedef struct {
    hci_con_handle_t        con_handle;
    bap_app_server_state_t  state;
    bd_addr_t               client_addr;
} bap_app_server_connection_t;

static bap_app_server_connection_t connections[BAP_SERVER_APP_MAX_NUM_CONNECTIONS];

hci_con_handle_t bap_app_server_get_active_con_handle(void){
    return connections[0].con_handle;
}

static void bap_app_server_connection_reset(bap_app_server_connection_t * connection){
    connection->con_handle = HCI_CON_HANDLE_INVALID;
    connection->state = BAP_APP_SERVER_STATE_IDLE;
    memset(connection->client_addr, 0, sizeof(bd_addr_t));
}

static bap_app_server_connection_t * bap_server_app_connections_find(hci_con_handle_t con_handle) {
    uint8_t i;
    for (i = 0; i < BAP_SERVER_APP_MAX_NUM_CONNECTIONS; i++){
        if (connections[i].con_handle == con_handle){
            return &connections[i];
        }
    }
    return NULL;
}

static uint8_t bap_app_server_connection_delete(hci_con_handle_t con_handle){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    bap_app_server_connection_t * connection = bap_server_app_connections_find(con_handle);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    bap_app_server_connection_reset(connection);
    return ERROR_CODE_SUCCESS;
}

static uint8_t bap_app_server_connection_add(hci_con_handle_t con_handle, bd_addr_t * address){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    bap_app_server_connection_t * connection = bap_server_app_connections_find(con_handle);
    if (connection != NULL){
        return ERROR_CODE_REPEATED_ATTEMPTS;
    }
    connection = bap_server_app_connections_find(HCI_CON_HANDLE_INVALID);
    if (connection == NULL){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    connection->con_handle = con_handle;
    connection->state = BAP_APP_SERVER_STATE_CONNECTED;
    memcpy(connection->client_addr, address, sizeof(bd_addr_t));

    return ERROR_CODE_SUCCESS;
}

static void bap_server_app_connections_init() {
    uint8_t i;
    for (i = 0; i < BAP_SERVER_APP_MAX_NUM_CONNECTIONS; i++){
        bap_app_server_connection_reset(&connections[i]);
    }
}


// Advertisement

#define APP_AD_FLAGS 0x06

static uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // RSI
    0x07, BLUETOOTH_DATA_TYPE_RESOLVABLE_SET_IDENTIFIER, 0x28, 0x31, 0xB6, 0x4C, 0x39, 0xCC,
    // CSIS, OTS
    0x05, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x46, 0x18, 0x25, 0x18,
    // Name
    0x04, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'I', 'U', 'T', 
};

static const uint8_t adv_sid = 0;
static uint8_t       adv_handle = 0;
static le_advertising_set_t le_advertising_set;

static const le_extended_advertising_parameters_t extended_params = {
        .advertising_event_properties = 0x13,  //  scannable & connectable
        .primary_advertising_interval_min = 0x030, // 30 ms
        .primary_advertising_interval_max = 0x030, // 30 ms
        .primary_advertising_channel_map = 7,
        .own_address_type = 0,
        .peer_address_type = 0,
        .peer_address =  { 0 },
        .advertising_filter_policy = 0,
        .advertising_tx_power = 10, // 10 dBm
        .primary_advertising_phy = 1, // LE 1M PHY
        .secondary_advertising_max_skip = 0,
        .secondary_advertising_phy = 1, // LE 1M PHY
        .advertising_sid = adv_sid,
        .scan_request_notification_enable = 0,
};

static void setup_advertising(void);

// Object Transfer Server (OTS)
typedef enum {
    OTS_DATABANK_TYPE_EMPTY = 0,
    OTS_DATABANK_TYPE_POPULATED
} ots_databank_type_t;

// Other Operations
static bool ots_server_operation_find_object_with_name(hci_con_handle_t con_handle, const char * name);
static ots_filter_t * ots_server_operation_read_filter(hci_con_handle_t con_handle, uint8_t filter_index);
static bool ots_server_operation_can_allocate_object_of_size(hci_con_handle_t con_handle, uint32_t object_size);

// Object operations
static oacp_result_code_t ots_server_operation_create(hci_con_handle_t con_handle, uint32_t object_size, ots_object_type_t object_type);
static oacp_result_code_t ots_server_operation_delete(hci_con_handle_t con_handle);
static oacp_result_code_t ots_server_operation_calculate_checksum(hci_con_handle_t con_handle, uint32_t offset, uint32_t length, uint32_t * crc_out);
static oacp_result_code_t ots_server_operation_execute(hci_con_handle_t con_handle);
static oacp_result_code_t ots_server_operation_read( hci_con_handle_t con_handle, uint32_t offset, uint32_t length, const uint8_t ** out_buffer);
static oacp_result_code_t ots_server_operation_write(hci_con_handle_t con_handle, uint32_t offset, uint8_t *buffer, uint16_t buffer_size);
static oacp_result_code_t ots_server_operation_increase_allocated_size(hci_con_handle_t con_handle, uint32_t length);
static oacp_result_code_t ots_server_operation_abort(hci_con_handle_t con_handle);


// View operations
static olcp_result_code_t ots_server_operation_first(hci_con_handle_t con_handle);
static olcp_result_code_t ots_server_operation_last(hci_con_handle_t con_handle);
static olcp_result_code_t ots_server_operation_next(hci_con_handle_t con_handle);
static olcp_result_code_t ots_server_operation_previous(hci_con_handle_t con_handle);
static olcp_result_code_t ots_server_operation_goto(hci_con_handle_t con_handle, ots_object_id_t * luid);
static olcp_result_code_t ots_server_operation_sort(hci_con_handle_t con_handle, olcp_list_sort_order_t order);
static olcp_result_code_t ots_server_operation_number_of_objects(hci_con_handle_t con_handle, uint32_t * out_num_objects);
static olcp_result_code_t ots_server_operation_clear_marking(hci_con_handle_t con_handle);

#define OTS_SERVER_MAX_NUM_CLIENTS 3
#define OTS_SERVER_MAX_NUM_OBJECTS 150


static ots_server_connection_t ots_server_connections_storage[OTS_SERVER_MAX_NUM_CLIENTS];


static ots_filter_t ots_filters[OTS_MAX_NUM_FILTERS];

static uint32_t     ots_operations_object_id_counter = 0;

static const ots_operations_t ots_server_operations_impl = {
    .find_object_with_name = &ots_server_operation_find_object_with_name,
    .get_filter = &ots_server_operation_read_filter,
    .can_allocate_object_of_size = &ots_server_operation_can_allocate_object_of_size,

    // object operations
    .create_object   = &ots_server_operation_create,
    .delete_object   = &ots_server_operation_delete,
    .calculate_checksum = &ots_server_operation_calculate_checksum,
    .execute  = &ots_server_operation_execute,
    .read     = &ots_server_operation_read,
    .write    = &ots_server_operation_write,
    .increase_allocated_size = &ots_server_operation_increase_allocated_size,
    .abort    = &ots_server_operation_abort,

    // view operations
    .first              = &ots_server_operation_first,
    .last               = &ots_server_operation_last,
    .next               = &ots_server_operation_next,
    .previous           = &ots_server_operation_previous,
    .go_to              = &ots_server_operation_goto,
    .sort               = &ots_server_operation_sort,
    .number_of_objects  = &ots_server_operation_number_of_objects,
    .clear_marking      = &ots_server_operation_clear_marking
};

//  An ID3v2 tag can be detected with the following pattern:
//     $49 44 33 yy yy xx zz zz zz zz
//   Where yy is less than $FF, xx is the 'flags' byte and zz is less than
//   $80.

static uint8_t ots_object_track_data[] = {
        // 133 bytes Id3v2.4 tag (header (10), no flags, frames 123
        0x49, 0x44, 0x33, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7b, 0x54, 0x58,
        0x58, 0x58, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x6a,
        0x6f, 0x72, 0x5f, 0x62, 0x72, 0x61, 0x6e, 0x64, 0x00, 0x6d, 0x70, 0x34,
        0x32, 0x54, 0x58, 0x58, 0x58, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00,
        0x53, 0x6f, 0x66, 0x74, 0x77, 0x61, 0x72, 0x65, 0x00, 0x4c, 0x61, 0x76,
        0x66, 0x35, 0x37, 0x2e, 0x34, 0x31, 0x2e, 0x31, 0x30, 0x30, 0x54, 0x58,
        0x58, 0x58, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x6d, 0x69, 0x6e,
        0x6f, 0x72, 0x5f, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x30,
        0x54, 0x58, 0x58, 0x58, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x63,
        0x6f, 0x6d, 0x70, 0x61, 0x74, 0x69, 0x62, 0x6c, 0x65, 0x5f, 0x62, 0x72,
        0x61, 0x6e, 0x64, 0x73, 0x00, 0x6d, 0x70, 0x34, 0x32, 0x6d, 0x70, 0x34,
        0x31,
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75, 0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B,
    0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69, 0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67,
    0x38, 0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D, 0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43,
    0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51, 0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F,
    0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05, 0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B,
    0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19, 0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17,
    0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D, 0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0, 0xA2, 0x33,
    0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21, 0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F,
    0xE0, 0x71, 0x03, 0x92, 0xE7, 0x76, 0x04, 0x95, 0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B,
    0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89, 0xF2, 0x63, 0x11, 0x80, 0xF5, 0x64, 0x16, 0x87,
    0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD, 0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
    0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1, 0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF,
    0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5, 0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB,
    0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9, 0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7,
    0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C, 0xDD, 0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3,
    0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1, 0xBA, 0x2B, 0x59, 0xC8, 0xBD, 0x2C, 0x5E, 0xCF,
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75, 0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


static uint8_t ots_object_group_data0[] = {
        (uint8_t)OTS_GROUP_OBJECT_TYPE_TRACK, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        (uint8_t)OTS_GROUP_OBJECT_TYPE_TRACK, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
        (uint8_t)OTS_GROUP_OBJECT_TYPE_TRACK, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
};

static uint8_t ots_object_group_data1[] = {
        (uint8_t)OTS_GROUP_OBJECT_TYPE_TRACK, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
        (uint8_t)OTS_GROUP_OBJECT_TYPE_TRACK, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        (uint8_t)OTS_GROUP_OBJECT_TYPE_TRACK, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
        (uint8_t)OTS_GROUP_OBJECT_TYPE_TRACK, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77
};

static uint8_t ots_object_group_data2[] = {
        (uint8_t)OTS_GROUP_OBJECT_TYPE_TRACK, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
        (uint8_t)OTS_GROUP_OBJECT_TYPE_TRACK, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99
};

static uint8_t ots_object_parent_group_data[] = {
        (uint8_t)OTS_GROUP_OBJECT_TYPE_GROUP, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        (uint8_t)OTS_GROUP_OBJECT_TYPE_GROUP, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
        (uint8_t)OTS_GROUP_OBJECT_TYPE_GROUP, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00
};

static uint8_t ots_object_directory_listing_data[] = {
        (uint8_t)OTS_GROUP_OBJECT_TYPE_GROUP, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,
};


static ots_object_id_t directory_listing = {0,0,0,0,0,0};

static uint8_t ots_object_segments_list_data[] = {
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x31, 0x00, 0x00, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x32, 0xD1, 0x07, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x33, 0xA1, 0x0F, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x34, 0x71, 0x17, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x35, 0x41, 0x1F, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x36, 0x11, 0x27, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x37, 0xE1, 0x2E, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x38, 0xB1, 0x36, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x39, 0x81, 0x3E, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x3A, 0x51, 0x46, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x3B, 0x21, 0x4E, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x3C, 0xF1, 0x55, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x3D, 0xC1, 0x5D, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x3E, 0x91, 0x65, 0x00, 0x00, 
        0x0D, 0x53, 0x65, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x31, 0x31, 0x2d, 0x45, 0x3F, 0x61, 0x6D, 0x00, 0x00, 
};

static uint8_t * ots_object_current_data = ots_object_track_data;
// Media Player Server (MCS)

#define MCS_MEDIA_PLAYER_TIMEOUT_IN_MS  500

typedef struct {
    int8_t p_value;               // 0xFFFFFFFF unknown, or not set
    float  s_value;               // 0xFFFFFFFF unknown, or not set
} mcs_speed_values_mapping_t;

static mcs_speed_values_mapping_t mcs_speed_values_mapping[] = {
    {-128, 0.25}, {-64, 0.5}, {0, 1.0}, {64, 2.0}, {127, 3.957}
};

typedef struct {
    ots_object_id_t parent_group_object_id;
    ots_object_id_t current_group_object_id;

    uint8_t tracks_num;
    mcs_track_t * tracks;
} mcs_track_group_t;

typedef struct {
    media_control_service_server_t media_server;
    uint16_t id;
    uint8_t playback_speed_index; 
    uint8_t seeking_speed_index;
    bool    seeking_forward;     

    mcs_media_state_t media_state;

    uint8_t current_track_index;
    uint8_t current_group_index;

    uint32_t previous_track_position_10ms;
    uint32_t previous_track_index;
    uint32_t previous_group_index;

    uint32_t previous_track_segment_index;
    uint32_t current_track_segment_index;

    uint8_t track_groups_num;
    mcs_track_group_t * track_groups;
} mcs_media_player_t;

static uint16_t current_media_player_id;
static btstack_timer_source_t mcs_seeking_speed_timer;
static mcs_media_player_t media_player1;
static mcs_media_player_t generic_media_player;

static void mcs_seeking_speed_timer_stop(uint16_t media_player_id);
static void mcs_seeking_speed_timer_start(uint16_t media_player_id);

// MCS Test
static mcs_track_t tracksA[] = {
    {30000, 0, {0x11, 0x11, 0x11, 0x11, 0x11, 0x11}, "Object 0", {0x01, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, "https://www.bluetooth.com/icon_url",
            {0x00, 0x00, 0x10, 0x00, 0x11, 0xAA}, 15, {
                    {2000,     0, {0x00, 0x00, 0x00, 0x00, 0x11, 0xA1}, "Segment 11-A1"},
                    {2000,  2001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xA2}, "Segment 11-A2"},
                    {2000,  4001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xA3}, "Segment 11-A3"},
                    {2000,  6001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xA4}, "Segment 11-A4"},
                    {2000,  8001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xA5}, "Segment 11-A5"},
                    {2000, 10001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xA6}, "Segment 11-A6"},
                    {2000, 12001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xA7}, "Segment 11-A7"},
                    {2000, 14001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xA8}, "Segment 11-A8"},
                    {2000, 16001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xA9}, "Segment 11-A9"},
                    {2000, 18001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xAA}, "Segment 11-AA"},
                    {2000, 20001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xAB}, "Segment 11-AB"},
                    {2000, 22001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xAC}, "Segment 11-AC"},
                    {2000, 24001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xAD}, "Segment 11-AD"},
                    {2000, 26001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xAE}, "Segment 11-AE"},
                    {2000, 28001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xAF}, "Segment 11-AF"},
    } },
    {30000, 0, {0x22, 0x22, 0x22, 0x22, 0x22, 0x22}, "Object 1", {0x02, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, "https://www.bluetooth.com/icon_url",
            {0x00, 0x00, 0x20, 0x00, 0x11, 0xBB},15, {
                    {2000,     0, {0x00, 0x00, 0x00, 0x00, 0x11, 0xB1}, "Segment 11-B1"},
                    {2000,  2001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xB2}, "Segment 11-B2"},
                    {2000,  4001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xB3}, "Segment 11-B3"},
                    {2000,  6001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xB4}, "Segment 11-B4"},
                    {2000,  8001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xB5}, "Segment 11-B5"},
                    {2000, 10001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xB6}, "Segment 11-B6"},
                    {2000, 12001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xB7}, "Segment 11-B7"},
                    {2000, 14001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xB8}, "Segment 11-B8"},
                    {2000, 16001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xB9}, "Segment 11-B9"},
                    {2000, 18001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xBA}, "Segment 11-BA"},
                    {2000, 20001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xBB}, "Segment 11-BB"},
                    {2000, 22001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xBC}, "Segment 11-BC"},
                    {2000, 24001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xBD}, "Segment 11-BD"},
                    {2000, 26001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xBE}, "Segment 11-BE"},
                    {2000, 28001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xBF}, "Segment 11-BF"},
    } },
    {30000, 0, {0x33, 0x33, 0x33, 0x33, 0x33, 0x33}, "Object 2", {0x03, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, "https://www.bluetooth.com/icon_url",
            {0x00, 0x00, 0x30, 0x00, 0x11, 0xCC},15, {
                    {2000,     0, {0x00, 0x00, 0x00, 0x00, 0x11, 0xC1}, "Segment 11-C1"},
                    {2000,  2001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xC2}, "Segment 11-C2"},
                    {2000,  4001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xC3}, "Segment 11-C3"},
                    {2000,  6001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xC4}, "Segment 11-C4"},
                    {2000,  8001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xC5}, "Segment 11-C5"},
                    {2000, 10001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xC6}, "Segment 11-C6"},
                    {2000, 12001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xC7}, "Segment 11-C7"},
                    {2000, 14001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xC8}, "Segment 11-C8"},
                    {2000, 16001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xC9}, "Segment 11-C9"},
                    {2000, 18001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xCA}, "Segment 11-CA"},
                    {2000, 20001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xCB}, "Segment 11-CB"},
                    {2000, 22001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xCC}, "Segment 11-CC"},
                    {2000, 24001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xCD}, "Segment 11-CD"},
                    {2000, 26001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xCE}, "Segment 11-CE"},
                    {2000, 28001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xCF}, "Segment 11-CF"},
    } }
};

static mcs_track_t tracksB[] = {
        {30000, 0, {0x44, 0x44, 0x44, 0x44, 0x44, 0x44}, "Track4", {0x04, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, "https://www.bluetooth.com/icon_url",
                {0x00, 0x00, 0x40, 0x00, 0x11, 0xDD},15, {
                    {2000,     0, {0x00, 0x00, 0x00, 0x00, 0x11, 0xD1}, "Segment 11-D1"},
                    {2000,  2001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xD2}, "Segment 11-D2"},
                    {2000,  4001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xD3}, "Segment 11-D3"},
                    {2000,  6001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xD4}, "Segment 11-D4"},
                    {2000,  8001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xD5}, "Segment 11-D5"},
                    {2000, 10001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xD6}, "Segment 11-D6"},
                    {2000, 12001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xD7}, "Segment 11-D7"},
                    {2000, 14001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xD8}, "Segment 11-D8"},
                    {2000, 16001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xD9}, "Segment 11-D9"},
                    {2000, 18001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xDA}, "Segment 11-DA"},
                    {2000, 20001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xDB}, "Segment 11-DB"},
                    {2000, 22001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xDC}, "Segment 11-DC"},
                    {2000, 24001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xDD}, "Segment 11-DD"},
                    {2000, 26001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xDE}, "Segment 11-DE"},
                    {2000, 28001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xDF}, "Segment 11-DF"},
        } },
        {30000, 0, {0x55, 0x55, 0x55, 0x55, 0x55, 0x55}, "Track5", {0x05, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, "https://www.bluetooth.com/icon_url",
                {0x00, 0x00, 0x50, 0x00, 0x11, 0xEE},15, {
                    {2000,     0, {0x00, 0x00, 0x00, 0x00, 0x11, 0xE1}, "Segment 11-E1"},
                    {2000,  2001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xE2}, "Segment 11-E2"},
                    {2000,  4001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xE3}, "Segment 11-E3"},
                    {2000,  6001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xE4}, "Segment 11-E4"},
                    {2000,  8001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xE5}, "Segment 11-E5"},
                    {2000, 10001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xE6}, "Segment 11-E6"},
                    {2000, 12001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xE7}, "Segment 11-E7"},
                    {2000, 14001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xE8}, "Segment 11-E8"},
                    {2000, 16001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xE9}, "Segment 11-E9"},
                    {2000, 18001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xEA}, "Segment 11-EA"},
                    {2000, 20001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xEB}, "Segment 11-EB"},
                    {2000, 22001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xEC}, "Segment 11-EC"},
                    {2000, 24001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xED}, "Segment 11-ED"},
                    {2000, 26001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xEE}, "Segment 11-EE"},
                    {2000, 28001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xEF}, "Segment 11-EF"},
        } },
        {30000, 0, {0x66, 0x66, 0x66, 0x66, 0x66, 0x66}, "Track6", {0x06, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, "https://www.bluetooth.com/icon_url",
                {0x00, 0x00, 0x60, 0x00, 0x11, 0xFF}, 15, {
                    {2000,     0, {0x00, 0x00, 0x00, 0x00, 0x11, 0xF1}, "Segment 11-F1"},
                    {2000,  2001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xF2}, "Segment 11-F2"},
                    {2000,  4001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xF3}, "Segment 11-F3"},
                    {2000,  6001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xF4}, "Segment 11-F4"},
                    {2000,  8001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xF5}, "Segment 11-F5"},
                    {2000, 10001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xF6}, "Segment 11-F6"},
                    {2000, 12001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xF7}, "Segment 11-F7"},
                    {2000, 14001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xF8}, "Segment 11-F8"},
                    {2000, 16001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xF9}, "Segment 11-F9"},
                    {2000, 18001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xFA}, "Segment 11-FA"},
                    {2000, 20001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xFB}, "Segment 11-FB"},
                    {2000, 22001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xFC}, "Segment 11-FC"},
                    {2000, 24001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xFD}, "Segment 11-FD"},
                    {2000, 26001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xFE}, "Segment 11-FE"},
                    {2000, 28001, {0x00, 0x00, 0x00, 0x00, 0x11, 0xFF}, "Segment 11-FF"},
        } },
        {30000, 0, {0x77, 0x77, 0x77, 0x77, 0x77, 0x77}, "Track7", {0x07, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, "https://www.bluetooth.com/icon_url",
                {0x00, 0x00, 0x70, 0x00, 0x11, 0xAA},15, {
                    {2000,     0, {0x00, 0x00, 0x00, 0x00, 0x21, 0xA1}, "Segment 21-A1"},
                    {2000,  2001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xA2}, "Segment 21-A2"},
                    {2000,  4001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xA3}, "Segment 21-A3"},
                    {2000,  6001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xA4}, "Segment 21-A4"},
                    {2000,  8001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xA5}, "Segment 21-A5"},
                    {2000, 10001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xA6}, "Segment 21-A6"},
                    {2000, 12001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xA7}, "Segment 21-A7"},
                    {2000, 14001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xA8}, "Segment 21-A8"},
                    {2000, 16001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xA9}, "Segment 21-A9"},
                    {2000, 18001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xAA}, "Segment 21-AA"},
                    {2000, 20001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xAB}, "Segment 21-AB"},
                    {2000, 22001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xAC}, "Segment 21-AC"},
                    {2000, 24001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xAD}, "Segment 21-AD"},
                    {2000, 26001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xAE}, "Segment 21-AE"},
                    {2000, 28001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xAF}, "Segment 21-AF"}
        } },
};

static mcs_track_t tracksC[] = {
        {30000, 0, {0x88, 0x88, 0x88, 0x88, 0x88, 0x88}, "Track8", {0x08, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, "https://www.bluetooth.com/icon_url",
                {0x00, 0x00, 0x80, 0x00, 0x11, 0xBB},15, {
                    {2000,     0, {0x00, 0x00, 0x00, 0x00, 0x21, 0xB1}, "Segment 21-B1"},
                    {2000,  2001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xB2}, "Segment 21-B2"},
                    {2000,  4001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xB3}, "Segment 21-B3"},
                    {2000,  6001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xB4}, "Segment 21-B4"},
                    {2000,  8001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xB5}, "Segment 21-B5"},
                    {2000, 10001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xB6}, "Segment 21-B6"},
                    {2000, 12001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xB7}, "Segment 21-B7"},
                    {2000, 14001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xB8}, "Segment 21-B8"},
                    {2000, 16001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xB9}, "Segment 21-B9"},
                    {2000, 18001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xBA}, "Segment 21-BA"},
                    {2000, 20001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xBB}, "Segment 21-BB"},
                    {2000, 22001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xBC}, "Segment 21-BC"},
                    {2000, 24001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xBD}, "Segment 21-BD"},
                    {2000, 26001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xBE}, "Segment 21-BE"},
                    {2000, 28001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xBF}, "Segment 21-BF"},
        } },
        {30000, 0, {0x99, 0x99, 0x99, 0x99, 0x99, 0x99}, "Track9", {0x09, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, "https://www.bluetooth.com/icon_url",
                {0x00, 0x00, 0xA0, 0x00, 0x11, 0xCC},15, {
                    {2000,     0, {0x00, 0x00, 0x00, 0x00, 0x21, 0xC1}, "Segment 21-C1"},
                    {2000,  2001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xC2}, "Segment 21-C2"},
                    {2000,  4001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xC3}, "Segment 21-C3"},
                    {2000,  6001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xC4}, "Segment 21-C4"},
                    {2000,  8001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xC5}, "Segment 21-C5"},
                    {2000, 10001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xC6}, "Segment 21-C6"},
                    {2000, 12001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xC7}, "Segment 21-C7"},
                    {2000, 14001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xC8}, "Segment 21-C8"},
                    {2000, 16001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xC9}, "Segment 21-C9"},
                    {2000, 18001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xCA}, "Segment 21-CA"},
                    {2000, 20001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xCB}, "Segment 21-CB"},
                    {2000, 22001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xCC}, "Segment 21-CC"},
                    {2000, 24001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xCD}, "Segment 21-CD"},
                    {2000, 26001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xCE}, "Segment 21-CE"},
                    {2000, 28001, {0x00, 0x00, 0x00, 0x00, 0x21, 0xCF}, "Segment 21-CF"},
        } },
};

//The Object ID value 0x000000000000 is reserved for the Directory Listing Object as described in Section 4.1.
//The Object ID values 0x000000000001 to 0x0000000000FF are reserved for future use.

static mcs_track_group_t  track_groups[] = {
    {{0x00, 0x00, 0x00, 0x00, 0xFF, 0x00},{0x00, 0x00, 0x00, 0x00, 0x01, 0x00}, 3, tracksA},
    {{0x00, 0x00, 0x00, 0x00, 0xFF, 0x00},{0x00, 0x00, 0x00, 0x00, 0x02, 0x00}, 4, tracksB},
    {{0x00, 0x00, 0x00, 0x00, 0xFF, 0x00},{0x00, 0x00, 0x00, 0x00, 0x03, 0x00}, 2, tracksC},
};


static uint32_t     ots_db_objects_num;
static ots_object_t ots_db_objects[OTS_SERVER_MAX_NUM_OBJECTS];
static olcp_list_sort_order_t  ots_db_sort_order;
static ots_databank_type_t ots_db_type;

// precomputed views of sorted indices
static uint32_t ots_db_object_indices_sorted[11][OTS_SERVER_MAX_NUM_OBJECTS] = {
        // OLCP_LIST_SORT_ORDER_NONE
        {0,1,2,3,4,5,6,7,8,9},
        // OLCP_LIST_SORT_ORDER_BY_FIRST_NAME_ASCENDING
        {0,1,2,3,4,5,6,7,8,9},
        // OLCP_LIST_SORT_ORDER_BY_OBJECT_TYPE_ASCENDING
        {0,1,2,3,4,5,6,7,8,9},
        // OLCP_LIST_SORT_ORDER_BY_OBJECT_CURRENT_SIZE_ASCENDING
        {0,1,2,3,4,5,6,7,8,9},
        // OLCP_LIST_SORT_ORDER_BY_FIRST_CREATED_ASCENDING
        {0,1,2,3,4,5,6,7,8,9},
        // OLCP_LIST_SORT_ORDER_BY_LAST_CREATED_ASCENDING
        {0,1,2,3,4,5,6,7,8,9},
        // OLCP_LIST_SORT_ORDER_BY_FIRST_NAME_DESCENDING
        {9,8,7,6,5,4,3,2,1,0},
        // OLCP_LIST_SORT_ORDER_BY_OBJECT_TYPE_DESCENDING
        {9,8,7,6,5,4,3,2,1,0},
        // OLCP_LIST_SORT_ORDER_BY_OBJECT_CURRENT_SIZE_DESCENDING
        {9,8,7,6,5,4,3,2,1,0},
        // OLCP_LIST_SORT_ORDER_BY_FIRST_CREATED_DESCENDING
        {9,8,7,6,5,4,3,2,1,0},
        // OLCP_LIST_SORT_ORDER_BY_LAST_CREATED_DESCENDING
        {9,8,7,6,5,4,3,2,1,0}
};

// active sorted view
static uint32_t * ots_db_object_indices_sorted_view;

// active filters to apply
static uint8_t  ots_db_active_filters_bitmap;

// storage for filtered and sorted = final object indices
static uint32_t ots_db_object_current_indices[OTS_SERVER_MAX_NUM_OBJECTS];
static uint32_t ots_db_object_current_num;

// index into ots_db_object_current_indices
static int ots_db_object_current_index = -1;
static int ots_db_group_current_index = -1;
static uint32_t ots_db_group_current_num;

static char * long_string1 = "Object 0 abcdefghijkabcdefghijkabcdefghijkabcdefghijkab";
static char * long_string2 = "Object 0 ghijkabcdefghijkabcdefghijkabcdefghijkabcdefghijkab";

#define MCS_TRACK_SEGMENT_DURATION_10MS 2000
static uint32_t mcs_track_num_segments(mcs_track_t * track) {
    return track->segments_num;
}

static void setup_advertising(void) {
    gap_extended_advertising_setup(&le_advertising_set, &extended_params, &adv_handle);
    gap_extended_advertising_set_adv_data(adv_handle, sizeof(adv_data), adv_data);
    gap_extended_advertising_start(adv_handle, 0, 0);
}

static void ots_dump_object(ots_object_t * object){
    printf("%2d / %4d, 0x%04x, %10s.......... ", object->current_size, object->allocated_size, object->type, object->name);
    printf_hexdump(&object->luid, 6);
}
static void ots_dump_selection(void){
    printf("\n");
    int i;
    for (i = 0; i < ots_db_object_current_num; i++){
        ots_dump_object(&ots_db_objects[ots_db_object_current_indices[i]]);
    }
    printf("\n");
}

static uint32_t ots_db_get_object_index_from_sorted_view(uint32_t i) {
    if (i > 10){
        return i;
    }
    return ots_db_object_indices_sorted_view[i];
}

static void ots_db_reset_filters(void) {
    memset(ots_filters, 0, sizeof(ots_filters));
    ots_db_active_filters_bitmap = 0;
    int j;
    for (j = 0; j < ots_db_object_current_num ; j++){
        ots_db_object_current_indices[j] = ots_db_get_object_index_from_sorted_view(j);
    }
}

static void ots_db_sort(olcp_list_sort_order_t order){
    switch (order){
        case OLCP_LIST_SORT_ORDER_NONE:
            ots_db_sort_order = 0;
            break;
        case OLCP_LIST_SORT_ORDER_BY_FIRST_NAME_ASCENDING:
            ots_db_sort_order = 1;
            break;
        case OLCP_LIST_SORT_ORDER_BY_OBJECT_TYPE_ASCENDING:
            ots_db_sort_order = 2;
            break;
        case OLCP_LIST_SORT_ORDER_BY_OBJECT_CURRENT_SIZE_ASCENDING:
            ots_db_sort_order = 3;
            break;
        case OLCP_LIST_SORT_ORDER_BY_FIRST_CREATED_ASCENDING:
            ots_db_sort_order = 4;
            break;
        case OLCP_LIST_SORT_ORDER_BY_LAST_CREATED_ASCENDING:
            ots_db_sort_order = 5;
            break;
        case OLCP_LIST_SORT_ORDER_BY_FIRST_NAME_DESCENDING:
            ots_db_sort_order = 6;
            break;
        case OLCP_LIST_SORT_ORDER_BY_OBJECT_TYPE_DESCENDING:
            ots_db_sort_order = 7;
            break;
        case OLCP_LIST_SORT_ORDER_BY_OBJECT_CURRENT_SIZE_DESCENDING:
            ots_db_sort_order = 8;
            break;
        case OLCP_LIST_SORT_ORDER_BY_FIRST_CREATED_DESCENDING:
            ots_db_sort_order = 9;
            break;
        case OLCP_LIST_SORT_ORDER_BY_LAST_CREATED_DESCENDING:
            ots_db_sort_order = 10;
            break;
        default:
            btstack_unreachable();
            break;
    }
    ots_db_object_indices_sorted_view = ots_db_object_indices_sorted[ots_db_sort_order];
    ots_db_object_current_num = ots_db_objects_num;
    ots_db_object_current_index = 0;
    ots_db_reset_filters();
    printf("Sort: num objects in list %d, index %d\n", ots_db_object_current_num, ots_db_object_current_index);
}

static void ots_db_filter(){
    // clear current view
    ots_db_object_current_num = 0;

    // add objects that match filter
    uint8_t i;
    for (i = 0; i < ots_db_objects_num; i++){
        uint32_t object_index = ots_db_get_object_index_from_sorted_view(i);
        const ots_object_t * object = &ots_db_objects[object_index];

        ots_object_type_t object_type;
        uint32_t min_size;
        uint32_t max_size;

        uint8_t filter_index;
        bool keep_object = true;
        for (filter_index = 0; filter_index < OTS_MAX_NUM_FILTERS; filter_index++) {
            if (keep_object == false){
                continue;
            }
            const ots_filter_t * filter = &ots_filters[filter_index];
            switch (filter->type){
                case OTS_FILTER_TYPE_NAME_STARTS_WITH: // var
                    if (strncmp(object->name, (const char *)filter->value, filter->value_length) != 0){
                        keep_object = false;
                    }
                    break;
                case OTS_FILTER_TYPE_NAME_CONTAINS:    // var
                    if (strstr(object->name, (const char *)filter->value) == NULL) {
                        keep_object = false;
                    }
                    break;
                case OTS_FILTER_TYPE_NAME_IS_EXACTLY:  // var
                    if (strcmp(object->name, (const char *)filter->value) != 0){
                        keep_object = false;
                    }
                    break;
//                case OTS_FILTER_TYPE_NAME_ENDS_WITH:   // var
//                    break;
                case OTS_FILTER_TYPE_OBJECT_TYPE:   // 2
                    object_type = (ots_object_type_t )little_endian_read_16(filter->value, 0);
                    if (object->type != object_type){
                        keep_object = false;
                    }
                    break;
//                case OTS_FILTER_TYPE_CREATED_BETWEEN:  // 14
//                    break;
//                case OTS_FILTER_TYPE_MODIFIED_BETWEEN: // 14
//                    break;
                case OTS_FILTER_TYPE_CURRENT_SIZE_BETWEEN:   // 8
                    min_size = little_endian_read_32(filter->value, 0);
                    max_size = little_endian_read_32(filter->value, 4);
                    if ((object->current_size < min_size) || (object->current_size > max_size)){
                        keep_object = false;
                    }
                    break;
//                case OTS_FILTER_TYPE_ALLOCATED_SIZE_BETWEEN: // 8
//                    break;
//                case OTS_FILTER_TYPE_MARKED_OBJECTS:
//                    break;
                default:
                    break;
            }
        }
        if (keep_object){
            ots_db_object_current_indices[ots_db_object_current_num++] = object_index;
        }
    }

    // set current index to first object if not empty
    if (ots_db_object_current_num > 0){
        ots_db_object_current_index = 0;
    } else {
        ots_db_object_current_index = -1;
    }
    printf("Filter: num objects in list %d, index %d\n", ots_db_object_current_num, ots_db_object_current_index);
    ots_dump_selection();
}


static ots_object_t * ots_object_iterator_first(void){
    if (ots_db_object_current_num == 0){
        printf("NULL\n");
        return NULL;
    }
    ots_db_object_current_index = 0;
    return &ots_db_objects[ots_db_object_current_indices[ots_db_object_current_index]];
}

static ots_object_t * ots_object_iterator_last(void){
    if (ots_db_object_current_num == 0){
        printf("NULL\n");
        return NULL;
    }
    ots_db_object_current_index = ots_db_object_current_num - 1;
    return &ots_db_objects[ots_db_object_current_indices[ots_db_object_current_index]];
}

static ots_object_t * ots_object_iterator_next(void){
    if (ots_db_object_current_num < 2){
        printf("NULL\n");
        return NULL;
    }
    if (ots_db_object_current_index >= (ots_db_object_current_num - 1)){
        printf("NULL\n");
        return NULL;
    }
    ots_db_object_current_index++;
    return &ots_db_objects[ots_db_object_current_indices[ots_db_object_current_index]];
}

static ots_object_t * ots_object_iterator_previous(void){
    if (ots_db_object_current_num < 2){
        printf("NULL\n");
        return NULL;
    }
    if (ots_db_object_current_index == 0){
        printf("NULL\n");
        return NULL;
    }
    ots_db_object_current_index--;
    return &ots_db_objects[ots_db_object_current_indices[ots_db_object_current_index]];
}

static mcs_media_player_t * mcs_get_media_player_for_id(uint16_t media_player_id);
static ots_object_t * ots_object_iterator_goto(ots_object_id_t * luid){
    int i;
    printf("OTS Server: GOTO ");
    printf_hexdump(*luid, 6);
    mcs_media_player_t * media_player = mcs_get_media_player_for_id(current_media_player_id);

    for (i = 0; i < ots_db_object_current_num; i++){
        ots_object_t * object = &ots_db_objects[ots_db_object_current_indices[i]];
//        printf("................."); printf_hexdump(&object->luid, 6);
        if (memcmp(&object->luid, *luid, sizeof(ots_object_id_t)) == 0){
            ots_db_object_current_index = i;
            switch (object->type){
                case OTS_OBJECT_TYPE_TRACK:
                    ots_object_current_data = ots_object_track_data;
                    break;
                case OTS_OBJECT_TYPE_GROUP:
                    if (memcmp(&object->luid, track_groups[0].parent_group_object_id, sizeof(ots_object_id_t)) == 0){
                        ots_object_current_data = ots_object_parent_group_data;
                    } else {
                        if (media_player->current_group_index == 0){
                            ots_object_current_data = ots_object_group_data0;
                        } else if (media_player->current_group_index == 1){
                            ots_object_current_data = ots_object_group_data1;
                        } else {
                            ots_object_current_data = ots_object_group_data2;
                        }
                        
                    }

                    break;
                case OTS_OBJECT_TYPE_TRACK_SEGMENTS:
                    ots_object_current_data = ots_object_segments_list_data;
                    break;
                case OTS_OBJECT_TYPE_DIRECTORY_LISTING:
                    if (memcmp(&object->luid, directory_listing, sizeof(ots_object_id_t)) == 0) {
                        ots_object_current_data = ots_object_directory_listing_data;
                    }
                    break;
                default:
                    btstack_assert(false);
                    break;
            }

            return object;
        }
    }
    return NULL;
}

static ots_object_t * ots_db_find_object_with_luid(ots_object_id_t * luid){
    int i;
    for (i = 0; i < ots_db_objects_num; i++){
        ots_object_t * object = &ots_db_objects[i];
        if (memcmp(object->luid, *luid, sizeof(ots_object_id_t)) == 0){
            return object;
        } 
    }
    return NULL;
}

static ots_object_t * ots_db_find_object_with_name(const char * name){
    int i;
    for (i = 0; i < OTS_SERVER_MAX_NUM_OBJECTS; i++){
        if (strcpy(ots_db_objects[i].name, name) == 0){
            return &ots_db_objects[i];
        }
    }
    return NULL;
}

static void ots_db_get_next_object_id(ots_object_id_t * object_id_out){
    ots_operations_object_id_counter++;
    if (ots_operations_object_id_counter < 0x0100) {
        ots_operations_object_id_counter = 0x0100;
    }
    memset((uint8_t *)object_id_out, 0, OTS_OBJECT_ID_LEN);
    little_endian_store_32((uint8_t *)object_id_out, 2, ots_operations_object_id_counter);
}

static ots_object_t * ots_db_delete_malformed_objects(void){
    ots_object_t * object = NULL;
    uint32_t num_deleted_objects = 0;

    int i;
    for (i = 0; i < ots_db_objects_num; i++){
        object = &ots_db_objects[i];
        if (object->first_created.year == 0 || (strlen(object->name) == 0)){
            object->allocated_size = 0;
            num_deleted_objects++;
        }
    }
    ots_db_objects_num -= num_deleted_objects;
    return object;
}


static ots_object_t * ots_db_allocate_object_of_size(uint32_t object_size){
    if (ots_db_objects_num < OTS_SERVER_MAX_NUM_OBJECTS){
        ots_object_t * object = &ots_db_objects[ots_db_objects_num];
        object->allocated_size = object_size;
        ots_db_objects_num++;
        return object;
    }
    printf("Could not allocate space for object in OTS DB\n");
    return NULL;
}

ots_object_t * ots_db_object_add(ots_object_id_t * object_id, char * name, uint32_t properties, ots_object_type_t type_uuid16,
                          uint32_t allocated_size, uint32_t current_size, btstack_utc_t * first_created, btstack_utc_t * last_modified){

    ots_object_t * object = ots_db_find_object_with_luid(object_id);
    if (object != NULL){
        return NULL;
    }

    if (strlen(name) > OTS_MAX_STRING_LENGHT){
        return NULL;
    }

    object = ots_db_allocate_object_of_size(allocated_size);
    if (object == NULL){
        return NULL;
    }

    memcpy(&object->luid, object_id, sizeof(ots_object_id_t));
    btstack_strcpy(object->name, OTS_MAX_STRING_LENGHT, name);
    object->properties = properties;
    object->type = type_uuid16;
    object->current_size = current_size;
    memcpy(&object->first_created, first_created, sizeof(btstack_utc_t));
    memcpy(&object->last_modified, last_modified, sizeof(btstack_utc_t));
    return object;
}

static void ots_db_load_from_memory(uint8_t track_groups_num, mcs_track_group_t * groups){
    ots_db_type = OTS_DATABANK_TYPE_POPULATED;
    uint32_t properties = 0xFF;
    uint16_t i;

    btstack_utc_t first_created = {2023, 6, 22, 10, 59, 30};
    btstack_utc_t last_modified = {2023, 6, 22, 10, 59, 30};

    ots_db_object_add(
            &directory_listing,
            "Directory Listing",
            OBJECT_PROPERTY_MASK_READ,
            OTS_OBJECT_TYPE_DIRECTORY_LISTING,
            // simulate increase of size to test sorting by size
            100,
            sizeof(ots_object_directory_listing_data),
            &first_created,
            &last_modified);

    ots_db_object_add(
            &groups[0].parent_group_object_id,
            "Parent Group",
            properties,
            OTS_OBJECT_TYPE_GROUP,
            // simulate increase of size to test sorting by size
            100,
            sizeof(ots_object_parent_group_data),
            &first_created,
            &last_modified);

    for (i = 0; i <  track_groups_num; i++){
        mcs_track_group_t * track_group = &groups[i];
        uint16_t data_size = 0;

        if (i == 0){
            data_size = sizeof(ots_object_group_data0);
        } else if (i == 1){
            data_size = sizeof(ots_object_group_data1);
        } else {
            data_size = sizeof(ots_object_group_data2);
        }

        ots_db_object_add(
            &track_group->current_group_object_id,
            "Group",
            properties,
            OTS_OBJECT_TYPE_GROUP,
            // simulate increase of size to test sorting by size
            100,
            data_size,
            &first_created,
            &last_modified);

        printf("add group     ");
        printf_hexdump(&track_group->current_group_object_id, 6);

        uint16_t j;
        for (j = 0; j < track_group->tracks_num; j++){
            mcs_track_t * track = &track_group->tracks[j];
            printf("  add track   ");
            printf_hexdump(&track->object_id, 6);

            uint32_t allocated_size = sizeof(ots_object_track_data) - 100 + i * 20 + j;
            uint32_t current_size = 133; //280 + i * 20 + j;
            first_created.seconds = i * 5;
            last_modified.seconds = i * 5;

            ots_db_object_add(
                &track->object_id,
                track->title,
                properties,
                OTS_OBJECT_TYPE_TRACK,
                // simulate increase of size to test sorting by size
                allocated_size,
                current_size,
                &first_created,
                &last_modified);

            printf("  add icon    ");
            printf_hexdump(&track->icon_object_id, 6);
            ots_db_object_add(
                    &track->icon_object_id,
                    "Icon",
                    properties,
                    OTS_OBJECT_TYPE_MEDIA_PLAYER_ICON,
                    allocated_size,
                    current_size,
                    &first_created,
                    &last_modified);


            printf("    add trseg ");
            printf_hexdump(&track->segments_object_id, 6);

            ots_db_object_add(
                &track->segments_object_id,
                "Segments Object",
                properties,
                OTS_OBJECT_TYPE_TRACK_SEGMENTS,
                300,
                sizeof(ots_object_segments_list_data),
                &first_created,
                &last_modified);

        }
    }

    ots_db_sort(ots_db_sort_order);
    ots_db_reset_filters();
    ots_dump_selection();
}

static void ots_db_init(void) {
    ots_db_sort_order = OLCP_LIST_SORT_ORDER_NONE;
    ots_db_objects_num = 0;
    ots_db_type = OTS_DATABANK_TYPE_EMPTY;

    ots_db_active_filters_bitmap = 0;
    ots_db_object_current_num = 0;
    ots_db_object_current_index = -1;

    ots_db_group_current_index = -1;
    ots_db_group_current_num = 0;
}

// OTS server operations

static bool ots_server_operation_find_object_with_name(hci_con_handle_t con_handle, const char * name){
    UNUSED(con_handle);
    return ots_db_find_object_with_name(name) != NULL;
}

static ots_filter_t * ots_server_operation_read_filter(hci_con_handle_t con_handle, uint8_t filter_index){
    UNUSED(con_handle);
    return &ots_filters[filter_index];
}

static bool ots_server_operation_can_allocate_object_of_size(hci_con_handle_t con_handle, uint32_t object_size){
    return ots_db_allocate_object_of_size(object_size) != NULL;
}

static oacp_result_code_t ots_server_operation_create(hci_con_handle_t con_handle, uint32_t object_size, ots_object_type_t object_type){
    UNUSED(con_handle);

    ots_object_id_t uuid;
    ots_db_get_next_object_id(&uuid);
    btstack_utc_t first_created = {0,0,0,0,0,0}; // data not valid
    btstack_utc_t last_modified = {0,0,0,0,0,0};

    // add directory listing
    ots_object_t * object = ots_db_object_add(
            &uuid,
            "",
            OBJECT_PROPERTY_MASK_WRITE,
            object_type,
            // simulate increase of size to test sorting by size
            object_size,
            0,
            &first_created,
            &last_modified);

    object_transfer_service_server_set_current_object(con_handle, object);
    ots_db_reset_filters();
    printf("Create: size %d, type 0x%04x, %s\n", object_size, object_type, bd_addr_to_str(object->luid));

    return OACP_RESULT_CODE_SUCCESS;
}

static oacp_result_code_t ots_server_operation_delete(hci_con_handle_t con_handle){
    // TODO
    printf("Delete\n");
    return OACP_RESULT_CODE_SUCCESS;
}

static const uint8_t * ots_server_get_current_object_bytes(hci_con_handle_t con_handle){
    // TODO; get object data
    return (const uint8_t *)ots_object_current_data;
}

static oacp_result_code_t ots_server_operation_calculate_checksum(hci_con_handle_t con_handle, uint32_t offset, uint32_t length, uint32_t * crc_out){
    const uint8_t * data = ots_server_get_current_object_bytes(con_handle);
    
    uint32_t crc;
    crc = btstack_crc32_init();
    crc = btstack_crc32_update(crc, &data[offset], length);
    crc = btstack_crc32_finalize(crc);
    *crc_out = crc;

    printf("OTS Server: CRC 0x%x\n", crc);
    return OACP_RESULT_CODE_SUCCESS;
}

static oacp_result_code_t ots_server_operation_execute(hci_con_handle_t con_handle){
    // TODO
    return OACP_RESULT_CODE_SUCCESS;
}

static oacp_result_code_t ots_server_operation_read(hci_con_handle_t con_handle, uint32_t offset, uint32_t length, const uint8_t ** out_buffer){
    printf("OTS read [offset %d, len %d]\n", offset, length);
    *out_buffer = NULL;
    if ((offset + length) <= object_transfer_service_server_current_object_size(con_handle)){
        const uint8_t * data = ots_server_get_current_object_bytes(con_handle);
        *out_buffer = &data[offset];
        // sleep(1000);
        return OACP_RESULT_CODE_SUCCESS;
    }
    return OACP_RESULT_CODE_OPERATION_FAILED;
}

static oacp_result_code_t ots_server_operation_write(hci_con_handle_t con_handle, uint32_t offset, uint8_t *buffer, uint16_t buffer_size){
    printf("OTS Server: write[offset %d, len %d]\n", offset, buffer_size);
    memcpy(&ots_object_track_data[offset], buffer, buffer_size);
    return OACP_RESULT_CODE_SUCCESS;
}

static oacp_result_code_t ots_server_operation_increase_allocated_size(hci_con_handle_t con_handle, uint32_t lenght){
    if (lenght > sizeof(ots_object_track_data)){
        return OACP_RESULT_CODE_INSUFFICIENT_RESOURCES;
    }
    return OACP_RESULT_CODE_SUCCESS;
}

static oacp_result_code_t ots_server_operation_abort(hci_con_handle_t con_handle){
    object_transfer_service_server_current_object_set_lock(con_handle, false);
    object_transfer_service_server_current_object_set_transfer_in_progress(con_handle, false);
    return OACP_RESULT_CODE_SUCCESS;
}

static olcp_result_code_t ots_server_operation_first(hci_con_handle_t con_handle){
    ots_object_t * object = ots_object_iterator_first();
    if (object == NULL){
        return OLCP_RESULT_CODE_NO_OBJECT;
    }
    printf("first: ");
    ots_dump_object(object);
    object_transfer_service_server_set_current_object(con_handle, object);
    return OLCP_RESULT_CODE_SUCCESS;
}

static olcp_result_code_t ots_server_operation_last(hci_con_handle_t con_handle){
    ots_object_t * object = ots_object_iterator_last();
    if (object == NULL){
        return OLCP_RESULT_CODE_NO_OBJECT;
    }
    printf("last: ");
    ots_dump_object(object);
    object_transfer_service_server_set_current_object(con_handle, object);
    return OLCP_RESULT_CODE_SUCCESS;
}

static olcp_result_code_t ots_server_operation_next(hci_con_handle_t con_handle){
    ots_object_t * object = ots_object_iterator_next();
    if (object == NULL){
        return OLCP_RESULT_CODE_OUT_OF_BOUNDS;
    }
    printf("next: ");
    ots_dump_object(object);
    object_transfer_service_server_set_current_object(con_handle, object);
    return OLCP_RESULT_CODE_SUCCESS;
}

static olcp_result_code_t ots_server_operation_previous(hci_con_handle_t con_handle){
    ots_object_t * object = ots_object_iterator_previous();
    if (object == NULL){
        return OLCP_RESULT_CODE_OUT_OF_BOUNDS;
    }
    printf("previous: ");
    ots_dump_object(object);
    object_transfer_service_server_set_current_object(con_handle, object);
    return OLCP_RESULT_CODE_SUCCESS;
}
static void mcs_goto_first_track(uint16_t media_player_id);
static mcs_track_t * mcs_get_current_track_for_media_player(mcs_media_player_t * media_player);
static mcs_media_player_t * mcs_get_media_player_for_id(uint16_t media_player_id);


static void mcs_change_current_track_for_luid(mcs_media_player_t * media_player, ots_object_id_t * luid);
static void mcs_change_current_track_segments_to_luid(mcs_media_player_t * media_player, ots_object_id_t * luid);

static olcp_result_code_t ots_server_operation_goto(hci_con_handle_t con_handle, ots_object_id_t * luid){
    ots_object_t * object = ots_object_iterator_goto(luid);
    if (object == NULL){
        return OLCP_RESULT_CODE_OBJECT_ID_NOT_FOUND;
    }
    mcs_track_t * current_track;
    mcs_media_player_t * media_player = mcs_get_media_player_for_id(current_media_player_id);
    switch (object->type) {
        case OTS_OBJECT_TYPE_GROUP:
            printf("MTS APP: ots_server_operation_goto GROUP: ");
            printf_hexdump(object->luid, 6);
            // mcs_change_current_group_for_luid(media_player, luid);
            break;

        case OTS_OBJECT_TYPE_TRACK:
            printf("MTS APP: ots_server_operation_goto TRACK: ");
            printf_hexdump(object->luid, 6);
            mcs_change_current_track_for_luid(media_player, luid);
            break;

        case OTS_OBJECT_TYPE_TRACK_SEGMENTS:
            printf("MTS APP: ots_server_operation_goto TRACK SEGMENT: ");
            printf_hexdump(object->luid, 6);
            mcs_change_current_track_segments_to_luid(media_player, luid);
            break;

        case OTS_OBJECT_TYPE_MEDIA_PLAYER_ICON:
            printf("MTS APP: ots_server_operation_goto MEDIA_PLAYER_ICON: ");
            printf_hexdump(object->luid, 6);
            break;
        default:
            break;
    }
    
    current_track = mcs_get_current_track_for_media_player(media_player);
    btstack_assert(current_track != NULL);

    object_transfer_service_server_set_current_object(con_handle, object);
    return OLCP_RESULT_CODE_SUCCESS;
}


static char * ots_sort_order2string(olcp_list_sort_order_t order){
    switch (order){
        case OLCP_LIST_SORT_ORDER_BY_FIRST_NAME_ASCENDING:
            return "FIRST_NAME_ASCENDING";
        case OLCP_LIST_SORT_ORDER_BY_OBJECT_TYPE_ASCENDING:
            return "OBJECT_TYPE_ASCENDING";
        case OLCP_LIST_SORT_ORDER_BY_OBJECT_CURRENT_SIZE_ASCENDING:
            return "OBJECT_CURRENT_SIZE_ASCENDING";
        case OLCP_LIST_SORT_ORDER_BY_FIRST_CREATED_ASCENDING:
            return "FIRST_CREATED_ASCENDING";
        case OLCP_LIST_SORT_ORDER_BY_LAST_CREATED_ASCENDING:
            return "LAST_CREATED_ASCENDING";
        case OLCP_LIST_SORT_ORDER_BY_FIRST_NAME_DESCENDING:
            return "FIRST_NAME_DESCENDING";
        case OLCP_LIST_SORT_ORDER_BY_OBJECT_TYPE_DESCENDING:
            return "OBJECT_TYPE_DESCENDING";
        case OLCP_LIST_SORT_ORDER_BY_OBJECT_CURRENT_SIZE_DESCENDING:
            return "OBJECT_CURRENT_SIZE_DESCENDING";
        case OLCP_LIST_SORT_ORDER_BY_FIRST_CREATED_DESCENDING:
            return "FIRST_CREATED_DESCENDING";
        case OLCP_LIST_SORT_ORDER_BY_LAST_CREATED_DESCENDING:
            return "LAST_CREATED_DESCENDING";
        default:
            btstack_unreachable();
            break;
    }
}
static olcp_result_code_t ots_server_operation_sort(hci_con_handle_t con_handle, olcp_list_sort_order_t order) {
    // TODO move to event
    printf("Sort: %s\n", ots_sort_order2string(order));
    ots_db_sort_order = order;
    ots_db_sort(ots_db_sort_order);
    ots_dump_selection();
    return OLCP_RESULT_CODE_SUCCESS;
}

static olcp_result_code_t ots_server_operation_number_of_objects(hci_con_handle_t con_handle, uint32_t * out_num_objects){
    *out_num_objects = ots_db_object_current_num;
    return OLCP_RESULT_CODE_SUCCESS;
}

static olcp_result_code_t ots_server_operation_clear_marking(hci_con_handle_t con_handle){
    int i;
    for (i = 0; i < OTS_SERVER_MAX_NUM_OBJECTS; i++){
        printf("propertis: 0x%04X -> ", ots_db_objects[i].properties);
        ots_db_objects[i].properties &= ~OBJECT_PROPERTY_MASK_MARK;
        printf("0x%04X \n", ots_db_objects[i].properties);
    }
    return OLCP_RESULT_CODE_SUCCESS;
}

// OTS Server Operations - END

static mcs_media_player_t * mcs_get_media_player_for_id(uint16_t media_player_id){
    if (media_player_id == media_player1.id){
        return &media_player1;
    }
    if (media_player_id == generic_media_player.id){
        return &generic_media_player;
    }
    return NULL;
}

static mcs_track_group_t * mcs_get_current_group_for_media_player(mcs_media_player_t * media_player){
    return &media_player->track_groups[media_player->current_group_index];
}

static mcs_track_t * mcs_get_current_track_for_media_player(mcs_media_player_t * media_player){
    if (media_player == NULL){
        return NULL;
    }
    mcs_track_group_t * current_track_group = mcs_get_current_group_for_media_player(media_player);

    if (current_track_group == NULL){
        return NULL;
    }

    return &current_track_group->tracks[media_player->current_track_index];
}

static mcs_track_t * mcs_get_current_track_for_media_player_id(uint16_t media_player_id){
    return mcs_get_current_track_for_media_player(mcs_get_media_player_for_id(media_player_id));
}

static bool mcs_next_track_index(mcs_media_player_t * media_player, bool next_forward, uint32_t * track_index);

static void mcs_reset_current_track(mcs_media_player_t * media_player){
    mcs_track_t * current_track = mcs_get_current_track_for_media_player(media_player);
    uint32_t next_track_index;
    mcs_next_track_index(media_player, true, &next_track_index);

    // reset current track
    current_track->track_position_10ms = 0;
    media_control_service_server_set_track_title(media_player->id, current_track->title);
    media_control_service_server_set_track_duration(media_player->id, current_track->track_duration_10ms);
    media_control_service_server_set_track_position(media_player->id, current_track->track_position_10ms);
    media_control_service_server_set_current_track_id(media_player->id, &current_track->object_id);
    media_control_service_server_set_next_track_id(media_player->id, &media_player->track_groups[media_player->current_group_index].tracks[next_track_index].object_id);

    if (current_track->segments_num != 0){
        media_control_service_server_set_current_track_segments_id(media_player->id,
                                                                   &current_track->segments_object_id);
    } else {
        media_control_service_server_set_current_track_segments_id(media_player->id, NULL);
    }


    mcs_track_t * track = mcs_get_current_track_for_media_player_id(current_media_player_id);

    printf("Change track: group [%d]: ", media_player->current_group_index);
    printf_hexdump(media_player->track_groups[media_player->current_group_index].current_group_object_id, 6);
    printf("Change track:  track[%d]: ", media_player->current_track_index);
    printf_hexdump(track->object_id, 6);
}


static void mcs_change_current_track(mcs_media_player_t * media_player, uint32_t track_index){
    //    if (media_player->current_track_index == track_index){
    //        return;
    //    }
    if (media_player->track_groups[media_player->current_group_index].tracks_num <= track_index){
        return;
    }

    printf(" * change_current_track: previous %d, current %d\n", media_player->current_track_index, track_index);
    media_player->previous_track_index = media_player->current_track_index;
    // reset current
    mcs_track_t * current_track = mcs_get_current_track_for_media_player(media_player);
    current_track->track_position_10ms = 0;

    // change track
    media_player->current_track_index = track_index;
    mcs_reset_current_track(media_player);
}

static void mcs_change_current_track_segments_to_luid(mcs_media_player_t * media_player, ots_object_id_t * luid){
    mcs_track_group_t * track_group = &track_groups[media_player->current_group_index];
    mcs_track_t * track = &track_group->tracks[media_player->current_track_index];

    if (track->segments_num != 0){
        media_control_service_server_set_current_track_segments_id(media_player->id,
                                                                   &track->segments_object_id);
    } else {
        media_control_service_server_set_current_track_segments_id(media_player->id, NULL);
    }
}

static void mcs_change_current_group(mcs_media_player_t * media_player, uint32_t group_index){
    if (media_player->current_group_index == group_index){
        return;
    }
    if (media_player->track_groups_num <= group_index){
        return;
    }
    printf(" * change_current_group: previous %d, current %d\n", media_player->current_group_index, group_index);
    media_player->previous_group_index = media_player->current_group_index;

    //mcs_reset_current_track(media_player);
    // change group
    media_player->current_group_index = group_index;
    media_player->current_track_index = 0;

    printf("Change group: [%d]: ", media_player->current_group_index);
    printf_hexdump(media_player->track_groups[media_player->current_group_index].current_group_object_id, 6);

    mcs_track_t * current_track = mcs_get_current_track_for_media_player(media_player);
    uint32_t next_track_index;
    mcs_next_track_index(media_player, true, &next_track_index);

    // reset current track
    current_track->track_position_10ms = 0;
    media_control_service_server_set_track_title(media_player->id, current_track->title);
    media_control_service_server_set_track_duration(media_player->id, current_track->track_duration_10ms);
    media_control_service_server_set_current_track_id(media_player->id, &current_track->object_id);
    media_control_service_server_set_next_track_id(media_player->id, &media_player->track_groups[media_player->current_group_index].tracks[next_track_index].object_id);

    if (current_track->segments_num != 0){
        media_control_service_server_set_current_track_segments_id(media_player->id, &current_track->segments_object_id);
    } else {
        media_control_service_server_set_current_track_segments_id(media_player->id, NULL);
    }
    media_control_service_server_set_parent_group_object_id(media_player->id, &media_player->track_groups[media_player->current_group_index].parent_group_object_id);
    media_control_service_server_set_current_group_object_id(media_player->id, &media_player->track_groups[media_player->current_group_index].current_group_object_id);

    // media_player->media_state = MCS_MEDIA_STATE_PAUSED;

//    mcs_reset_current_track(media_player);
}

static void mcs_change_current_track_for_luid(mcs_media_player_t * media_player, ots_object_id_t * luid){
    uint32_t track_index = media_player->current_track_index;
    mcs_track_group_t * track_group = &track_groups[media_player->current_group_index];

    int i;
    for (i = 0; i < track_group->tracks_num; i++){
        mcs_track_t * track = &track_group->tracks[i];

        if (memcmp(track->object_id, luid, 6) == 0){
            track_index = i;
            break;
        }
    }
    mcs_change_current_track(media_player, track_index);
}

static void mcs_goto_first_track(uint16_t media_player_id) {
    mcs_media_player_t * media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }

    if (media_player->current_track_index == 0){
        return;
    }
    mcs_change_current_track(media_player, 0);
}

static void mcs_goto_last_track(uint16_t media_player_id) {
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    mcs_track_group_t * current_track_group = mcs_get_current_group_for_media_player(media_player);
    if (current_track_group == NULL){
        return;
    }

    if (current_track_group->tracks_num == 0){
        return;
    }

    mcs_change_current_track(media_player, current_track_group->tracks_num - 1);
}

static void mcs_goto_next_track(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    mcs_track_group_t * current_track_group = mcs_get_current_group_for_media_player(media_player);
    if (current_track_group == NULL){
        return;
    }

    if (current_track_group->tracks_num <= 1){
        return;
    }

    if (media_player->current_track_index >= (current_track_group->tracks_num - 1)){
        return;
    }
    mcs_change_current_track(media_player, media_player->current_track_index + 1);
}

static void mcs_goto_previous_track(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    mcs_track_group_t * current_track_group = mcs_get_current_group_for_media_player(media_player);
    if (current_track_group == NULL){
        return;
    }

    if (current_track_group->tracks_num == 0){
        return;
    }

    if (media_player->current_track_index == 0){
        return;
    }
    mcs_change_current_track(media_player, media_player->current_track_index - 1);
}

static void mcs_goto_track(uint16_t media_player_id, int32_t track_index){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    mcs_track_group_t * current_track_group = mcs_get_current_group_for_media_player(media_player);
    if (current_track_group == NULL){
        return;
    }
    if (current_track_group->tracks_num == 0){
        return;
    }
    if (media_player->current_track_index == track_index){
        return;
    }

    if (track_index == 0) {
        return;
    }

    uint32_t new_track_index = media_player->current_track_index;

    if (track_index > 0){
        track_index--;
    }

    if (track_index >= 0){
        if (track_index < current_track_group->tracks_num){
            new_track_index = track_index;
        }
    } else {
        uint32_t abs_track_index = -track_index;
        if (abs_track_index <= current_track_group->tracks_num){
            new_track_index = current_track_group->tracks_num - abs_track_index;
        }
    }

    mcs_change_current_track(media_player, new_track_index);
}

static bool mcs_goto_group(uint16_t media_player_id, int32_t group_index){
    printf("   ** mcs_goto_group: %d\n",  group_index);
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return NULL;
    }
    if (media_player->track_groups_num == 0){
        return NULL;
    }

    media_player->previous_group_index = media_player->current_group_index;
    if (group_index != 0){

        if (group_index > 0 ){
            group_index--;
        }

        if (media_player->current_group_index != group_index){
            if (group_index >= 0 ){
                if (group_index < media_player->track_groups_num){
                    mcs_change_current_group(media_player, group_index);
                }
            } else {
                int32_t abs_group_index = -group_index;
                if (abs_group_index <= media_player->track_groups_num){
                    mcs_change_current_group(media_player, media_player->track_groups_num - abs_group_index);
                }
            }
        }
    }

    printf(" * mcs_goto_group: previous %d, current %d\n",  media_player->previous_group_index, media_player->current_group_index);
    return   media_player->previous_group_index != media_player->current_group_index;
}

static void mcs_goto_previous_group(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    if (media_player->track_groups_num == 0){
        return;
    }
    if (media_player->current_group_index == 0){
        return;
    }
    mcs_change_current_group(media_player, media_player->current_group_index - 1);
 }

static void mcs_goto_first_group(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    if (media_player->track_groups_num == 0){
        return;
    }
    if (media_player->current_group_index == 0){
        return;
    }
    mcs_change_current_group(media_player, 0);
}

static void mcs_goto_next_group(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    if (media_player->track_groups_num == 0){
        return;
    }
    if (media_player->current_group_index >= media_player->track_groups_num){
        return;
    }
    mcs_change_current_group(media_player, media_player->current_group_index + 1);
}

static void mcs_goto_last_group(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    if (media_player->track_groups_num == 0){
        return;
    }
    mcs_change_current_group(media_player, media_player->track_groups_num - 1);
}


static void mcs_current_track_apply_relative_offset(uint16_t media_player_id, int32_t offset_10ms){
    mcs_track_t * track = mcs_get_current_track_for_media_player_id(media_player_id);
    if (track == NULL) {
        return;
    }
    uint32_t position_10ms = track->track_position_10ms;
    printf(" * mcs_current_track_apply_relative_offset: previous position %d, offset %d, ", position_10ms, offset_10ms);
    if (offset_10ms >= 0){
        // forward
        if ((position_10ms + offset_10ms) < track->track_duration_10ms){
            position_10ms += offset_10ms;
        } else {
            position_10ms = track->track_duration_10ms;
        }
    } else {
        // backward
        uint32_t abs_offset = (uint32_t)(-offset_10ms);
        if (position_10ms > abs_offset){
            position_10ms -= offset_10ms;
        } else {
            position_10ms = 0;
        }
    }

    track->track_position_10ms = position_10ms;
    printf("current %d\n", track->track_position_10ms);
}



static uint32_t mcs_track_current_segment(mcs_track_t * track) {
    // uint32_t num_segments = mcs_track_num_segments(track);
    uint32_t current_segment = track->track_position_10ms / MCS_TRACK_SEGMENT_DURATION_10MS + 1;
    return current_segment;
}

static void mcs_current_track_goto_first_segment(mcs_track_t * track) {
    int32_t old_position = track->track_position_10ms;
    track->track_position_10ms = 0;

    printf("mcs_current_track_goto_first_segment: duration %d, num_segments %d, current segment %d, old position %d, new position %d\n",
        track->track_duration_10ms, mcs_track_num_segments(track), mcs_track_current_segment(track), old_position,
        track->track_position_10ms);
}

static void mcs_current_track_go_from_backward_for_num_steps(mcs_track_t * track, uint32_t backward_steps_num) {
    uint32_t step_10ms = backward_steps_num *  MCS_TRACK_SEGMENT_DURATION_10MS;
    uint32_t old_position = track->track_position_10ms;
    track->track_position_10ms = track->track_duration_10ms - step_10ms;

    printf("mcs_current_track_go_from_backward_for_num_steps: duration %d, num_segments %d, current segment %d, old position %d, backward_steps_num %d [%d], new position %d\n",
        track->track_duration_10ms, mcs_track_num_segments(track), mcs_track_current_segment(track), old_position,
        backward_steps_num, step_10ms, track->track_position_10ms);
}

static void mcs_current_track_go_from_begining_for_num_steps(mcs_track_t * track, uint32_t forward_steps_num) {
    uint32_t step_10ms = forward_steps_num * MCS_TRACK_SEGMENT_DURATION_10MS + 1;
    uint32_t old_position = track->track_position_10ms;
    track->track_position_10ms = step_10ms;

    printf("mcs_current_track_go_from_begining_for_num_steps: duration %d, num_segments %d, current segment %d, old position %d, forward_steps_num %d [%d], new position %d\n",
        track->track_duration_10ms, mcs_track_num_segments(track), mcs_track_current_segment(track), old_position,
        forward_steps_num, step_10ms, track->track_position_10ms);
}

static void mcs_current_track_goto_last_segment(mcs_track_t * track) {
    uint32_t old_position = track->track_position_10ms;
    track->track_position_10ms = track->track_duration_10ms - MCS_TRACK_SEGMENT_DURATION_10MS + 1;
            printf("mcs_current_track_goto_last_segment: duration %d, num_segments %d, current segment %d, old position %d, new position %d\n",
        track->track_duration_10ms, mcs_track_num_segments(track), mcs_track_current_segment(track), old_position, track->track_position_10ms);
}

static void mcs_current_track_goto_segment(mcs_track_t * track, int32_t segment_index) {

    if (segment_index > 0) {
        if (segment_index < mcs_track_num_segments(track)){
            printf("    1: ");
            mcs_current_track_go_from_begining_for_num_steps(track, (segment_index - 1));
        } else {
            printf("    2: ");
            mcs_current_track_goto_last_segment(track);
        }
    } else if (segment_index < 0){
        uint32_t abs_index = -segment_index;
        if (abs_index < mcs_track_num_segments(track)){
            printf("    3: ");
            mcs_current_track_go_from_backward_for_num_steps(track, abs_index);
        } else {
            printf("    4: ");
            mcs_current_track_goto_first_segment(track);
        }
    }
}

static void mcs_current_track_goto_next_segment(mcs_track_t * track) {
    mcs_current_track_goto_segment(track, mcs_track_current_segment(track) + 1);
}

static void mcs_current_track_goto_previous_segment(mcs_track_t * track) {
    printf("mcs_current_track_goto_previous_segment -> current %d \n", mcs_track_current_segment(track));
    mcs_current_track_goto_segment(track, mcs_track_current_segment(track) - 1);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    hci_con_handle_t con_handle;
    bd_addr_t client_address;
    uint8_t status;
    bap_app_server_connection_t * connection;

    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CAN_SEND_NOW:
            // att_server_notify(con_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
            break;
       case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Accept Just Works\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
       case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;

       case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    hci_subevent_le_connection_complete_get_peer_address(packet, client_address);

                    status = bap_app_server_connection_add(con_handle, &client_address);
                    if (status != ERROR_CODE_SUCCESS){
                        printf("BAP Server: Connection to %s failed, error 0x%02x\n", bd_addr_to_str(client_address), status);
                        break;
                    }

                    printf("BAP Server: Connection to %s established\n", bd_addr_to_str(client_address));
                    mcs_track_t * track = mcs_get_current_track_for_media_player_id(current_media_player_id);
                    mcs_media_player_t * media_player = mcs_get_media_player_for_id(current_media_player_id);

                    printf("BAP Server: current group[%d]: ", media_player->current_group_index);
                    printf_hexdump(media_player->track_groups[media_player->current_group_index].current_group_object_id, 6);
                    printf("BAP Server: current track[%d]: ", media_player->current_track_index);
                    printf_hexdump(track->object_id, 6);
                    printf("BAP Server: current track segments ID[%d]: ", media_player->current_track_segment_index);
                    printf_hexdump(track->segments_object_id, 6);

                    if (track != NULL){
                        ots_object_t * object = ots_db_find_object_with_luid(&track->object_id);
                        if (object != NULL){
                            object_transfer_service_server_set_current_object(con_handle, object);
                            object_transfer_service_server_update_current_object_name(con_handle, long_string1);
                        }
                    }
                    break;
                case HCI_SUBEVENT_LE_ADVERTISING_SET_TERMINATED:
                    // restart advertising
                    gap_extended_advertising_start(adv_handle, 0, 0);
                    break;
                default:
                    return;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            connection = bap_server_app_connections_find(con_handle);
            if (connection == NULL){
                break;
            }
            printf("BAP Server: Disconnected from %s\n", bd_addr_to_str(connection->client_addr));
            bap_app_server_connection_delete(con_handle);
            mcs_seeking_speed_timer_stop(current_media_player_id);
            break;

        default:
            break;
    }
}


static float mcs_playing_speed_step(mcs_media_player_t * media_player){
    mcs_track_t * current_track = mcs_get_current_track_for_media_player(media_player);
    btstack_assert(current_track != NULL);

    float playback_speed = mcs_speed_values_mapping[media_player->playback_speed_index].s_value;
    uint32_t track_duration_ms = current_track->track_duration_10ms * 10;
    uint32_t playing_speed_delta = (uint32_t)(((float)track_duration_ms / MCS_MEDIA_PLAYER_TIMEOUT_IN_MS) * playback_speed);
    return playing_speed_delta;
}

static float mcs_seeking_speed_step(mcs_media_player_t * media_player){
    float seeking_speed  = mcs_speed_values_mapping[media_player->seeking_speed_index].s_value;

    uint32_t playing_speed_delta = mcs_playing_speed_step(media_player);
    uint32_t seeking_speed_delta = (uint32_t)((float)playing_speed_delta * seeking_speed);
    return seeking_speed_delta;
}

static bool mcs_next_track_index(mcs_media_player_t * media_player, bool next_forward, uint32_t * track_index){
    // TODO: set new track, respect playing order, and/or stop seeking at the end
    mcs_track_group_t * current_group = mcs_get_current_group_for_media_player(media_player);
    btstack_assert(current_group != NULL);

    if (next_forward){
        if ((media_player->current_track_index + 1) < current_group->tracks_num){
            *track_index = media_player->current_track_index + 1;
        } else {
            *track_index = 0;
        }
    } else {
        if (media_player->current_track_index <= 1){
            *track_index = current_group->tracks_num - 1;
        } else {
            *track_index = media_player->current_track_index - 1;
        }
    }
    return true;
}

static void mcs_move_forward_with_speed_ms(mcs_media_player_t * media_player, uint32_t speed_delta_ms){
    mcs_track_t * current_track = mcs_get_current_track_for_media_player(media_player);
    btstack_assert(current_track != NULL);

    uint32_t track_duration_ms = current_track->track_duration_10ms * 10;
    uint32_t track_position_ms = current_track->track_position_10ms * 10;
    uint32_t remaining_track_ms = track_duration_ms - track_position_ms;

    if (speed_delta_ms > remaining_track_ms){
        // printf("- switch to the next track\n");
        // check if there is next track available
        uint32_t track_index;
        bool track_changed = mcs_next_track_index(media_player, true, &track_index);

        if (track_changed){
            mcs_change_current_track(media_player, track_index);
            current_track = mcs_get_current_track_for_media_player(media_player);
            btstack_assert(current_track != NULL);

            uint32_t remaining_playing_speed_delta_ms = speed_delta_ms - remaining_track_ms;
            current_track->track_position_10ms = remaining_playing_speed_delta_ms/10;
            // printf("- go forward 1, index %d, position %d, remaining %d \n", media_player->current_track_index, track_position_ms, remaining_playing_speed_delta_ms);
        }
    } else {
        current_track->track_position_10ms += speed_delta_ms/10;
        // printf("- advance with the same track, new pos %d\n", current_track->track_position_10ms);
    }
}

static void mcs_seek_forward(mcs_media_player_t * media_player){
    uint32_t seeking_speed_delta_ms = mcs_seeking_speed_step(media_player);
    mcs_move_forward_with_speed_ms(media_player, seeking_speed_delta_ms);
}

static void mcs_play(mcs_media_player_t * media_player){
    uint32_t playing_speed_delta_ms = mcs_playing_speed_step(media_player);
    mcs_move_forward_with_speed_ms(media_player, playing_speed_delta_ms);
}

static void mcs_seek_backward(mcs_media_player_t * media_player){
    mcs_track_t * current_track = mcs_get_current_track_for_media_player(media_player);
    btstack_assert(current_track != NULL);

    uint32_t seeking_speed_delta_ms = mcs_seeking_speed_step(media_player);

    // uint32_t track_duration_ms = current_track->track_duration_10ms * 10;
    uint32_t track_position_ms = current_track->track_position_10ms * 10;
    uint32_t remaining_track_ms = track_position_ms;

    // printf("try seek backward, index %d, duration %d, position %d, step %d, remaining %d \n", media_player->current_track_index, track_duration_ms, track_position_ms, seeking_speed_delta_ms, remaining_track_ms);
    if (seeking_speed_delta_ms > remaining_track_ms){
         // printf("- switch to the previous track\n");

        mcs_reset_current_track(media_player);

        uint32_t track_index;
        bool track_changed = mcs_next_track_index(media_player, true, &track_index);

        if (track_changed){
            mcs_change_current_track(media_player, track_index);
            current_track = mcs_get_current_track_for_media_player(media_player);
            btstack_assert(current_track != NULL);

            uint32_t remaining_seeking_speed_delta_ms = seeking_speed_delta_ms - remaining_track_ms;
            current_track->track_position_10ms = current_track->track_duration_10ms - remaining_seeking_speed_delta_ms/10;
            // printf("- go backward, index %d, duration %d, position %d, remaining %d \n",
                // media_player->current_track_index, current_track->track_duration_10ms,
                // current_track->track_position_10ms,
                // remaining_seeking_speed_delta_ms);
        }
    } else {
        // printf("- go backward with the same track\n");
        current_track->track_position_10ms -= seeking_speed_delta_ms/10;
    }
}

static void mcs_server_trigger_notifications_for_opcode(mcs_media_player_t * media_player, media_control_point_opcode_t opcode){
    mcs_track_t * track = mcs_get_current_track_for_media_player(media_player);
    btstack_assert(track != NULL);

    bool mcs_track_changed = ( media_player->previous_group_index != media_player->current_group_index) || ( media_player->previous_track_index != media_player->current_track_index);
    bool mcs_position_changed = ( media_player->previous_track_index == media_player->current_track_index) && (media_player->previous_track_position_10ms != track->track_position_10ms);
    bool ots_change_to_group_object = false;

    bool notify_track_change = mcs_track_changed;
    bool notify_track_position_change = mcs_position_changed;

    switch (opcode){
        // track operation
        case MEDIA_CONTROL_POINT_OPCODE_MOVE_RELATIVE:
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_TRACK:
        case MEDIA_CONTROL_POINT_OPCODE_LAST_TRACK:
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_TRACK:
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_TRACK:
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_TRACK:
            notify_track_change = true;
            switch (media_player->media_state){
                case MCS_MEDIA_STATE_SEEKING:
                case MCS_MEDIA_STATE_PAUSED:
                    notify_track_position_change = true;
                    break;
                default:
                    notify_track_position_change = false;
                    break;
            }
            break;

        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_SEGMENT:
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_SEGMENT:
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_SEGMENT:
        case MEDIA_CONTROL_POINT_OPCODE_LAST_SEGMENT:
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_SEGMENT:
            notify_track_position_change = false;
            break;

        // group command
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_GROUP:
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_GROUP:
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_GROUP:
        case MEDIA_CONTROL_POINT_OPCODE_LAST_GROUP:
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_GROUP:
            switch (media_player->media_state){
                case MCS_MEDIA_STATE_SEEKING:
                case MCS_MEDIA_STATE_PAUSED:
                    notify_track_position_change = true;
                    break;
                default:
                    notify_track_position_change = false;
                    break;
            }
            ots_change_to_group_object = true;
            break;

        case MEDIA_CONTROL_POINT_OPCODE_PAUSE:
        case MEDIA_CONTROL_POINT_OPCODE_STOP:
            notify_track_position_change = true;
            break;
        default:
            break;
    }

    media_control_service_server_update_current_track_info(media_player->id, track);

    if (media_player->previous_group_index != media_player->current_group_index){
        media_player->previous_group_index = media_player->current_group_index;

        // change group
        media_player->current_track_index = 0;
        track = mcs_get_current_track_for_media_player(media_player);


        media_control_service_server_set_parent_group_object_id(media_player->id, &media_player->track_groups[media_player->current_group_index].parent_group_object_id);
        media_control_service_server_set_current_group_object_id(media_player->id, &media_player->track_groups[media_player->current_group_index].current_group_object_id);
        notify_track_change = true;
    }

    if (notify_track_change){
        notify_track_position_change = false;
        printf("Update track info, title %s, length %d, pos %d\n", track->title, track->track_duration_10ms, track->track_position_10ms);
        track->track_position_10ms = 0;
        media_control_service_server_set_media_track_changed(media_player->id);
        media_control_service_server_set_track_duration(media_player->id, track->track_duration_10ms);
        media_control_service_server_set_track_title(media_player->id, track->title);
        media_control_service_server_set_track_position(media_player->id, track->track_position_10ms);
        media_control_service_server_set_current_track_id(media_player->id, &track->object_id);
        media_control_service_server_set_current_track_segments_id(media_player->id, &track->segments_object_id);

        uint32_t next_track_index;
        mcs_next_track_index(media_player, true, &next_track_index);
        media_control_service_server_set_next_track_id(media_player->id, &media_player->track_groups[media_player->current_group_index].tracks[next_track_index].object_id);

        if (media_player->media_state == MCS_MEDIA_STATE_SEEKING){
            uint8_t status = media_control_service_server_set_media_state(media_player->id, MCS_MEDIA_STATE_PLAYING);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_PLAYING;
            }
        }

        if (ots_change_to_group_object == false){
            ots_object_t * object = ots_db_find_object_with_luid(&track->object_id);
            if (object != NULL){
                object_transfer_service_server_set_current_object(bap_app_server_get_active_con_handle(), object);
                object_transfer_service_server_update_current_object_name(bap_app_server_get_active_con_handle(), track->title);
            }
        }

    }

    if (notify_track_position_change){
        notify_track_position_change = false;
        media_control_service_server_set_track_position(media_player->id, track->track_position_10ms);
    }

    if (ots_change_to_group_object){
        ots_object_t * object = ots_db_find_object_with_luid(&media_player->track_groups[media_player->current_group_index].current_group_object_id);
        if (object != NULL){
            object_transfer_service_server_set_current_object(bap_app_server_get_active_con_handle(), object);
            //object_transfer_service_server_update_current_object_name(bap_app_server_con_handle, track->title);
        }
    }

    media_player->previous_group_index = media_player->current_group_index;
    media_player->previous_track_index = media_player->current_track_index;
}

static void mcs_server_trigger_notifications(mcs_media_player_t * media_player){
    mcs_server_trigger_notifications_for_opcode(media_player, MEDIA_CONTROL_POINT_OPCODE_RFU);
}

static void mcs_seeking_speed_timer_timeout_handler(btstack_timer_source_t * timer){
    uint16_t media_player_id = (uint16_t)(uintptr_t) btstack_run_loop_get_timer_context(timer);
    mcs_media_player_t * media_player = mcs_get_media_player_for_id(media_player_id);

    switch (media_player->media_state){
        case MCS_MEDIA_STATE_SEEKING:
            if (media_player->seeking_forward) {
                mcs_seek_forward(media_player);
            } else {
                mcs_seek_backward(media_player);
            }
            break;

        case MCS_MEDIA_STATE_PLAYING:
            mcs_play(media_player);
            break;

        default:
            break;
    }

    mcs_server_trigger_notifications(media_player);
    log_info("timer restart");

    // enforce fixed interval
#if 0
    btstack_run_loop_set_timer(&mcs_seeking_speed_timer, MCS_MEDIA_PLAYER_TIMEOUT_IN_MS);
#else
#ifdef ENABLE_TESTING_SUPPORT_REPLAY
        mcs_seeking_speed_timer.timeout += MCS_MEDIA_PLAYER_TIMEOUT_IN_MS * 1000;
#else
        mcs_seeking_speed_timer.timeout += MCS_MEDIA_PLAYER_TIMEOUT_IN_MS;
#endif
#endif
    btstack_run_loop_add_timer(&mcs_seeking_speed_timer);

}

static void mcs_seeking_speed_timer_start(uint16_t media_player_id){
    mcs_media_player_t * media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }

    btstack_run_loop_set_timer_handler(&mcs_seeking_speed_timer, mcs_seeking_speed_timer_timeout_handler);
    btstack_run_loop_set_timer_context(&mcs_seeking_speed_timer, (void *)(uintptr_t)media_player_id);

    btstack_run_loop_set_timer(&mcs_seeking_speed_timer, MCS_MEDIA_PLAYER_TIMEOUT_IN_MS);
    btstack_run_loop_add_timer(&mcs_seeking_speed_timer);
    log_info("timer start");
}

static void mcs_seeking_speed_timer_stop(uint16_t media_player_id){
    btstack_run_loop_remove_timer(&mcs_seeking_speed_timer);
    log_info("timer stop");
}

static void mcs_server_execute_track_operation(mcs_media_player_t * media_player,
    media_control_point_opcode_t opcode, uint8_t * packet, uint16_t packet_size){

    int32_t value_int32;

    mcs_track_t * track = mcs_get_current_track_for_media_player_id(media_player->id);
    if (track == NULL) {
        return;
    }
    mcs_seeking_speed_timer_stop(media_player->id);

    switch (opcode){
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_TRACK:
            mcs_goto_first_track(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_TRACK:
            mcs_goto_last_track(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_TRACK:
            mcs_goto_previous_track(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_TRACK:
            mcs_goto_next_track(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_TRACK:
            value_int32 = (int32_t) leaudio_subevent_mcs_server_media_control_point_notification_task_get_data(packet);
            if (value_int32 == 0){
                return;
            }
            mcs_goto_track(media_player->id, value_int32);
            break;

        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_SEGMENT:
            printf(" - Goto previous segment, current %d\n", mcs_track_current_segment(track));
            mcs_current_track_goto_previous_segment(track);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_SEGMENT:
            printf(" - Goto next segment, current %d\n", mcs_track_current_segment(track));
            mcs_current_track_goto_next_segment(track);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_SEGMENT:
            printf(" - Goto first segment, current %d\n", mcs_track_current_segment(track));
            mcs_current_track_goto_first_segment(track);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_SEGMENT:
            printf(" - Goto last segment, current %d\n", mcs_track_current_segment(track));
            mcs_current_track_goto_last_segment(track);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_SEGMENT:
            value_int32 = (int32_t) leaudio_subevent_mcs_server_media_control_point_notification_task_get_data(packet);
            printf(" - Goto %d segment, current %d\n", value_int32, mcs_track_current_segment(track));
            mcs_current_track_goto_segment(track, value_int32);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_GROUP:
            mcs_goto_previous_group(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_GROUP:
            mcs_goto_next_group(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_GROUP:
            mcs_goto_first_group(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_GROUP:
            mcs_goto_last_group(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_GROUP:
            value_int32 = (int32_t) leaudio_subevent_mcs_server_media_control_point_notification_task_get_data(packet);
            mcs_goto_group(media_player->id, value_int32);
            break;

        case MEDIA_CONTROL_POINT_OPCODE_MOVE_RELATIVE:
            value_int32 = (int32_t) leaudio_subevent_mcs_server_media_control_point_notification_task_get_data(packet);
            mcs_current_track_apply_relative_offset(media_player->id, value_int32);
            break;

        case MEDIA_CONTROL_POINT_OPCODE_STOP:
            track->track_position_10ms = 0;
            break;

        default:
            break;
    }
    mcs_server_trigger_notifications_for_opcode(media_player, opcode);

    switch (media_player->media_state){
        case MCS_MEDIA_STATE_PLAYING:
        case MCS_MEDIA_STATE_SEEKING:
            mcs_seeking_speed_timer_start(media_player->id);
            break;
        default:
            break;
    }
}

static void ots_dump_filter(uint8_t filter_index);

static void ots_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    uint8_t filter_index;
    ots_object_t * object;

    switch (hci_event_leaudio_meta_get_subevent_code(packet)) {
        case LEAUDIO_SUBEVENT_OTS_SERVER_FILTER:
            filter_index = leaudio_subevent_ots_server_filter_get_filter_index(packet);
            btstack_assert(filter_index < OTS_MAX_NUM_FILTERS);

            ots_filters[filter_index].type = (ots_filter_type_t)leaudio_subevent_ots_server_filter_get_filter_type(packet);
            ots_filters[filter_index].value_length = (ots_filter_type_t)leaudio_subevent_ots_server_filter_get_data_length(packet);
            memcpy(ots_filters[filter_index].value, leaudio_subevent_ots_server_filter_get_data(packet), ots_filters[filter_index].value_length);

            if (ots_filters[filter_index].type == OTS_FILTER_TYPE_NO_FILTER){
                ots_db_active_filters_bitmap &= ~(1 << filter_index);
            } else {
                ots_db_active_filters_bitmap |=  (1 << filter_index);
            }

            ots_dump_filter(filter_index);
            ots_db_filter();
            object = ots_object_iterator_first();
            object_transfer_service_server_set_current_object(leaudio_subevent_ots_server_filter_get_con_handle(packet), object);
            break;
        case LEAUDIO_SUBEVENT_OTS_SERVER_DISCONNECTED:
            ots_db_delete_malformed_objects();
            break;
        default:
            break;
    }
}

static void ots_dump_filter(uint8_t filter_index) {
    printf("received filter[%d/0x%02x] ", filter_index, ots_db_active_filters_bitmap);

    switch (ots_filters[filter_index].type){
        case OTS_FILTER_TYPE_NAME_STARTS_WITH: // var
        case OTS_FILTER_TYPE_NAME_CONTAINS:    // var
        case OTS_FILTER_TYPE_NAME_IS_EXACTLY:  // var
        case OTS_FILTER_TYPE_NAME_ENDS_WITH:   // var
            printf("\"%s\"\n", ots_filters[filter_index].value);
            break;

        case OTS_FILTER_TYPE_OBJECT_TYPE:   // 2
            printf("0x%4x\n", little_endian_read_16(ots_filters[filter_index].value, 0));
            break;
        case OTS_FILTER_TYPE_CURRENT_SIZE_BETWEEN:   // 8
            printf("[%d, %d]\n", little_endian_read_32(ots_filters[filter_index].value, 0), little_endian_read_32(ots_filters[filter_index].value, 4));
            break;

//                case OTS_FILTER_TYPE_CREATED_BETWEEN:  // 14
//                    break;
//                case OTS_FILTER_TYPE_MODIFIED_BETWEEN: // 14
//                    break;
//                case OTS_FILTER_TYPE_ALLOCATED_SIZE_BETWEEN: // 8
//                    break;
//                case OTS_FILTER_TYPE_MARKED_OBJECTS:
//                    break;
        default:
            printf("\n");
            break;
    }
}

static char * search_field_type_str[] = {
        "FIRST_FIELD",
        "TRACK_NAME",
        "ARTIST_NAME",
        "ALBUM_NAME",
        "GROUP_NAME",
        "EARLIEST_YEAR",
        "LATEST_YEAR",
        "GENRE",
        "ONLY_TRACKS",
        "ONLY_GROUPS",
        "RFU"
};

static char * mcs_server_search_type2str(search_control_point_type_t index){
    if (index > SEARCH_CONTROL_POINT_TYPE_RFU){
        return search_field_type_str[SEARCH_CONTROL_POINT_TYPE_RFU];
    }
    return search_field_type_str[index];
}

static void mcs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    uint8_t characteristic_id;
    media_control_point_opcode_t opcode;
    uint16_t media_player_id;
    mcs_media_player_t * media_player;
    uint8_t status;

    const uint8_t * search_data;
    uint8_t search_data_len;
    uint8_t pos = 0;

    switch (hci_event_leaudio_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_MCS_SERVER_VALUE_CHANGED:
            characteristic_id = leaudio_subevent_mcs_server_value_changed_get_characteristic_id(packet);
            printf("MCS Server App: changed value, characteristic_id %d\n", characteristic_id);
            break;

        case LEAUDIO_SUBEVENT_MCS_SERVER_SEARCH_CONTROL_POINT_NOTIFICATION_TASK:
            media_player_id = leaudio_subevent_mcs_server_search_control_point_notification_task_get_media_player_id(packet);
            media_player = mcs_get_media_player_for_id(media_player_id);

            if (media_player == NULL){
                return;
            }
            search_data     = leaudio_subevent_mcs_server_search_control_point_notification_task_get_data(packet);
            search_data_len = leaudio_subevent_mcs_server_search_control_point_notification_task_get_data_length(packet);

            printf("MCS Server App: Search Notification, data len %d\n", search_data_len);

            int num_search_params = 0;
            search_control_point_type_t search_field_type[3];

            pos = 0;
            while (pos < search_data_len){
                uint8_t search_field_data_len = search_data[pos] - 1;
                search_field_type[num_search_params] = (search_control_point_type_t)search_data[pos+1];

                const char * search_field_data;
                if (search_field_data_len > 0){
                    search_field_data = (const char *)&search_data[pos+2];
                }

                if (search_field_data_len > 0){
                    printf("  ** Search field len %d, type %s, data %s\n", search_field_data_len, mcs_server_search_type2str(search_field_type[num_search_params]), search_field_data);
                } else {
                    printf("  ** Search field len %d, type %s\n", search_field_data_len, mcs_server_search_type2str(search_field_type[num_search_params]));
                }
                pos += search_field_data_len + 2;
                num_search_params++;
            }
            // TODO shortcut for PTS test
                switch (search_field_type[0]){
                    case SEARCH_CONTROL_POINT_TYPE_GENRE:
                        media_control_service_server_search_control_point_response(media_player_id, tracksA[0].object_id);
                        break;
                    case SEARCH_CONTROL_POINT_TYPE_GROUP_NAME:
                        media_control_service_server_search_control_point_response(media_player_id, track_groups[0].current_group_object_id);
                        break;
                    case SEARCH_CONTROL_POINT_TYPE_ONLY_GROUPS:
                        media_control_service_server_search_control_point_response(media_player_id, track_groups[0].current_group_object_id);
                        break;
                    case SEARCH_CONTROL_POINT_TYPE_ONLY_TRACKS:
                        media_control_service_server_search_control_point_response(media_player_id, tracksA[0].object_id);
                        break;
                    default:
                        media_control_service_server_search_control_point_response(media_player_id, tracksA[0].object_id);
                        break;
                }

        break;

        case LEAUDIO_SUBEVENT_MCS_SERVER_MEDIA_CONTROL_POINT_NOTIFICATION_TASK:
            opcode = (media_control_point_opcode_t)leaudio_subevent_mcs_server_media_control_point_notification_task_get_opcode(packet);
            media_player_id = leaudio_subevent_mcs_server_media_control_point_notification_task_get_media_player_id(packet);
            media_player = mcs_get_media_player_for_id(media_player_id);

            if (media_player == NULL){
                return;
            }

            printf("MCS Server App: Control Notification, opcode %s, state %s\n",
                mcs_server_media_control_opcode2str(opcode),
                mcs_server_media_state2str(media_player->media_state));

            if (media_player->media_state == MCS_MEDIA_STATE_INACTIVE){
                switch (opcode){
                    case MEDIA_CONTROL_POINT_OPCODE_PLAY:
                    case MEDIA_CONTROL_POINT_OPCODE_PAUSE:
                        // the application should deal with this state below
                        break;
                    default:
                        media_control_service_server_respond_to_media_control_point_command(media_player_id, opcode,
                                                                                            MEDIA_CONTROL_POINT_ERROR_CODE_MEDIA_PLAYER_INACTIVE);
                        return;
                }
            }

            // accept command
            media_control_service_server_respond_to_media_control_point_command(media_player_id, opcode,
                                                                                MEDIA_CONTROL_POINT_ERROR_CODE_SUCCESS);

            switch (opcode){
                case MEDIA_CONTROL_POINT_OPCODE_FAST_REWIND:
                case MEDIA_CONTROL_POINT_OPCODE_FAST_FORWARD:
                    mcs_seeking_speed_timer_stop(media_player_id);

                    if (opcode == MEDIA_CONTROL_POINT_OPCODE_FAST_FORWARD){
                        media_player->seeking_forward = true;
                    } else {
                        media_player->seeking_forward = false;
                    }

                    status = media_control_service_server_set_seeking_speed(media_player_id, 64);
                    if (status != ERROR_CODE_SUCCESS){
                        return;
                    }

                    status = media_control_service_server_set_media_state(media_player_id, MCS_MEDIA_STATE_SEEKING);
                    if (status == ERROR_CODE_SUCCESS){
                        media_player->media_state = MCS_MEDIA_STATE_SEEKING;
                        mcs_seeking_speed_timer_start(media_player_id);
                    }
                    return;

                case MEDIA_CONTROL_POINT_OPCODE_PLAY:
                    if (media_player->media_state == MCS_MEDIA_STATE_PLAYING){
                        // ignore command
                        return;
                    }
                    mcs_seeking_speed_timer_stop(media_player_id);
                    status = media_control_service_server_set_media_state(media_player_id, MCS_MEDIA_STATE_PLAYING);
                    if (status == ERROR_CODE_SUCCESS){
                        media_player->media_state = MCS_MEDIA_STATE_PLAYING;
                        mcs_seeking_speed_timer_start(media_player_id);
                    }
                    return;

                case MEDIA_CONTROL_POINT_OPCODE_PAUSE:
                    if (media_player->media_state == MCS_MEDIA_STATE_PAUSED){
                        // ignore command
                        return;
                    }
                    status = media_control_service_server_set_media_state(media_player_id, MCS_MEDIA_STATE_PAUSED);
                    if (status == ERROR_CODE_SUCCESS){
                         media_player->media_state = MCS_MEDIA_STATE_PAUSED;
                         mcs_seeking_speed_timer_stop(media_player_id);
                    }
                    mcs_server_trigger_notifications_for_opcode(media_player, opcode);
                    return;

                case MEDIA_CONTROL_POINT_OPCODE_STOP:
                    status = media_control_service_server_set_media_state(media_player_id, MCS_MEDIA_STATE_PAUSED);
                    if (status == ERROR_CODE_SUCCESS){
                         media_player->media_state = MCS_MEDIA_STATE_PAUSED;
                    }
                    mcs_server_execute_track_operation(media_player, opcode, packet, size);
                    return;

                case MEDIA_CONTROL_POINT_OPCODE_MOVE_RELATIVE:
                    mcs_server_execute_track_operation(media_player, opcode, packet, size);
                    break;

                case MEDIA_CONTROL_POINT_OPCODE_FIRST_TRACK:
                case MEDIA_CONTROL_POINT_OPCODE_LAST_TRACK:
                case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_TRACK:
                case MEDIA_CONTROL_POINT_OPCODE_NEXT_TRACK:
                case MEDIA_CONTROL_POINT_OPCODE_GOTO_TRACK:
                case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_SEGMENT:
                case MEDIA_CONTROL_POINT_OPCODE_NEXT_SEGMENT:
                case MEDIA_CONTROL_POINT_OPCODE_FIRST_SEGMENT:
                case MEDIA_CONTROL_POINT_OPCODE_LAST_SEGMENT:
                case MEDIA_CONTROL_POINT_OPCODE_GOTO_SEGMENT:
                    mcs_server_execute_track_operation(media_player, opcode, packet, size);
                    return;

                case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_GROUP:
                case MEDIA_CONTROL_POINT_OPCODE_NEXT_GROUP:
                case MEDIA_CONTROL_POINT_OPCODE_FIRST_GROUP:
                case MEDIA_CONTROL_POINT_OPCODE_LAST_GROUP:
                case MEDIA_CONTROL_POINT_OPCODE_GOTO_GROUP:
                    if (media_player->media_state == MCS_MEDIA_STATE_SEEKING){
                        status = media_control_service_server_set_media_state(media_player_id, MCS_MEDIA_STATE_PAUSED);
                        if (status == ERROR_CODE_SUCCESS){
                             mcs_seeking_speed_timer_stop(media_player_id);
                             media_player->media_state = MCS_MEDIA_STATE_PAUSED;
                        }
                    }

                    mcs_server_execute_track_operation(media_player, opcode, packet, size);
                    return;

                default:
                    break;
            }

            break;

        default:
            break;
    }
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);

    return 0;
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    return 0;
}

static void show_usage(void){
    uint8_t iut_address_type;
    bd_addr_t      iut_address;
    gap_le_get_own_address(&iut_address_type, iut_address);

    printf("IUT: addr type %u, addr %s", iut_address_type, bd_addr_to_str(iut_address));

    char * current_media_state = mcs_server_media_state2str(media_control_service_server_get_media_state(current_media_player_id));

    printf("\n## MCS\n");
    printf("0 - set INACTIVE state, current %s\n", current_media_state);
    printf("1 - set PLAYING  state, current %s\n", current_media_state);
    printf("2 - set PAUSED   state, current %s\n", current_media_state);
    printf("3 - set SEEKING  state, current %s\n", current_media_state);

    printf("4 - goto 5th segment\n");
    printf("5 - goto last song\n");
    printf("6 - goto second group\n");
    printf("7 - set long filter\n");
    printf("8 - Reset filters\n");
    printf("9 - Use empty databank\n");

    printf("I - Invalidate current object\n");

    printf("j - set long media player name 1\n");
    printf("J - set long media player name 2\n");
    printf("k - set long track title 1\n");
    printf("K - set long track title 2\n");

    printf("G - set generic media player as current media_player\n");

}

static void mcs_server_init_media_player(mcs_media_player_t * media_player){
    media_player->playback_speed_index = 2;            // 1
    media_player->seeking_speed_index  = 3;            // 2.0
    media_player->track_groups = track_groups;
    media_player->track_groups_num =  3;
    media_player->seeking_forward = true;

    media_player->previous_track_position_10ms = 0;
    media_player->previous_track_index = 1;
    media_player->previous_group_index = 2;

    media_player->current_track_index = media_player->previous_track_index;
    media_player->current_group_index = 1;

    mcs_track_t * current_track = mcs_get_current_track_for_media_player(media_player);
    btstack_assert(current_track != NULL);

    media_control_service_server_set_media_player_name(media_player->id, "BK Player1");
    media_control_service_server_set_icon_object_id(media_player->id, &current_track->icon_object_id);
    media_control_service_server_set_icon_url(media_player->id, current_track->icon_url);

    media_control_service_server_set_track_title(media_player->id, current_track->title);
    media_control_service_server_set_track_duration(media_player->id, current_track->track_duration_10ms);
    media_control_service_server_set_track_position(media_player->id, current_track->track_position_10ms);

    media_control_service_server_set_parent_group_object_id(media_player->id, &media_player->track_groups[media_player->current_group_index].parent_group_object_id);
    media_control_service_server_set_current_group_object_id(media_player->id, &media_player->track_groups[media_player->current_group_index].current_group_object_id);
    media_control_service_server_set_current_track_id(media_player->id, &current_track->object_id);
    media_control_service_server_set_current_track_segments_id(media_player->id, &current_track->segments_object_id);

    uint32_t next_track_index;
    mcs_next_track_index(media_player, true, &next_track_index);
    media_control_service_server_set_next_track_id(media_player->id, &media_player->track_groups[media_player->current_group_index].tracks[next_track_index].object_id);


    media_control_service_server_set_playing_orders_supported(media_player->id, 0x3FF);
    media_control_service_server_set_playing_order(media_player->id, PLAYING_ORDER_IN_ORDER_ONCE);
}

static void mcs_server_set_current_media_player(mcs_media_player_t * media_player){
    mcs_seeking_speed_timer_stop(current_media_player_id);
    current_media_player_id = media_player->id;
    mcs_seeking_speed_timer_start(current_media_player_id);
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;

    printf("MCS Server App: Command %c, media player ID %d, state %s\n", cmd, current_media_player_id, 
        mcs_server_media_state2str(media_control_service_server_get_media_state(current_media_player_id)));
    
    mcs_media_player_t * media_player = mcs_get_media_player_for_id(current_media_player_id);
    btstack_assert(media_player != NULL);
            
    switch (cmd){
        case '0':
            printf(" - Set INACTIVE state\n");
            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_INACTIVE);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_INACTIVE;
            }
            break;

        case '1':
            printf(" - Set PLAYING state\n");
            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_PLAYING);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_PLAYING;
            }
            break;

        case '2':
            printf(" - Set PAUSED state\n");
            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_PAUSED);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_PAUSED;
            }
            break;

        case '3':
            printf(" - Set SEEKING state\n");
            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_SEEKING);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_SEEKING;
            }
            break;

        case '4':
            printf(" - Goto the 5th segment\n");

            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_PLAYING);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_PLAYING;
            }
            mcs_current_track_goto_segment(mcs_get_current_track_for_media_player_id(current_media_player_id), 5);
            break;
            
        case '5':
            printf(" - Goto the last song\n");

            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_PLAYING);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_PLAYING;
            }
            mcs_goto_last_track(current_media_player_id);
            break;
        
        case '6':
            printf(" - Goto the second group\n");

            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_PLAYING);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_PLAYING;
            }
            mcs_goto_group(current_media_player_id, 2);
            break;
        case '8':
            printf(" - Reset filters\n");
            ots_db_reset_filters();
            break;
        case '9':
            printf("Use empty databank\n");
            ots_db_type = OTS_DATABANK_TYPE_EMPTY;
            break;
        case 'j':
            status = media_control_service_server_set_media_player_name(current_media_player_id, long_string1);
            break;
        case 'J':
            status = media_control_service_server_set_media_player_name(current_media_player_id, long_string2);
            break;
        case 'k':
            status = media_control_service_server_set_track_title(current_media_player_id, long_string1);
            if (status == ERROR_CODE_SUCCESS){
                mcs_track_t * track = mcs_get_current_track_for_media_player_id(current_media_player_id);
                if (track != NULL){
                    ots_object_t * object = ots_db_find_object_with_luid(&track->object_id);
                    if (object != NULL){
                        object_transfer_service_server_set_current_object(bap_app_server_get_active_con_handle(), object);
                        status = object_transfer_service_server_update_current_object_name(bap_app_server_get_active_con_handle(), long_string1);
                    }
                }
            }
            break;
        case 'K':
            status = media_control_service_server_set_track_title(current_media_player_id, long_string2);
            if (status == ERROR_CODE_SUCCESS){
                mcs_track_t * track = mcs_get_current_track_for_media_player_id(current_media_player_id);
                if (track != NULL){
                    ots_object_t * object = ots_db_find_object_with_luid(&track->object_id);
                    if (object != NULL){
                        object_transfer_service_server_set_current_object(bap_app_server_get_active_con_handle(), object);
                        status = object_transfer_service_server_update_current_object_name(bap_app_server_get_active_con_handle(), long_string1);
                    }
                }
            }
            break;
        
        case '7':
            printf("set long filter\n");
            ots_filters[0].type = OTS_FILTER_TYPE_NAME_STARTS_WITH;
            ots_filters[0].value_length = strlen(long_string2) - 10;

            memset(ots_filters[0].value, 0, sizeof(ots_filters[0].value));
            memcpy(ots_filters[0].value, (uint8_t *)long_string2, ots_filters[0].value_length - 1);
            break;

        case 'I':
            printf("Invalidate current object\n");
            ots_db_object_current_index = -1;
            break;

        case 'G':
            mcs_server_set_current_media_player(&generic_media_player);
            break;

        case 'p':
            printf("Set no parent group\n");
            media_control_service_server_set_parent_group_object_id(media_player->id, &media_player->track_groups[media_player->current_group_index].current_group_object_id);
            break;

        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf(" - Command '%c' could not be performed, status 0x%02x\n", cmd, status);
    }
}


int btstack_main(void);
void ots_db_init();


void bap_server_app_connections_init();

int btstack_main(void)
{
    
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
    sm_allow_ltk_reconstruction_without_le_device_db_entry(0);
    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    

    // setup MCS
    media_control_service_server_init();

    media_control_service_server_register_generic_player(&media_player1.media_server,
                                                         &mcs_server_packet_handler, 0x1FFFFF,
                                                         &media_player1.id);

//    media_control_service_server_register_player(&media_player1.media_server,
//        &mcs_server_packet_handler, 0, //0x1FFFFF,
//        &media_player1.id);

    mcs_server_init_media_player(&media_player1);

    media_control_service_server_register_generic_player(&generic_media_player.media_server,
                                                         &mcs_server_packet_handler, 0, //0x1FFFFF,
                                                         &generic_media_player.id);

    mcs_server_init_media_player(&generic_media_player);

    mcs_server_set_current_media_player(&media_player1);

    // setup OTS
    object_transfer_service_server_init(0x3FF, 0x0F, 
        OTS_SERVER_MAX_NUM_CLIENTS, ots_server_connections_storage, &ots_server_operations_impl);
    object_transfer_service_server_register_packet_handler(&ots_server_packet_handler);

    ots_db_init();
    ots_db_load_from_memory(sizeof(track_groups)/sizeof(mcs_track_group_t), track_groups);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    gap_set_max_number_peripheral_connections(2);
    // setup advertisements
    setup_advertising();
    gap_periodic_advertising_sync_transfer_set_default_parameters(2, 0, 0x2000, 0);

    bap_server_app_connections_init();

    btstack_stdin_setup(stdin_process);
    
    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}

