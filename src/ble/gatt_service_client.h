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

#define GATT_SERVICE_CLIENT_INVALID_INDEX 0xff

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
} gatt_service_client_characteristic_t;

struct gatt_service_client;

typedef struct {
    btstack_linked_item_t item;

    struct gatt_service_client * client;

    hci_con_handle_t  con_handle;
    uint16_t          cid;
    gatt_service_client_state_t  state;

    // service
    uint16_t service_uuid16;
    uint8_t  service_index;
    uint16_t service_instances_num;
    uint16_t start_handle;
    uint16_t end_handle;

    uint8_t characteristic_index;
    gatt_service_client_characteristic_t * characteristics;

    gatt_client_service_notification_t notification_listener;
} gatt_service_client_connection_t;

typedef struct gatt_service_client {
    btstack_linked_item_t item;
    btstack_linked_list_t connections;
    uint16_t service_id;
    uint16_t cid_counter;

    // characteristics
    uint8_t  characteristics_desc16_num;     // uuid16s_num
    const uint16_t * characteristics_desc16; // uuid16s
    
    btstack_packet_handler_t packet_handler;
} gatt_service_client_t;

/* API_START */


/**
 * @brief Initialize GATT Service Client infrastructure
 */
void gatt_service_client_init(void);

/**
 * @brief Register new GATT Service Client with list of Characteristic UUID16s
 * @param client
 * @param characteristic_uuid16s
 * @param characteristic_uuid16s_num
 * @param trampoline_packet_handler packet handler that calls gatt_service_client_trampoline_packet_handler with client
 */
void gatt_service_client_register_client(gatt_service_client_t *client, btstack_packet_handler_t packet_handler,
                                         const uint16_t *characteristic_uuid16s, uint16_t characteristic_uuid16s_num);

/**
 * @brief Get Characteristic UUID16 for given Characteristic index
 *
 * @param client
 * @param characteristic_index
 * @return uuid16 or 0 if index out of range
 */
uint16_t gatt_service_client_characteristic_uuid16_for_index(const gatt_service_client_t * client, uint8_t characteristic_index);

/**
 * @bbreif Unregister GATT Service Client
 * @param client
 */
void gatt_service_client_unregister_client(gatt_service_client_t * client);

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
 * @return
 */
uint8_t
gatt_service_client_connect_primary_service_with_uuid16(hci_con_handle_t con_handle, gatt_service_client_t *client,
                                                        gatt_service_client_connection_t *connection,
                                                        uint16_t service_uuid16, uint8_t service_index,
                                                        gatt_service_client_characteristic_t *characteristics,
                                                        uint8_t characteristics_num);

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
 * @return
 */
uint8_t
gatt_service_client_connect_secondary_service_with_uuid16(hci_con_handle_t con_handle, gatt_service_client_t *client,
                                                          gatt_service_client_connection_t *connection,
                                                          uint16_t service_uuid16, uint8_t service_index,
                                                          uint16_t service_start_handle, uint16_t service_end_handle,
                                                          gatt_service_client_characteristic_t *characteristics,
                                                          uint8_t characteristics_num);

/**
 * @brief Disconnect service client
 * @param client
 * @param connection
 * @return
 */
uint8_t gatt_service_client_disconnect(gatt_service_client_connection_t *connection);

/**
 * @brief Check if Characteristic is available and can be queried
 * @param client
 * @param connection
 * @param characteristic_index
 * @return ERROR_CODE_SUCCESS if ready, ERROR_CODE_COMMAND_DISALLOWED or ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE otherwise
 */
uint8_t
gatt_service_client_can_query_characteristic(const gatt_service_client_connection_t *connection,
                                             uint8_t characteristic_index);

/**
 * @brief Get Characteristic Value Handle for given Characteristic index
 * *
 * @param connection
 * @param characteristic_index
 * @return
 */
uint16_t gatt_service_client_characteristic_value_handle_for_index(const gatt_service_client_connection_t *connection,
                                                                   uint8_t characteristic_index);

/**
 * @brief Get Characteristic index Handle for given Characteristic Value Handle
 * *
 * @param connection
 * @param value_handle
 * @return index of characteristic in initial list or GATT_SERVICE_CLIENT_INVALID_INDEX
 */
uint8_t gatt_service_client_characteristic_index_for_value_handle(const gatt_service_client_connection_t *connection,
                                                                  uint16_t value_handle);

/**
 * @brief Get connection id
 * @param client
 * @param connection
 * @returns connection_id
 */
uint16_t gatt_service_client_get_connection_id(const gatt_service_client_connection_t * connection);

/**
 * @brief Get connection handle
 * @param client
 * @param connection
 * @returns con_handle
 */
hci_con_handle_t gatt_service_client_get_con_handle(const gatt_service_client_connection_t * connection);

/**
 * @brief Get service index provided in connect call
 * @param client
 * @param connection
 * @returns connection_id
 */
uint8_t gatt_service_client_get_service_index(const gatt_service_client_connection_t * connection);

/**
 * @brief Get remote MTU
 * @param client
 * @param connection
 * @returns MTU or 0 in case of error
 */
uint16_t gatt_service_client_get_mtu(const gatt_service_client_connection_t *connection);


/**
 * @brief Dump characteristic value handles
 * @param client
 * @param connection
 * @param characteristic_names
 */
void gatt_service_client_dump_characteristic_value_handles(const gatt_service_client_connection_t *connection,
                                                           const char **characteristic_names);

/**
 * @brief De-Init
 * @param client
 */
void gatt_service_client_deinit(void);


/* API_END */

#if defined __cplusplus
}
#endif

#endif
