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

static uint16_t hfp_supported_features = HFP_DEFAULT_AG_SUPPORTED_FEATURES;

static uint8_t hfp_codecs_nr = 0;
static uint8_t hfp_codecs[HFP_MAX_NUM_CODECS];

static int  hfp_ag_indicators_nr = 0;
static hfp_ag_indicator_t hfp_ag_indicators[HFP_MAX_NUM_AG_INDICATORS];

static int  hfp_ag_call_hold_services_nr = 0;
static char *hfp_ag_call_hold_services[6];
static hfp_callback_t hfp_callback;

static void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void hfp_run_for_context(hfp_connection_t *context);

hfp_generic_status_indicator_t * get_hfp_generic_status_indicators();
int get_hfp_generic_status_indicators_nr();
void set_hfp_generic_status_indicators(hfp_generic_status_indicator_t * indicators, int indicator_nr);
void set_hfp_ag_indicators(hfp_ag_indicator_t * indicators, int indicator_nr);
int get_hfp_ag_indicators_nr(hfp_connection_t * context);
hfp_ag_indicator_t * get_hfp_ag_indicators(hfp_connection_t * context);


hfp_ag_indicator_t * get_hfp_ag_indicators(hfp_connection_t * context){
    // TODO: save only value, and value changed in the context?
    if (context->ag_indicators_nr != hfp_ag_indicators_nr){
        context->ag_indicators_nr = hfp_ag_indicators_nr;
        memcpy(context->ag_indicators, hfp_ag_indicators, hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));
    }
    return (hfp_ag_indicator_t *)&(context->ag_indicators);
}

static hfp_ag_indicator_t * get_ag_indicator_for_name(const char * name){
    int i;
    for (i = 0; i < hfp_ag_indicators_nr; i++){
        if (strcmp(hfp_ag_indicators[i].name, name) == 0){
            return &hfp_ag_indicators[i];
        }
    }
    return NULL;
}

void set_hfp_ag_indicators(hfp_ag_indicator_t * indicators, int indicator_nr){
    memcpy(hfp_ag_indicators, indicators, indicator_nr * sizeof(hfp_ag_indicator_t));
    hfp_ag_indicators_nr = indicator_nr;
}

int get_hfp_ag_indicators_nr(hfp_connection_t * context){
    if (context->ag_indicators_nr != hfp_ag_indicators_nr){
        context->ag_indicators_nr = hfp_ag_indicators_nr;
        memcpy(context->ag_indicators, hfp_ag_indicators, hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));
    }
    return context->ag_indicators_nr;
}


void hfp_ag_register_packet_handler(hfp_callback_t callback){
    if (callback == NULL){
        log_error("hfp_ag_register_packet_handler called with NULL callback");
        return;
    }
    hfp_callback = callback;
}

static int use_in_band_tone(){
    return get_bit(hfp_supported_features, HFP_AGSF_IN_BAND_RING_TONE);
}

static int has_codec_negotiation_feature(hfp_connection_t * connection){
    int hf = get_bit(connection->remote_supported_features, HFP_HFSF_CODEC_NEGOTIATION);
    int ag = get_bit(hfp_supported_features, HFP_AGSF_CODEC_NEGOTIATION);
    return hf && ag;
}

static int has_call_waiting_and_3way_calling_feature(hfp_connection_t * connection){
    int hf = get_bit(connection->remote_supported_features, HFP_HFSF_THREE_WAY_CALLING);
    int ag = get_bit(hfp_supported_features, HFP_AGSF_THREE_WAY_CALLING);
    return hf && ag;
}

static int has_hf_indicators_feature(hfp_connection_t * connection){
    int hf = get_bit(connection->remote_supported_features, HFP_HFSF_HF_INDICATORS);
    int ag = get_bit(hfp_supported_features, HFP_AGSF_HF_INDICATORS);
    return hf && ag;
}

void hfp_ag_create_sdp_record(uint8_t * service, int rfcomm_channel_nr, const char * name, uint8_t ability_to_reject_call, uint16_t supported_features){
    if (!name){
        name = default_hfp_ag_service_name;
    }
    hfp_create_sdp_record(service, SDP_HandsfreeAudioGateway, rfcomm_channel_nr, name);
    
    /*
     * 0x01 – Ability to reject a call
     * 0x00 – No ability to reject a call
     */
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0301);    // Hands-Free Profile - Network
    de_add_number(service, DE_UINT, DE_SIZE_8, ability_to_reject_call);

    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0311);    // Hands-Free Profile - SupportedFeatures
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_features);
}

