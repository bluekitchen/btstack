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
#include "hfp_gsm_model.h"
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

static hfp_response_and_hold_state_t hfp_ag_response_and_hold_state;
static int hfp_ag_response_and_hold_active = 0;

// Subcriber information entries
static hfp_phone_number_t * subscriber_numbers = NULL;
static int subscriber_numbers_count = 0;

static void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void hfp_run_for_context(hfp_connection_t *context);
static void hfp_ag_setup_audio_connection(hfp_connection_t * connection);
static void hfp_ag_hf_start_ringing(hfp_connection_t * context);

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

static int get_ag_indicator_index_for_name(const char * name){
    int i;
    for (i = 0; i < hfp_ag_indicators_nr; i++){
        if (strcmp(hfp_ag_indicators[i].name, name) == 0){
            return i;
        }
    }
    return -1;
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

static int hfp_ag_send_clip(uint16_t cid){
    char buffer[50];
    sprintf(buffer, "\r\n%s: \"%s\",%u\r\n", HFP_ENABLE_CLIP, hfp_gsm_clip_number(), hfp_gsm_clip_type());
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_send_subscriber_number_cmd(uint16_t cid, uint8_t type, const char * number){
    char buffer[50];
    sprintf(buffer, "\r\n%s: ,\"%s\",%u, , \r\n", HFP_SUBSCRIBER_NUMBER_INFORMATION, number, type);
    return send_str_over_rfcomm(cid, buffer);
}
        
static int hfp_ag_send_phone_number_for_voice_tag_cmd(uint16_t cid){
    char buffer[50];
    sprintf(buffer, "\r\n%s: %s\r\n", HFP_PHONE_NUMBER_FOR_VOICE_TAG, hfp_gsm_clip_number());
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_call_waiting_notification(uint16_t cid){
    char buffer[50];
    sprintf(buffer, "\r\n+CCWA: \"%s\",%u\r\n", hfp_gsm_clip_number(), hfp_gsm_clip_type());
    return send_str_over_rfcomm(cid, buffer);
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

// fast & small implementation for fixed int size
static int string_len_for_uint32(uint32_t i){
    if (i <         10) return 1;
    if (i <        100) return 2;
    if (i <       1000) return 3;
    if (i <      10000) return 4;
    if (i <     100000) return 5;
    if (i <    1000000) return 6;      
    if (i <   10000000) return 7;
    if (i <  100000000) return 8;
    if (i < 1000000000) return 9;
    return 10;
}

// get size for indicator string
static int hfp_ag_indicators_string_size(hfp_connection_t * context, int i){
    // template: ("$NAME",($MIN,$MAX))
    return 8 + strlen(get_hfp_ag_indicators(context)[i].name)
         + string_len_for_uint32(get_hfp_ag_indicators(context)[i].min_range)
         + string_len_for_uint32(get_hfp_ag_indicators(context)[i].min_range); 
}

// store indicator
static void hfp_ag_indicators_string_store(hfp_connection_t * context, int i, uint8_t * buffer){
    sprintf((char *) buffer, "(\"%s\",(%d,%d)),", 
            get_hfp_ag_indicators(context)[i].name, 
            get_hfp_ag_indicators(context)[i].min_range, 
            get_hfp_ag_indicators(context)[i].max_range);
}

// structure: header [indicator [comma indicator]] footer
static int hfp_ag_indicators_cmd_generator_num_segments(hfp_connection_t * context){
    int num_indicators = get_hfp_ag_indicators_nr(context);
    if (!num_indicators) return 2;
    return 3 + (num_indicators-1) * 2;
}

// get size of individual segment for hfp_ag_retrieve_indicators_cmd
static int hfp_ag_indicators_cmd_generator_get_segment_len(hfp_connection_t * context, int index){
    if (index == 0) {
        return strlen(HFP_INDICATOR) + 3;   // "\n\r%s:""
    }
    index--;
    int num_indicators = get_hfp_ag_indicators_nr(context);
    int indicator_index = index >> 1;
    if ((index & 1) == 0){
        return hfp_ag_indicators_string_size(context, indicator_index);
    }
    if (indicator_index == num_indicators - 1){
        return 8; // "\r\n\r\nOK\r\n"
    }
    return 1; // comma
}

static void hgp_ag_indicators_cmd_generator_store_segment(hfp_connection_t * context, int index, uint8_t * buffer){
    if (index == 0){
        *buffer++ = '\r';
        *buffer++ = '\n';
        int len = strlen(HFP_INDICATOR);
        memcpy(buffer, HFP_INDICATOR, len);
        buffer += len;
        *buffer++ = ':';
        return;
    }
    index--;
    int num_indicators = get_hfp_ag_indicators_nr(context);
    int indicator_index = index >> 1;
    if ((index & 1) == 0){
        hfp_ag_indicators_string_store(context, indicator_index, buffer);
        return;
    }
    if (indicator_index == num_indicators-1){
        memcpy(buffer, "\r\n\r\nOK\r\n", 8);
        return;
    }
    *buffer = ',';
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

static int hfp_ag_cmd_via_generator(uint16_t cid, hfp_connection_t * context,
    int start_segment, int num_segments,
    int (*get_segment_len)(hfp_connection_t * context, int segment),
    void (*store_segment) (hfp_connection_t * context, int segment, uint8_t * buffer)){

    // assumes: can send now == true
    // assumes: num segments > 0
    rfcomm_reserve_packet_buffer();
    int mtu = rfcomm_get_max_frame_size(cid);
    uint8_t * data = rfcomm_get_outgoing_buffer();
    int offset = 0;
    int segment = start_segment;
    while (segment < num_segments){
        int segment_len = get_segment_len(context, segment);
        if (offset + segment_len <= mtu){
            store_segment(context, segment, data+offset);
            offset += segment_len;
            segment++;
        }
    }
    rfcomm_send_prepared(cid, offset);
    return segment;
}

// returns next segment to store
static int hfp_ag_retrieve_indicators_cmd_via_generator(uint16_t cid, hfp_connection_t * context, int start_segment){
    int num_segments = hfp_ag_indicators_cmd_generator_num_segments(context);
    return hfp_ag_cmd_via_generator(cid, context, start_segment, num_segments,
        hfp_ag_indicators_cmd_generator_get_segment_len, hgp_ag_indicators_cmd_generator_store_segment);
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
    char buffer[40];
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

static int hfp_ag_activate_voice_recognition_cmd(uint16_t cid, uint8_t activate_voice_recognition){
    char buffer[30];
    sprintf(buffer, "\r\n%s: %d\r\n", HFP_ACTIVATE_VOICE_RECOGNITION, activate_voice_recognition);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_set_speaker_gain_cmd(uint16_t cid, uint8_t gain){
    char buffer[30];
    sprintf(buffer, "\r\n%s:%d\r\n", HFP_SET_SPEAKER_GAIN, gain);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_set_microphone_gain_cmd(uint16_t cid, uint8_t gain){
    char buffer[30];
    sprintf(buffer, "\r\n%s:%d\r\n", HFP_SET_MICROPHONE_GAIN, gain);
    return send_str_over_rfcomm(cid, buffer);
}
            
static int hfp_ag_set_response_and_hold(uint16_t cid, int state){
    char buffer[30];
    sprintf(buffer, "\r\n%s: %d\r\n", HFP_RESPONSE_AND_HOLD, state);
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

static void hfp_init_link_settings(hfp_connection_t * context){
    // determine highest possible link setting
    context->link_setting = HFP_LINK_SETTINGS_D1;
    if (hci_remote_eSCO_supported(context->con_handle)){
        context->link_setting = HFP_LINK_SETTINGS_S3;
        if ((context->remote_supported_features & (1<<HFP_HFSF_ESCO_S4))
        &&  (hfp_supported_features             & (1<<HFP_AGSF_ESCO_S4))){
            context->link_setting = HFP_LINK_SETTINGS_S4;
        }
    }
}

static void hfp_ag_slc_established(hfp_connection_t * context){
    context->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
    hfp_emit_event(hfp_callback, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED, 0);

    hfp_init_link_settings(context);

    // if active call exist, set per-connection state active, too (when audio is on)
    if (hfp_gsm_call_status() == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT){
        context->call_state = HFP_CALL_W4_AUDIO_CONNECTION_FOR_ACTIVE;
    }
    // if AG is ringing, also start ringing on the HF
    if (hfp_gsm_call_status() == HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS &&
        hfp_gsm_callsetup_status() == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS){
        hfp_ag_hf_start_ringing(context);
    }
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
            if (context->state == HFP_W4_RETRIEVE_INDICATORS) {
                context->command = HFP_CMD_NONE;  // prevent reentrance
                int next_segment = hfp_ag_retrieve_indicators_cmd_via_generator(context->rfcomm_cid, context, context->send_ag_indicators_segment);
                if (next_segment < hfp_ag_indicators_cmd_generator_num_segments(context)){
                    // prepare sending of next segment
                    context->send_ag_indicators_segment = next_segment;
                    context->command = HFP_CMD_RETRIEVE_AG_INDICATORS;
                } else {
                    // done, go to next state
                    context->send_ag_indicators_segment = 0;
                    context->state = HFP_W4_RETRIEVE_INDICATORS_STATUS;
                }
                return 1;
            }
            break;
        
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
                hfp_ag_slc_established(context);
            }
            hfp_ag_set_indicator_status_update_cmd(context->rfcomm_cid, 1);
            return 1;
                
        case HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES:
            if (context->state != HFP_W4_RETRIEVE_CAN_HOLD_CALL) break;
            if (has_hf_indicators_feature(context)){
                context->state = HFP_W4_LIST_GENERIC_STATUS_INDICATORS;
            }
            hfp_ag_retrieve_can_hold_call_cmd(context->rfcomm_cid);
            if (!has_hf_indicators_feature(context)){
                hfp_ag_slc_established(context);
            }
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
            hfp_ag_slc_established(context);
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
        case HFP_CMD_AG_ACTIVATE_VOICE_RECOGNITION:
            hfp_supported_features = store_bit(hfp_supported_features, HFP_AGSF_VOICE_RECOGNITION_FUNCTION, context->ag_activate_voice_recognition);
            hfp_ag_activate_voice_recognition_cmd(context->rfcomm_cid, context->ag_activate_voice_recognition);
            return 1;
        case HFP_CMD_HF_ACTIVATE_VOICE_RECOGNITION:
            if (get_bit(hfp_supported_features, HFP_AGSF_VOICE_RECOGNITION_FUNCTION)){
                hfp_supported_features = store_bit(hfp_supported_features, HFP_AGSF_VOICE_RECOGNITION_FUNCTION, context->ag_activate_voice_recognition);
                hfp_ag_ok(context->rfcomm_cid);
                hfp_ag_setup_audio_connection(context);
            } else {
                hfp_ag_error(context->rfcomm_cid);
            }
            return 1;
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
            hfp_ag_ok(context->rfcomm_cid);
            return 1;
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
        hfp_setup_synchronous_connection(context->con_handle, context->link_setting);
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
    context->ag_send_clip = hfp_gsm_clip_type() && context->clip_enabled;

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

//
// transitition implementations for hfp_ag_call_state_machine
//

static void hfp_ag_hf_start_ringing(hfp_connection_t * context){
    if (use_in_band_tone()){
        context->call_state = HFP_CALL_W4_AUDIO_CONNECTION_FOR_IN_BAND_RING;
        hfp_ag_establish_audio_connection(context->remote_addr);
    } else {
        hfp_timeout_start(context);
        context->ag_ring = 1;
        context->ag_send_clip = hfp_gsm_clip_type() && context->clip_enabled;
        context->call_state = HFP_CALL_RINGING;
        hfp_emit_event(hfp_callback, HFP_SUBEVENT_START_RINGINIG, 0);
    }
}

static void hfp_ag_hf_stop_ringing(hfp_connection_t * context){
    context->ag_ring = 0;
    context->ag_send_clip = 0;
    hfp_timeout_stop(context);
    hfp_emit_event(hfp_callback, HFP_SUBEVENT_STOP_RINGINIG, 0);
}

static void hfp_ag_trigger_incoming_call(void){
    int indicator_index = get_ag_indicator_index_for_name("callsetup");
    if (indicator_index < 0) return;

    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        hfp_ag_establish_service_level_connection(connection->remote_addr);
        if (connection->call_state == HFP_CALL_IDLE){
            connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, indicator_index, 1);
            hfp_ag_hf_start_ringing(connection);
        }
        if (connection->call_state == HFP_CALL_ACTIVE){
            connection->call_state = HFP_CALL_W2_SEND_CALL_WAITING;
        }
        hfp_run_for_context(connection);
    }
}

static void hfp_ag_transfer_callsetup_state(void){
    int indicator_index = get_ag_indicator_index_for_name("callsetup");
    if (indicator_index < 0) return;

    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        hfp_ag_establish_service_level_connection(connection->remote_addr);
        connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, indicator_index, 1);
        hfp_run_for_context(connection);
    }
}

static void hfp_ag_transfer_call_state(void){
    int indicator_index = get_ag_indicator_index_for_name("call");
    if (indicator_index < 0) return;

    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        hfp_ag_establish_service_level_connection(connection->remote_addr);
        connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, indicator_index, 1);
        hfp_run_for_context(connection);
    }
}

static void hfp_ag_transfer_callheld_state(void){
    int indicator_index = get_ag_indicator_index_for_name("callheld");
    if (indicator_index < 0) return;

    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        hfp_ag_establish_service_level_connection(connection->remote_addr);
        connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, indicator_index, 1);
        hfp_run_for_context(connection);
    }
}

static void hfp_ag_hf_accept_call(hfp_connection_t * source){
    
    int call_indicator_index = get_ag_indicator_index_for_name("call");
    int callsetup_indicator_index = get_ag_indicator_index_for_name("callsetup");

    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        if (connection->call_state != HFP_CALL_RINGING &&
            connection->call_state != HFP_CALL_W4_AUDIO_CONNECTION_FOR_IN_BAND_RING) continue;

        hfp_ag_hf_stop_ringing(connection);
        if (connection == source){
            connection->ok_pending = 1;

            if (use_in_band_tone()){
                connection->call_state = HFP_CALL_ACTIVE;
            } else {
                connection->call_state = HFP_CALL_W4_AUDIO_CONNECTION_FOR_ACTIVE;
                hfp_ag_establish_audio_connection(connection->remote_addr);
            }

            connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, call_indicator_index, 1);
            connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);

        } else {
            connection->call_state = HFP_CALL_IDLE;
        }
        hfp_run_for_context(connection);
    }    
}

