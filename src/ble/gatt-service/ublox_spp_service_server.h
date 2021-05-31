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
 * @title u-blox SPP Service Server
 * 
 */

#ifndef UBLOX_SPP_H
#define UBLOX_SPP_H

#include <stdint.h>
#include "bluetooth.h"
#include "btstack_defines.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @text The u-blox SPP Service is implementation of the u-Blox SPP-like profile.
 *
 * To use with your application, add `#import <ublox_spp_service.gatt` to your .gatt file
 * and call all functions below. All strings and blobs need to stay valid after calling the functions.
 */

/**
 * @brief Init ublox SPP Service Server with ATT DB
 * @param packet_handler for events and tx data from peer as RFCOMM_DATA_PACKET
 */
void ublox_spp_service_server_init(btstack_packet_handler_t packet_handler);

/** 
 * @brief Queue send request. When called, one packet can be send via ublox_spp_service_send below
 * @param request
 * @param con_handle
 */
void ublox_spp_service_server_request_can_send_now(btstack_context_callback_registration_t * request, hci_con_handle_t con_handle);

/**
 * @brief Send data
 * @param con_handle
 * @param data
 * @param size
 */
int ublox_spp_service_server_send(hci_con_handle_t con_handle, const uint8_t * data, uint16_t size);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

