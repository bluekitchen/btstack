/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

/*
 * avrcp.h
 * 
 * Audio/Video Remote Control Profile
 *
 */

#ifndef __AVRCP_TARGET_H
#define __AVRCP_TARGET_H

#include <stdint.h>
#include "avrcp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */


/**
 * @brief AVDTP Source service record. 
 * @param service
 * @param service_record_handle
 * @param browsing  1 - supported, 0 - not supported
 * @param supported_features 16-bit bitmap, see AVDTP_SINK_SF_* values in avdtp.h
 * @param service_name
 * @param service_provider_name
 */
void    avrcp_target_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name);

void    avrcp_target_init(void);

void    avrcp_target_register_packet_handler(btstack_packet_handler_t callback);

uint8_t avrcp_target_connect(bd_addr_t bd_addr, uint16_t * avrcp_cid);

uint8_t avrcp_target_disconnect(uint16_t avrcp_cid);

uint8_t avrcp_target_unit_info(uint16_t avrcp_cid, avrcp_subunit_type_t unit_type, uint32_t company_id);
uint8_t avrcp_target_subunit_info(uint16_t avrcp_cid, avrcp_subunit_type_t subunit_type, uint8_t offset, uint8_t * subunit_info_data);

uint8_t avrcp_target_supported_companies(uint16_t avrcp_cid, uint8_t capabilities_length, uint8_t * capabilities, uint8_t size);
uint8_t avrcp_target_supported_events(uint16_t avrcp_cid, uint8_t capabilities_length, uint8_t * capabilities, uint8_t size);

uint8_t avrcp_target_play_status(uint16_t avrcp_cid, uint32_t song_length_ms, uint32_t song_position_ms, avrcp_play_status_t status); 

uint8_t avrcp_target_set_now_playing_title(uint16_t avrcp_cid, const char * title);
uint8_t avrcp_target_set_now_playing_artist(uint16_t avrcp_cid, const char * artist);
uint8_t avrcp_target_set_now_playing_album(uint16_t avrcp_cid, const char * album);
uint8_t avrcp_target_set_now_playing_genre(uint16_t avrcp_cid, const char * genre);
uint8_t avrcp_target_set_now_playing_song_length_ms(uint16_t avrcp_cid, const uint32_t song_length_ms);
uint8_t avrcp_target_set_now_playing_total_tracks(uint16_t avrcp_cid, const int total_tracks);
uint8_t avrcp_target_set_now_playing_track_nr(uint16_t avrcp_cid, const int track_nr);
uint8_t avrcp_target_now_playing_info(uint16_t avrcp_cid);

/* API_END */
#if defined __cplusplus
}
#endif

#endif // __AVRCP_TARGET_H