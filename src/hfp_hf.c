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
static uint16_t hfp_supported_features = HFP_Default_HF_Supported_Features;
static uint8_t hfp_codecs_nr = 0;
static uint8_t hfp_codecs[HFP_MAX_NUM_CODECS];

static uint8_t hfp_indicators_nr = 0;
static uint8_t hfp_indicators[HFP_MAX_NUM_INDICATORS];
static uint8_t hfp_indicators_status;


static void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void hfp_hf_create_service(uint8_t * service, int rfcomm_channel_nr, const char * name, uint16_t supported_features){
    if (!name){
        name = default_hfp_hf_service_name;
    }
    hfp_create_service(service, SDP_Handsfree, rfcomm_channel_nr, name, supported_features);
}


static int get_bit(uint16_t bitmap, int position){
    return (bitmap >> position) & 1;
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
    sprintf(buffer, "AT%s=%d\r\n", HFP_Supported_Features, hfp_supported_features);
    // printf("exchange_supported_features %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_hs_retrieve_codec_cmd(uint16_t cid){
    char buffer[30];
    int buffer_offset = sprintf(buffer, "AT%s=", HFP_Available_Codecs);
    join(buffer, sizeof(buffer), buffer_offset, hfp_codecs, hfp_codecs_nr, 1);
    // printf("retrieve_codec %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}


int hfp_hs_retrieve_indicators_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s=?\r\n", HFP_Indicator);
    // printf("retrieve_indicators %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_hs_retrieve_indicators_status_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s?\r\n", HFP_Indicator);
    // printf("retrieve_indicators_status %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_hs_toggle_indicator_status_update_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s\r\n", HFP_Enable_Indicator_Status_Update);
    // printf("toggle_indicator_status_update %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_hs_retrieve_can_hold_call_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s\r\n", HFP_Support_Call_Hold_And_Multiparty_Services);
    // printf("retrieve_can_hold_call %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}


int hfp_hs_list_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[30];
    int buffer_offset = sprintf(buffer, "AT%s=", HFP_Generic_Status_Indicator); 
    join(buffer, sizeof(buffer), buffer_offset, hfp_indicators, hfp_indicators_nr, 2); 
    // printf("list_supported_generic_status_indicators %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_hs_retrieve_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s=?\r\rn", HFP_Generic_Status_Indicator); 
    // printf("retrieve_supported_generic_status_indicators %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

int hfp_hs_list_initital_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[20];
    sprintf(buffer, "AT%s?\r\n", HFP_Generic_Status_Indicator);
    // printf("list_initital_supported_generic_status_indicators %s\n", buffer);
    return send_str_over_rfcomm(cid, buffer);
}

// TODO: +CIEV service, call, call setup change => new indicator value
// TODO: AT+CMER => disable ind. status update

int has_call_waiting_and_3way_calling_feature(hfp_connection_t * connection){
    return get_bit(hfp_supported_features,1) && get_bit(connection->remote_supported_features,0);
}

int has_codec_negotiation_feature(hfp_connection_t * connection){
    return get_bit(hfp_supported_features,7) && get_bit(connection->remote_supported_features,9);
}

int has_hf_indicators_feature(hfp_connection_t * connection){
    return get_bit(hfp_supported_features,8) && get_bit(connection->remote_supported_features,10);
}

