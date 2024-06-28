/*
 * Copyright (C) 2024 BlueKitchen GmbH
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
 * @title Telephony and Media AudioService Client
 * 
 */

#ifndef TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_H
#define TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_H

#include <stdint.h>
#include "le-audio/le_audio.h"
#include "ble/gatt_client.h"

#if defined __cplusplus
extern "C" {
#endif

#define TMAS_ROLE_BITMASK_CALL_GATEWAY_SUPPORT                      0x0001
#define TMAS_ROLE_BITMASK_CALL_TERMINAL_SUPPORT                     0x0002
#define TMAS_ROLE_BITMASK_UNICAST_MEDIA_SENDER_SUPPORT              0x0004
#define TMAS_ROLE_BITMASK_UNICAST_MEDIA_RECEIVER_SUPPORT            0x0008
#define TMAS_ROLE_BITMASK_BROADCAST_MEDIA_SENDER_SUPPORT            0x0010
#define TMAS_ROLE_BITMASK_BROADCAST_MEDIA_RECEIVER_SUPPORT          0x0020
#define TMAS_ROLE_BITMASK_ALL                                       0x003F


/* API_START */

/**
 * @brief Init Telephony and Media Audio Service Client
 */
void telephony_and_media_audio_service_client_init(void);

/**
 * @brief Read supported roles. The result will be delivered via LEAUDIO_SUBEVENT_TMAS_CLIENT_SUPPORTED_ROLES_BITMAP event.
 * 
 * @param packet_handler
 * @param con_handle
 * @return ERROR_CODE_SUCCESS if ok, otherwise:
 *              - ERROR_CODE_REPEATED_ATTEMPTS if query already in progress,
 *              - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if con_handle unknown, and
 *              - ERROR_CODE_COMMAND_DISALLOWED if callback already registered
 */
uint8_t telephony_and_media_audio_service_client_read_roles(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle);

/**
 * @brief Deinit CSIS Client
 */
void coordinated_set_identification_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

