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

hfp_generic_status_indicator_t * get_hfp_generic_status_indicators();
int get_hfp_generic_status_indicators_nr();
void set_hfp_generic_status_indicators(hfp_generic_status_indicator_t * indicators, int indicator_nr);

hfp_ag_indicator_t * get_hfp_ag_indicators(){
    return (hfp_ag_indicator_t *)&hfp_ag_indicators;
}
int get_hfp_ag_indicators_nr(){
    return hfp_ag_indicators_nr;
}
void set_hfp_ag_indicators(hfp_ag_indicator_t * indicators, int indicator_nr){
    memcpy(hfp_ag_indicators, indicators, indicator_nr * sizeof(hfp_ag_indicator_t));
    hfp_ag_indicators_nr = indicator_nr;
}


void hfp_ag_register_packet_handler(hfp_callback_t callback){
    if (callback == NULL){
        log_error("hfp_ag_register_packet_handler called with NULL callback");
        return;
    }
    hfp_callback = callback;
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
    hfp_create_sdp_record(service, SDP_HandsfreeAudioGateway, rfcomm_channel_nr, name, supported_features);
    
    // Network
    de_add_number(service, DE_UINT, DE_SIZE_8, ability_to_reject_call);
    /*
     * 0x01 – Ability to reject a call
     * 0x00 – No ability to reject a call
     */
}

