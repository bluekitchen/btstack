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
 * @title Human Interface Device (HID)
 *
 */

#ifndef HID_H
#define HID_H

#if defined __cplusplus
extern "C" {
#endif


#include <stdint.h>

#define HID_BOOT_MODE_KEYBOARD_ID 1
#define HID_BOOT_MODE_MOUSE_ID    2

typedef enum {
    HID_MESSAGE_TYPE_HANDSHAKE = 0,
    HID_MESSAGE_TYPE_HID_CONTROL,
    HID_MESSAGE_TYPE_RESERVED_2,
    HID_MESSAGE_TYPE_RESERVED_3,
    HID_MESSAGE_TYPE_GET_REPORT,
    HID_MESSAGE_TYPE_SET_REPORT,
    HID_MESSAGE_TYPE_GET_PROTOCOL,
    HID_MESSAGE_TYPE_SET_PROTOCOL,
    HID_MESSAGE_TYPE_GET_IDLE_DEPRECATED,
    HID_MESSAGE_TYPE_SET_IDLE_DEPRECATED,
    HID_MESSAGE_TYPE_DATA,
    HID_MESSAGE_TYPE_DATC_DEPRECATED
} hid_message_type_t;

typedef enum {
    HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL = 0x00,        // This code is used to acknowledge requests. A device that has correctly received SET_REPORT, SET_IDLE or SET_PROTOCOL payload transmits an acknowledgment to the host.
    HID_HANDSHAKE_PARAM_TYPE_NOT_READY,                // This code indicates that a device is too busy to accept data. The Bluetooth HID Host should retransmit the data the next time it communicates with the device.
    HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_REPORT_ID,    // Invalid report ID transmitted.
    HID_HANDSHAKE_PARAM_TYPE_ERR_UNSUPPORTED_REQUEST,  // The device does not support the request. This result code shall be used if the HIDP message type is unsupported.
    HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER,    // A parameter value is out of range or inappropriate for the request.
    HID_HANDSHAKE_PARAM_TYPE_ERR_UNKNOWN = 0x0E,       // Device could not identify the error condition.
    HID_HANDSHAKE_PARAM_TYPE_ERR_FATAL = 0x0F,         // Restart is essential to resume functionality
    // BTstack custom error codes
    HID_HANDSHAKE_PARAM_TYPE_ERR_DISCONNECT            
} hid_handshake_param_type_t;

typedef enum {
    HID_CONTROL_PARAM_NOP_DEPRECATED = 0,              // Deprecated: No Operation.
    HID_CONTROL_PARAM_HARD_RESET_DEPRECATED,           // Deprecated: Device performs Power On System Test (POST) then initializes all internal variables and initiates normal operations.
    HID_CONTROL_PARAM_SOFT_RESET_DEPRECATED,           // Deprecated: Device initializes all internal variables and initiates normal operations.
    HID_CONTROL_PARAM_SUSPEND = 0x03,                  // Go to reduced power mode.
    HID_CONTROL_PARAM_EXIT_SUSPEND,                    // Exit reduced power mode.
    HID_CONTROL_PARAM_VIRTUAL_CABLE_UNPLUG
} hid_control_param_t;

typedef enum {
    HID_PROTOCOL_MODE_BOOT = 0,   
    HID_PROTOCOL_MODE_REPORT,

    // the following item is only used for API calls in hid_host.h: hid_host_connect, hid_host_accept_connection
    // in contrast to previous two enum items that will enforce given mode, this one enables fallback from report to boot mode
    HID_PROTOCOL_MODE_REPORT_WITH_FALLBACK_TO_BOOT
} hid_protocol_mode_t;

typedef enum {
    HID_REPORT_TYPE_RESERVED = 0,
    HID_REPORT_TYPE_INPUT,
    HID_REPORT_TYPE_OUTPUT,
    HID_REPORT_TYPE_FEATURE
} hid_report_type_t;

typedef enum {
    HID_REPORT_ID_UNDECLARED,
    HID_REPORT_ID_VALID,
    HID_REPORT_ID_INVALID
} hid_report_id_status_t;

/* API_START */

/*
 * @brief Get boot descriptor data
 * @result data
 */
const uint8_t * btstack_hid_get_boot_descriptor_data(void);

/*
 * @brief Get boot descriptor length
 * @result length
 */
uint16_t btstack_hid_get_boot_descriptor_len(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
