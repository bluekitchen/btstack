/*
 * Copyright (C) 2020 BlueKitchen GmbH
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

#ifndef HID_HOST_H
#define HID_HOST_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_hid_parser.h"
#include "classic/hid.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */
/**
 * @brief Set up HID Host 
 * @param boot_protocol_mode_supported
 * @param hid_descriptor_storage
 * @param hid_descriptor_storage_len
 */
void hid_host_init(const uint8_t * hid_descriptor_storage, uint16_t hid_descriptor_storage_len);

/**
 * @brief Register callback for the HID Device Host. 
 * @param callback
 */
void hid_host_register_packet_handler(btstack_packet_handler_t callback);

/*
 * @brief Create HID connection to HID Host
 * @param addr
 * @param hid_cid to use for other commands
 * @result status
 */
uint8_t hid_host_connect(bd_addr_t addr, hid_protocol_mode_t protocol_mode, uint16_t * hid_cid);

/*
 * @brief Disconnect from HID Host
 * @param hid_cid
 */
void hid_host_disconnect(uint16_t hid_cid);

/**
 * @brief Request can send now event to send HID Report
 * Generates an HID_SUBEVENT_CAN_SEND_NOW subevent
 * @param hid_cid
 */
void hid_host_request_can_send_now_event(uint16_t hid_cid);

/**
 * @brief Send HID message on interrupt channel
 * @param hid_cid
 */
void hid_host_send_interrupt_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len);

/**
 * @brief Send HID message on control channel
 * @param hid_cid
 */
void hid_host_send_control_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len);



/* API_END */


#if defined __cplusplus
}
#endif

#endif