static void hfp_ag_ag_accept_call(void){
    
    int call_indicator_index = get_ag_indicator_index_for_name("call");
    int callsetup_indicator_index = get_ag_indicator_index_for_name("callsetup");

    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        if (connection->call_state != HFP_CALL_RINGING) continue;

        hfp_ag_hf_stop_ringing(connection);
        connection->call_state = HFP_CALL_TRIGGER_AUDIO_CONNECTION;

        connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, call_indicator_index, 1);
        connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);

        hfp_run_for_context(connection);
        break;  // only single 
    }    
}

static void hfp_ag_trigger_reject_call(void){
    int callsetup_indicator_index = get_ag_indicator_index_for_name("callsetup");
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        if (connection->call_state != HFP_CALL_RINGING &&
            connection->call_state != HFP_CALL_W4_AUDIO_CONNECTION_FOR_IN_BAND_RING) continue;
        hfp_ag_hf_stop_ringing(connection);
        connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);
        connection->call_state = HFP_CALL_IDLE;
        hfp_run_for_context(connection);
    }    
}

static void hfp_ag_trigger_terminate_call(void){
    int call_indicator_index = get_ag_indicator_index_for_name("call");

    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        hfp_ag_establish_service_level_connection(connection->remote_addr);
        if (connection->call_state == HFP_CALL_IDLE) continue;
        connection->call_state = HFP_CALL_IDLE;
        connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, call_indicator_index, 1);
        connection->release_audio_connection = 1;
        hfp_run_for_context(connection);
    }
    hfp_emit_event(hfp_callback, HFP_SUBEVENT_CALL_TERMINATED, 0);
}

