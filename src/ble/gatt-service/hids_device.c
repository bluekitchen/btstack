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

#define BTSTACK_FILE__ "hids_device.c"

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

#define HIDS_DEVICE_ERROR_CODE_INAPPROPRIATE_CONNECTION_PARAMETERS    0x80

typedef struct{
    uint16_t        con_handle;

    uint8_t         hid_country_code;
    const uint8_t * hid_descriptor;
    uint16_t        hid_descriptor_size;

    uint16_t        hid_report_map_handle;
    uint16_t        hid_protocol_mode;
    uint16_t        hid_protocol_mode_value_handle;

    uint16_t        hid_boot_mouse_input_value_handle;
    uint16_t        hid_boot_mouse_input_client_configuration_handle;
    uint16_t        hid_boot_mouse_input_client_configuration_value;

    uint16_t        hid_boot_keyboard_input_value_handle;
    uint16_t        hid_boot_keyboard_input_client_configuration_handle;
    uint16_t        hid_boot_keyboard_input_client_configuration_value;

    uint16_t        hid_report_input_value_handle;
    uint16_t        hid_report_input_client_configuration_handle;
    uint16_t        hid_report_input_client_configuration_value;

    uint16_t        hid_report_output_value_handle;
    uint16_t        hid_report_output_client_configuration_handle;
    uint16_t        hid_report_output_client_configuration_value;

    uint16_t        hid_report_feature_value_handle;
    uint16_t        hid_report_feature_client_configuration_handle;
    uint16_t        hid_report_feature_client_configuration_value;

    uint16_t        hid_control_point_value_handle;
    uint8_t         hid_control_point_suspend;

    btstack_context_callback_registration_t  battery_callback;
} hids_device_t;

static hids_device_t hids_device;

static btstack_packet_handler_t packet_handler;
static att_service_handler_t hid_service;

// TODO: store hids device connection into list
static hids_device_t * hids_device_get_instance_for_con_handle(uint16_t con_handle){
    UNUSED(con_handle);
    return &hids_device;
}

static hids_device_t * hids_device_create_instance(void){
    return &hids_device;
}


static void hids_device_emit_event_with_uint8(uint8_t event, hci_con_handle_t con_handle, uint8_t value){
    hids_device_t * instance = hids_device_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return;
    }

    if (!packet_handler) return;
    uint8_t buffer[6];
    buffer[0] = HCI_EVENT_HIDS_META;
    buffer[1] = 4;
    buffer[2] = event;
    little_endian_store_16(buffer, 3, (uint16_t) con_handle);
    buffer[5] = value;
    (*packet_handler)(HCI_EVENT_PACKET, 0, buffer, sizeof(buffer));
}

static void hids_device_emit_event(uint8_t event, hci_con_handle_t con_handle){
    hids_device_t * instance = hids_device_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return;
    }

    if (!packet_handler) return;
    uint8_t buffer[5];
    buffer[0] = HCI_EVENT_HIDS_META;
    buffer[1] = 4;
    buffer[2] = event;
    little_endian_store_16(buffer, 3, (uint16_t) con_handle);
    (*packet_handler)(HCI_EVENT_PACKET, 0, buffer, sizeof(buffer));
}

static void hids_device_can_send_now(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t) (uintptr_t) context;
    // notify client
    hids_device_t * instance = hids_device_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return;
    }

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
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    hids_device_t * instance = hids_device_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return HIDS_DEVICE_ERROR_CODE_INAPPROPRIATE_CONNECTION_PARAMETERS;
    }

    if (att_handle == instance->hid_protocol_mode_value_handle){
        log_info("Read protocol mode");
        return att_read_callback_handle_byte(instance->hid_protocol_mode, offset, buffer, buffer_size);
    }
    
    if (att_handle == instance->hid_report_map_handle){
        log_info("Read report map");
        return att_read_callback_handle_blob(instance->hid_descriptor, instance->hid_descriptor_size, offset, buffer, buffer_size);
    }

    if (att_handle == instance->hid_boot_mouse_input_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(instance->hid_boot_mouse_input_client_configuration_value, offset, buffer, buffer_size);
    }

    if (att_handle == instance->hid_boot_keyboard_input_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(instance->hid_boot_keyboard_input_client_configuration_value, offset, buffer, buffer_size);
    }

    if (att_handle == instance->hid_report_input_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(instance->hid_report_input_client_configuration_value, offset, buffer, buffer_size);
    }
    
    if (att_handle == instance->hid_report_output_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(instance->hid_report_output_client_configuration_value, offset, buffer, buffer_size);
    }
    
    if (att_handle == instance->hid_report_feature_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(instance->hid_report_feature_client_configuration_value, offset, buffer, buffer_size);
    }
    
    if (att_handle == instance->hid_control_point_value_handle){
        if (buffer && buffer_size >= 1){
            buffer[0] = instance->hid_control_point_suspend;
        } 
        return 1;
    }
    return 0;
}