static int hfp_ag_change_in_band_ring_tone_setting_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "\r\n%s:%d\r\n", HFP_CHANGE_IN_BAND_RING_TONE_SETTING, use_in_band_tone());
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_exchange_supported_features_cmd(uint16_t cid){
    char buffer[40];
    sprintf(buffer, "\r\n%s:%d\r\n\r\nOK\r\n", HFP_SUPPORTED_FEATURES, hfp_supported_features);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_ok(uint16_t cid){
    char buffer[10];
    sprintf(buffer, "\r\nOK\r\n");
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_ring(uint16_t cid){
    return send_str_over_rfcomm(cid, (char *) "\r\nRING\r\n");
}

static int hfp_ag_error(uint16_t cid){
    char buffer[10];
    sprintf(buffer, "\r\nERROR\r\n");
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_report_extended_audio_gateway_error(uint16_t cid, uint8_t error){
    char buffer[20];
    sprintf(buffer, "\r\n%s=%d\r\n", HFP_EXTENDED_AUDIO_GATEWAY_ERROR, error);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_indicators_join(char * buffer, int buffer_size, hfp_connection_t * context){
    if (buffer_size < get_hfp_ag_indicators_nr(context) * (1 + sizeof(hfp_ag_indicator_t))) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < get_hfp_ag_indicators_nr(context)-1; i++) {
        offset += snprintf(buffer+offset, buffer_size-offset, "(\"%s\",(%d,%d)),", 
            get_hfp_ag_indicators(context)[i].name, 
            get_hfp_ag_indicators(context)[i].min_range, 
            get_hfp_ag_indicators(context)[i].max_range);
    }
    if ( i < get_hfp_ag_indicators_nr(context)){
        offset += snprintf(buffer+offset, buffer_size-offset, "(\"%s\",(%d,%d))", 
            get_hfp_ag_indicators(context)[i].name, 
            get_hfp_ag_indicators(context)[i].min_range, 
            get_hfp_ag_indicators(context)[i].max_range);
    }
    return offset;
}

static int hfp_hf_indicators_join(char * buffer, int buffer_size){
    if (buffer_size < hfp_ag_indicators_nr * 3) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < get_hfp_generic_status_indicators_nr()-1; i++) {
        offset += snprintf(buffer+offset, buffer_size-offset, "%d,", get_hfp_generic_status_indicators()[i].uuid);
    }
    if (i < get_hfp_generic_status_indicators_nr()){
        offset += snprintf(buffer+offset, buffer_size-offset, "%d,", get_hfp_generic_status_indicators()[i].uuid);
    }
    return offset;
}

static int hfp_hf_indicators_initial_status_join(char * buffer, int buffer_size){
    if (buffer_size < get_hfp_generic_status_indicators_nr() * 3) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < get_hfp_generic_status_indicators_nr(); i++) {
        offset += snprintf(buffer+offset, buffer_size-offset, "\r\n%s:%d,%d\r\n", HFP_GENERIC_STATUS_INDICATOR, get_hfp_generic_status_indicators()[i].uuid, get_hfp_generic_status_indicators()[i].state);
    }
    return offset;
}

static int hfp_ag_indicators_status_join(char * buffer, int buffer_size){
    if (buffer_size < hfp_ag_indicators_nr * 3) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < hfp_ag_indicators_nr-1; i++) {
        offset += snprintf(buffer+offset, buffer_size-offset, "%d,", hfp_ag_indicators[i].status); 
    }
    if (i<hfp_ag_indicators_nr){
        offset += snprintf(buffer+offset, buffer_size-offset, "%d", hfp_ag_indicators[i].status);
    }
    return offset;
}

static int hfp_ag_call_services_join(char * buffer, int buffer_size){
    if (buffer_size < hfp_ag_call_hold_services_nr * 3) return 0;
    int i;
    int offset = snprintf(buffer, buffer_size, "("); 
    for (i = 0; i < hfp_ag_call_hold_services_nr-1; i++) {
        offset += snprintf(buffer+offset, buffer_size-offset, "%s,", hfp_ag_call_hold_services[i]); 
    }
    if (i<hfp_ag_call_hold_services_nr){
        offset += snprintf(buffer+offset, buffer_size-offset, "%s)", hfp_ag_call_hold_services[i]);
    }
    return offset;
}

