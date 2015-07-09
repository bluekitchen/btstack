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
//  HFP Hands-Free (HF) unit and Audio-Gateway Commons
//
// *****************************************************************************


#ifndef btstack_hfp_h
#define btstack_hfp_h

#include "hci.h"
#include "sdp_query_rfcomm.h"

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    HFP_IDLE,
    HFP_SDP_QUERY_RFCOMM_CHANNEL,
    HFP_W4_SDP_QUERY_COMPLETE,
    HFP_W4_RFCOMM_CONNECTED,
    HFP_ACTIVE,
    HFP_W2_DISCONNECT_RFCOMM,
    HFP_W4_RFCOMM_DISCONNECTED, 
    HFP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN
} hfp_state_t;

typedef void (*hfp_callback_t)(uint8_t * event, uint16_t event_size);

typedef struct hfp_connection {
    linked_item_t    item;
    hfp_state_t state;

    bd_addr_t remote_addr;
    uint16_t con_handle;
    uint16_t rfcomm_channel_nr;
    uint16_t rfcomm_cid;
    
    hfp_callback_t callback;
} hfp_connection_t;


void hfp_create_service(uint8_t * service, uint16_t service_uuid, int rfcomm_channel_nr, const char * name, uint16_t supported_features);

void hfp_init(uint16_t rfcomm_channel_nr);
void hfp_register_packet_handler(hfp_callback_t callback);

hfp_connection_t * provide_hfp_connection_context_for_conn_handle(uint16_t con_handle);
void hfp_connect(bd_addr_t bd_addr);

#if defined __cplusplus
}
#endif

#endif