static void hfp_run_for_context(hfp_connection_t * connection){
    if (!connection) return;
    if (!rfcomm_can_send_packet_now(connection->rfcomm_cid)) return;

    int err = 0;
    switch (connection->state){
        case HFP_EXCHANGE_SUPPORTED_FEATURES:
            err = hfp_hs_exchange_supported_features_cmd(connection->rfcomm_cid);
            if (!err) connection->state = HFP_W4_EXCHANGE_SUPPORTED_FEATURES;
            break;
        case HFP_NOTIFY_ON_CODECS:
            err = hfp_hs_retrieve_codec_cmd(connection->rfcomm_cid);
            if (!err) connection->state = HFP_W4_NOTIFY_ON_CODECS;
            break;
        case HFP_RETRIEVE_INDICATORS:
            err = hfp_hs_retrieve_indicators_cmd(connection->rfcomm_cid);
            if (!err) connection->state = HFP_W4_RETRIEVE_INDICATORS;
            break;
        case HFP_RETRIEVE_INDICATORS_STATUS:
            err = hfp_hs_retrieve_indicators_cmd(connection->rfcomm_cid);
            if (!err) connection->state = HFP_W4_EXCHANGE_SUPPORTED_FEATURES;
            break;
        case HFP_ENABLE_INDICATORS_STATUS_UPDATE:
            if (connection->remote_indicators_update_enabled == 0){
                err = hfp_hs_toggle_indicator_status_update_cmd(connection->rfcomm_cid);
                if (!err) connection->state = HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE;
            }
            break;
        case HFP_RETRIEVE_CAN_HOLD_CALL:
            err = hfp_hs_retrieve_can_hold_call_cmd(connection->rfcomm_cid);
            if (!err) connection->state = HFP_W4_RETRIEVE_CAN_HOLD_CALL;
            break;
        case HFP_LIST_GENERIC_STATUS_INDICATORS:
            err = hfp_hs_list_supported_generic_status_indicators_cmd(connection->rfcomm_cid);
            if (!err) connection->state = HFP_W4_LIST_GENERIC_STATUS_INDICATORS;
            break;
        case HFP_RETRIEVE_GENERIC_STATUS_INDICATORS:
            err = hfp_hs_retrieve_supported_generic_status_indicators_cmd(connection->rfcomm_cid);
            if (!err) connection->state = HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS;
            break;
        case HFP_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
            err = hfp_hs_list_initital_supported_generic_status_indicators_cmd(connection->rfcomm_cid);
            if (!err) connection->state = HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
            break;
        default:
            break;
    }
}

void hfp_parse_indicators(hfp_connection_t * context, uint8_t *packet, uint16_t size){
    char indicator[10];
    char min_range[3];
    char max_range[3];
    int i, pos;
    int index = 0;
    int state = 0;
    
    for (pos = 0; pos < size; pos++){
        uint8_t byte = packet[pos];

        switch (state){
            case 0: // pre-indicator
                if (byte != '"') break;
                state++;
                i = 0;
                break;
            case 1: // indicator
                if (byte == '"'){
                    state++;
                    indicator[i] = 0;
                    break;
                }
                indicator[i++] = byte;
                break;
            case 2: // pre-range
                if (byte != '(') break;
                state++;
                i = 0;
                break;
            case 3: // min-range
                if (byte == ','){
                    state++;
                    min_range[i] = 0;
                    i = 0;
                    break;
                }
                min_range[i++] = byte;
                break;
            case 4:
                if (byte == ')'){
                    state = 0;
                    max_range[i] = 0;
                    printf("Indicator %d: %s, range [%d, %d] \n", index, indicator, atoi(min_range), atoi(max_range));
                    index++;
                    i = 0;
                    break;
                }
                max_range[i++] = byte;
                break;
            default:
                break;
        }
    }
}