static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(buffer_size);
    UNUSED(offset);

    hids_device_t * instance = hids_device_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return HIDS_DEVICE_ERROR_CODE_INAPPROPRIATE_CONNECTION_PARAMETERS;
    }

    if (att_handle == instance->hid_boot_mouse_input_client_configuration_handle){
        uint16_t new_value = little_endian_read_16(buffer, 0);
        instance->hid_boot_mouse_input_client_configuration_value = new_value;
        hids_device_emit_event_with_uint8(HIDS_SUBEVENT_BOOT_MOUSE_INPUT_REPORT_ENABLE, con_handle, new_value);
    }
    if (att_handle == instance->hid_boot_keyboard_input_client_configuration_handle){
        uint16_t new_value = little_endian_read_16(buffer, 0);
        instance->hid_boot_keyboard_input_client_configuration_value = new_value;
        hids_device_emit_event_with_uint8(HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE, con_handle, new_value);
    }
    if (att_handle == instance->hid_report_input_client_configuration_handle){
        uint16_t new_value = little_endian_read_16(buffer, 0);
        instance->hid_report_input_client_configuration_value = new_value;
        log_info("Enable Report Input notifications: %x", new_value);
        hids_device_emit_event_with_uint8(HIDS_SUBEVENT_INPUT_REPORT_ENABLE, con_handle, new_value);
    }
    if (att_handle == instance->hid_report_output_client_configuration_handle){
        uint16_t new_value = little_endian_read_16(buffer, 0);
        instance->hid_report_output_client_configuration_value = new_value;
        log_info("Enable Report Output notifications: %x", new_value);
        hids_device_emit_event_with_uint8(HIDS_SUBEVENT_OUTPUT_REPORT_ENABLE, con_handle, new_value);
    }
    if (att_handle == instance->hid_report_feature_client_configuration_handle){
        uint16_t new_value = little_endian_read_16(buffer, 0);
        instance->hid_report_feature_client_configuration_value = new_value;
        log_info("Enable Report Feature notifications: %x", new_value);
        hids_device_emit_event_with_uint8(HIDS_SUBEVENT_FEATURE_REPORT_ENABLE, con_handle, new_value);
    }

    if (att_handle == instance->hid_protocol_mode_value_handle){
        instance->hid_protocol_mode = buffer[0];
        log_info("Set protocol mode: %u", instance->hid_protocol_mode);
        hids_device_emit_event_with_uint8(HIDS_SUBEVENT_PROTOCOL_MODE, con_handle, instance->hid_protocol_mode);
    }
    
    if (att_handle == instance->hid_control_point_value_handle){
        if (buffer_size < 1){
            return ATT_ERROR_INVALID_OFFSET;
        }
        instance->hid_control_point_suspend = buffer[0];
        instance->con_handle = con_handle;
        log_info("Set suspend tp: %u", instance->hid_control_point_suspend );
        if (instance->hid_control_point_suspend == 0){
            hids_device_emit_event(HIDS_SUBEVENT_SUSPEND, con_handle);
        } else if (instance->hid_control_point_suspend == 1){ 
            hids_device_emit_event(HIDS_SUBEVENT_EXIT_SUSPEND, con_handle);
        }
    }
    return 0;
}

/**
 * @brief Set up HIDS Device
 */