static int hfp_ag_retrieve_indicators_cmd(uint16_t cid, hfp_connection_t * context){
    char buffer[250];
    int offset = snprintf(buffer, sizeof(buffer), "\r\n%s:", HFP_INDICATOR);
    offset += hfp_ag_indicators_join(buffer+offset, sizeof(buffer)-offset, context);
    
    buffer[offset] = 0;
    
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, "\r\n\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_retrieve_indicators_status_cmd(uint16_t cid){
    char buffer[40];
    int offset = snprintf(buffer, sizeof(buffer), "\r\n%s:", HFP_INDICATOR);
    offset += hfp_ag_indicators_status_join(buffer+offset, sizeof(buffer)-offset);
    
    buffer[offset] = 0;
    
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, "\r\n\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_set_indicator_status_update_cmd(uint16_t cid, uint8_t activate){
    // AT\r\n%s:3,0,0,%d\r\n
    return hfp_ag_ok(cid);
}


static int hfp_ag_retrieve_can_hold_call_cmd(uint16_t cid){
    char buffer[100];
    int offset = snprintf(buffer, sizeof(buffer), "\r\n%s:", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES);
    offset += hfp_ag_call_services_join(buffer+offset, sizeof(buffer)-offset);
    
    buffer[offset] = 0;
    
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, "\r\n\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}


static int hfp_ag_list_supported_generic_status_indicators_cmd(uint16_t cid){
    return hfp_ag_ok(cid);
}

static int hfp_ag_retrieve_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[40];
    int offset = snprintf(buffer, sizeof(buffer), "\r\n%s:(", HFP_GENERIC_STATUS_INDICATOR);
    offset += hfp_hf_indicators_join(buffer+offset, sizeof(buffer)-offset);
    
    buffer[offset] = 0;
    
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, ")\r\n\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_retrieve_initital_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[40];
    int offset = hfp_hf_indicators_initial_status_join(buffer, sizeof(buffer));
    
    buffer[offset] = 0;
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, "\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_transfer_ag_indicators_status_cmd(uint16_t cid, hfp_ag_indicator_t * indicator){
    char buffer[20];
    sprintf(buffer, "\r\n%s:%d,%d\r\n", HFP_TRANSFER_AG_INDICATOR_STATUS, indicator->index, indicator->status);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_report_network_operator_name_cmd(uint16_t cid, hfp_network_opearator_t op){
    char buffer[40];
    if (strlen(op.name) == 0){
        sprintf(buffer, "\r\n%s:%d,,\r\n\r\nOK\r\n", HFP_QUERY_OPERATOR_SELECTION, op.mode);
    } else {
        sprintf(buffer, "\r\n%s:%d,%d,%s\r\n\r\nOK\r\n", HFP_QUERY_OPERATOR_SELECTION, op.mode, op.format, op.name);
    }
    return send_str_over_rfcomm(cid, buffer);
}


static int hfp_ag_cmd_suggest_codec(uint16_t cid, uint8_t codec){
    char buffer[30];
    sprintf(buffer, "\r\n%s:%d\r\n", HFP_CONFIRM_COMMON_CODEC, codec);
    return send_str_over_rfcomm(cid, buffer);
}

static uint8_t hfp_ag_suggest_codec(hfp_connection_t *context){
    int i,j;
    uint8_t codec = HFP_CODEC_CVSD;
    for (i = 0; i < hfp_codecs_nr; i++){
        for (j = 0; j < context->remote_codecs_nr; j++){
            if (context->remote_codecs[j] == hfp_codecs[i]){
                codec = context->remote_codecs[j];
                continue;
            }
        }
    }
    return codec;
}

static int codecs_exchange_state_machine(hfp_connection_t * context){
    /* events ( == commands):
        HFP_CMD_AVAILABLE_CODECS == received AT+BAC with list of codecs
        HFP_CMD_TRIGGER_CODEC_CONNECTION_SETUP:
            hf_trigger_codec_connection_setup == received BCC
            ag_trigger_codec_connection_setup == received from AG to send BCS
        HFP_CMD_HF_CONFIRMED_CODEC == received AT+BCS
    */
            
     switch (context->codecs_state){
        case HFP_CODECS_RECEIVED_TRIGGER_CODEC_EXCHANGE:
            context->command = HFP_CMD_AG_SEND_COMMON_CODEC;
            break;
        case HFP_CODECS_AG_RESEND_COMMON_CODEC:
            context->command = HFP_CMD_AG_SEND_COMMON_CODEC;
            break;
        default:
            break;
    }

    // printf(" -> State machine: CC\n");
    
    switch (context->command){
        case HFP_CMD_AVAILABLE_CODECS:
            //printf("HFP_CODECS_RECEIVED_LIST \n");
            if (context->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED){
                context->codecs_state = HFP_CODECS_RECEIVED_LIST;
                hfp_ag_ok(context->rfcomm_cid);
                return 1;    
            }

            switch (context->codecs_state){
                case HFP_CODECS_AG_SENT_COMMON_CODEC:
                case HFP_CODECS_EXCHANGED:
                    context->codecs_state = HFP_CODECS_AG_RESEND_COMMON_CODEC;
                    break;
                default:
                    break;
            }
            hfp_ag_ok(context->rfcomm_cid);
            return 1;
        
        case HFP_CMD_TRIGGER_CODEC_CONNECTION_SETUP:
            //printf(" HFP_CMD_TRIGGER_CODEC_CONNECTION_SETUP \n");
            context->codecs_state = HFP_CODECS_RECEIVED_TRIGGER_CODEC_EXCHANGE;
            hfp_ag_ok(context->rfcomm_cid);
            return 1;
        
        case HFP_CMD_AG_SEND_COMMON_CODEC:
            //printf(" HFP_CMD_AG_SEND_COMMON_CODEC \n");
            context->codecs_state = HFP_CODECS_AG_SENT_COMMON_CODEC;
            context->suggested_codec = hfp_ag_suggest_codec(context);
            hfp_ag_cmd_suggest_codec(context->rfcomm_cid, context->suggested_codec);
            return 1;

        case HFP_CMD_HF_CONFIRMED_CODEC:
            //printf("HFP_CMD_HF_CONFIRMED_CODEC \n");
            if (context->codec_confirmed != context->suggested_codec){
                context->codecs_state = HFP_CODECS_ERROR;
                hfp_ag_error(context->rfcomm_cid);
                return 1;
            } 
            context->negotiated_codec = context->codec_confirmed;
            context->codecs_state = HFP_CODECS_EXCHANGED;
            hfp_emit_event(hfp_callback, HFP_SUBEVENT_CODECS_CONNECTION_COMPLETE, 0);
            hfp_ag_ok(context->rfcomm_cid);           
            return 1; 
        default:
            break;
    }
    return 0;
}


static int hfp_ag_run_for_context_service_level_connection(hfp_connection_t * context){
    if (context->state >= HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) return 0;
    int done = 0;
    // printf(" -> State machine: SLC\n");
    switch(context->command){
        case HFP_CMD_SUPPORTED_FEATURES:
            switch(context->state){
                case HFP_W4_EXCHANGE_SUPPORTED_FEATURES:
                case HFP_EXCHANGE_SUPPORTED_FEATURES:
                    if (has_codec_negotiation_feature(context)){
                        context->state = HFP_W4_NOTIFY_ON_CODECS;
                    } else {
                        context->state = HFP_W4_RETRIEVE_INDICATORS;
                    }
                    hfp_ag_exchange_supported_features_cmd(context->rfcomm_cid);
                    return 1;
                default:
                    break;
            }
            break;
        case HFP_CMD_AVAILABLE_CODECS:
            done = codecs_exchange_state_machine(context);

            if (context->codecs_state == HFP_CODECS_RECEIVED_LIST){
                context->state = HFP_W4_RETRIEVE_INDICATORS;
            }
            return done;

        case HFP_CMD_RETRIEVE_AG_INDICATORS:
            if (context->state != HFP_W4_RETRIEVE_INDICATORS) break;
            context->state = HFP_W4_RETRIEVE_INDICATORS_STATUS;
            hfp_ag_retrieve_indicators_cmd(context->rfcomm_cid, context);
            return 1;
        
        case HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS:
            if (context->state != HFP_W4_RETRIEVE_INDICATORS_STATUS) break;
            context->state = HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE;
            hfp_ag_retrieve_indicators_status_cmd(context->rfcomm_cid);
            return 1;

        case HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE:
            if (context->state != HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE) break;
            if (has_call_waiting_and_3way_calling_feature(context)){
                context->state = HFP_W4_RETRIEVE_CAN_HOLD_CALL;
            } else if (has_hf_indicators_feature(context)){
                context->state = HFP_W4_LIST_GENERIC_STATUS_INDICATORS;
            } else {
                context->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
                hfp_emit_event(hfp_callback, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED, 0);
            }
            hfp_ag_set_indicator_status_update_cmd(context->rfcomm_cid, 1);
            return 1;
                
        case HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES:
            if (context->state != HFP_W4_RETRIEVE_CAN_HOLD_CALL) break;
            if (has_hf_indicators_feature(context)){
                context->state = HFP_W4_LIST_GENERIC_STATUS_INDICATORS;
            } else {
                context->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
                hfp_emit_event(hfp_callback, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED, 0);
            }
            hfp_ag_retrieve_can_hold_call_cmd(context->rfcomm_cid);
            return 1;
        
        case HFP_CMD_LIST_GENERIC_STATUS_INDICATORS:
            if (context->state != HFP_W4_LIST_GENERIC_STATUS_INDICATORS) break;
            context->state = HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS;
            hfp_ag_list_supported_generic_status_indicators_cmd(context->rfcomm_cid);
            return 1;

        case HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS:
            if (context->state != HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS) break;
            context->state = HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS; 
            hfp_ag_retrieve_supported_generic_status_indicators_cmd(context->rfcomm_cid);
            return 1;

        case HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS_STATE:
            if (context->state != HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS) break;
            context->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
            hfp_emit_event(hfp_callback, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED, 0);
            hfp_ag_retrieve_initital_supported_generic_status_indicators_cmd(context->rfcomm_cid);
            return 1;
        default:
            break;
    }
    return done;
}

static int hfp_ag_run_for_context_service_level_connection_queries(hfp_connection_t * context){
    // if (context->state != HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) return 0;
    
    int done = codecs_exchange_state_machine(context);
    if (done) return done;
   
    // printf(" -> State machine: SLC Queries\n");
    switch(context->command){
        case HFP_CMD_CHANGE_IN_BAND_RING_TONE_SETTING:
            hfp_ag_change_in_band_ring_tone_setting_cmd(context->rfcomm_cid);
            return 1;
        case HFP_CMD_QUERY_OPERATOR_SELECTION_NAME:
            hfp_ag_report_network_operator_name_cmd(context->rfcomm_cid, context->network_operator);
            return 1;
        case HFP_CMD_QUERY_OPERATOR_SELECTION_NAME_FORMAT:
            if (context->network_operator.format != 0){
                hfp_ag_error(context->rfcomm_cid);
            } else {
                hfp_ag_ok(context->rfcomm_cid);
            }
            return 1;
        case HFP_CMD_ENABLE_INDIVIDUAL_AG_INDICATOR_STATUS_UPDATE:
            hfp_ag_ok(context->rfcomm_cid);
            return 1;
        case HFP_CMD_ENABLE_EXTENDED_AUDIO_GATEWAY_ERROR:
            if (context->extended_audio_gateway_error){
                context->extended_audio_gateway_error = 0;
                hfp_ag_report_extended_audio_gateway_error(context->rfcomm_cid, context->extended_audio_gateway_error);
                return 1;
            }
        case HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE:
            printf("TODO\n");
            break;
        default:
            break;
    }
    return 0;
}


static int hfp_ag_run_for_audio_connection(hfp_connection_t * context){
    if (context->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED ||
        context->state > HFP_W2_DISCONNECT_SCO) return 0;


    if (context->state == HFP_AUDIO_CONNECTION_ESTABLISHED && context->release_audio_connection){
        context->state = HFP_W4_SCO_DISCONNECTED;
        context->release_audio_connection = 0;
        gap_disconnect(context->sco_handle);
        return 1;
    }

    if (context->state == HFP_AUDIO_CONNECTION_ESTABLISHED) return 0;
    
    // run codecs exchange
    int done = codecs_exchange_state_machine(context);
    if (done) return done;
    // printf(" -> State machine: Audio Connection\n");

    if (context->codecs_state != HFP_CODECS_EXCHANGED) return done;
    if (context->establish_audio_connection){
        context->state = HFP_W4_SCO_CONNECTED;
        context->establish_audio_connection = 0;
        hci_send_cmd(&hci_setup_synchronous_connection, context->con_handle, 8000, 8000, 0xFFFF, hci_get_sco_voice_setting(), 0xFF, 0x003F);
        return 1;
    
    }
    return 0;
}

static hfp_connection_t * hfp_ag_context_for_timer(timer_source_t * ts){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());

    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        if ( &connection->hfp_timeout == ts) {
            return connection;
        }
    }
    return NULL;
}

