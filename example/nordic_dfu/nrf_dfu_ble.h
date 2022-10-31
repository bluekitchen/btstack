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

/**
 * @title NRF DFU Service Server
 * 
 */

#ifndef __NRF_DFU_BLE_H__
#define __NRF_DFU_BLE_H__

#include <stdint.h>
#include "bluetooth.h"
#include "btstack_defines.h"

#if defined __cplusplus
extern "C" {
#endif

#define GATT_MAX_MTU_SIZE          247
#define GATT_HEADER_LEN            3                                                       /**< GATT header length. */
#define GATT_PAYLOAD(mtu)          ((mtu) - GATT_HEADER_LEN)                               /**< Length of the ATT payload for a given ATT MTU. */
#define MAX_DFU_PKT_LEN            (GATT_MAX_MTU_SIZE - GATT_HEADER_LEN)                   /**< Maximum length (in bytes) of the DFU Packet characteristic (3 bytes are used for the GATT opcode and handle). */
#define MAX_RESPONSE_LEN           17                                                      /**< Maximum length (in bytes) of the response to a Control Point command. */
#define RESPONSE_HEADER_LEN        3                                                       /**< The length of the header of a response. I.E. the index of the opcode-specific payload. */

#define NRF_DFU_BUTTONLESS_RSP     0x20

typedef enum {
    NRF_DFU_EVT_CHANGE_BOOTLOADER_NAME = 0,
    NRF_DFU_EVT_ENTER_BOOTLOADER_MODE
} nrf_dfu_ble_evt_t;

typedef void (*nrf_dfu_ble_packet_handler_t)(nrf_dfu_ble_evt_t evt, uint8_t *packet, uint16_t size);

typedef enum {
    NRF_DFU_BUT_CMD_ENTR_BOOTLOADER = 0x01,
    NRF_DFU_BUT_CMD_CHANGE_BOOTLOADER_NAME = 0x02
} nrf_dfu_buttonless_cmd_code_t;

typedef struct {
    uint8_t rsp_code;
    uint8_t cmd_code;
    uint8_t result;
} __attribute__((packed)) nrf_dfu_buttonless_rsp_t;

typedef struct {
  uint16_t value_handle;       /**< Handle to the characteristic value. */
  uint16_t user_desc_handle;   /**< Handle to the User Description descriptor. */
  uint16_t cccd_handle;        /**< Handle to the Client Characteristic Configuration Descriptor. */
  uint16_t sccd_handle;        /**< Handle to the Server Characteristic Configuration Descriptor. */    
} nrf_dfu_cha_handles_t;

typedef struct {
    hci_con_handle_t con_handle;
    nrf_dfu_cha_handles_t dfu_ctrl_pt_handles;
    nrf_dfu_cha_handles_t dfu_pkt_handles;
    nrf_dfu_cha_handles_t dfu_buttonless_handles;
} nrf_dfu_ble_t;

/* API_START */

/**
 * @text The NRF DFU Service is implementation of the Nordic DFU profile.
 *
 * To use with your application, add `#import <nordic_dfu_service.gatt` to your .gatt file
 * and call all functions below. All strings and blobs need to stay valid after calling the functions.
 */

/**
 * @brief Init NRF DFU Service Server with ATT DB
 * @param packet_handler for events and data from dfu controller
 */
void nrf_dfu_ble_init(nrf_dfu_ble_packet_handler_t packet_handler);



/* API_END */

#if defined __cplusplus
}
#endif

#endif

