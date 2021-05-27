/*
 * Copyright (C) 2021 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
 * @title Battery Service Client
 * 
 */

#ifndef BATTERY_SERVICE_CLIENT_H
#define BATTERY_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"

#if defined __cplusplus
extern "C" {
#endif

/** 
 * @text The Battery Service Client connects to the Battery Services of a remote device 
 * and queries its battery level values. Level updates are either received via notifications 
 * (if supported by the remote Battery Service), or by manual polling.
 */

#ifndef MAX_NUM_BATTERY_SERVICES
#define MAX_NUM_BATTERY_SERVICES 3
#endif

#if MAX_NUM_BATTERY_SERVICES > 8
    #error "Maximum number of Battery Services exceeded"
#endif


typedef enum {
    BATTERY_SERVICE_CLIENT_STATE_IDLE,
    BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
    BATTERY_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
    BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS,
    BATTERY_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,

#ifdef ENABLE_TESTING_SUPPORT
    BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS,
    BATTERY_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT,
#endif

    BATTERY_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION,
    BATTERY_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED,
    BATTERY_SERVICE_CLIENT_STATE_CONNECTED,

#ifdef ENABLE_TESTING_SUPPORT
    BATTERY_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION,
    BATTERY_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT,
#endif
} battery_service_client_state_t;


typedef struct {
    // service
    uint16_t start_handle;
    uint16_t end_handle;
    
    // characteristic
    uint16_t properties;
    uint16_t value_handle;

#ifdef ENABLE_TESTING_SUPPORT
    uint16_t ccc_handle;
#endif
    
    gatt_client_notification_t notification_listener;
} battery_service_t;


typedef struct {
    btstack_linked_item_t item;
    
    hci_con_handle_t  con_handle;
    uint16_t          cid;
    battery_service_client_state_t  state;
    btstack_packet_handler_t client_handler;

    uint32_t poll_interval_ms;
    
    uint8_t num_instances;
    battery_service_t services[MAX_NUM_BATTERY_SERVICES];

    // used for discovering characteristics and polling
    uint8_t service_index;
    uint8_t poll_bitmap;
    uint8_t need_poll_bitmap;
    uint8_t polled_service_index;
    btstack_timer_source_t poll_timer;
} battery_service_client_t;

/* API_START */

    
/**
 * @brief Initialize Battery Service. 
 */
void battery_service_client_init(void);

/**
 * @brief Connect to Battery Services of remote device. The client will try to register for notifications. 
 * If notifications are not supported by remote Battery Service, the client will poll battery level
 * If poll_interval_ms is 0, polling is disabled, and only notifications will be received.
 * In either case, the battery level is received via GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL event.
 * The battery level is reported as percentage, i.e. 100 = full and it is valid if the ATTT status is equal to ATT_ERROR_SUCCESS, 
 * see ATT errors (see bluetooth.h) for other values.
 *   
 * For manual polling, see battery_service_client_read_battery_level below.
 *
 * Event GATTSERVICE_SUBEVENT_BATTERY_SERVICE_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
 * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no battery service is found, or ATT errors (see bluetooth.h). 
 * This event event also returns number of battery instances found on remote server, as well as poll bitmap that indicates which indexes 
 * of services require polling, i.e. they do not support notification on battery level change,
 *
 * @param con_handle
 * @param packet_handler
 * @param poll_interval_ms or 0 to disable polling
 * @param battery_service_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED 
 */
uint8_t battery_service_client_connect(hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler, uint32_t poll_interval_ms, uint16_t * battery_service_cid);

/**
 * @brief Read battery level for service with given index. Event GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL is 
 * received with battery level (unit is in percentage, i.e. 100 = full). The battery level is valid if the ATTT status 
 * is equal to ATT_ERROR_SUCCESS, see ATT errors (see bluetooth.h) for other values.
 * @param battery_service_cid
 * @param service_index
 * @return status
 */
uint8_t battery_service_client_read_battery_level(uint16_t battery_service_cid, uint8_t service_index);

/**
 * @brief Disconnect from Battery Service.
 * @param battery_service_cid
 * @return status
 */
uint8_t battery_service_client_disconnect(uint16_t battery_service_cid);

/**
 * @brief De-initialize Battery Service. 
 */
void battery_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