static void hfp_timeout_handler(timer_source_t * timer){
    hfp_connection_t * context = hfp_ag_context_for_timer(timer);
    if (!context) return;

    log_info("HFP start ring timeout, con handle 0x%02x", context->con_handle);
    context->ag_ring = 1;

    run_loop_set_timer(&context->hfp_timeout, 2000); // 5 seconds timeout
    run_loop_add_timer(&context->hfp_timeout);

    hfp_run_for_context(context);
}

static void hfp_timeout_start(hfp_connection_t * context){
    run_loop_remove_timer(&context->hfp_timeout);
    run_loop_set_timer_handler(&context->hfp_timeout, hfp_timeout_handler);
    run_loop_set_timer(&context->hfp_timeout, 2000); // 5 seconds timeout
    run_loop_add_timer(&context->hfp_timeout);
}

static void hfp_timeout_stop(hfp_connection_t * context){
    log_info("HFP stop ring timeout, con handle 0x%02x", context->con_handle);
    run_loop_remove_timer(&context->hfp_timeout);
} 

static int incoming_call_state_machine(hfp_connection_t * context){
    if (!context->run_call_state_machine) return 0;
    if (context->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) return 0;

    // printf(" -> State machine: Incoming Call\n");
    hfp_ag_indicator_t * indicator;
    if (context->command == HFP_CMD_HANG_UP_CALL){
        context->terminate_call = 1;
        hfp_ag_ok(context->rfcomm_cid);
        return 1;
    }

    if (context->terminate_call){
        printf(" -> State machine: Terminate Incoming Call\n");
        indicator = get_ag_indicator_for_name("call");
        if (!indicator) return 0;

        indicator->status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
        context->terminate_call = 0;
        context->run_call_state_machine = 0;
        context->call_state = HFP_CALL_IDLE;
        hfp_emit_event(hfp_callback, HFP_SUBEVENT_CALL_TERMINATED, 0);
        hfp_ag_transfer_ag_indicators_status_cmd(context->rfcomm_cid, indicator);
        return 1;
    }

    int done = 0;
    switch (context->call_state){
        case HFP_CALL_IDLE:
            //printf(" HFP_CALL_TRIGGER_AUDIO_CONNECTION \n");
            indicator = get_ag_indicator_for_name("callsetup");
            if (!indicator) return 0;

            indicator->status = HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS;
            hfp_ag_transfer_ag_indicators_status_cmd(context->rfcomm_cid, indicator);
            
            hfp_timeout_start(context);
            context->ag_ring = 1;

            if (use_in_band_tone()){
                context->call_state = HFP_CALL_TRIGGER_AUDIO_CONNECTION;
            } else {
                context->call_state = HFP_CALL_W4_ANSWER;
                hfp_emit_event(hfp_callback, HFP_SUBEVENT_START_RINGINIG, 0);
            }
            return 1;
        
        case HFP_CALL_W4_ANSWER:
            if (context->command != HFP_CMD_CALL_ANSWERED) {
                if (context->ag_ring){
                    context->ag_ring = 0;
                    hfp_ag_ring(context->rfcomm_cid);
                    return 1;
                }
                return 0;
            }
            context->ag_ring = 0;
            hfp_timeout_stop(context);
            //printf(" HFP_CALL_W4_ANSWER, cmd %d \n", context->command);
            context->call_state = HFP_CALL_TRANSFER_CALL_STATUS;
            hfp_emit_event(hfp_callback, HFP_SUBEVENT_STOP_RINGINIG, 0);
            hfp_ag_ok(context->rfcomm_cid);
            return 1;

        case HFP_CALL_TRANSFER_CALL_STATUS:
            //printf(" HFP_CALL_TRANSFER_CALL_STATUS \n");
            context->call_state = HFP_CALL_TRANSFER_CALLSETUP_STATUS;
            indicator = get_ag_indicator_for_name("call");
            indicator->status = HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT;
            hfp_ag_transfer_ag_indicators_status_cmd(context->rfcomm_cid, indicator);
            return 1;

        case HFP_CALL_TRANSFER_CALLSETUP_STATUS:
            //printf(" HFP_CALL_TRANSFER_CALLSETUP_STATUS \n");
            if (use_in_band_tone()){
                context->call_state = HFP_CALL_ACTIVE;
            } else {
                context->call_state = HFP_CALL_TRIGGER_AUDIO_CONNECTION;
            }
            
            indicator = get_ag_indicator_for_name("callsetup");
            indicator->status = HFP_HELDCALL_STATUS_NO_CALLS_HELD;
            hfp_ag_transfer_ag_indicators_status_cmd(context->rfcomm_cid, indicator);
            return 1;
        case HFP_CALL_TRIGGER_AUDIO_CONNECTION:
            //printf(" HFP_CALL_TRIGGER_AUDIO_CONNECTION \n");
            context->call_state = HFP_CALL_W4_AUDIO_CONNECTION;
            hfp_ag_establish_audio_connection(context->remote_addr);
            break;

        case HFP_CALL_W4_AUDIO_CONNECTION:
            //printf(" HFP_CALL_W4_AUDIO_CONNECTION \n");
            if (context->state < HFP_AUDIO_CONNECTION_ESTABLISHED) return 0;
            
            if (use_in_band_tone()){
                context->call_state = HFP_CALL_W4_ANSWER;
                hfp_emit_event(hfp_callback, HFP_SUBEVENT_START_RINGINIG, 0);
            } else {
                context->call_state = HFP_CALL_ACTIVE;
            }
            return 0;
        case HFP_CALL_ACTIVE:
            // printf(" HFP_CALL_ACTIVE \n");
            break;
        default:
            break;
    }
    return done;
}


