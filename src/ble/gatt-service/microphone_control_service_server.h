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
 * @title Microphone Control Service Server
 * 
 */

#ifndef MICROPHONE_CONTROL_SERVICE_SERVER_H
#define MICROPHONE_CONTROL_SERVICE_SERVER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

#define ATT_ERROR_RESPONSE_MICROPHONE_CONTROL_MUTE_DISABLED    0x80

/**
 * @text The Microphone Control Service enables a device to expose the mute control and state of one or more microphones.
 * Only server can disable and enable mute. Currently one one client supported.
 * 
 * To use with your application, add `#import <microphone_control_service.gatt>` to your .gatt file. 
 */

/* API_START */

/**
 * @brief Init Microphone Control Service Server with ATT DB
 * @param mute_state
 */
void microphone_control_service_server_init(gatt_microphone_control_mute_t mute_state);

/**
 * @brief Register callback to receive updates of mute value from remote side via MICS_MUTE event
 * @param callback
 */
void microphone_control_service_server_register_packet_hanlder(btstack_packet_handler_t callback);

/**
 * @brief Set mute value.
 * @param mute_state
 */
void microphone_control_service_server_set_mute(gatt_microphone_control_mute_t mute_state);


/* API_END */

#if defined __cplusplus
}
#endif

#endif

