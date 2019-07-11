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
#ifndef HIDS_DEVICE_H
#define HIDS_DEVICE_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * Implementation of the GATT HIDS Device
 * To use with your application, add '#import <hids.gatt>' to your .gatt file
 */

/**
 * @brief Set up HIDS Device
 */
void hids_device_init(uint8_t hid_country_code, const uint8_t * hid_descriptor, uint16_t hid_descriptor_size);

/**
 * @brief Register callback for the HIDS Device client.
 * @param callback
 */
void hids_device_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Request can send now event to send HID Report
 * Generates an HIDS_SUBEVENT_CAN_SEND_NOW subevent
 * @param hid_cid
 */
void hids_device_request_can_send_now_event(hci_con_handle_t con_handle);

/**
 * @brief Send HID Report: Input
 */
void hids_device_send_input_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len);

/**
 * @brief Send HID Report: Output
 */
void hids_device_send_output_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len);

/**
 * @brief Send HID Report: Feature
 */
void hids_device_send_feature_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len);

/**
 * @brief Send HID Boot Mouse Input Report
 */
void hids_device_send_boot_mouse_input_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len);

/**
 * @brief Send HID Boot Mouse Input Report
 */
void hids_device_send_boot_keyboard_input_report(hci_con_handle_t con_handle, const uint8_t * report, uint16_t report_len);

#if defined __cplusplus
}
#endif

#endif