static void hfp_run_for_context(hfp_connection_t *context){
    if (!context) return;
    if (!rfcomm_can_send_packet_now(context->rfcomm_cid)) return;
    
    if (context->command == HFP_CMD_UNKNOWN){
        context->ok_pending = 0;
        context->send_error = 0;
        context->command = HFP_CMD_NONE;
        hfp_ag_error(context->rfcomm_cid);
        return;
    }

    if (context->ok_pending){
        context->ok_pending = 0;
        context->command = HFP_CMD_NONE;
        hfp_ag_ok(context->rfcomm_cid);
        return;
    }

    if (context->send_error){
        context->send_error = 0;
        context->command = HFP_CMD_NONE;
        hfp_ag_error(context->rfcomm_cid); 
        return;
    }
    
    int done = hfp_ag_run_for_context_service_level_connection(context);
    if (!done){
        done = hfp_ag_run_for_context_service_level_connection_queries(context);
    } 

    if (!done){
        done = incoming_call_state_machine(context);
    }

    if (!done){  
        done = hfp_ag_run_for_audio_connection(context);
    }
    

    if (context->command == HFP_CMD_NONE && !done){
        log_info("context->command == HFP_CMD_NONE");
        switch(context->state){
            case HFP_W2_DISCONNECT_RFCOMM:
                context->state = HFP_W4_RFCOMM_DISCONNECTED;
                rfcomm_disconnect_internal(context->rfcomm_cid);
                break;
            default:
                break;
        }
    }
    if (done){
        context->command = HFP_CMD_NONE;
    }
}

