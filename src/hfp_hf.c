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
// Minimal setup for HFP Hands-Free (HF) unit (!! UNDER DEVELOPMENT !!)
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
#include "hfp_hf.h"


static const char default_hfp_hf_service_name[] = "Hands-Free unit";
static uint16_t hfp_supported_features = HFP_DEFAULT_HF_SUPPORTED_FEATURES;
static uint8_t hfp_codecs_nr = 0;
static uint8_t hfp_codecs[HFP_MAX_NUM_CODECS];

static uint8_t hfp_indicators_nr = 0;
static uint8_t hfp_indicators[HFP_MAX_NUM_INDICATORS];
static uint8_t hfp_indicators_status;

static int get_bit(uint16_t bitmap, int position){
    return (bitmap >> position) & 1;
}


int has_codec_negotiation_feature(hfp_connection_t * connection){
    int hf = get_bit(hfp_supported_features, HFP_HFSF_CODEC_NEGOTIATION);
    int ag = get_bit(connection->remote_supported_features, HFP_AGSF_CODEC_NEGOTIATION);
    return hf && ag;
}

int has_call_waiting_and_3way_calling_feature(hfp_connection_t * connection){
    int hf = get_bit(hfp_supported_features, HFP_HFSF_THREE_WAY_CALLING);
    int ag = get_bit(connection->remote_supported_features, HFP_AGSF_THREE_WAY_CALLING);
    return hf && ag;
}


int has_hf_indicators_feature(hfp_connection_t * connection){
    int hf = get_bit(hfp_supported_features, HFP_HFSF_HF_INDICATORS);
    int ag = get_bit(connection->remote_supported_features, HFP_AGSF_HF_INDICATORS);
    return hf && ag;
}

static void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void hfp_hf_create_service(uint8_t * service, int rfcomm_channel_nr, const char * name, uint16_t supported_features){
    if (!name){
        name = default_hfp_hf_service_name;
    }
    hfp_create_service(service, SDP_Handsfree, rfcomm_channel_nr, name, supported_features);
}

static int store_bit(uint32_t bitmap, int position, uint8_t value){
    if (value){
        bitmap |= 1 << position;
    } else {
        bitmap &= ~ (1 << position);
    }
    return bitmap;
}

int hfp_hs_exchange_supported_features_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s=%d\r\n", HFP_SUPPORTED_FEATURES, hfp_supported_features);
    // printf("exchange_supported_features %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_hs_retrieve_codec_cmd(uint16_t cid){
    char buffer[30];
    int buffer_offset = snprintf(buffer, sizeof(buffer), "AT%s=", HFP_AVAILABLE_CODECS);
    join(buffer+buffer_offset, sizeof(buffer)-buffer_offset, hfp_codecs, hfp_codecs_nr);
    // printf("retrieve_codec %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}


int hfp_hs_retrieve_indicators_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s=?\r\n", HFP_INDICATOR);
    // printf("retrieve_indicators %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_hs_retrieve_indicators_status_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s?\r\n", HFP_INDICATOR);
    // printf("retrieve_indicators_status %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_hs_toggle_indicator_status_update_cmd(uint16_t cid, uint8_t activate){
    char buffer[20];
    sprintf(buffer, "AT%s=3,0,0,%d\r\n", HFP_ENABLE_INDICATOR_STATUS_UPDATE, activate);
    // printf("toggle_indicator_status_update %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}


int hfp_hs_retrieve_can_hold_call_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s=?\r\n", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES);
    // printf("retrieve_can_hold_call %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}


int hfp_hs_list_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[30];
    int buffer_offset = snprintf(buffer, sizeof(buffer), "AT%s=", HFP_GENERIC_STATUS_INDICATOR); 
    join(buffer+buffer_offset, sizeof(buffer)-buffer_offset, hfp_indicators, hfp_indicators_nr); 
    // printf("list_supported_generic_status_indicators %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_hs_retrieve_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s=?\r\rn", HFP_GENERIC_STATUS_INDICATOR); 
    // printf("retrieve_supported_generic_status_indicators %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_hs_list_initital_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s?\r\n", HFP_GENERIC_STATUS_INDICATOR);
    // printf("list_initital_supported_generic_status_indicators %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

