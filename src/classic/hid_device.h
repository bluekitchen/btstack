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

#include <stdint.h>
#include "btstack_defines.h"
/**
 * @brief Create HID Device SDP service record. 
 * @param service Empty buffer in which a new service record will be stored.
 * @param have_remote_audio_control 
 * @param service
 * @param service_record_handle
 * @param hid_device_subclass
 * @param hid_country_code
 * @param hid_virtual_cable
 * @param hid_reconnect_initiate
 * @param hid_boot_device
 * @param hid_descriptor
 * @param hid_descriptor_size size of hid_descriptor
 * @param device_name
 */
void hid_create_sdp_record(
    uint8_t *       service, 
    uint32_t        service_record_handle,
    uint16_t        hid_device_subclass,
    uint8_t         hid_country_code,
    uint8_t         hid_virtual_cable,
    uint8_t         hid_reconnect_initiate,
    uint8_t         hid_boot_device,
    const uint8_t * hid_descriptor,
    uint16_t 		hid_descriptor_size,
    const char *    device_name);

/**
 * @brief Set up HID Device 
 */
void hid_device_init(void);

/**
 * @brief Register callback for the HID Device client. 
 * @param callback
 */
void hid_device_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Request can send now event to send HID Report
 * Generates an HID_SUBEVENT_CAN_SEND_NOW subevent
 * @param hid_cid
 */
void hid_device_request_can_send_now_event(uint16_t hid_cid);

/**
 * @brief Send HID messageon interrupt channel
 * @param hid_cid
 */
void hid_device_send_interrupt_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len);

/**
 * @brief Send HID messageon control channel
 * @param hid_cid
 */
void hid_device_send_contro_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len);