static void hfp_handle_rfcomm_data(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hfp_connection_t * context = get_hfp_connection_context_for_rfcomm_cid(channel);
    if (!context) return;
    int pos;
    for (pos = 0; pos < size ; pos++){
        hfp_parse(context, packet[pos], 0);
    }
}

static void hfp_run(void){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        hfp_run_for_context(connection);
    }
}

static void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    switch (packet_type){
        case RFCOMM_DATA_PACKET:
            hfp_handle_rfcomm_data(packet_type, channel, packet, size);
            break;
        case HCI_EVENT_PACKET:
            hfp_handle_hci_event(hfp_callback, packet_type, packet, size);
            break;
        default:
            break;
    }

    hfp_run();
}

void hfp_ag_init(uint16_t rfcomm_channel_nr, uint32_t supported_features, 
    uint8_t * codecs, int codecs_nr, 
    hfp_ag_indicator_t * ag_indicators, int ag_indicators_nr,
    hfp_generic_status_indicator_t * hf_indicators, int hf_indicators_nr,
    const char *call_hold_services[], int call_hold_services_nr){
    if (codecs_nr > HFP_MAX_NUM_CODECS){
        log_error("hfp_init: codecs_nr (%d) > HFP_MAX_NUM_CODECS (%d)", codecs_nr, HFP_MAX_NUM_CODECS);
        return;
    }
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);

    rfcomm_register_packet_handler(packet_handler);

    hfp_init(rfcomm_channel_nr);
        
    hfp_supported_features = supported_features;
    hfp_codecs_nr = codecs_nr;

    int i;
    for (i=0; i<codecs_nr; i++){
        hfp_codecs[i] = codecs[i];
    }

    hfp_ag_indicators_nr = ag_indicators_nr;
    memcpy(hfp_ag_indicators, ag_indicators, ag_indicators_nr * sizeof(hfp_ag_indicator_t));

    set_hfp_generic_status_indicators(hf_indicators, hf_indicators_nr);

    hfp_ag_call_hold_services_nr = call_hold_services_nr;
    memcpy(hfp_ag_call_hold_services, call_hold_services, call_hold_services_nr * sizeof(char *));
}