static void hfp_ag_set_callsetup_indicator(){
    hfp_ag_indicator_t * indicator = get_ag_indicator_for_name("callsetup");
    if (!indicator){
        log_error("hfp_ag_set_callsetup_indicator: callsetup indicator is missing");
    };
    indicator->status = hfp_gsm_callsetup_status();
}

static void hfp_ag_set_callheld_indicator(){
    hfp_ag_indicator_t * indicator = get_ag_indicator_for_name("callheld");
    if (!indicator){
        log_error("hfp_ag_set_callheld_state: callheld indicator is missing");
    };
    indicator->status = hfp_gsm_callheld_status();
}

static void hfp_ag_set_call_indicator(){
    hfp_ag_indicator_t * indicator = get_ag_indicator_for_name("call");
    if (!indicator){
        log_error("hfp_ag_set_call_state: call indicator is missing");
    };
    indicator->status = hfp_gsm_call_status();
}

static void hfp_ag_stop_ringing(void){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        if (connection->call_state != HFP_CALL_RINGING &&
            connection->call_state != HFP_CALL_W4_AUDIO_CONNECTION_FOR_IN_BAND_RING) continue;
        hfp_ag_hf_stop_ringing(connection);
    }    
}

static hfp_connection_t * hfp_ag_connection_for_call_state(hfp_call_state_t call_state){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        if (connection->call_state == call_state) return connection;
    }
    return NULL;
}

static void hfp_ag_send_response_and_hold_state(hfp_response_and_hold_state_t state){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        connection->send_response_and_hold_status = state + 1;
    }
}