hfp_connection_t * handle_message(hfp_connection_t * context, uint8_t *packet, uint16_t size){
    int offset = 0;
    if (context->wait_ok){
        if (strncmp((char *)packet, HFP_OK, strlen(HFP_OK)) == 0){
            printf("Parsed %s\n", HFP_OK);
            switch (context->state){
                case HFP_W4_NOTIFY_ON_CODECS:
                    context->state = HFP_RETRIEVE_INDICATORS;
                    break;
                case HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE:
                    if (has_call_waiting_and_3way_calling_feature(context)){
                        context->state = HFP_RETRIEVE_CAN_HOLD_CALL;
                        break;
                    }

                    if (has_hf_indicators_feature(context)){
                        context->state = HFP_LIST_GENERIC_STATUS_INDICATORS;
                        break;
                    }
                    break;

                case HFP_W4_LIST_GENERIC_STATUS_INDICATORS:
                    context->state = HFP_RETRIEVE_GENERIC_STATUS_INDICATORS;
                    break;

                case HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
                    context->state = HFP_ACTIVE;
                    printf(" state Active! \n");
                    break;
                default:
                    break;
            }
            context->wait_ok = 0;
            return context;
        }
    }

    if (strncmp((char *)packet, HFP_Supported_Features, strlen(HFP_Supported_Features)) == 0){
        offset = strlen(HFP_Supported_Features) + 1; // +1 for =
        context->remote_supported_features = atoi((char*)&packet[offset]);
        printf("Parsed %s: %d (expected 224)\n", HFP_Supported_Features, context->remote_supported_features);
        if (has_codec_negotiation_feature(context)){
            context->state = HFP_NOTIFY_ON_CODECS;
        } else {
            context->state = HFP_RETRIEVE_INDICATORS;
        }
        context->wait_ok = 1;
        return context;
    }

    if (strncmp((char *)packet, HFP_Indicator, strlen(HFP_Indicator)) == 0){
        // https://www.bluetooth.org/en-us/specification/assigned-numbers/hands-free-profile
        /* 
         * 0x01 Enhanced Safety​, on/off
         * 0x02 Battery Level,   0-100
         */
       // printf("Parsed %s, expected  ("service",(0,1)),("call",(0,1)),("callsetup",(0,3)),("callheld",(0,2)),("signal",(0,5)),("roam",(0,1)),("battchg",(0,5))\n", HFP_Indicator);
        
        offset = strlen(HFP_Indicator) + 1;
        char * token = strtok((char*)&packet[offset], ",");
        int pos = 0;
        switch (context->state){
            case HFP_W4_RETRIEVE_INDICATORS:
                hfp_parse_indicators(context, &packet[offset], size-offset);
                context->remote_indicators_status = 0;
                context->state = HFP_RETRIEVE_INDICATORS_STATUS; 
                break;
            case HFP_W4_RETRIEVE_INDICATORS_STATUS:
                while (token){
                    printf("%s\n", token);
                    store_bit(context->remote_indicators_status, pos, atoi(token));
                    pos++;
                    token = strtok(NULL, ",");
                }
                context->state = HFP_ENABLE_INDICATORS_STATUS_UPDATE;
                break;
            default:
                break;
        }
        context->wait_ok = 1;
        return context;
    }

    if (strncmp((char *)packet, HFP_Support_Call_Hold_And_Multiparty_Services, strlen(HFP_Support_Call_Hold_And_Multiparty_Services)) == 0){
        offset = strlen(HFP_Support_Call_Hold_And_Multiparty_Services) + 1; // +1 for =
        printf("AT+CHLD %s, offset %u, size %u\n", (char *)&packet[offset], offset, size);
        
        context->wait_ok = 1;
        return context;
    } 

    if (strncmp((char *)packet, HFP_Generic_Status_Indicator, strlen(HFP_Generic_Status_Indicator)) == 0){
        offset = strlen(HFP_Generic_Status_Indicator) + 1; // +1 for =
        char * token = strtok((char*)&packet[offset], ",");
       
        int pos = 0;
        switch (context->state){
            case HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS:
                while (token){
                    printf("%s\n", token);
                    context->remote_hf_indicators[pos++] = atoi(token);
                    token = strtok(NULL, ",");
                }
                context->remote_hf_indicators_nr = pos;
                context->remote_hf_indicators_status = 0;
                context->state = HFP_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
                break;
            case HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS:{
                uint16_t indicator = atoi(token);
                int status = atoi(strtok(NULL, ","));
                if (!status) break;

                int i;
                for (i=0; i<context->remote_hf_indicators_nr; i++){
                    if (context->remote_hf_indicators[i] == indicator){
                       store_bit(context->remote_hf_indicators_status, i, atoi(token));
                    }
                }
                break;
            }
            default:
                break;
        }
        context->wait_ok = 1;
        return context;
    }

    return context;
}

static void hfp_parse(hfp_connection_t * context, uint8_t byte){
    if (byte != '\n' && byte != '\r'){
        context->line_buffer[context->line_size] = byte;
        context->line_size++;
        return;
    }

    context->line_buffer[context->line_size] = '\0';
    if (context->line_size > 0){
        handle_message(context, context->line_buffer, context->line_size);
    }
    context->line_size = 0;
}

static void hfp_handle_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hfp_connection_t * context = provide_hfp_connection_context_for_rfcomm_cid(channel);
    if (!context) return;

    packet[size] = 0;
    printf("Received %s\n", packet);

    int i;
    for (i=0;i<size;i++){
        hfp_parse(context, packet[i]);
    }

}

static void hfp_run(){
    linked_item_t *it;
    for (it = hfp_get_connections(); it ; it = it->next){
        hfp_connection_t * connection = (hfp_connection_t *) it;
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