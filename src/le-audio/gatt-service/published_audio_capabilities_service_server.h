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
 * @title Published Audio Capabilities Service Server (PACS)
 * 
 */

#ifndef PUBLISHED_AUDIO_CAPABILITIES_SERVICE_SERVER_H
#define PUBLISHED_AUDIO_CAPABILITIES_SERVICE_SERVER_H

#include <stdint.h>
#include "le-audio/le_audio.h"
#include "le-audio/gatt-service/published_audio_capabilities_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @text The Published Audio Capabilities Service Server exposes server audio capabilities and audio
 * availability. 
 * 
 * To use with your application, add `#import <published_audio_capabilities_service.gatt>` to your .gatt file. 
 */


/**
 * @brief Init Published Audio Capabilities Service Server with ATT DB
 * @param sink_endpoint
 * @param source_endpoint
 */
void published_audio_capabilities_service_server_init(pacs_streamendpoint_t * sink_endpoint, pacs_streamendpoint_t * source_endpoint);

/**
 * @brief Register callback.
 * @param callback
 */
void published_audio_capabilities_service_server_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Trigger notification of Sink PAC record values.
 */
void published_audio_capabilities_service_server_sink_pac_modified(void);

/**
 * @brief Trigger notification of Source PAC record values.
 */
void published_audio_capabilities_service_server_source_pac_modified(void);

/**
 * @brief Set sink audio locations mask. The last subscribed client will be notified on change (this will be extended to all subscribed clients).
 * @param audio_locations_mask
 */
void published_audio_capabilities_service_server_set_sink_audio_locations(uint32_t audio_locations_mask);

/**
 * @brief Set source audio locations mask. The last subscribed client will be notified on change (this will be extended to all subscribed clients).
 * @param audio_locations_mask
 */
void published_audio_capabilities_service_server_set_source_audio_locations(uint32_t audio_locations_mask);

/**
 * @brief Set available audio context mask. The last subscribed client will be notified on change (this will be extended to all subscribed clients).
 * @param available_sink_audio_contexts_mask
 * @param available_source_audio_contexts_mask
 */
void published_audio_capabilities_service_server_set_available_audio_contexts(
    uint16_t available_sink_audio_contexts_mask, 
    uint16_t available_source_audio_contexts_mask);

/**
 * @brief Set supported audio context mask. The last subscribed client will be notified on change (this will be extended to all subscribed clients).
 * @param supported_sink_audio_contexts_mask
 * @param supported_source_audio_contexts_mask
 */
void published_audio_capabilities_service_server_set_supported_audio_contexts(
    uint16_t supported_sink_audio_contexts_mask,
    uint16_t supported_source_audio_contexts_mask);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