int hfp_ag_exchange_supported_features_cmd(uint16_t cid){
    char buffer[40];
    sprintf(buffer, "\r\n%s:%d\r\n\r\nOK\r\n", HFP_SUPPORTED_FEATURES, hfp_supported_features);
    // printf("exchange_supported_features %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_ok(uint16_t cid){
    char buffer[10];
    sprintf(buffer, "\r\nOK\r\n");
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_error(uint16_t cid){
    char buffer[10];
    sprintf(buffer, "\r\nERROR\r\n");
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_retrieve_codec_cmd(uint16_t cid){
    return hfp_ag_ok(cid);
}

int hfp_ag_indicators_join(char * buffer, int buffer_size){
    if (buffer_size < hfp_ag_indicators_nr * (1 + sizeof(hfp_ag_indicator_t))) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < hfp_ag_indicators_nr-1; i++) {
        offset += snprintf(buffer+offset, buffer_size-offset, "(\"%s\",(%d,%d)),", hfp_ag_indicators[i].name, hfp_ag_indicators[i].min_range, hfp_ag_indicators[i].max_range);;
    }
    if (i<hfp_ag_indicators_nr){
        offset += snprintf(buffer+offset, buffer_size-offset, "(\"%s\",(%d,%d))", hfp_ag_indicators[i].name, hfp_ag_indicators[i].min_range, hfp_ag_indicators[i].max_range);
    }
    return offset;
}

int hfp_hf_indicators_join(char * buffer, int buffer_size){
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

int hfp_hf_indicators_initial_status_join(char * buffer, int buffer_size){
    if (buffer_size < get_hfp_generic_status_indicators_nr() * 3) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < get_hfp_generic_status_indicators_nr(); i++) {
        offset += snprintf(buffer+offset, buffer_size-offset, "\r\n%s:%d,%d\r\n", HFP_GENERIC_STATUS_INDICATOR, get_hfp_generic_status_indicators()[i].uuid, get_hfp_generic_status_indicators()[i].state);
    }
    return offset;
}

int hfp_ag_indicators_status_join(char * buffer, int buffer_size){
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

int hfp_ag_call_services_join(char * buffer, int buffer_size){
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

int hfp_ag_retrieve_indicators_cmd(uint16_t cid){
    char buffer[150];
    int offset = snprintf(buffer, sizeof(buffer), "\r\n%s:", HFP_INDICATOR);
    offset += hfp_ag_indicators_join(buffer+offset, sizeof(buffer)-offset);
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, "\r\n\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_retrieve_indicators_status_cmd(uint16_t cid){
    char buffer[40];
    int offset = snprintf(buffer, sizeof(buffer), "\r\n%s:", HFP_INDICATOR);
    offset += hfp_ag_indicators_status_join(buffer+offset, sizeof(buffer)-offset);
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, "\r\n\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_toggle_indicator_status_update_cmd(uint16_t cid, uint8_t activate){
    // AT\r\n%s:3,0,0,%d\r\n
    return hfp_ag_ok(cid);
}


int hfp_ag_retrieve_can_hold_call_cmd(uint16_t cid){
    char buffer[100];
    int offset = snprintf(buffer, sizeof(buffer), "\r\n%s:", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES);
    offset += hfp_ag_call_services_join(buffer+offset, sizeof(buffer)-offset);
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, "\r\n\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}


int hfp_ag_list_supported_generic_status_indicators_cmd(uint16_t cid){
    return hfp_ag_ok(cid);
}

int hfp_ag_retrieve_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[40];
    int offset = snprintf(buffer, sizeof(buffer), "\r\n%s:", HFP_GENERIC_STATUS_INDICATOR);
    offset += hfp_hf_indicators_join(buffer+offset, sizeof(buffer)-offset);
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, "\r\n\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_retrieve_initital_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[40];
    int offset = hfp_hf_indicators_initial_status_join(buffer, sizeof(buffer));
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, "\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_transfer_ag_indicators_status_cmd(uint16_t cid, hfp_ag_indicator_t indicator){
    char buffer[20];
    sprintf(buffer, "\r\n%s:%d,%d\r\n\r\nOK\r\n", HFP_TRANSFER_AG_INDICATOR_STATUS, indicator.index, indicator.status);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_report_network_operator_name_cmd(uint16_t cid, hfp_network_opearator_t op){
    char buffer[40];
    if (strlen(op.name) == 0){
        sprintf(buffer, "\r\n%s:%d,,\r\n\r\nOK\r\n", HFP_QUERY_OPERATOR_SELECTION, op.mode);
    } else {
        sprintf(buffer, "\r\n%s:%d,%d,%s\r\n\r\nOK\r\n", HFP_QUERY_OPERATOR_SELECTION, op.mode, op.format, op.name);
    }
    return send_str_over_rfcomm(cid, buffer);
}

void update_command(hfp_connection_t * context){
    context->command = HFP_CMD_NONE; 
    printf("Received %s\n", context->line_buffer);
        
    if (strncmp((char *)context->line_buffer+2, HFP_SUPPORTED_FEATURES, strlen(HFP_SUPPORTED_FEATURES)) == 0){
        context->command = HFP_CMD_SUPPORTED_FEATURES;
        return;
    }

    if (strncmp((char *)context->line_buffer+2, HFP_INDICATOR, strlen(HFP_INDICATOR)) == 0){
        if (strncmp((char *)context->line_buffer+strlen(HFP_INDICATOR)+2, "?", 1) == 0){
            context->command = HFP_CMD_INDICATOR_STATUS;    
        } else {
            context->command = HFP_CMD_INDICATOR;    
        }
        return;
    }


    if (strncmp((char *)context->line_buffer+2, HFP_AVAILABLE_CODECS, strlen(HFP_AVAILABLE_CODECS)) == 0){
        context->command = HFP_CMD_AVAILABLE_CODECS;
        return;
    }

    if (strncmp((char *)context->line_buffer+2, HFP_ENABLE_STATUS_UPDATE_FOR_AG_INDICATORS, strlen(HFP_ENABLE_STATUS_UPDATE_FOR_AG_INDICATORS)) == 0){
        context->command = HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE;
        return;
    }

    if (strncmp((char *)context->line_buffer+2, HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES, strlen(HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES)) == 0){
        context->command = HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES;
        return;
    } 

    if (strncmp((char *)context->line_buffer+2, HFP_GENERIC_STATUS_INDICATOR, strlen(HFP_GENERIC_STATUS_INDICATOR)) == 0){
        context->command = HFP_CMD_GENERIC_STATUS_INDICATOR;
        return;
    } 

    if (strncmp((char *)context->line_buffer+2, HFP_UPDATE_ENABLE_STATUS_FOR_INDIVIDUAL_AG_INDICATORS, strlen(HFP_UPDATE_ENABLE_STATUS_FOR_INDIVIDUAL_AG_INDICATORS)) == 0){
        context->command = HFP_CMD_ENABLE_INDIVIDUAL_AG_INDICATOR_STATUS_UPDATE;
        return;
    } 

    if (strncmp((char *)context->line_buffer+2, HFP_QUERY_OPERATOR_SELECTION, strlen(HFP_QUERY_OPERATOR_SELECTION)) == 0){
        context->command = HFP_CMD_QUERY_OPERATOR_SELECTION;
        if (strncmp((char *)context->line_buffer+strlen(HFP_QUERY_OPERATOR_SELECTION)+2, "?", 1) == 0){
            context->operator_name_format = 1; 
            context->operator_name = 0; 
        } else {
            context->operator_name_format = 1; 
            context->operator_name = 0;    
        }
        return;
    } 
}


void hfp_run_for_context(hfp_connection_t *context){
    if (!context) return;
    if (!rfcomm_can_send_packet_now(context->rfcomm_cid)) return;
    
    if (context->state == HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED){
        if (context->enable_status_update_for_ag_indicators == 1){
            int i;
            for (i = 0; i < context->ag_indicators_nr; i++){
                if (context->ag_indicators[i].enabled == 0) continue;
                if (context->ag_indicators[i].status_changed == 0) continue;
                            
                hfp_ag_transfer_ag_indicators_status_cmd(context->rfcomm_cid, context->ag_indicators[i]);
                context->ag_indicators[i].status_changed = 0;
                return;
            }
        }
        return;
    }

    switch(context->command){
        case HFP_CMD_SUPPORTED_FEATURES:
            switch(context->state){
                case HFP_W4_EXCHANGE_SUPPORTED_FEATURES:
                    hfp_ag_exchange_supported_features_cmd(context->rfcomm_cid);
                    if (has_codec_negotiation_feature(context)){
                        context->state = HFP_W4_NOTIFY_ON_CODECS;
                        break;
                    } 
                    context->state = HFP_W4_RETRIEVE_INDICATORS;
                    break;
                default:
                    break;
            }
            break;
        case HFP_CMD_AVAILABLE_CODECS:
            switch(context->state){
                case HFP_W4_NOTIFY_ON_CODECS:
                    hfp_ag_retrieve_codec_cmd(context->rfcomm_cid);
                    context->state = HFP_W4_RETRIEVE_INDICATORS;
                    break;
                default:
                    break;
            }
            break;
        case HFP_CMD_INDICATOR:
            switch(context->state){
                case HFP_W4_RETRIEVE_INDICATORS:
                    hfp_ag_retrieve_indicators_cmd(context->rfcomm_cid);
                    context->state = HFP_W4_RETRIEVE_INDICATORS_STATUS;
                    break;
                default:
                    break;
            }
            break;
        case HFP_CMD_INDICATOR_STATUS:
             switch(context->state){
                case HFP_W4_RETRIEVE_INDICATORS_STATUS:
                    hfp_ag_retrieve_indicators_status_cmd(context->rfcomm_cid);
                    context->state = HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE;
                    break;
                default:
                    break;
            }
            break;
        case HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE:
            switch(context->state){
                case HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE:
                    hfp_ag_toggle_indicator_status_update_cmd(context->rfcomm_cid, 1);
                    if (has_call_waiting_and_3way_calling_feature(context)){
                        context->state = HFP_W4_RETRIEVE_CAN_HOLD_CALL;
                        break;
                    }
                    if (has_hf_indicators_feature(context)){
                        context->state = HFP_W4_LIST_GENERIC_STATUS_INDICATORS;
                        break;
                    } 
                    context->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
                    hfp_emit_event(hfp_callback, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED, 0);
                    break;
                case HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
                    // TODO 
                    break;
                default:
                    break;
            }
            break;
        case HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES:
            switch(context->state){
                case HFP_W4_RETRIEVE_CAN_HOLD_CALL:
                    hfp_ag_retrieve_can_hold_call_cmd(context->rfcomm_cid);
                    if (has_hf_indicators_feature(context)){
                        context->state = HFP_W4_LIST_GENERIC_STATUS_INDICATORS;
                        break;
                    } 
                    context->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
                    hfp_emit_event(hfp_callback, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED, 0);
                    break;
                default:
                    break;
            }
            break;
        case HFP_CMD_GENERIC_STATUS_INDICATOR:
            switch(context->state){
                case HFP_W4_LIST_GENERIC_STATUS_INDICATORS:
                    hfp_ag_list_supported_generic_status_indicators_cmd(context->rfcomm_cid);
                    context->state = HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS;
                    break;
                case HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS:
                    hfp_ag_retrieve_supported_generic_status_indicators_cmd(context->rfcomm_cid);
                    context->state = HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS; 
                    break;
                case HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
                    hfp_ag_retrieve_initital_supported_generic_status_indicators_cmd(context->rfcomm_cid);
                    
                    context->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
                    hfp_emit_event(hfp_callback, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED, 0);
                    break;
                default:
                    break;
            }
            break;
        case HFP_CMD_QUERY_OPERATOR_SELECTION:
            if (context->state != HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) break;
            if (context->operator_name_format == 1){
                if (context->network_operator.format != 0){
                    hfp_ag_error(context->rfcomm_cid);
                    break;
                }
                hfp_ag_ok(context->rfcomm_cid);    
                break;
            }
            if (context->operator_name == 1){
                hfp_ag_report_network_operator_name_cmd(context->rfcomm_cid, context->network_operator);
                break;
            }
            break;
        case HFP_CMD_NONE:
            switch(context->state){
                case HFP_W2_DISCONNECT_RFCOMM:
                    // printf("rfcomm_disconnect_internal cid 0x%02x\n", context->rfcomm_cid);
                    context->state = HFP_W4_RFCOMM_DISCONNECTED;
                    rfcomm_disconnect_internal(context->rfcomm_cid);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    // done
    context->command = HFP_CMD_NONE;
}

static void hfp_handle_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hfp_connection_t * context = get_hfp_connection_context_for_rfcomm_cid(channel);
    if (!context) return;
    if (context->state == HFP_EXCHANGE_SUPPORTED_FEATURES){
        context->state = HFP_W4_EXCHANGE_SUPPORTED_FEATURES;   
    }

    printf("hfp_handle_rfcomm_event %s\n", &packet[0]);
    packet[size] = 0;
    int pos;
    for (pos = 0; pos < size ; pos++){
        hfp_parse(context, packet[pos]);

        // trigger next action after CMD received
        if (context->command == HFP_CMD_NONE) continue;
        // hfp_run_for_context(context);
    }
}

static void hfp_run(){
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
            hfp_handle_rfcomm_event(packet_type, channel, packet, size);
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
    char *call_hold_services[], int call_hold_services_nr){
    if (codecs_nr > HFP_MAX_NUM_CODECS){
        log_error("hfp_init: codecs_nr (%d) > HFP_MAX_NUM_CODECS (%d)", codecs_nr, HFP_MAX_NUM_CODECS);
        return;
    }
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

