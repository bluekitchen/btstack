/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "hids_device.c"

/**
 * Implementation of the GATT HIDS Device
 * To use with your application, add '#import <hids.gatt>' to your .gatt file
 */

#include "hids_device.h"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_util.h"
#include "btstack_debug.h"

static btstack_packet_handler_t packet_handler;
static btstack_context_callback_registration_t  battery_callback;

static uint8_t         hid_country_code;
static const uint8_t * hid_descriptor;
static uint16_t        hid_descriptor_size;

static uint16_t        hid_report_map_handle;
static uint16_t        hid_protocol_mode;
static uint16_t        hid_protocol_mode_value_handle;

static uint16_t        hid_boot_mouse_input_value_handle;
static uint16_t        hid_boot_mouse_input_client_configuration_handle;
static uint16_t        hid_boot_mouse_input_client_configuration_value;

static uint16_t        hid_boot_keyboard_input_value_handle;
static uint16_t        hid_boot_keyboard_input_client_configuration_handle;
static uint16_t        hid_boot_keyboard_input_client_configuration_value;

static uint16_t        hid_report_input_value_handle;
static uint16_t        hid_report_input_client_configuration_handle;
static uint16_t        hid_report_input_client_configuration_value;

static att_service_handler_t hid_service;

static void hids_device_emit_event_with_uint8(uint8_t event, hci_con_handle_t con_handle, uint8_t value){
    if (!packet_handler) return;
    uint8_t buffer[6];
    buffer[0] = HCI_EVENT_HIDS_META;
    buffer[1] = 4;
    buffer[2] = event;
    little_endian_store_16(buffer, 3, (uint16_t) con_handle);
    buffer[5] = value;
    (*packet_handler)(HCI_EVENT_PACKET, 0, buffer, sizeof(buffer));
}

static void hids_device_can_send_now(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t) (uintptr_t) context;
    // notify client
    if (!packet_handler) return;
    uint8_t buffer[5];
    buffer[0] = HCI_EVENT_HIDS_META;
    buffer[1] = 3;
    buffer[2] = HIDS_SUBEVENT_CAN_SEND_NOW;
    little_endian_store_16(buffer, 3, (uint16_t) con_handle);
    (*packet_handler)(HCI_EVENT_PACKET, 0, buffer, sizeof(buffer));
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(connection_handle);

    if (att_handle == hid_protocol_mode_value_handle){
        log_info("Read protocol mode");
        return att_read_callback_handle_byte(hid_protocol_mode, offset, buffer, buffer_size);
    }
    if (att_handle == hid_report_map_handle){
        log_info("Read report map");
        return att_read_callback_handle_blob(hid_descriptor, hid_descriptor_size, offset, buffer, buffer_size);
    }
    // if (att_handle == hid_boot_mouse_input_value_handle){
    // }
    if (att_handle == hid_boot_mouse_input_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(hid_boot_keyboard_input_client_configuration_value, offset, buffer, buffer_size);
    }
    // if (att_handle == hid_boot_keyboard_input_value_handle){
    // }
    if (att_handle == hid_boot_keyboard_input_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(hid_boot_keyboard_input_client_configuration_value, offset, buffer, buffer_size);
    }
    // if (att_handle == hid_report_input_value_handle){
    // }
    if (att_handle == hid_report_input_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(hid_report_input_client_configuration_value, offset, buffer, buffer_size);
    }
    return 0;
}

