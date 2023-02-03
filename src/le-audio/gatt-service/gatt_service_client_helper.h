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
 * @title GATT Service Client Helper
 * 
 */

#ifndef GATT_SERVICE_CLIENT_HELPER_H
#define GATT_SERVICE_CLIENT_HELPER_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"

#if defined __cplusplus
extern "C" {
#endif

/** 
 * @text The Media Control Service Client 
 */


typedef enum {
    GATT_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
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
    // used to restrict the number of found services to 1
    uint16_t service_instances_num;
    uint16_t start_handle;
    uint16_t end_handle;
    
    uint8_t characteristics_num;
    uint8_t characteristic_index;
    gatt_service_client_characteristic_t * characteristics;

    void (*handle_gatt_server_notification)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
} gatt_service_client_connection_helper_t;

typedef struct {
    uint16_t uuid16;
} gatt_service_client_characteristic_desc16_t;

typedef struct {
    btstack_linked_item_t item;

    btstack_linked_list_t connections;
    uint16_t cid_counter;

    // service
    uint16_t service_uuid16;
    // characteristics
    uint8_t  characteristics_desc16_num;
    const gatt_service_client_characteristic_desc16_t * characteristics_desc16;
    // control point
    uint16_t control_point_uuid;
    
    btstack_packet_callback_registration_t hci_event_callback_registration;
    
    btstack_packet_handler_t packet_handler;
} gatt_service_client_helper_t;

/* API_START */

void gatt_service_client_init(gatt_service_client_helper_t * client,
     void (*hci_event_handler_trampoline)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size));


/**
 * @brief Register callback for the GATT client. 
 * @param client
 * @param packet_handler
 */
void gatt_service_client_register_packet_handler(gatt_service_client_helper_t * client, btstack_packet_handler_t packet_handler);
gatt_service_client_connection_helper_t * gatt_service_client_get_connection_for_cid(gatt_service_client_helper_t * client, uint16_t connection_cid);

uint8_t gatt_service_client_connect(
        hci_con_handle_t con_handle,
        gatt_service_client_helper_t * client, gatt_service_client_connection_helper_t * connection, 
        gatt_service_client_characteristic_t * characteristics, uint8_t characteristics_num,
        btstack_packet_handler_t packet_handler, uint16_t * connection_cid);

void gatt_service_client_hci_event_handler(gatt_service_client_helper_t * client, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

uint8_t gatt_service_client_disconnect(gatt_service_client_helper_t * client, uint16_t connection_cid);

void gatt_service_client_deinit(gatt_service_client_helper_t * client);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
