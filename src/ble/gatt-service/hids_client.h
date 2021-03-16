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
#define HIDS_CLIENT_INVALID_REPORT_INDEX 0xFF

#define HIDS_CLIENT_NUM_REPORTS 10

typedef enum {
    HIDS_CLIENT_STATE_IDLE,
    
    // get all HID services
    HIDS_CLIENT_STATE_W2_QUERY_SERVICE,
    HIDS_CLIENT_STATE_W4_SERVICE_RESULT,
    
    // for each service, discover all characteristics
    HIDS_CLIENT_STATE_W2_QUERY_CHARACTERISTIC,
    HIDS_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,

    // for each REPORT_MAP characteristic, read HID Descriptor (Report Map Characteristic Value)
    HIDS_CLIENT_STATE_W2_READ_REPORT_MAP_CHARACTERISTIC_VALUE,
    HIDS_CLIENT_STATE_W4_REPORT_MAP_CHARACTERISTIC_VALUE_RESULT,
    
    // for REPORT_MAP characteristic, discover descriptor 
    HIDS_CLIENT_STATE_W2_REPORT_MAP_DISCOVER_CHARACTERISTIC_DESCRIPTORS,
    HIDS_CLIENT_STATE_W4_REPORT_MAP_CHARACTERISTIC_DESCRIPTORS_RESULT,

    // for REPORT_MAP characteristic, read External Report Reference Characteristic Descriptor
    HIDS_CLIENT_STATE_W2_REPORT_MAP_READ_CHARACTERISTIC_DESCRIPTOR_VALUE,
    HIDS_CLIENT_STATE_W4_REPORT_MAP_CHARACTERISTIC_DESCRIPTOR_VALUE_RESULT,

    // for every external report reference uuid, discover external Report characteristic
    HIDS_CLIENT_STATE_W2_REPORT_MAP_DISCOVER_EXTERNAL_REPORT_CHARACTERISTIC,
    HIDS_CLIENT_STATE_W4_REPORT_MAP_EXTERNAL_REPORT_CHARACTERISTIC_RESULT,

    // for each Report characteristics, discover characteristic descriptor
    HIDS_CLIENT_STATE_W2_REPORT_QUERY_CHARACTERISTIC_DESCRIPTORS,
    HIDS_CLIENT_STATE_W4_REPORT_CHARACTERISTIC_DESCRIPTORS_RESULT,
    // then read value of characteristic descriptor to get report ID and type  
    HIDS_CLIENT_STATE_W2_REPORT_READ_CHARACTERISTIC_DESCRIPTOR_VALUE,
    HIDS_CLIENT_STATE_W4_REPORT_CHARACTERISTIC_DESCRIPTOR_VALUE_RESULT,
    
    // Boot Mode
    HIDS_CLIENT_STATE_W2_ENABLE_KEYBOARD,
    HIDS_CLIENT_STATE_W4_KEYBOARD_ENABLED,
    HIDS_CLIENT_STATE_W2_ENABLE_MOUSE,
    HIDS_CLIENT_STATE_W4_MOUSE_ENABLED,
    

    HIDS_CLIENT_STATE_W2_SET_PROTOCOL_MODE,
    HIDS_CLIENT_STATE_W4_SET_PROTOCOL_MODE,
    
    HIDS_CLIENT_STATE_CONNECTED,
    HIDS_CLIENT_W2_SEND_REPORT
} hid_service_client_state_t;


typedef struct {
    // GATT cache
    uint16_t value_handle;
    uint16_t end_handle;
    uint16_t properties;
    // this chould be moved to hids_client_t
    uint16_t external_report_reference_uuid;

    // service mapping
    uint8_t service_index;
    uint8_t report_id;
    hid_report_type_t report_type;
    gatt_client_notification_t notifications;
} hids_client_report_t;

typedef struct {
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

    // send report
    hids_client_report_t reports[HIDS_CLIENT_NUM_REPORTS];
    uint8_t num_reports;

    // index used for report and report map search
    uint8_t   active_report_index;
    uint16_t  descriptor_handle;
    uint16_t  report_len;
    const uint8_t * report;
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
 * @brief Send HID output report.
 * @param hids_cid
 * @param report_id
 * @param report
 * @param report_len
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t hids_client_send_report(uint16_t hids_cid, uint8_t report_id, const uint8_t * report, uint8_t report_len);

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
