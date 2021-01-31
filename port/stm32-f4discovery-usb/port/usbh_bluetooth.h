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

// *****************************************************************************
//
// USB Bluetooth Host Class for STM32Cube USB Host Library
//
// *****************************************************************************

#ifndef USBH_BLUETOOTH_H
#define USBH_BLUETOOTH_H

#include "btstack_config.h"
#include "btstack_bool.h"

#include <stdint.h>

#include "usbh_core.h"

#if defined __cplusplus
extern "C" {
#endif

/* Bluetooth Class Codes */
#define USB_BLUETOOTH_CLASS                                 0xE0U

extern USBH_ClassTypeDef  Bluetooth_Class;
#define USBH_BLUETOOTH_CLASS    &Bluetooth_Class

/**
 * @brief Check stack if a packet can be sent now
 * @return true if packet can be sent
 */
bool usbh_bluetooth_can_send_now(void);

/**
 * @brief Send HCI Command packet
 * @param packet
 * @param len
 */
void usbh_bluetooth_send_cmd(const uint8_t * packet, uint16_t len);

/**
 * @brief Send HCI ACL packet
 * @param packet
 * @param len
 */
void usbh_bluetooth_send_acl(const uint8_t * packet, uint16_t len);

/**
 * @brief Set packet sent callback
 * @param callback
 */
void usbh_bluetooth_set_packet_sent(void (*callback)(void));

/**
 * @brief Set packet handler
 * @param callback
 */
void usbh_bluetooth_set_packet_received(void (*callback)(uint8_t packet_type, uint8_t * packet, uint16_t size));

#endif
