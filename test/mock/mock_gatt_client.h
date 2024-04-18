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
#ifndef MOCK_GATT_CLIENT_H
#define MOCK_GATT_CLIENT_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

#include "ble/gatt_client.h"

typedef struct {
    btstack_linked_item_t    item;
    uint16_t start_group_handle;
    uint16_t end_group_handle;
    uint16_t uuid16;
    uint8_t  uuid128[16];
   
    btstack_linked_list_t characteristics;
} mock_gatt_client_service_t;

typedef struct {
    btstack_linked_item_t    item;
    uint16_t start_handle;
    uint16_t value_handle;
    uint16_t end_handle;
    uint16_t properties;
    uint16_t uuid16;
    uint8_t  uuid128[16];
    btstack_linked_list_t descriptors;

    uint8_t * value_buffer;
    uint16_t  value_len;

    uint8_t notification_status_code;
    gatt_client_notification_t * notification;
} mock_gatt_client_characteristic_t;

typedef struct {
    btstack_linked_item_t    item;
    uint16_t handle;
    uint16_t uuid16;
    uint8_t  uuid128[16];

    uint8_t * value_buffer;
    uint16_t  value_len;
} mock_gatt_client_characteristic_descriptor_t;


void mock_gatt_client_reset(void);

void mock_gatt_client_emit_dummy_event(void);
void mock_gatt_client_emit_complete(uint8_t status);

void mock_gatt_client_set_att_error_discover_primary_services(void);
void mock_gatt_client_set_att_error_discover_characteristics(void);
void mock_gatt_client_set_att_error_read_value_characteristics(void);
void mock_gatt_client_set_att_error_discover_characteristic_descriptors(void);

void mock_gatt_client_enable_notification(mock_gatt_client_characteristic_t * characteristic, bool command_allowed);
void mock_gatt_client_send_notification(mock_gatt_client_characteristic_t * characteristic, const uint8_t * value_buffer, uint16_t value_len);
void mock_gatt_client_send_notification_with_handle(mock_gatt_client_characteristic_t * characteristic, uint16_t value_handle, const uint8_t * value_buffer, uint16_t value_len);
void mock_gatt_client_send_notification(mock_gatt_client_characteristic_t * characteristic, const uint8_t * value_buffer, uint16_t value_len);
void mock_gatt_client_send_indication_with_handle(mock_gatt_client_characteristic_t * characteristic, uint16_t value_handle, const uint8_t * value_buffer, uint16_t value_len);


mock_gatt_client_service_t * mock_gatt_client_add_primary_service_uuid16(uint16_t service_uuid);
mock_gatt_client_service_t * mock_gatt_client_add_primary_service_uuid128(const uint8_t * service_uuid);
mock_gatt_client_characteristic_t * mock_gatt_client_add_characteristic_uuid16(uint16_t characteristic_uuid, uint16_t properties);
mock_gatt_client_characteristic_t * mock_gatt_client_add_characteristic_uuid128(const uint8_t * characteristic_uuid, uint16_t properties);
mock_gatt_client_characteristic_descriptor_t * mock_gatt_client_add_characteristic_descriptor_uuid16(uint16_t descriptor_uuid);
mock_gatt_client_characteristic_descriptor_t * mock_gatt_client_add_characteristic_descriptor_uuid128(const uint8_t * descriptor_uuid);

void mock_gatt_client_set_descriptor_characteristic_value(mock_gatt_client_characteristic_descriptor_t * descriptor, uint8_t * value_buffer, uint16_t value_len);
void mock_gatt_client_set_characteristic_value(mock_gatt_client_characteristic_t * characteristic, uint8_t * value_buffer, uint16_t value_len);

void mock_gatt_client_run(void);
void mock_gatt_client_run_once(void);


void mock_gatt_client_dump_services(void);

void mock_hci_emit_event(uint8_t * packet, uint16_t size);

void mock_hci_emit_le_connection_complete(uint8_t address_type, const bd_addr_t address, hci_con_handle_t con_handle, uint8_t status);
void mock_hci_emit_connection_encrypted(hci_con_handle_t con_handle, uint8_t encrypted);
void mock_hci_emit_disconnection_complete(hci_con_handle_t con_handle, uint8_t reason);

#if defined __cplusplus
}
#endif

#endif

