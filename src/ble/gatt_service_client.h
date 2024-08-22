/*
 * Copyright (C) 2023 BlueKitchen GmbH
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
 * @title GATT Service Client
 * 
 */

#ifndef GATT_SERVICE_CLIENT_H
#define GATT_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"

#if defined __cplusplus
extern "C" {
#endif

/** 
 * @text A generic GATT Service Client
 */


typedef enum {
    GATT_SERVICE_CLIENT_STATE_W2_QUERY_PRIMARY_SERVICE,
    GATT_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
    GATT_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS,
    GATT_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,
    GATT_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS,
    GATT_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT,

    GATT_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION,
    GATT_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED,
    GATT_SERVICE_CLIENT_STATE_CONNECTED
} gatt_service_client_state_t;

typedef struct {
    uint16_t value_handle;
    uint16_t client_configuration_handle; 
    uint16_t properties;
    uint16_t end_handle;
    gatt_client_notification_t notification_listener;
} gatt_service_client_characteristic_t;

typedef struct {
    btstack_linked_item_t item;

    hci_con_handle_t  con_handle;
    uint16_t          cid;
    uint16_t          mtu;
    gatt_service_client_state_t  state;

    // service
    // used to restrict the number of found primary services to 1
    uint16_t service_uuid16;
    uint8_t  service_index;
    uint16_t service_instances_num;
    uint16_t start_handle;
    uint16_t end_handle;

    uint8_t characteristics_num;
    uint8_t characteristic_index;
    gatt_service_client_characteristic_t * characteristics;

    btstack_packet_handler_t event_callback;
} gatt_service_client_connection_t;

typedef struct {
    btstack_linked_list_t connections;
    uint16_t cid_counter;

    // characteristics
    uint8_t  characteristics_desc16_num;     // uuid16s_num
    const uint16_t * characteristics_desc16; // uuid16s
    
    btstack_packet_callback_registration_t hci_event_callback_registration;
    
    btstack_packet_handler_t packet_handler;
} gatt_service_client_t;

/* API_START */

/**
 * @brief Initialize GATT Service Client
 * @param client
 * @param trampoline_packet_handler packet handler that calls gatt_service_client_trampoline_packet_handler with client
 */
void gatt_service_client_init(gatt_service_client_t * client,
                              void (*trampoline_packet_handler)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size));

/**
 * @brief Packet Handler to be called by trampoline registered with gatt_service_client_init
 * @param client
 * @param packet_type
 * @param channel
 * @param packet
 * @param size
 */
void gatt_service_client_trampoline_packet_handler(gatt_service_client_t * client, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

/**
 * @brief Register callback for the GATT client. 
 * @param client
 * @param packet_handler
 */
void gatt_service_client_register_packet_handler(gatt_service_client_t * client, btstack_packet_handler_t packet_handler);

/**
 * @brief Connect to the n-th instance of Primary GATT Service with UUID16
 *
 * @param con_handle
 * @param client
 * @param connection
 * @param service_uuid16
 * @param service_index
 * @param characteristics
 * @param characteristics_num
 * @param packet_handler
 * @param connection_cid
 * @return
 */
uint8_t gatt_service_client_connect_primary_service(
        hci_con_handle_t con_handle,
        gatt_service_client_t * client, gatt_service_client_connection_t * connection,
        uint16_t service_uuid16, uint8_t service_index,
        gatt_service_client_characteristic_t * characteristics, uint8_t characteristics_num,
        btstack_packet_handler_t packet_handler, uint16_t * connection_cid);

/**
 * @brief Check if connection to peer already exists
 * @param con_handle
 * @param client
 * @return ERROR_CODE_SUCCESS if not connection exists, otherwise ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t gatt_service_client_connect_secondary_service_ready_to_connect(hci_con_handle_t con_handle,
                                                                       gatt_service_client_t *client);


/**
 * @brief Connect to the Secondary GATT Service with given handle range
 *
 * UUID16 and Service Index are stored for GATT Service Client user only
 *
 * @param con_handle
 * @param client
 * @param connection
 * @param service_uuid16
 * @param service_start_handle
 * @param service_end_handle
 * @param service_index
 * @param characteristics
 * @param characteristics_num
 * @param packet_handler
 * @return
 */
uint8_t gatt_service_client_connect_secondary_service(
        hci_con_handle_t con_handle,
        gatt_service_client_t * client, gatt_service_client_connection_t * connection,
        uint16_t service_uuid16, uint16_t service_start_handle, uint16_t service_end_handle, uint8_t service_index,
        gatt_service_client_characteristic_t * characteristics, uint8_t characteristics_num,
        btstack_packet_handler_t packet_handler);

/**
 * @brief Check if Characteristic is available and can be queried
 * @param connection
 * @param characteristic_index
 * @return ERROR_CODE_SUCCESS if ready, ERROR_CODE_COMMAND_DISALLOWED or ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE otherwise
 */
uint8_t gatt_service_client_can_query_characteristic(gatt_service_client_connection_t * connection, uint8_t characteristic_index);

/**
 * @brief Get connection object provided by gatt_service_client_connect_primary_service or gatt_service_client_connect_secondary_service by connection id
 * @param client
 * @param connection_cid
 * @return
 */
gatt_service_client_connection_t * gatt_service_client_get_connection_for_cid(const gatt_service_client_t * client, uint16_t connection_cid);

/**
 * @breif Get connection object  provided by gatt_service_client_connect_primary_service or gatt_service_client_connect_secondary_service by con handle
 *
 * This only works if only a single Primary Service is used
 *
 * @param client
 * @param con_handle
 * @return
 */
gatt_service_client_connection_t * gatt_service_client_get_connection_for_con_handle(const gatt_service_client_t * client, hci_con_handle_t con_handle);

/**
 * @brief Get Characteristic UUID16 for value handle
 *
 * Used to handle incoming characteristic indications/notifications
 *
 * @param client
 * @param connection_helper
 * @param value_handle
 * @return
 */
uint16_t gatt_service_client_characteristic_uuid16_for_value_handle(const gatt_service_client_t * client, gatt_service_client_connection_t * connection_helper, uint16_t value_handle);

/**
 * @brief Get Characteristic UUID16 for given Characteristic index
 *
 * @param client
 * @param index
 * @return
 */
uint16_t gatt_service_client_characteristic_uuid16_for_index(const gatt_service_client_t * client, uint8_t index);

/**
 * @brief Get Characteristic Value Handle for given Characteristic index
 * *
 * @param connection_helper
 * @param characteristic_index
 * @return
 */
uint16_t gatt_service_client_characteristic_value_handle_for_index(gatt_service_client_connection_t * connection_helper, uint8_t characteristic_index);

/**
 * @brief Disconnect service client
 * @param client
 * @param connection_cid
 * @return
 */
uint8_t gatt_service_client_disconnect(gatt_service_client_t * client, uint16_t connection_cid);

/**
 * @brief Remove connection from connection list
 *
 * @param client
 * @param connection
 */
void gatt_service_client_finalize_connection(gatt_service_client_t * client, gatt_service_client_connection_t * connection);

/**
 * @brief De-Init
 * @param client
 */
void gatt_service_client_deinit(gatt_service_client_t * client);

/**
 * @brief Map ATT Error Code to (extended) Error Codes
 * @param att_error_code
 * @return
 */
uint8_t gatt_service_client_att_status_to_error_code(uint8_t att_error_code);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
