/*
 * Copyright (C) 2022 BlueKitchen GmbH
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
 * @title Broadcast Audio Scan Service Server (BASS)
 *
 * @text The Broadcast Audio Scan Service is used by servers to expose their status with respect 
 * to synchronization to broadcast Audio Streams and associated data, including Broadcast_Codes 
 * used to decrypt encrypted broadcast Audio Streams. Clients can use the attributes exposed by 
 * servers to observe and/or request changes in server behavior.
 * 
 * To use with your application, add `#import <broadcast_audio_scan_service.gatt>` to your .gatt file. 
 */

#ifndef BROADCAST_AUDIO_SCAN_SERVICE_SERVER_H
#define BROADCAST_AUDIO_SCAN_SERVICE_SERVER_H

#include <stdint.h>

#include "btstack_defines.h"
#include "le-audio/le_audio.h"

#include "broadcast_audio_scan_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */
// memory for list of these structs is allocated by the application
typedef struct {
    // assigned by client via control point
    bass_source_data_t data;

    uint8_t  update_counter;
    uint8_t  source_id; 
    bool     in_use;

    le_audio_big_encryption_t big_encryption;
    uint8_t  bad_code[16];

    uint16_t bass_receive_state_handle;
    uint16_t bass_receive_state_client_configuration_handle;
    uint16_t bass_receive_state_client_configuration;
} bass_server_source_t;

typedef struct {
    hci_con_handle_t con_handle;
    uint16_t sources_to_notify;

    // used for caching long write
    uint8_t  long_write_buffer[512];
    uint16_t long_write_value_size;
    uint16_t long_write_attribute_handle;
} bass_server_connection_t;

/**
 * @brief Init Broadcast Audio Scan Service Server with ATT DB
 * @param sources_num
 * @param sources
 * @param clients_num
 * @param clients
 */
void broadcast_audio_scan_service_server_init(uint8_t const sources_num, bass_server_source_t * sources, uint8_t const clients_num, bass_server_connection_t * clients);

/**
 * @brief Register packet handler to receive events:
 * - LEAUDIO_SUBEVENT_BASS_SERVER_SCAN_STOPPED
 * - LEAUDIO_SUBEVENT_BASS_SERVER_SCAN_STARTED
 * - LEAUDIO_SUBEVENT_BASS_SERVER_BROADCAST_CODE
 * - LEAUDIO_SUBEVENT_BASS_SERVER_SOURCE_ADDED
 * - LEAUDIO_SUBEVENT_BASS_SERVER_SOURCE_MODIFIED
 * - LEAUDIO_SUBEVENT_BASS_SERVER_SOURCE_DELETED
 * @param packet_handler
 */
void broadcast_audio_scan_service_server_register_packet_handler(btstack_packet_handler_t packet_handler);

/**
 * @brief Set PA state of source.
 * @param source_index
 * @param sync_state
 */
void broadcast_audio_scan_service_server_set_pa_sync_state(uint8_t source_index, le_audio_pa_sync_state_t sync_state);

/**
 * @brief Add source.
 * @param source_data
 * @param source_index
 */
void broadcast_audio_scan_service_server_add_source(const bass_source_data_t *source_data, uint8_t * source_index);

/**
 * @brief Deinit Broadcast Audio Scan Service Server
 */
void broadcast_audio_scan_service_server_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