void hids_device_init(uint8_t country_code, const uint8_t * descriptor, uint16_t descriptor_size){
    hids_device_t * instance = hids_device_create_instance();
    if (!instance){
        log_error("hids_device_init: instance could not be created, not enough memory");
        return;
    }

    instance->hid_country_code    = country_code;
    instance->hid_descriptor      = descriptor;
    instance->hid_descriptor_size = descriptor_size;

    // default
    instance->hid_protocol_mode   = 1;

    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xfff;
    int service_found = gatt_server_get_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE, &start_handle, &end_handle);
    if (!service_found) return;

    // get report map handle
    instance->hid_report_map_handle                               = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_REPORT_MAP);

    // get report map handle
    instance->hid_protocol_mode_value_handle                      = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_PROTOCOL_MODE);

    // get value and client configuration handles for boot mouse input, boot keyboard input and report input
    instance->hid_boot_mouse_input_value_handle                   = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT);
    instance->hid_boot_mouse_input_client_configuration_handle    = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT);

    instance->hid_boot_keyboard_input_value_handle                = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT);
    instance->hid_boot_keyboard_input_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT);

    instance->hid_report_input_value_handle                       = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_REPORT);
    instance->hid_report_input_client_configuration_handle        = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_REPORT);

    instance->hid_report_output_value_handle                      = gatt_server_get_value_handle_for_characteristic_with_uuid16(instance->hid_report_input_client_configuration_handle+1, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_REPORT);
    instance->hid_report_output_client_configuration_handle       = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(instance->hid_report_input_client_configuration_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_REPORT);

    instance->hid_report_feature_value_handle                     = gatt_server_get_value_handle_for_characteristic_with_uuid16(instance->hid_report_output_client_configuration_handle+1, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_REPORT);
    instance->hid_report_feature_client_configuration_handle      = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(instance->hid_report_output_client_configuration_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_REPORT);

    instance->hid_control_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HID_CONTROL_POINT);
    
    log_info("hid_report_map_handle                               0x%02x", instance->hid_report_map_handle);
    log_info("hid_protocol_mode_value_handle                      0x%02x", instance->hid_protocol_mode_value_handle);
    log_info("hid_boot_mouse_input_value_handle                   0x%02x", instance->hid_boot_mouse_input_value_handle);
    log_info("hid_boot_mouse_input_client_configuration_handle    0x%02x", instance->hid_boot_mouse_input_client_configuration_handle);
    log_info("hid_boot_keyboard_input_value_handle                0x%02x", instance->hid_boot_keyboard_input_value_handle);
    log_info("hid_boot_keyboard_input_client_configuration_handle 0x%02x", instance->hid_boot_keyboard_input_client_configuration_handle);
    log_info("hid_report_input_value_handle                       0x%02x", instance->hid_report_input_value_handle);
    log_info("hid_report_input_client_configuration_handle        0x%02x", instance->hid_report_input_client_configuration_handle);
    log_info("hid_report_output_value_handle                      0x%02x", instance->hid_report_output_value_handle);
    log_info("hid_report_output_client_configuration_handle       0x%02x", instance->hid_report_output_client_configuration_handle);
    log_info("hid_report_feature_value_handle                     0x%02x", instance->hid_report_feature_value_handle);
    log_info("hid_report_feature_client_configuration_handle      0x%02x", instance->hid_report_feature_client_configuration_handle);
    log_info("hid_control_point_value_handle                      0x%02x", instance->hid_control_point_value_handle);

    // register service with ATT Server
    hid_service.start_handle   = start_handle;
    hid_service.end_handle     = end_handle;
    hid_service.read_callback  = &att_read_callback;
    hid_service.write_callback = &att_write_callback;
    att_server_register_service_handler(&hid_service);
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
    hids_device_t * instance = hids_device_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return;
    }

    instance->battery_callback.callback = &hids_device_can_send_now;
    instance->battery_callback.context  = (void*) (uintptr_t) con_handle;
    att_server_register_can_send_now_callback(&instance->battery_callback, con_handle);
}

/**
 * @brief Send HID Report: Input
 */
void hids_device_send_input_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len){
    hids_device_t * instance = hids_device_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return;
    }
    att_server_notify(con_handle, instance->hid_report_input_value_handle, report, report_len);
}

/**
 * @brief Send HID Report: Output
 */
void hids_device_send_output_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len){
    hids_device_t * instance = hids_device_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return;
    }
    att_server_notify(con_handle, instance->hid_report_output_value_handle, report, report_len);
}

/**
 * @brief Send HID Report: Feature
 */
void hids_device_send_feature_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len){
    hids_device_t * instance = hids_device_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return;
    }
    att_server_notify(con_handle, instance->hid_report_feature_value_handle, report, report_len);
}

/**
 * @brief Send HID Boot Mouse Input Report
 */
void hids_device_send_boot_mouse_input_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len){
    hids_device_t * instance = hids_device_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return;
    }
    att_server_notify(con_handle, instance->hid_boot_mouse_input_value_handle, report, report_len);
}

/**
 * @brief Send HID Boot Mouse Input Report
 */
void hids_device_send_boot_keyboard_input_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len){
    hids_device_t * instance = hids_device_get_instance_for_con_handle(con_handle);
    if (!instance){
        log_error("no instance for handle 0x%02x", con_handle);
        return;
    }
    att_server_notify(con_handle, instance->hid_boot_keyboard_input_value_handle, report, report_len);
}