static int call_setup_state_machine(hfp_connection_t * connection){
    int indicator_index;
    switch (connection->call_state){
        case HFP_CALL_W4_AUDIO_CONNECTION_FOR_IN_BAND_RING:
            if (connection->state != HFP_AUDIO_CONNECTION_ESTABLISHED) return 0;
            // we got event: audio connection established
            hfp_timeout_start(connection);
            connection->ag_ring = 1;
            connection->ag_send_clip = hfp_gsm_clip_type() && connection->clip_enabled;
            connection->call_state = HFP_CALL_RINGING;
            connection->call_state = HFP_CALL_RINGING;
            hfp_emit_event(hfp_callback, HFP_SUBEVENT_START_RINGINIG, 0);
            break;        
        case HFP_CALL_W4_AUDIO_CONNECTION_FOR_ACTIVE:
            if (connection->state != HFP_AUDIO_CONNECTION_ESTABLISHED) return 0;
            // we got event: audio connection established
            connection->call_state = HFP_CALL_ACTIVE;
            break;    
        case HFP_CALL_W2_SEND_CALL_WAITING:
            connection->call_state = HFP_CALL_W4_CHLD;
            hfp_ag_send_call_waiting_notification(connection->rfcomm_cid);
            indicator_index = get_ag_indicator_index_for_name("callsetup");
            connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, indicator_index, 1);
            break;
        default:
            break;
    }
    return 0;
}
// connection is used to identify originating HF
static void hfp_ag_call_sm(hfp_ag_call_event_t event, hfp_connection_t * connection){
    int indicator_index;
    int callsetup_indicator_index = get_ag_indicator_index_for_name("callsetup");
    int callheld_indicator_index = get_ag_indicator_index_for_name("callheld");
    int call_indicator_index = get_ag_indicator_index_for_name("call");
    
    //printf("hfp_ag_call_sm event %d \n", event);
    switch (event){
        case HFP_AG_INCOMING_CALL:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handle_event(HFP_AG_INCOMING_CALL);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_trigger_incoming_call();
                            printf("AG rings\n");
                            break;
                        default:
                            break;
                    }
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handle_event(HFP_AG_INCOMING_CALL);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_trigger_incoming_call();
                            printf("AG call waiting\n");
                            break;
                        default:
                            break;
                    }
                    break;
            }
            break;
        case HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handle_event(HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG);
                            hfp_ag_set_call_indicator();
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_ag_accept_call();
                            printf("AG answers call, accept call by GSM\n");
                            break;
                        default:
                            break;
                    }
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            printf("AG: current call is placed on hold, incoming call gets active\n");
                            hfp_gsm_handle_event(HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_set_callheld_indicator();
                            hfp_ag_transfer_callsetup_state();
                            hfp_ag_transfer_callheld_state();
                            break;
                        default:
                            break;
                    }
                    break;
            }
            break;
        
        case HFP_AG_HELD_CALL_JOINED_BY_AG:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    switch (hfp_gsm_callheld_status()){
                        case HFP_CALLHELD_STATUS_CALL_ON_HOLD_OR_SWAPPED:
                            printf("AG: joining held call with active call\n");
                            hfp_gsm_handle_event(HFP_AG_HELD_CALL_JOINED_BY_AG);
                            hfp_ag_set_callheld_indicator();
                            hfp_ag_transfer_callheld_state();
                            hfp_emit_event(hfp_callback, HFP_SUBEVENT_CONFERENCE_CALL, 0);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handle_event(HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_set_call_indicator();
                            hfp_ag_hf_accept_call(connection);
                            printf("HF answers call, accept call by GSM\n");
                            hfp_emit_event(hfp_callback, HFP_CMD_CALL_ANSWERED, 0);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_AG:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handle_event(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_AG);
                            hfp_ag_response_and_hold_active = 1;
                            hfp_ag_response_and_hold_state = HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD;
                            hfp_ag_send_response_and_hold_state(hfp_ag_response_and_hold_state);
                            // as with regualr call
                            hfp_ag_set_call_indicator();
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_ag_accept_call();
                            printf("AG response and hold - hold by AG\n");
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_HF:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handle_event(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_HF);
                            hfp_ag_response_and_hold_active = 1;
                            hfp_ag_response_and_hold_state = HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD;
                            hfp_ag_send_response_and_hold_state(hfp_ag_response_and_hold_state);
                            // as with regualr call
                            hfp_ag_set_call_indicator();
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_hf_accept_call(connection);
                            printf("AG response and hold - hold by HF\n");
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_AG:
        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_HF:
            if (!hfp_ag_response_and_hold_active) break;
            if (hfp_ag_response_and_hold_state != HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD) break;
            hfp_gsm_handle_event(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_AG);
            hfp_ag_response_and_hold_active = 0;
            hfp_ag_response_and_hold_state = HFP_RESPONSE_AND_HOLD_HELD_INCOMING_ACCEPTED;
            hfp_ag_send_response_and_hold_state(hfp_ag_response_and_hold_state);
            printf("Held Call accepted and active\n");
            break;

        case HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_AG:
        case HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_HF:
            if (!hfp_ag_response_and_hold_active) break;
            if (hfp_ag_response_and_hold_state != HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD) break;
            hfp_gsm_handle_event(HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_AG);
            hfp_ag_response_and_hold_active = 0;
            hfp_ag_response_and_hold_state = HFP_RESPONSE_AND_HOLD_HELD_INCOMING_REJECTED;
            hfp_ag_send_response_and_hold_state(hfp_ag_response_and_hold_state);
            // from terminate by ag
            hfp_ag_set_call_indicator();
            hfp_ag_trigger_terminate_call();
            break;

        case HFP_AG_TERMINATE_CALL_BY_HF:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handle_event(HFP_AG_TERMINATE_CALL_BY_HF);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_transfer_callsetup_state();
                            hfp_ag_trigger_reject_call();
                            printf("HF Rejected Incoming call, AG terminate call\n");
                            break;
                        case HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_DIALING_STATE:
                        case HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_ALERTING_STATE:
                            hfp_gsm_handle_event(HFP_AG_TERMINATE_CALL_BY_HF);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_transfer_callsetup_state();
                            printf("AG terminate outgoing call process\n");                            
                        default:
                            break;
                    }
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    hfp_gsm_handle_event(HFP_AG_TERMINATE_CALL_BY_HF);
                    hfp_ag_set_call_indicator();
                    hfp_ag_transfer_call_state();
                    connection->call_state = HFP_CALL_IDLE;
                    printf("AG terminate call\n");
                    break;
            }
            break;

        case HFP_AG_TERMINATE_CALL_BY_AG:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handle_event(HFP_AG_TERMINATE_CALL_BY_AG);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_trigger_reject_call();
                            printf("AG Rejected Incoming call, AG terminate call\n");
                            break;
                        default:
                            break;
                    }
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    hfp_gsm_handle_event(HFP_AG_TERMINATE_CALL_BY_AG);
                    hfp_ag_set_callsetup_indicator();
                    hfp_ag_set_call_indicator();
                    hfp_ag_trigger_terminate_call();
                    printf("AG terminate call\n");
                    break;
                default:
                    break;
            }
            break;
        case HFP_AG_CALL_DROPPED:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_ag_stop_ringing();
                            printf("Incoming call interrupted\n");
                            break;
                        case HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_DIALING_STATE:
                        case HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_ALERTING_STATE:
                            printf("Outgoing call interrupted\n");
                            printf("AG notify call dropped\n");
                            break;
                        default:
                            break;
                    }
                    hfp_gsm_handle_event(HFP_AG_CALL_DROPPED);
                    hfp_ag_set_callsetup_indicator();
                    hfp_ag_transfer_callsetup_state();
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    if (hfp_ag_response_and_hold_active) {
                        hfp_gsm_handle_event(HFP_AG_CALL_DROPPED);
                        hfp_ag_response_and_hold_state = HFP_RESPONSE_AND_HOLD_HELD_INCOMING_REJECTED;
                        hfp_ag_send_response_and_hold_state(hfp_ag_response_and_hold_state);
                    }
                    hfp_gsm_handle_event(HFP_AG_CALL_DROPPED);
                    hfp_ag_set_callsetup_indicator();
                    hfp_ag_set_call_indicator();
                    hfp_ag_trigger_terminate_call();
                    printf("AG notify call dropped\n");
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_OUTGOING_CALL_INITIATED:
            // directly reject call if number of free slots is exceeded
            if (!hfp_gsm_call_possible()){
                connection->send_error = 1;
                hfp_run_for_context(connection);  
                break;
            }
            hfp_gsm_handle_event_with_call_number(HFP_AG_OUTGOING_CALL_INITIATED, (const char *) &connection->line_buffer[3]);
            
            connection->call_state = HFP_CALL_OUTGOING_INITIATED;

            hfp_emit_string_event(hfp_callback, HFP_SUBEVENT_PLACE_CALL_WITH_NUMBER, (const char *) &connection->line_buffer[3]);
            break;

        case HFP_AG_OUTGOING_REDIAL_INITIATED:{
            // directly reject call if number of free slots is exceeded
            if (!hfp_gsm_call_possible()){
                connection->send_error = 1;
                hfp_run_for_context(connection);  
                break;
            }

            hfp_gsm_handle_event(HFP_AG_OUTGOING_REDIAL_INITIATED);
            connection->call_state = HFP_CALL_OUTGOING_INITIATED;

            printf("\nRedial last number");
            char * last_dialed_number = hfp_gsm_last_dialed_number();
            
            if (strlen(last_dialed_number) > 0){
                printf("\nLast number exists: accept call");
                hfp_emit_string_event(hfp_callback, HFP_SUBEVENT_PLACE_CALL_WITH_NUMBER, last_dialed_number);
            } else {
                printf("\nLast number missing: reject call");
                hfp_ag_outgoing_call_rejected();
            }
            break;
        }
        case HFP_AG_OUTGOING_CALL_REJECTED:
            connection = hfp_ag_connection_for_call_state(HFP_CALL_OUTGOING_INITIATED);
            if (!connection){
                log_info("hfp_ag_call_sm: did not find outgoing connection in initiated state");
                break;
            }
            
            hfp_gsm_handle_event(HFP_AG_OUTGOING_CALL_REJECTED);
            connection->call_state = HFP_CALL_IDLE;
            connection->send_error = 1;
            hfp_run_for_context(connection);
            break;

        case HFP_AG_OUTGOING_CALL_ACCEPTED:{
            connection = hfp_ag_connection_for_call_state(HFP_CALL_OUTGOING_INITIATED);
            if (!connection){
                log_info("hfp_ag_call_sm: did not find outgoing connection in initiated state");
                break;
            }
            
            connection->ok_pending = 1;
            connection->call_state = HFP_CALL_OUTGOING_DIALING;

            // trigger callsetup to be
            int put_call_on_hold = hfp_gsm_call_status() == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT;
            hfp_gsm_handle_event(HFP_AG_OUTGOING_CALL_ACCEPTED);

            hfp_ag_set_callsetup_indicator();
            indicator_index = get_ag_indicator_index_for_name("callsetup");
            connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, indicator_index, 1);

            // put current call on hold if active
            if (put_call_on_hold){
                printf("AG putting current call on hold for new outgoing call\n");
                hfp_ag_set_callheld_indicator();
                indicator_index = get_ag_indicator_index_for_name("callheld");
                hfp_ag_transfer_ag_indicators_status_cmd(connection->rfcomm_cid, &hfp_ag_indicators[indicator_index]);
            }

            // start audio if needed
            hfp_ag_establish_audio_connection(connection->remote_addr);
            break;
        }
        case HFP_AG_OUTGOING_CALL_RINGING:
            connection = hfp_ag_connection_for_call_state(HFP_CALL_OUTGOING_DIALING);
            if (!connection){
                log_info("hfp_ag_call_sm: did not find outgoing connection in dialing state");
                break;
            }
            
            hfp_gsm_handle_event(HFP_AG_OUTGOING_CALL_RINGING);
            connection->call_state = HFP_CALL_OUTGOING_RINGING;
            hfp_ag_set_callsetup_indicator();
            hfp_ag_transfer_callsetup_state();
            break;

        case HFP_AG_OUTGOING_CALL_ESTABLISHED:{
            // get outgoing call
            connection = hfp_ag_connection_for_call_state(HFP_CALL_OUTGOING_RINGING);
            if (!connection){
                connection = hfp_ag_connection_for_call_state(HFP_CALL_OUTGOING_DIALING);
            }
            if (!connection){
                log_info("hfp_ag_call_sm: did not find outgoing connection");
                break;
            }

            int CALLHELD_STATUS_CALL_ON_HOLD_AND_NO_ACTIVE_CALLS = hfp_gsm_callheld_status() == HFP_CALLHELD_STATUS_CALL_ON_HOLD_AND_NO_ACTIVE_CALLS;
            hfp_gsm_handle_event(HFP_AG_OUTGOING_CALL_ESTABLISHED);
            connection->call_state = HFP_CALL_ACTIVE;
            hfp_ag_set_callsetup_indicator();
            hfp_ag_set_call_indicator();
            hfp_ag_transfer_call_state();
            hfp_ag_transfer_callsetup_state();
            if (CALLHELD_STATUS_CALL_ON_HOLD_AND_NO_ACTIVE_CALLS){
                hfp_ag_set_callheld_indicator();
                hfp_ag_transfer_callheld_state();
            }
            break;
        }

        case HFP_AG_CALL_HOLD_USER_BUSY:
            hfp_gsm_handle_event(HFP_AG_CALL_HOLD_USER_BUSY);
            hfp_ag_set_callsetup_indicator();
            connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);
            connection->call_state = HFP_CALL_ACTIVE;
            printf("AG: Call Waiting, User Busy\n");
            break;
        
        case HFP_AG_CALL_HOLD_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL:{
            int call_setup_in_progress = hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
            int call_held = hfp_gsm_callheld_status() != HFP_CALLHELD_STATUS_NO_CALLS_HELD;
            
            // Releases all active calls (if any exist) and accepts the other (held or waiting) call.
            if (call_held || call_setup_in_progress){
                hfp_gsm_handle_event_with_call_index(HFP_AG_CALL_HOLD_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL, connection->call_index);
            
            }

            if (call_setup_in_progress){
                printf("AG: Call Dropped, Accept new call\n");
                hfp_ag_set_callsetup_indicator();
                connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);
            } else {
                printf("AG: Call Dropped, Resume held call\n");
            }
            if (call_held){
                hfp_ag_set_callheld_indicator();
                connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, callheld_indicator_index, 1);
            }
            
            connection->call_state = HFP_CALL_ACTIVE;
            break;
        }

        case HFP_AG_CALL_HOLD_PARK_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL:{
            // Places all active calls (if any exist) on hold and accepts the other (held or waiting) call.
            // only update if callsetup changed
            int call_setup_in_progress = hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
            hfp_gsm_handle_event_with_call_index(HFP_AG_CALL_HOLD_PARK_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL, connection->call_index);
            
            if (call_setup_in_progress){
                printf("AG: Call on Hold, Accept new call\n");
                hfp_ag_set_callsetup_indicator();
                connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);
            } else {
                printf("AG: Swap calls\n");
            }

            hfp_ag_set_callheld_indicator();
            // hfp_ag_set_callheld_state(HFP_CALLHELD_STATUS_CALL_ON_HOLD_OR_SWAPPED);
            connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, callheld_indicator_index, 1);
            connection->call_state = HFP_CALL_ACTIVE;
            break;
        }

        case HFP_AG_CALL_HOLD_ADD_HELD_CALL:
            // Adds a held call to the conversation.
            if (hfp_gsm_callheld_status() != HFP_CALLHELD_STATUS_NO_CALLS_HELD){
                printf("AG: Join 3-way-call\n");
                hfp_gsm_handle_event(HFP_AG_CALL_HOLD_ADD_HELD_CALL);
                hfp_ag_set_callheld_indicator();
                connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, callheld_indicator_index, 1);
                hfp_emit_event(hfp_callback, HFP_SUBEVENT_CONFERENCE_CALL, 0);
            }
            connection->call_state = HFP_CALL_ACTIVE;
            break;
        case HFP_AG_CALL_HOLD_EXIT_AND_JOIN_CALLS:
            // Connects the two calls and disconnects the subscriber from both calls (Explicit Call Transfer)
            hfp_gsm_handle_event(HFP_AG_CALL_HOLD_EXIT_AND_JOIN_CALLS);
            printf("AG: Transfer call -> Connect two calls and disconnect\n");
            hfp_ag_set_call_indicator();
            hfp_ag_set_callheld_indicator();
            connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, call_indicator_index, 1);
            connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, callheld_indicator_index, 1);
            connection->call_state = HFP_CALL_IDLE;
            break;
        
        default:
            break;
    }
   

}