static void hfp_run_for_context(hfp_connection_t * connection){
    if (!connection) return;
    // printf("hfp send cmd: context %p, RFCOMM cid %u \n", connection, connection->rfcomm_cid );
    if (!rfcomm_can_send_packet_now(connection->rfcomm_cid)) return;
    
    switch (connection->state){
        case HFP_EXCHANGE_SUPPORTED_FEATURES:
            hfp_hs_exchange_supported_features_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_EXCHANGE_SUPPORTED_FEATURES;
            break;
        case HFP_NOTIFY_ON_CODECS:
            if (has_codec_negotiation_feature(connection)){
                hfp_hs_retrieve_codec_cmd(connection->rfcomm_cid);
                connection->state = HFP_W4_NOTIFY_ON_CODECS;
                break;
            } 
            connection->state = HFP_RETRIEVE_INDICATORS;
            printf("fall through to HFP_RETRIEVE_INDICATORS (no codec feature)\n");
            // 
        case HFP_RETRIEVE_INDICATORS:
            hfp_hs_retrieve_indicators_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_RETRIEVE_INDICATORS;
            break;
        case HFP_RETRIEVE_INDICATORS_STATUS:
            hfp_hs_retrieve_indicators_status_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_RETRIEVE_INDICATORS_STATUS;
            break;
        case HFP_ENABLE_INDICATORS_STATUS_UPDATE:
            if (connection->remote_indicators_update_enabled == 0){
                hfp_hs_toggle_indicator_status_update_cmd(connection->rfcomm_cid, 1);
                connection->state = HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE;
                connection->wait_ok = 1;
                break;
            }
            printf("fall through to HFP_RETRIEVE_CAN_HOLD_CALL\n");
            connection->state = HFP_RETRIEVE_CAN_HOLD_CALL;
            // 
        case HFP_RETRIEVE_CAN_HOLD_CALL:
            if (has_call_waiting_and_3way_calling_feature(connection)){
                hfp_hs_retrieve_can_hold_call_cmd(connection->rfcomm_cid);
                connection->state = HFP_W4_RETRIEVE_CAN_HOLD_CALL;
                break;
            }
            printf("fall through to HFP_LIST_GENERIC_STATUS_INDICATORS (no CAN_HOLD_CALL feature)\n");
            connection->state = HFP_LIST_GENERIC_STATUS_INDICATORS;
        case HFP_LIST_GENERIC_STATUS_INDICATORS:
            if (has_hf_indicators_feature(connection)){
                hfp_hs_list_supported_generic_status_indicators_cmd(connection->rfcomm_cid);
                connection->state = HFP_W4_LIST_GENERIC_STATUS_INDICATORS;
                break;
            } 
            printf("fall through to HFP_ACTIVE (no hf indicators feature)\n");
            connection->state = HFP_ACTIVE;
            //
        case HFP_ACTIVE:
            printf("HFP_ACTIVE\n");
            break;
        case HFP_RETRIEVE_GENERIC_STATUS_INDICATORS:
            hfp_hs_retrieve_supported_generic_status_indicators_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS;
            break;
        case HFP_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
            hfp_hs_list_initital_supported_generic_status_indicators_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
            break;
        default:
            break;
    }
}

void update_command(hfp_connection_t * context){
    context->command = HFP_CMD_NONE;
    if (strncmp((char *)context->line_buffer, HFP_OK, strlen(HFP_OK)) == 0){
        context->command = HFP_CMD_OK;
        printf("Received OK\n");
        return;
    }

    if (strncmp((char *)context->line_buffer, HFP_SUPPORTED_FEATURES, strlen(HFP_SUPPORTED_FEATURES)) == 0){
        printf("Received +BRSF\n");
        context->command = HFP_CMD_SUPPORTED_FEATURES;
        return;
    }

    if (strncmp((char *)context->line_buffer, HFP_INDICATOR, strlen(HFP_INDICATOR)) == 0){
        printf("Received +CIND\n");
        context->command = HFP_CMD_INDICATOR;
        return;
    }


    if (strncmp((char *)context->line_buffer, HFP_AVAILABLE_CODECS, strlen(HFP_AVAILABLE_CODECS)) == 0){
        printf("Received +BAC\n");
        context->command = HFP_CMD_AVAILABLE_CODECS;
        return;
    }

    if (strncmp((char *)context->line_buffer, HFP_ENABLE_INDICATOR_STATUS_UPDATE, strlen(HFP_ENABLE_INDICATOR_STATUS_UPDATE)) == 0){
        printf("Received +CMER\n");
        context->command = HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE;
        return;
    }

    if (strncmp((char *)context->line_buffer, HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES, strlen(HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES)) == 0){
        printf("Received +CHLD\n");
        context->command = HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES;
        return;
    } 

    if (strncmp((char *)context->line_buffer, HFP_GENERIC_STATUS_INDICATOR, strlen(HFP_GENERIC_STATUS_INDICATOR)) == 0){
        printf("Received +BIND\n");
        context->command = HFP_CMD_GENERIC_STATUS_INDICATOR;
        return;
    } 
}

