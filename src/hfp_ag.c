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
// Minimal setup for HFP Audio Gateway (AG) unit (!! UNDER DEVELOPMENT !!)
//
// *****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "sdp_query_rfcomm.h"
#include "sdp.h"
#include "debug.h"
#include "hfp.h"
#include "hfp_ag.h"

static const char default_hfp_ag_service_name[] = "Voice gateway";
static uint16_t hfp_supported_features = HFP_DEFAULT_HF_SUPPORTED_FEATURES;
static uint8_t hfp_codecs_nr = 0;
static uint8_t hfp_codecs[HFP_MAX_NUM_CODECS];

static void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void hfp_ag_create_service(uint8_t * service, int rfcomm_channel_nr, const char * name, uint8_t ability_to_reject_call, uint16_t supported_features){
    if (!name){
        name = default_hfp_ag_service_name;
    }
    hfp_create_service(service, SDP_HandsfreeAudioGateway, rfcomm_channel_nr, name, supported_features);
    
    // Network
    de_add_number(service, DE_UINT, DE_SIZE_8, ability_to_reject_call);
    /*
     * 0x01 – Ability to reject a call
     * 0x00 – No ability to reject a call
     */
}

static void hfp_run(hfp_connection_t * connection){
    if (!connection) return;

    switch (connection->state){
        case HFP_EXCHANGE_SUPPORTED_FEATURES:
            
            break;
        default:
            break;
    }
}

hfp_connection_t * hfp_handle_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hfp_connection_t * context = provide_hfp_connection_context_for_rfcomm_cid(channel);
    if (!context) return NULL;
    while (size > 0 && (packet[0] == '\n' || packet[0] == '\r')){
        size--;
        packet++;
    }

    return context;
}

static void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    // printf("packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);
    hfp_connection_t * context = NULL;

    switch (packet_type){
        case RFCOMM_DATA_PACKET:
            context = hfp_handle_rfcomm_event(packet_type, channel, packet, size);
            break;
        case HCI_EVENT_PACKET:
            context = hfp_handle_hci_event(packet_type, packet, size);
            break;
        default:
            break;
        }
    hfp_run(context);
}

void hfp_ag_init(uint16_t rfcomm_channel_nr, uint32_t supported_features, uint8_t * codecs, int codecs_nr){
    if (codecs_nr > HFP_MAX_NUM_CODECS){
        log_error("hfp_init: codecs_nr (%d) > HFP_MAX_NUM_CODECS (%d)", codecs_nr, HFP_MAX_NUM_CODECS);
        return;
    }
    hfp_init(rfcomm_channel_nr);
    rfcomm_register_packet_handler(packet_handler);
    // connection->codecs = codecs;
    hfp_supported_features = supported_features;
    hfp_codecs_nr = codecs_nr;

    int i;
    for (i=0; i<codecs_nr; i++){
        hfp_codecs[i] = codecs[i];
    }
}

void hfp_ag_connect(bd_addr_t bd_addr){
    hfp_connect(bd_addr, SDP_Handsfree);
}

void hfp_ag_disconnect(bd_addr_t bd_addr){

}