void hfp_ag_establish_service_level_connection(bd_addr_t bd_addr){
    hfp_establish_service_level_connection(bd_addr, SDP_Handsfree);
}

void hfp_ag_release_service_level_connection(bd_addr_t bd_addr){
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);
    hfp_release_service_level_connection(connection);
    hfp_run_for_context(connection);
}

void hfp_ag_report_extended_audio_gateway_error_result_code(bd_addr_t bd_addr, hfp_cme_error_t error){
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);
    if (!connection){
        log_error("HFP HF: connection doesn't exist.");
        return;
    }
    connection->extended_audio_gateway_error = 0;
    if (!connection->enable_extended_audio_gateway_error_report){
        return;
    }
    connection->extended_audio_gateway_error = error;
    hfp_run_for_context(connection);
}


void hfp_ag_establish_audio_connection(bd_addr_t bd_addr){
    hfp_ag_establish_service_level_connection(bd_addr);
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);

    connection->establish_audio_connection = 0;
    if (connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED) return;
    if (connection->state >= HFP_W2_DISCONNECT_SCO) return;
        
    connection->establish_audio_connection = 1;

    if (!has_codec_negotiation_feature(connection)){
        log_info("hfp_ag_establish_audio_connection - no codec negotiation feature, using defaults");
        connection->codecs_state = HFP_CODECS_EXCHANGED;
    } 

    switch (connection->codecs_state){
        case HFP_CODECS_IDLE:
        case HFP_CODECS_RECEIVED_LIST:
        case HFP_CODECS_AG_RESEND_COMMON_CODEC:
        case HFP_CODECS_ERROR:
            connection->command = HFP_CMD_AG_SEND_COMMON_CODEC;
            break;
        default:
            break;
    } 

    hfp_run_for_context(connection);
}

