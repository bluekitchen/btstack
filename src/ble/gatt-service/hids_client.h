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

#ifndef HIDS_CLIENT_H
#define HIDS_CLIENT_H

#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "hid.h"
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"

#ifndef MAX_NUM_HID_SERVICES
#define MAX_NUM_HID_SERVICES 3
#endif

typedef enum {
    HIDS_CLIENT_STATE_IDLE,
    HIDS_CLIENT_STATE_W2_QUERY_SERVICE,
    HIDS_CLIENT_STATE_W4_SERVICE_RESULT,
    HIDS_CLIENT_STATE_W2_QUERY_CHARACTERISTIC,
    HIDS_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,
    HIDS_CLIENT_STATE_W4_KEYBOARD_ENABLED,
    HIDS_CLIENT_STATE_W4_MOUSE_ENABLED,
    HIDS_CLIENT_STATE_W4_SET_PROTOCOL_MODE,
    HIDS_CLIENT_STATE_CONNECTED
} hid_service_client_state_t;


typedef struct {
    // service
    uint16_t start_handle;
    uint16_t end_handle;
    
} hid_service_t;


typedef struct {
    btstack_linked_item_t item;
    
    hci_con_handle_t  con_handle;
    uint16_t          cid;
    hid_protocol_mode_t protocol_mode;
    hid_service_client_state_t state;
    btstack_packet_handler_t   client_handler;

    uint8_t num_instances;
    hid_service_t services[MAX_NUM_HID_SERVICES];

    // used for discovering characteristics
    uint8_t service_index;
    hid_protocol_mode_t required_protocol_mode;

    uint16_t protocol_mode_value_handle;

    uint16_t boot_keyboard_input_value_handle;
    uint16_t boot_keyboard_input_end_handle;
    gatt_client_notification_t boot_keyboard_notifications;
    
    uint16_t boot_mouse_input_value_handle;
    uint16_t boot_mouse_input_end_handle;
    gatt_client_notification_t boot_mouse_notifications;

} hids_client_t;

/* API_START */

/**
 * @brief Initialize Battery Service. 
 */
void hids_client_init(void);

/* @brief Connect to HID Services of remote device.
 *
 * @param con_handle
 * @param packet_handler
 * @param protocol_mode see hid_protocol_mode_t in hid.h
 * @param hids_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED 
 */
uint8_t hids_client_connect(hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler, hid_protocol_mode_t protocol_mode, uint16_t * hids_cid);

/**
 * @brief Disconnect from Battery Service.
 * @param hids_cid
 * @return status
 */
uint8_t hids_client_disconnect(uint16_t hids_cid);

/**
 * @brief De-initialize Battery Service. 
 */
void hids_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
