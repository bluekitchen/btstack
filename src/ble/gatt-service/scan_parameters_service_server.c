/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "scan_parameters_service_server.c"

/**
 * Implementation of the Nordic SPP-like profile
 *
 * To use with your application, add '#import <scan_parameters_service.gatt' to your .gatt file
 * and call all functions below. All strings and blobs need to stay valid after calling the functions.
 */

#include <stdint.h>
#include <string.h>

#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"

#include "ble/gatt-service/scan_parameters_service_server.h"

static btstack_packet_handler_t                 scan_parameters_packet_handler;
static btstack_context_callback_registration_t  scan_parameters_callback;
static att_service_handler_t                    scan_parameters_service;

static uint16_t scan_interval_window_value;

static uint16_t scan_interval_window_value_handle;
static uint16_t scan_interval_window_value_handle_client_configuration;


static uint16_t scan_refresh_value;
static uint16_t scan_refresh_value_client_configuration;
static hci_con_handle_t scan_refresh_value_client_configuration_connection;

static uint16_t scan_refresh_value_handle;
static uint16_t scan_refresh_value_handle_client_configuration;

static uint16_t scan_parameters_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(attribute_handle);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    return 0;
}

static int scan_parameters_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer_size);

    if (attribute_handle == scan_interval_window_value_handle){
        if (buffer != NULL){
            scan_interval_window_value = little_endian_read_16(buffer, 0);
        }
        return 2;
    }

    if (attribute_handle == scan_refresh_value_handle_client_configuration){
        if (buffer != NULL){
            scan_refresh_value_client_configuration = little_endian_read_16(buffer, 0);
            scan_refresh_value_client_configuration_connection = con_handle;
        }
        return 2;
    }
    return 0;
}

static void scan_parameters_service_refresh_can_send_now(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t) (uintptr_t) context;
    uint8_t value[2];
    little_endian_store_16(value, 0, scan_refresh_value);
    att_server_notify(con_handle, scan_refresh_value_handle, value, 2);
}

/**
 * @brief Init Nordic SPP Service Server with ATT DB
 * @param callback for tx data from peer
 */
void scan_parameters_service_server_init(btstack_packet_handler_t packet_handler){
    scan_parameters_packet_handler = packet_handler;

    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xfff;
    int service_found = gatt_server_get_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_SCAN_PARAMETERS, &start_handle, &end_handle);
    btstack_assert(service_found != 0);
    UNUSED(service_found);

    // get characteristic value handle and client configuration handle
    scan_interval_window_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SCAN_INTERVAL_WINDOW);
    scan_interval_window_value_handle_client_configuration = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SCAN_INTERVAL_WINDOW);

    scan_refresh_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SCAN_REFRESH);
    scan_refresh_value_handle_client_configuration = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SCAN_REFRESH);

    // register service with ATT Server
    scan_parameters_service.start_handle   = start_handle;
    scan_parameters_service.end_handle     = end_handle;
    scan_parameters_service.read_callback  = &scan_parameters_service_read_callback;
    scan_parameters_service.write_callback = &scan_parameters_service_write_callback;
    att_server_register_service_handler(&scan_parameters_service);
}


void scan_parameters_service_server_set_scan_refresh(uint16_t scan_refresh){
    scan_refresh_value = scan_refresh;
    if (scan_refresh_value_client_configuration != 0){
        scan_parameters_callback.callback = &scan_parameters_service_refresh_can_send_now;
        scan_parameters_callback.context  = (void*) (uintptr_t) scan_refresh_value_client_configuration_connection;
        att_server_register_can_send_now_callback(&scan_parameters_callback, scan_refresh_value_client_configuration_connection);
    }
}