static void hfp_ag_send_call_status(hfp_connection_t * connection, int call_index){
    hfp_gsm_call_t * active_call = hfp_gsm_call(call_index);
    if (!active_call) return;

    int idx = active_call->index;
    hfp_enhanced_call_dir_t dir = active_call->direction;
    hfp_enhanced_call_status_t status = active_call->enhanced_status;
    hfp_enhanced_call_mode_t mode = active_call->mode;
    hfp_enhanced_call_mpty_t mpty = active_call->mpty;
    uint8_t type = active_call->clip_type;
    char * number = active_call->clip_number;

    char buffer[100];
    // TODO: check length of a buffer, to fit the MTU
    int offset = snprintf(buffer, sizeof(buffer), "\r\n%s: %d,%d,%d,%d,%d", HFP_LIST_CURRENT_CALLS, idx, dir, status, mode, mpty);
    if (number){
        offset += snprintf(buffer+offset, sizeof(buffer)-offset, ", \"%s\",%u", number, type);
    } 
    snprintf(buffer+offset, sizeof(buffer)-offset, "\r\n");
    printf("hfp_ag_send_current_call_status 000 index %d, dir %d, status %d, mode %d, mpty %d, type %d, number %s\n", idx, dir, status,
       mode, mpty, type, number);
    send_str_over_rfcomm(connection->rfcomm_cid, buffer);
}

