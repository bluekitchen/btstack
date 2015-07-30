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

static uint8_t hfp_indicators_nr = 0;
static uint8_t hfp_indicators[HFP_MAX_NUM_INDICATORS];
static uint8_t hfp_indicators_status;

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

int hfp_ag_exchange_supported_features_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "%s=%d\r\nOK\r\n", HFP_SUPPORTED_FEATURES, hfp_supported_features);
    // printf("exchange_supported_features %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}


int hfp_ag_retrieve_indicators_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "%s=?\r\nOK\r\n", HFP_INDICATOR);
    // printf("retrieve_indicators %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_retrieve_indicators_status_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "%s?\r\nOK\r\n", HFP_INDICATOR);
    // printf("retrieve_indicators_status %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_toggle_indicator_status_update_cmd(uint16_t cid, uint8_t activate){
    char buffer[20];
    sprintf(buffer, "%s=3,0,0,%d\r\nOK\r\n", HFP_ENABLE_INDICATOR_STATUS_UPDATE, activate);
    // printf("toggle_indicator_status_update %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}


int hfp_ag_retrieve_can_hold_call_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "%s=?\r\nOK\r\n", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES);
    // printf("retrieve_can_hold_call %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}


int hfp_ag_retrieve_codec_cmd(uint16_t cid){
    char buffer[30];
    int offset = snprintf(buffer, sizeof(buffer), "%s=", HFP_AVAILABLE_CODECS);
    offset += join(buffer+offset, sizeof(buffer)-offset, hfp_codecs, hfp_codecs_nr);
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, "\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_list_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[30];
    int offset = snprintf(buffer, sizeof(buffer), "%s=", HFP_GENERIC_STATUS_INDICATOR);
    offset += join(buffer+offset, sizeof(buffer)-offset, hfp_indicators, hfp_indicators_nr);
    offset += snprintf(buffer+offset, sizeof(buffer)-offset, "\r\nOK\r\n");
    buffer[offset] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_retrieve_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "%s=?\r\nOK\r\n", HFP_GENERIC_STATUS_INDICATOR); 
    // printf("retrieve_supported_generic_status_indicators %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_ag_list_initital_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "%s?\r\nOK\r\n", HFP_GENERIC_STATUS_INDICATOR);
    // printf("list_initital_supported_generic_status_indicators %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}


static void hfp_run_for_context(hfp_connection_t * connection){
    if (!connection) return;
    // printf("hfp send cmd: context %p, RFCOMM cid %u \n", connection, connection->rfcomm_cid );
    if (!rfcomm_can_send_packet_now(connection->rfcomm_cid)) return;
    
    switch (connection->state){
        case HFP_EXCHANGE_SUPPORTED_FEATURES:
            hfp_ag_exchange_supported_features_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_EXCHANGE_SUPPORTED_FEATURES;
            break;
        case HFP_NOTIFY_ON_CODECS:
            hfp_ag_retrieve_codec_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_NOTIFY_ON_CODECS;
            break;
        case HFP_RETRIEVE_INDICATORS:
            hfp_ag_retrieve_indicators_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_RETRIEVE_INDICATORS;
            break;
        case HFP_RETRIEVE_INDICATORS_STATUS:
            hfp_ag_retrieve_indicators_status_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_RETRIEVE_INDICATORS_STATUS;
            break;
        case HFP_ENABLE_INDICATORS_STATUS_UPDATE:
            hfp_ag_toggle_indicator_status_update_cmd(connection->rfcomm_cid, 1);
            connection->state = HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE;
            break;
        case HFP_RETRIEVE_CAN_HOLD_CALL:
            hfp_ag_retrieve_can_hold_call_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_RETRIEVE_CAN_HOLD_CALL;
            break;
        case HFP_LIST_GENERIC_STATUS_INDICATORS:
            hfp_ag_list_supported_generic_status_indicators_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_LIST_GENERIC_STATUS_INDICATORS;
            break;
        case HFP_RETRIEVE_GENERIC_STATUS_INDICATORS:
            hfp_ag_retrieve_supported_generic_status_indicators_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS;
            break;
        case HFP_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
            hfp_ag_list_initital_supported_generic_status_indicators_cmd(connection->rfcomm_cid);
            connection->state = HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
            break;
        case HFP_ACTIVE:
            printf("HFP_ACTIVE\n");
            break;
    
        default:
            break;
    }
}