static void hfp_parse2(hfp_connection_t * context, uint8_t byte){
    int i;
    if (byte == ' ') return;
    if ( (byte == '\n' || byte == '\r') && context->parser_state > HFP_PARSER_CMD_SEQUENCE) return;
    
    switch (context->parser_state){
        case HFP_PARSER_CMD_HEADER: // header
            if (byte == ':'){
                context->parser_state = HFP_PARSER_CMD_SEQUENCE;
                context->line_buffer[context->line_size] = 0;
                context->line_size = 0;
                update_command(context);
                return;
            }
            if (byte == '\n' || byte == '\r'){
                // received OK
                context->line_buffer[context->line_size] = 0;
                context->line_size = 0;
                update_command(context);
                return;
            }
            context->line_buffer[context->line_size++] = byte;
            break;

        case HFP_PARSER_CMD_SEQUENCE: // parse comma separated sequence, ignore breacktes
            if (byte == '"'){  // indicators
                context->parser_state = HFP_PARSER_CMD_INDICATOR_NAME;
                context->line_size = 0;
                break;
            } 
        
            if (byte == '(' ){ // tuple separated mit comma
                break;
            }
        
            if (byte == ',' || byte == '\n' || byte == '\r' || byte == ')'){
                context->line_buffer[context->line_size] = 0;
                context->line_size = 0;
                switch (context->state){
                    case HFP_W4_EXCHANGE_SUPPORTED_FEATURES:
                        context->remote_supported_features = atoi((char *)&context->line_buffer[0]);
                        for (i=0; i<16; i++){
                            if (get_bit(context->remote_supported_features,i)){
                                printf("AG supported feature: %s\n", hfp_ag_feature(i));
                            }
                        }
                        context->parser_state = HFP_PARSER_CMD_HEADER;
                        break;
                    case HFP_W4_RETRIEVE_INDICATORS:
                        break;
                    case HFP_W4_RETRIEVE_INDICATORS_STATUS:
                        printf("Indicator with status: %s\n", context->line_buffer);
                        break;
                    case HFP_W4_RETRIEVE_CAN_HOLD_CALL:
                        printf("Support call hold: %s\n", context->line_buffer);
                        break;
                    case HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS: // comma separated ints
                        printf("Generic status indicator: %s\n", context->line_buffer);
                        break;
                    case HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
                        if (!context->generic_indicator_name_read){
                            printf("Generic status indicator: %s, ", context->line_buffer); 
                            context->generic_indicator_name_read = 1;
                        } else {
                            context->generic_indicator_name_read = 0;
                            printf("status %s\n", context->line_buffer); 
                        }
                        break;
                    default:
                        break;
                }
                if (byte == '\n' || byte == '\r'){
                    context->parser_state = HFP_PARSER_CMD_HEADER;
                    break;
                }
                if (byte == ')' && context->state == HFP_W4_RETRIEVE_CAN_HOLD_CALL){ // tuple separated mit comma
                    context->parser_state = HFP_PARSER_CMD_HEADER;
                    break;
                }
                break;
            }
            context->line_buffer[context->line_size++] = byte;
            break;

        case HFP_PARSER_CMD_INDICATOR_NAME: // parse indicator name
            if (byte == '"'){
                context->line_buffer[context->line_size] = 0;
                printf("Indicator %d: %s (", context->remote_indicators_nr, context->line_buffer);
                context->line_size = 0;
                break;
            }
            if (byte == '('){ // parse indicator range
                context->parser_state = HFP_PARSER_CMD_INDICATOR_MIN_RANGE;
                break;
            }
            context->line_buffer[context->line_size++] = byte;
            break;
        case HFP_PARSER_CMD_INDICATOR_MIN_RANGE: 
            if (byte == ',' || byte == '-'){ // end min_range
                context->parser_state = HFP_PARSER_CMD_INDICATOR_MAX_RANGE;
                context->line_buffer[context->line_size] = 0;
                printf("%d, ", atoi((char *)&context->line_buffer[0]));
                context->line_size = 0;
                break;
            }
            // min. range
            context->line_buffer[context->line_size++] = byte;
            break;
        case HFP_PARSER_CMD_INDICATOR_MAX_RANGE:
            if (byte == ')'){ // end max_range
                context->parser_state = HFP_PARSER_CMD_SEQUENCE;

                context->line_buffer[context->line_size] = 0;
                printf("%d)\n", atoi((char *)&context->line_buffer[0]));
                context->line_size = 0;
                context->remote_indicators_nr+=1;
                break;
            }
            // 
            context->line_buffer[context->line_size++] = byte;
            break;
        
    }
}