static void hfp_run_for_context(hfp_connection_t *context){
    if (!context) return;
    if (!rfcomm_can_send_packet_now(context->rfcomm_cid)) return;
    
    if (context->send_status_of_current_calls){
        context->ok_pending = 0; 
        if (context->next_call_index < hfp_gsm_get_number_of_calls()){
            context->next_call_index++;
            hfp_ag_send_call_status(context, context->next_call_index);
        } else {
            context->next_call_index = 0;
            context->ok_pending = 1;
            context->send_status_of_current_calls = 0;
        }
        return;
    } 

    if (context->command == HFP_CMD_UNKNOWN){
        context->ok_pending = 0;
        context->send_error = 0;
        context->command = HFP_CMD_NONE;
        hfp_ag_error(context->rfcomm_cid);
        return;
    }

    if (context->send_error){
        context->send_error = 0;
        context->command = HFP_CMD_NONE;
        hfp_ag_error(context->rfcomm_cid); 
        return;
    }

    // note: before update AG indicators and ok_pending 
    if (context->send_response_and_hold_status){
        int status = context->send_response_and_hold_status - 1;
        context->send_response_and_hold_status = 0;
        hfp_ag_set_response_and_hold(context->rfcomm_cid, status);
        return;
    }

    if (context->ok_pending){
        context->ok_pending = 0;
        context->command = HFP_CMD_NONE;
        hfp_ag_ok(context->rfcomm_cid);
        return;
    }

    // update AG indicators
    if (context->ag_indicators_status_update_bitmap){
        int i;
        for (i=0;i<context->ag_indicators_nr;i++){
            if (get_bit(context->ag_indicators_status_update_bitmap, i)){
                context->ag_indicators_status_update_bitmap = store_bit(context->ag_indicators_status_update_bitmap, i, 0);
                if (!context->enable_status_update_for_ag_indicators) {
                    log_info("+CMER:3,0,0,0 - not sending update for '%s'", hfp_ag_indicators[i].name);
                    break;
                }
                hfp_ag_transfer_ag_indicators_status_cmd(context->rfcomm_cid, &hfp_ag_indicators[i]);
                return;
            }
        }
    }

    if (context->ag_ring){
        context->ag_ring = 0;
        context->command = HFP_CMD_NONE;
        hfp_ag_ring(context->rfcomm_cid);
        return;
    }

    if (context->ag_send_clip){
        context->ag_send_clip = 0;
        context->command = HFP_CMD_NONE;
        hfp_ag_send_clip(context->rfcomm_cid);
        return;
    }
    
    if (context->send_phone_number_for_voice_tag){
        context->send_phone_number_for_voice_tag = 0;
        context->command = HFP_CMD_NONE;
        context->ok_pending = 1;
        hfp_ag_send_phone_number_for_voice_tag_cmd(context->rfcomm_cid);
        return;
    }

    if (context->send_subscriber_number){
        if (context->next_subscriber_number_to_send < subscriber_numbers_count){
            hfp_phone_number_t phone = subscriber_numbers[context->next_subscriber_number_to_send++];
            hfp_send_subscriber_number_cmd(context->rfcomm_cid, phone.type, phone.number);
        } else {
            context->send_subscriber_number = 0;
            context->next_subscriber_number_to_send = 0;
            hfp_ag_ok(context->rfcomm_cid);
        }
        context->command = HFP_CMD_NONE;
    }

    if (context->send_microphone_gain){
        context->send_microphone_gain = 0;
        context->command = HFP_CMD_NONE;
        hfp_ag_set_microphone_gain_cmd(context->rfcomm_cid, context->microphone_gain);
        return;
    }
    
    if (context->send_speaker_gain){
        context->send_speaker_gain = 0;
        context->command = HFP_CMD_NONE;
        hfp_ag_set_speaker_gain_cmd(context->rfcomm_cid, context->speaker_gain);
        return;
    }
    
    if (context->send_ag_status_indicators){
        context->send_ag_status_indicators = 0;
        hfp_ag_retrieve_indicators_status_cmd(context->rfcomm_cid);
        return;
    }

    int done = hfp_ag_run_for_context_service_level_connection(context);
    if (!done){
        done = hfp_ag_run_for_context_service_level_connection_queries(context);
    } 

    if (!done){
        done = call_setup_state_machine(context);
    }

    if (!done){  
        done = hfp_ag_run_for_audio_connection(context);
    }

    if (context->command == HFP_CMD_NONE && !done){
        // log_info("context->command == HFP_CMD_NONE");
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
static hfp_generic_status_indicator_t *get_hf_indicator_by_number(int number){
    int i;
    for (i=0;i< get_hfp_generic_status_indicators_nr();i++){
        hfp_generic_status_indicator_t * indicator = &get_hfp_generic_status_indicators()[i];
        if (indicator->uuid == number){
            return indicator;
        }
    }
    return NULL;
}

static void hfp_handle_rfcomm_data(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hfp_connection_t * context = get_hfp_connection_context_for_rfcomm_cid(channel);
    if (!context) return;
    
    char last_char = packet[size-1];
    packet[size-1] = 0;
    log_info("HFP_RX %s", packet);
    packet[size-1] = last_char;
    
    int pos;
    for (pos = 0; pos < size ; pos++){
        hfp_parse(context, packet[pos], 0);
    }
    hfp_generic_status_indicator_t * indicator;
    int value;
    switch(context->command){
        case HFP_CMD_RESPONSE_AND_HOLD_QUERY:
            if (hfp_ag_response_and_hold_active){
                context->send_response_and_hold_status = HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD + 1;
            }
            context->ok_pending = 1;
            break;
        case HFP_CMD_RESPONSE_AND_HOLD_COMMAND:
            value = atoi((char *)&context->line_buffer[0]);
            printf("HF Response and Hold: %u\n", value);
            switch(value){
                case HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD:
                    hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_HF, context);
                    break;
                case HFP_RESPONSE_AND_HOLD_HELD_INCOMING_ACCEPTED:
                    hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_HF, context);
                    break;
                case HFP_RESPONSE_AND_HOLD_HELD_INCOMING_REJECTED:
                    hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_HF, context);
                    break;
                default:
                    break;
            }
            context->ok_pending = 1;
            break;
        case HFP_CMD_HF_INDICATOR_STATUS:
            context->command = HFP_CMD_NONE;
            // find indicator by assigned number 
            indicator = get_hf_indicator_by_number(context->parser_indicator_index);
            if (!indicator){
                context->send_error = 1;
                break;
            }
            value = atoi((char *)&context->line_buffer[0]);
            switch (indicator->uuid){
                case 1: // enhanced security
                    if (value > 1) {
                        context->send_error = 1;
                        return;
                    }
                    printf("HF Indicator 'enhanced security' set to %u\n", value);
                    break;
                case 2: // battery level
                    if (value > 100){
                        context->send_error = 1;
                        return;
                    }
                    printf("HF Indicator 'battery' set to %u\n", value);
                    break;
                default:
                    printf("HF Indicator unknown set to %u\n", value);
                    break;
            }
            context->ok_pending = 1;
            break;
        case HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS:
            // expected by SLC state machine
            if (context->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) break;
            context->send_ag_indicators_segment = 0;
            context->send_ag_status_indicators = 1;
            break;
        case HFP_CMD_LIST_CURRENT_CALLS:   
            context->command = HFP_CMD_NONE;
            context->next_call_index = 0;
            context->send_status_of_current_calls = 1;
            break;
        case HFP_CMD_GET_SUBSCRIBER_NUMBER_INFORMATION:
            if (subscriber_numbers_count == 0){
                hfp_ag_ok(context->rfcomm_cid);
                break;
            }
            context->next_subscriber_number_to_send = 0;
            context->send_subscriber_number = 1;
            break;
        case HFP_CMD_TRANSMIT_DTMF_CODES:
            context->command = HFP_CMD_NONE;
            hfp_emit_string_event(hfp_callback, HFP_SUBEVENT_TRANSMIT_DTMF_CODES, (const char *) &context->line_buffer[0]);
            break;
        case HFP_CMD_HF_REQUEST_PHONE_NUMBER:
            context->command = HFP_CMD_NONE;
            hfp_emit_event(hfp_callback, HFP_SUBEVENT_ATTACH_NUMBER_TO_VOICE_TAG, 0);
            break;
        case HFP_CMD_TURN_OFF_EC_AND_NR:
            context->command = HFP_CMD_NONE;
            if (get_bit(hfp_supported_features, HFP_AGSF_EC_NR_FUNCTION)){
                context->ok_pending = 1;
                hfp_supported_features = store_bit(hfp_supported_features, HFP_AGSF_EC_NR_FUNCTION, context->ag_echo_and_noise_reduction);
                printf("AG: EC/NR = %u\n", context->ag_echo_and_noise_reduction);
            } else {
                context->send_error = 1;
            }
            break;
        case HFP_CMD_CALL_ANSWERED:
            context->command = HFP_CMD_NONE;
            printf("HFP: ATA\n");
            hfp_ag_call_sm(HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF, context);
            break;
        case HFP_CMD_HANG_UP_CALL:
            context->command = HFP_CMD_NONE;
            context->ok_pending = 1;
            hfp_ag_call_sm(HFP_AG_TERMINATE_CALL_BY_HF, context);
            break;
        case HFP_CMD_CALL_HOLD: {
            // TODO: fully implement this
            log_error("HFP: unhandled call hold type %c", context->line_buffer[0]);
            context->command = HFP_CMD_NONE;
            context->ok_pending = 1;
            context->call_index = 0;
            
            if (context->line_buffer[1] != '\0'){
                context->call_index = atoi((char *)&context->line_buffer[1]);
            }

            switch (context->line_buffer[0]){
                case '0':
                    // Releases all held calls or sets User Determined User Busy (UDUB) for a waiting call.
                    hfp_ag_call_sm(HFP_AG_CALL_HOLD_USER_BUSY, context);
                    break;
                case '1':
                    // Releases all active calls (if any exist) and accepts the other (held or waiting) call.
                    // Where both a held and a waiting call exist, the above procedures shall apply to the
                    // waiting call (i.e., not to the held call) in conflicting situation.
                    hfp_ag_call_sm(HFP_AG_CALL_HOLD_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL, context);
                    break;
                case '2':
                    // Places all active calls (if any exist) on hold and accepts the other (held or waiting) call.
                    // Where both a held and a waiting call exist, the above procedures shall apply to the
                    // waiting call (i.e., not to the held call) in conflicting situation.
                    hfp_ag_call_sm(HFP_AG_CALL_HOLD_PARK_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL, context);
                    break;
                case '3':
                    // Adds a held call to the conversation.
                    hfp_ag_call_sm(HFP_AG_CALL_HOLD_ADD_HELD_CALL, context);
                    break;
                case '4':
                    // Connects the two calls and disconnects the subscriber from both calls (Explicit Call Transfer).
                    hfp_ag_call_sm(HFP_AG_CALL_HOLD_EXIT_AND_JOIN_CALLS, context);
                    break;
                default:
                    break;
            }
            break;
        }
        case HFP_CMD_CALL_PHONE_NUMBER:
            context->command = HFP_CMD_NONE;
            hfp_ag_call_sm(HFP_AG_OUTGOING_CALL_INITIATED, context);
            break;
        case HFP_CMD_REDIAL_LAST_NUMBER:
            context->command = HFP_CMD_NONE;
            hfp_ag_call_sm(HFP_AG_OUTGOING_REDIAL_INITIATED, context);
            break;
        case HFP_CMD_ENABLE_CLIP:
            context->command = HFP_CMD_NONE;
            context->clip_enabled = context->line_buffer[8] != '0';
            log_info("hfp: clip set, now: %u", context->clip_enabled);
            context->ok_pending = 1;
            break;
        case HFP_CMD_ENABLE_CALL_WAITING_NOTIFICATION:
            context->command = HFP_CMD_NONE;
            context->call_waiting_notification_enabled = context->line_buffer[8] != '0';
            log_info("hfp: call waiting notification set, now: %u", context->call_waiting_notification_enabled);
            context->ok_pending = 1;
            break;
        case HFP_CMD_SET_SPEAKER_GAIN:
            context->command = HFP_CMD_NONE;
            context->ok_pending = 1;
            printf("HF speaker gain = %u\n", context->speaker_gain);
            break;
        case HFP_CMD_SET_MICROPHONE_GAIN:
            context->command = HFP_CMD_NONE;
            context->ok_pending = 1;
            printf("HF microphone gain = %u\n", context->microphone_gain);
            break;
        default:
            break;
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

static void hfp_ag_set_ag_indicators(hfp_ag_indicator_t * ag_indicators, int ag_indicators_nr){
    hfp_ag_indicators_nr = ag_indicators_nr;
    memcpy(hfp_ag_indicators, ag_indicators, ag_indicators_nr * sizeof(hfp_ag_indicator_t));
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

    hfp_ag_set_ag_indicators(ag_indicators, ag_indicators_nr);

    set_hfp_generic_status_indicators(hf_indicators, hf_indicators_nr);

    hfp_ag_call_hold_services_nr = call_hold_services_nr;
    memcpy(hfp_ag_call_hold_services, call_hold_services, call_hold_services_nr * sizeof(char *));

    hfp_ag_response_and_hold_active = 0;
    subscriber_numbers = NULL;
    subscriber_numbers_count = 0;

    hfp_gsm_init();
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

static void hfp_ag_setup_audio_connection(hfp_connection_t * connection){
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
}

void hfp_ag_establish_audio_connection(bd_addr_t bd_addr){
    hfp_ag_establish_service_level_connection(bd_addr);
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);

    connection->establish_audio_connection = 0;
    hfp_ag_setup_audio_connection(connection);
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
 * @brief Called from GSM
 */
void hfp_ag_incoming_call(void){
    hfp_ag_call_sm(HFP_AG_INCOMING_CALL, NULL);
}

/**
 * @brief number is stored.
 */
void hfp_ag_set_clip(uint8_t type, const char * number){
    hfp_gsm_handle_event_with_clip(HFP_AG_SET_CLIP, type, number);
}

void hfp_ag_call_dropped(void){
    hfp_ag_call_sm(HFP_AG_CALL_DROPPED, NULL);
}

// call from AG UI
void hfp_ag_answer_incoming_call(void){
    hfp_ag_call_sm(HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG, NULL);
}

void hfp_ag_join_held_call(void){
    hfp_ag_call_sm(HFP_AG_HELD_CALL_JOINED_BY_AG, NULL);
}

void hfp_ag_terminate_call(void){
    hfp_ag_call_sm(HFP_AG_TERMINATE_CALL_BY_AG, NULL);
}

void hfp_ag_outgoing_call_ringing(void){
    hfp_ag_call_sm(HFP_AG_OUTGOING_CALL_RINGING, NULL);
}

void hfp_ag_outgoing_call_established(void){
    hfp_ag_call_sm(HFP_AG_OUTGOING_CALL_ESTABLISHED, NULL);
}

void hfp_ag_outgoing_call_rejected(void){
    hfp_ag_call_sm(HFP_AG_OUTGOING_CALL_REJECTED, NULL);
}

void hfp_ag_outgoing_call_accepted(void){
    hfp_ag_call_sm(HFP_AG_OUTGOING_CALL_ACCEPTED, NULL);
}

void hfp_ag_hold_incoming_call(void){
    hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_AG, NULL);
}

void hfp_ag_accept_held_incoming_call(void) {
    hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_AG, NULL);
}

