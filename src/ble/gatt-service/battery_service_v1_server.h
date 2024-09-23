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
 * @title Battery Service Server v1.1
 * 
 */

#ifndef BATTERY_SERVICE_V1_SERVER_H
#define BATTERY_SERVICE_V1_SERVER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The Battery Service allows to query your device's battery level in a standardized way.
 * 
 * To use with your application, add `#import <battery_service.gatt>` to your .gatt file. 
 * After adding it to your .gatt file, you call *battery_service_server_init(value)* with the
 * current value of your battery. The valid range for the battery level is 0-100.
 *
 * If the battery level changes, you can call *battery_service_server_set_battery_value(value)*. 
 * The service supports sending Notifications if the client enables them.
 */
struct battery_service_v1;

typedef struct {
    hci_con_handle_t con_handle;

    btstack_context_callback_registration_t  scheduled_tasks_callback;
    uint8_t scheduled_tasks;

    // ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL
    uint16_t battery_value_client_configuration;
    struct battery_service_v1 * service;
} battery_service_v1_server_connection_t;


typedef struct battery_service_v1 {
    btstack_linked_item_t item;

    // service
    uint16_t start_handle;
    uint16_t end_handle;
    uint8_t index;

    att_service_handler_t    service_handler;

    // ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL
    uint16_t battery_value_handle;
    uint16_t battery_value_client_configuration_handle;
    uint8_t  battery_value;

    uint8_t connections_max_num;
    battery_service_v1_server_connection_t * connections;
} battery_service_v1_t;

/* API_START */

/**
 * @brief Init Battery Service Server with ATT DB
 */
void battery_service_v1_server_init(void);

void battery_service_v1_server_register(battery_service_v1_t *service, battery_service_v1_server_connection_t *connections, uint8_t connection_max_num);

void battery_service_v1_server_deregister(battery_service_v1_t *service);
/**
 * @brief Update battery value
 * @note triggers notifications if subscribed
 * @param battery_value in range 0-100
 */
void battery_service_v1_server_set_battery_value(battery_service_v1_t * service, uint8_t value);

void battery_service_v1_server_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