void update_command(hfp_connection_t * context){
    context->command = HFP_CMD_NONE; 
    
    if (strncmp((char *)context->line_buffer+2, HFP_SUPPORTED_FEATURES, strlen(HFP_SUPPORTED_FEATURES)) == 0){
        printf("Received AT+BRSF\n");
        context->command = HFP_CMD_SUPPORTED_FEATURES;
        return;
    }

    if (strncmp((char *)context->line_buffer+2, HFP_INDICATOR, strlen(HFP_INDICATOR)) == 0){
        printf("Received AT+CIND\n");
        context->command = HFP_CMD_INDICATOR;
        return;
    }


    if (strncmp((char *)context->line_buffer+2, HFP_AVAILABLE_CODECS, strlen(HFP_AVAILABLE_CODECS)) == 0){
        printf("Received AT+BAC\n");
        context->command = HFP_CMD_AVAILABLE_CODECS;
        return;
    }

    if (strncmp((char *)context->line_buffer+2, HFP_ENABLE_INDICATOR_STATUS_UPDATE, strlen(HFP_ENABLE_INDICATOR_STATUS_UPDATE)) == 0){
        printf("Received AT+CMER\n");
        context->command = HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE;
        return;
    }

    if (strncmp((char *)context->line_buffer+2, HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES, strlen(HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES)) == 0){
        printf("Received AT+CHLD\n");
        context->command = HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES;
        return;
    } 

    if (strncmp((char *)context->line_buffer+2, HFP_GENERIC_STATUS_INDICATOR, strlen(HFP_GENERIC_STATUS_INDICATOR)) == 0){
        printf("Received AT+BIND\n");
        context->command = HFP_CMD_GENERIC_STATUS_INDICATOR;
        return;
    } 
}

static void hfp_parse(hfp_connection_t * context, uint8_t byte){
    int i;
    if (byte == ' ') return;
    /*if ( (byte == '\n' || byte == '\r') && context->parser_state > HFP_PARSER_CMD_SEQUENCE) return;
    
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
                        context->command = HFP_CMD_NONE;
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
                        printf("Generic status indicator: %s, ", context->line_buffer); 
                        context->parser_state = HFP_PARSER_CMD_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
                        break;
                    default:
                        break;
                }
                if (byte == '\n' || byte == '\r'){
                    context->command = HFP_CMD_NONE;
                    context->parser_state = HFP_PARSER_CMD_HEADER;
                    break;
                }
                if (byte == ')' && context->state == HFP_W4_RETRIEVE_CAN_HOLD_CALL){ // tuple separated mit comma
                    context->command = HFP_CMD_NONE;
                    context->parser_state = HFP_PARSER_CMD_HEADER;
                    break;
                }
                break;
            }
            context->line_buffer[context->line_size++] = byte;
            break;
        case HFP_PARSER_CMD_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
            if (byte == '\n' || byte == '\r'){
                context->line_buffer[context->line_size] = 0;
                context->line_size = 0;
                context->command = HFP_CMD_NONE;
                context->parser_state = HFP_PARSER_CMD_HEADER;
                printf("status %s [0-dissabled, 1-enabled]\n", context->line_buffer);
                break;
            }
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
    }*/
}


void handle_switch_on_ok(hfp_connection_t *context){
    printf("handle switch on OK\n");
    switch (context->state){
        default:
            break;
    }
    // done
    context->command = HFP_CMD_NONE;
}

static void hfp_handle_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hfp_connection_t * context = provide_hfp_connection_context_for_rfcomm_cid(channel);
    if (!context) return;

    packet[size] = 0;
    int pos;
    for (pos = 0; pos < size ; pos++){
        hfp_parse(context, packet[pos]);

        // trigger next action after CMD received
        if (context->command != HFP_CMD_OK) continue;
        handle_switch_on_ok(context);
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
            hfp_handle_hci_event(packet_type, packet, size);
            break;
        default:
            break;
        }
    hfp_run();
}

void hfp_ag_init(uint16_t rfcomm_channel_nr, uint32_t supported_features, uint8_t * codecs, int codecs_nr, uint16_t * indicators, int indicators_nr, uint32_t indicators_status){
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

    hfp_indicators_nr = indicators_nr;
    hfp_indicators_status = indicators_status;
    for (i=0; i<indicators_nr; i++){
        hfp_indicators[i] = indicators[i];
    }
}

void hfp_ag_connect(bd_addr_t bd_addr){
    hfp_connect(bd_addr, SDP_Handsfree);
}

void hfp_ag_disconnect(bd_addr_t bd_addr){

}