void hfp_ag_reject_held_incoming_call(void){
    hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_AG, NULL);
}

static void hfp_ag_set_ag_indicator(const char * name, int value){
    int indicator_index = get_ag_indicator_index_for_name(name);
    if (indicator_index < 0) return;
    hfp_ag_indicators[indicator_index].status = value;


    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        if (!connection->ag_indicators[indicator_index].enabled) {
            log_info("AG indicator '%s' changed to %u but not enabled", hfp_ag_indicators[indicator_index].name, value);
            continue;
        }
        log_info("AG indicator '%s' changed to %u, request transfer statur", hfp_ag_indicators[indicator_index].name, value);
        connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, indicator_index, 1);
        hfp_run_for_context(connection);
    }    
}

/*
 * @brief
 */
void hfp_ag_set_registration_status(int status){
    hfp_ag_set_ag_indicator("service", status);
}

/*
 * @brief
 */
void hfp_ag_set_signal_strength(int strength){
    hfp_ag_set_ag_indicator("signal", strength);
}

/*
 * @brief
 */
void hfp_ag_set_roaming_status(int status){
    hfp_ag_set_ag_indicator("roam", status);
}

/*
 * @brief
 */
void hfp_ag_set_battery_level(int level){
    hfp_ag_set_ag_indicator("battchg", level);
}

