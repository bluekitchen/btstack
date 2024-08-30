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

/**
 * @title Object Transfer Service Client
 * 
 */

#ifndef OBJECT_TRANSFER_SERVICE_CLIENT_H
#define OBJECT_TRANSFER_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"
#include "ble/gatt_service_client.h"
#include "le-audio/gatt-service/object_transfer_service_util.h"

#if defined __cplusplus
extern "C" {
#endif


/** 
 * @text The Object Transfer Service Client 
 */
typedef enum {
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_IDLE = 0,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_CONNECTED,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_QUERY_OTS_FEATURES,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_OTS_FEATURES_RESULT,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_OTS_FEATURES_READING_FAILED,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_READ_LONG_CHARACTERISTIC_VALUE,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_READ_LONG_CHARACTERISTIC_VALUE_RESULT,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W2_WRITE_LONG_CHARACTERISTIC_VALUE,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT,
    OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_L2CAP_CBM_CHANNEL_OPENED
} object_transfer_service_client_state_t;


typedef struct {
    char name;
    hci_con_handle_t connection_handle;
    uint16_t cid;
    int  mtu;
} otp_le_cbm_connection_t;

typedef struct {
    btstack_linked_item_t item;

    gatt_service_client_connection_t basic_connection;
    btstack_packet_handler_t packet_handler;

    object_transfer_service_client_state_t state;

    otp_le_cbm_connection_t le_cbm_connection;
    bool le_cbm_channel_opened;
    bool current_object_transfer_in_progress;

    bool current_object_read_transfer_in_progress;
    bool current_object_write_transfer_in_progress;
    uint8_t  * cbm_data;
    uint16_t cbm_data_length;

    bool     cbm_data_truncated;
    uint32_t cbm_data_offset;
    uint32_t cbm_data_chunk_length;
    uint32_t cbm_data_chunk_bytes_transferred;


    // Used for read characteristic queries
    uint8_t characteristic_index;
    // Used to store parameters for write characteristic

    uint8_t data_length;
    union {
        uint8_t  data_bytes[OTS_MAX_STRING_LENGHT + 1];
        const char * data_string;
    } data;

    uint32_t oacp_features;
    uint32_t olcp_features;

    gatt_service_client_characteristic_t characteristics_storage[OBJECT_TRANSFER_SERVICE_NUM_CHARACTERISTICS];
    btstack_context_callback_registration_t gatt_query_can_send_now;

    uint8_t long_value_buffer[OTS_MAX_STRING_LENGHT + 1];
    uint8_t long_value_buffer_length;

    btstack_timer_source_t operation_timer;
} ots_client_connection_t;

/* API_START */

    
/**
 * @brief Initialize Object Transfer Service. 
 */
void object_transfer_service_client_init(void);

 /**
  * @brief Connect to Object Transfer Service of remote device. The client will automatically register for notifications.
  *
  * Event LEAUDIO_SUBEVENT_OTS_CLIENT_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
  * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no media control service is found, or ATT errors (see bluetooth.h).
  *
 * @param con_handle
 * @param packet_handler
 * @param connection
 * @param service_start_handle
 * @param service_end_handle
 * @param service_index
 * @param ots_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED
 */
 uint8_t object_transfer_service_client_connect(
    hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    ots_client_connection_t * connection,
    uint16_t service_start_handle, uint16_t service_end_handle, 
    uint8_t  service_index,
    uint16_t * ots_cid);

uint8_t object_transfer_service_client_connect_secondary_service(
        hci_con_handle_t con_handle,
        btstack_packet_handler_t packet_handler,
        uint16_t service_start_handle, uint16_t service_end_handle, uint8_t service_index, 
        ots_client_connection_t * connection);

uint8_t object_transfer_service_client_read_ots_feature(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_read_object_name(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_read_long_object_name(ots_client_connection_t * connection);

uint8_t object_transfer_service_client_read_object_type(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_read_object_size(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_read_object_first_created(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_read_object_last_modified(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_read_object_id(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_read_object_properties(ots_client_connection_t * connection);

uint8_t object_transfer_service_client_read_object_list_filter_1(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_read_object_list_filter_2(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_read_object_list_filter_3(ots_client_connection_t * connection);

uint8_t object_transfer_service_client_read_object_long_list_filter_1(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_read_object_long_list_filter_2(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_read_object_long_list_filter_3(ots_client_connection_t * connection);


uint8_t object_transfer_service_client_write_object_name(ots_client_connection_t * connection, const char * name);

uint8_t object_transfer_service_client_write_object_first_created(ots_client_connection_t * connection, btstack_utc_t * date);
uint8_t object_transfer_service_client_write_object_last_modified(ots_client_connection_t * connection, btstack_utc_t * date);
uint8_t object_transfer_service_client_write_object_properties(ots_client_connection_t * connection, uint32_t properties);

uint8_t object_transfer_service_client_write_object_list_filter_1(ots_client_connection_t * connection, ots_filter_type_t filter_type, uint8_t data_length, const uint8_t * data);
uint8_t object_transfer_service_client_write_object_list_filter_2(ots_client_connection_t * connection, ots_filter_type_t filter_type, uint8_t data_length, const uint8_t * data);
uint8_t object_transfer_service_client_write_object_list_filter_3(ots_client_connection_t * connection, ots_filter_type_t filter_type, uint8_t data_length, const uint8_t * data);

uint8_t object_transfer_service_client_command_first(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_command_last(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_command_previous(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_command_next(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_command_goto(ots_client_connection_t * connection, ots_object_id_t * object_id);
uint8_t object_transfer_service_client_command_order(ots_client_connection_t * connection, olcp_list_sort_order_t order);
uint8_t object_transfer_service_client_command_request_number_of_objects(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_command_request_clear_marking(ots_client_connection_t * connection);

uint8_t object_transfer_service_client_open_object_channel(ots_client_connection_t * connection, uint8_t * cbm_receive_buffer, uint16_t cbm_receive_buffer_len);
uint8_t object_transfer_service_client_close_object_channel(ots_client_connection_t * connection);

uint8_t object_transfer_service_client_read(ots_client_connection_t * connection, uint32_t offset, uint32_t length);
uint8_t object_transfer_service_client_calculate_checksum(ots_client_connection_t * connection, uint32_t offset, uint32_t length);
uint8_t object_transfer_service_client_write(ots_client_connection_t * connection, bool truncated, uint32_t data_offset, uint8_t *data, uint16_t data_len);

uint8_t object_transfer_service_client_execute(ots_client_connection_t * connection);
uint8_t object_transfer_service_client_abort(ots_client_connection_t * connection);

uint8_t object_transfer_service_client_create_object(ots_client_connection_t * connection, uint32_t object_size, ots_object_type_t type_uuid16);
uint8_t object_transfer_service_client_delete_object(ots_client_connection_t * connection);

/**
 * @brief Disconnect.
 * @param ots_cid
 * @return status
 */
uint8_t object_transfer_service_client_disconnect(ots_client_connection_t * connection);

/**
 * @brief De-initialize Object Transfer Service. 
 */
void object_transfer_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