static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(buffer_size);
    UNUSED(offset);

    if (att_handle == hid_boot_mouse_input_client_configuration_handle){
        uint16_t new_value = little_endian_read_16(buffer, 0);
        if (new_value == hid_boot_mouse_input_client_configuration_value) return 0;
        hid_boot_mouse_input_client_configuration_value = new_value;
        hids_device_emit_event_with_uint8(HIDS_SUBEVENT_BOOT_MOUSE_INPUT_REPORT_ENABLE, con_handle, hid_protocol_mode);
    }
    if (att_handle == hid_boot_keyboard_input_client_configuration_handle){
        uint16_t new_value = little_endian_read_16(buffer, 0);
        if (new_value == hid_boot_keyboard_input_client_configuration_value) return 0;
        hid_boot_keyboard_input_client_configuration_value = new_value;
        hids_device_emit_event_with_uint8(HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE, con_handle, hid_protocol_mode);
    }
    if (att_handle == hid_report_input_client_configuration_handle){
        uint16_t new_value = little_endian_read_16(buffer, 0);
        if (new_value == hid_report_input_client_configuration_value) return 0;
        hid_report_input_client_configuration_value = new_value;
        log_info("Enable Report Input notifications: %x", new_value);
        hids_device_emit_event_with_uint8(HIDS_SUBEVENT_INPUT_REPORT_ENABLE, con_handle, hid_protocol_mode);
    }
    if (att_handle == hid_protocol_mode_value_handle){
        hid_protocol_mode = buffer[0];
        log_info("Set protocol mode: %u", hid_protocol_mode);
        hids_device_emit_event_with_uint8(HIDS_SUBEVENT_PROTOCOL_MODE, con_handle, hid_protocol_mode);
    }
    return 0;
}

/**
 * @brief Set up HIDS Device
 */
void hids_device_init(uint8_t country_code, const uint8_t * descriptor, uint16_t descriptor_size){

    hid_country_code    = country_code;
    hid_descriptor      = descriptor;
    hid_descriptor_size = descriptor_size;

    // default
    hid_protocol_mode   = 1;

    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xfff;
    int service_found = gatt_server_get_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE, &start_handle, &end_handle);
    if (!service_found) return;

    // get report map handle
    hid_report_map_handle                               = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_REPORT_MAP);

    // get report map handle
    hid_protocol_mode_value_handle                      = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_PROTOCOL_MODE);

    // get value and client configuration handles for boot mouse input, boot keyboard input and report input
    hid_boot_mouse_input_value_handle                   = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT);
    hid_boot_mouse_input_client_configuration_handle    = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT);

    hid_boot_keyboard_input_value_handle                = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT);
    hid_boot_keyboard_input_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT);

    hid_report_input_value_handle                       = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_REPORT);
    hid_report_input_client_configuration_handle        = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_REPORT);

    // register service with ATT DB
    hid_service.start_handle   = start_handle;
    hid_service.end_handle     = end_handle;
    hid_service.read_callback  = &att_read_callback;
    hid_service.write_callback = &att_write_callback;
    att_register_service_handler(&hid_service);
}

/**
 * @brief Register callback for the HIDS Device client.
 * @param callback
 */
void hids_device_register_packet_handler(btstack_packet_handler_t callback){
    packet_handler = callback;
}

/**
 * @brief Request can send now event to send HID Report
 * Generates an HIDS_SUBEVENT_CAN_SEND_NOW subevent
 * @param hid_cid
 */
void hids_device_request_can_send_now_event(hci_con_handle_t con_handle){
    battery_callback.callback = &hids_device_can_send_now;
    battery_callback.context  = (void*) (uintptr_t) con_handle;
    att_server_register_can_send_now_callback(&battery_callback, con_handle);
}

/**
 * @brief Send HID Report: Input
 */
void hids_device_send_input_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len){
    att_server_notify(con_handle, hid_report_input_value_handle, (uint8_t*) report, report_len);
}

/**
 * @brief Send HID Boot Mouse Input Report
 */
void hids_device_send_boot_mouse_input_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len){
    att_server_notify(con_handle, hid_boot_mouse_input_value_handle, (uint8_t*) report, report_len);
}

/**
 * @brief Send HID Boot Mouse Input Report
 */
void hids_device_send_boot_keyboard_input_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len){
    att_server_notify(con_handle, hid_boot_keyboard_input_value_handle, (uint8_t*) report, report_len);
}