/*
 * @brief
 */
void hfp_ag_activate_voice_recognition(bd_addr_t bd_addr, int activate){
    if (!get_bit(hfp_supported_features, HFP_AGSF_VOICE_RECOGNITION_FUNCTION)) return;
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);

    if (!get_bit(connection->remote_supported_features, HFP_HFSF_VOICE_RECOGNITION_FUNCTION)) {
        printf("AG cannot acivate voice recognition - not supported by HF\n");
        return;
    }

    if (activate){
        hfp_ag_establish_audio_connection(bd_addr);
    }

    connection->ag_activate_voice_recognition = activate;
    connection->command = HFP_CMD_AG_ACTIVATE_VOICE_RECOGNITION;
    hfp_run_for_context(connection);
}

/*
 * @brief
 */
void hfp_ag_set_microphone_gain(bd_addr_t bd_addr, int gain){
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);
    if (connection->microphone_gain != gain){
        connection->command = HFP_CMD_SET_MICROPHONE_GAIN;
        connection->microphone_gain = gain;
        connection->send_microphone_gain = 1;
    } 
    hfp_run_for_context(connection);
}

/*
 * @brief
 */
void hfp_ag_set_speaker_gain(bd_addr_t bd_addr, int gain){
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);
    if (connection->speaker_gain != gain){
        connection->speaker_gain = gain;
        connection->send_speaker_gain = 1;
    } 
    hfp_run_for_context(connection);
}

/*
 * @brief
 */
void hfp_ag_send_phone_number_for_voice_tag(bd_addr_t bd_addr, const char * number){
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);
    hfp_ag_set_clip(0, number);
    connection->send_phone_number_for_voice_tag = 1;
}

void hfp_ag_reject_phone_number_for_voice_tag(bd_addr_t bd_addr){
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);
    connection->send_error = 1;
}

void hfp_ag_send_dtmf_code_done(bd_addr_t bd_addr){
    hfp_connection_t * connection = get_hfp_connection_context_for_bd_addr(bd_addr);
    connection->ok_pending = 1;
}

void hfp_ag_set_subcriber_number_information(hfp_phone_number_t * numbers, int numbers_count){
    subscriber_numbers = numbers;
    subscriber_numbers_count = numbers_count;
}

void hfp_ag_clear_last_dialed_number(void){
    hfp_gsm_clear_last_dialed_number();
}