int wait_for_more_cmd_data(hfp_connection_t * context){
    return context->command == HFP_CMD_NONE;
}

int wait_for_more_data(hfp_connection_t * context){
    return context->parser_state != 0;
}

static void hfp_handle_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hfp_connection_t * context = provide_hfp_connection_context_for_rfcomm_cid(channel);
    if (!context) return;

    packet[size] = 0;
    
    int pos = 0;
    
    context->command = HFP_CMD_NONE;
    while (wait_for_more_cmd_data(context) && pos < size){
        hfp_parse2(context, packet[pos++]);
    }
    if (wait_for_more_cmd_data(context)) return;
    
    // printf("context->command %d == %d OK \n", context->command, HFP_CMD_OK);

    if (context->command == HFP_CMD_OK){
        context->wait_ok = 0;
        switch (context->state){
            case HFP_W4_NOTIFY_ON_CODECS:
                printf("switch on OK: HFP_RETRIEVE_INDICATORS\n");
                context->state = HFP_RETRIEVE_INDICATORS;
                break;
            case HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE:
                printf("switch on OK: HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE\n");
                context->state = HFP_RETRIEVE_CAN_HOLD_CALL;
                break;
            case HFP_W4_LIST_GENERIC_STATUS_INDICATORS:
                printf("switch on OK: HFP_W4_LIST_GENERIC_STATUS_INDICATORS\n");
                context->state = HFP_RETRIEVE_GENERIC_STATUS_INDICATORS;
                break;
            default:
                break;
        }
        return;
    }

    while (wait_for_more_data(context) && pos < size){
        hfp_parse2(context, packet[pos++]);
    }
    if (wait_for_more_data(context)) return;

    switch (context->state){
        case HFP_W4_EXCHANGE_SUPPORTED_FEATURES:
            context->state = HFP_NOTIFY_ON_CODECS;
            break;

        case HFP_W4_RETRIEVE_INDICATORS:
            context->remote_indicators_status = 0;
            context->state = HFP_RETRIEVE_INDICATORS_STATUS; 
            break;
        case HFP_W4_RETRIEVE_INDICATORS_STATUS:
            context->state = HFP_ENABLE_INDICATORS_STATUS_UPDATE;
            break;

        case HFP_W4_RETRIEVE_CAN_HOLD_CALL:
            context->state = HFP_LIST_GENERIC_STATUS_INDICATORS;
            break;
        case HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS:
            context->remote_hf_indicators_status = 0;
            context->state = HFP_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
            break;
        case HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
            printf("Supported initial state generic status indicators \n");
            break;
        default:
            return;
    }
    context->wait_ok = 1;
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
            hfp_handle_hci_event(packet_type, packet, size);
            break;
        default:
            break;
    }
    hfp_run();
}

void hfp_hf_init(uint16_t rfcomm_channel_nr, uint32_t supported_features, uint8_t * codecs, int codecs_nr, uint16_t * indicators, int indicators_nr, uint32_t indicators_status){
    if (codecs_nr > HFP_MAX_NUM_CODECS){
        log_error("hfp_init: codecs_nr (%d) > HFP_MAX_NUM_CODECS (%d)", codecs_nr, HFP_MAX_NUM_CODECS);
        return;
    }
    rfcomm_register_packet_handler(packet_handler);
    hfp_init(rfcomm_channel_nr);
    
    int i;
    
    // connection->codecs = codecs;
    hfp_supported_features = supported_features;
    
    hfp_codecs_nr = codecs_nr;
    for (i=0; i<codecs_nr; i++){
        hfp_codecs[i] = codecs[i];
    }

    hfp_indicators_nr = indicators_nr;
    hfp_indicators_status = indicators_status;
    for (i=0; i<indicators_nr; i++){
        hfp_indicators[i] = indicators[i];
    }
}

void hfp_hf_connect(bd_addr_t bd_addr){
    hfp_connect(bd_addr, SDP_HandsfreeAudioGateway);
}

void hfp_hf_disconnect(bd_addr_t bd_addr){

}