void hfp_ag_release_audio_connection(bd_addr_t bd_addr){
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);
    hfp_release_audio_connection(connection);
    hfp_run_for_context(connection);
}

/**
 * @brief Enable in-band ring tone
 */
void hfp_ag_set_use_in_band_ring_tone(int use_in_band_ring_tone){
    if (get_bit(hfp_supported_features, HFP_AGSF_IN_BAND_RING_TONE) == use_in_band_ring_tone){
        return;
    } 
    hfp_supported_features = store_bit(hfp_supported_features, HFP_AGSF_IN_BAND_RING_TONE, use_in_band_ring_tone);
        
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        connection->command = HFP_CMD_CHANGE_IN_BAND_RING_TONE_SETTING;
        hfp_run_for_context(connection);
    }
}

/**
 * @brief 
 */
void hfp_ag_incoming_call(void){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        hfp_ag_establish_service_level_connection(connection->remote_addr);
        connection->run_call_state_machine = 1;
        hfp_run_for_context(connection);
    }
}

/**
 * @brief 
 */
void hfp_ag_terminate_call(void){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        hfp_ag_establish_service_level_connection(connection->remote_addr);
        connection->terminate_call = 1;
        hfp_run_for_context(connection);
    }
}

void hfp_ag_audio_connection_transfer_towards_ag(bd_addr_t bd_addr){
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);
    if (connection->call_state != HFP_CALL_ACTIVE) return;
    if (connection->state != HFP_AUDIO_CONNECTION_ESTABLISHED) return;
    hfp_ag_release_audio_connection(bd_addr);
}

/**
 * @brief 
 */
void hfp_ag_audio_connection_transfer_towards_hf(bd_addr_t bd_addr){
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);
    if (connection->call_state != HFP_CALL_ACTIVE) return;
    if (connection->state != HFP_AUDIO_CONNECTION_ESTABLISHED) return;
    hfp_ag_establish_audio_connection(bd_addr);
}


