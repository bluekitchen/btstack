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

// *****************************************************************************
//
// HSP Headset (!! UNDER DEVELOPMENT !!)
//
// *****************************************************************************


#ifndef btstack_hsp_hs_h
#define btstack_hsp_hs_h

#include "hci.h"
#include "sdp_query_rfcomm.h"

#if defined __cplusplus
extern "C" {
#endif

typedef void (*hsp_hs_callback_t)(uint8_t * event, uint16_t event_size);
// Register callback (packet handler) for hsp headset
void hsp_hs_register_packet_handler(hsp_hs_callback_t callback);
void hsp_hs_create_service(uint8_t * service, int rfcomm_channel_nr, const char * name, uint8_t have_remote_audio_control);

void hsp_hs_init(uint8_t rfcomm_channel_nr);
void hsp_hs_connect(bd_addr_t bd_addr);
void hsp_hs_disconnect();

// AT+VGM=[0..15]
void hsp_hs_set_microphone_gain(uint8_t gain);
// AT+VGS=[0..15]
void hsp_hs_set_speaker_gain(uint8_t gain);

void hsp_hs_support_custom_indications(int enable);

// When support custom commands is enabled, AG will send HSP_SUBEVENT_AG_INDICATION.
// On occurance of this event, client's packet handler must send the result back
// by calling hsp_hs_send_result function.
int hsp_hs_send_result(char * indication);

#if defined __cplusplus
}
#endif

#endif