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
#include <inttypes.h>

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "sdp_query_rfcomm.h"
#include "sdp.h"
#include "debug.h"

#define HFP_HF_FEATURES_SIZE 10
#define HFP_AG_FEATURES_SIZE 12

static const char * hfp_hf_features[] = {
    "EC and/or NR function",
    "Three-way calling",
    "CLI presentation capability",
    "Voice recognition activation",
    "Remote volume control",
    "Enhanced call status",
    "Enhanced call control",
    "Codec negotiation",
    "HF Indicators",
    "eSCO S4 (and T2) Settings Supported",
    "Reserved for future definition"
};

static const char * hfp_ag_features[] = {
    "Three-way calling",
    "EC and/or NR function",
    "Voice recognition function",
    "In-band ring tone capability",
    "Attach a number to a voice tag",
    "Ability to reject a call",
    "Enhanced call status",
    "Enhanced call control",
    "Extended Error Result Codes",
    "Codec negotiation",
    "HF Indicators",
    "eSCO S4 (and T2) Settings Supported",
    "Reserved for future definition"
};

static int hfp_hf_indicators_nr = 0;
static hfp_generic_status_indicators_t hfp_hf_indicators[HFP_MAX_NUM_HF_INDICATORS];

static linked_list_t hfp_connections = NULL;

hfp_generic_status_indicators_t * get_hfp_generic_status_indicators(){
    return (hfp_generic_status_indicators_t *) &hfp_hf_indicators;
}

int get_hfp_generic_status_indicators_nr(){
    return hfp_hf_indicators_nr;
}

void set_hfp_generic_status_indicators(hfp_generic_status_indicators_t * indicators, int indicator_nr){
    if (indicator_nr > HFP_MAX_NUM_HF_INDICATORS) return;
    hfp_hf_indicators_nr = indicator_nr;
    memcpy(hfp_hf_indicators, indicators, indicator_nr * sizeof(hfp_generic_status_indicators_t));
}

const char * hfp_hf_feature(int index){
    if (index > HFP_HF_FEATURES_SIZE){
        return hfp_hf_features[HFP_HF_FEATURES_SIZE];
    }
    return hfp_hf_features[index];
}

const char * hfp_ag_feature(int index){
    if (index > HFP_AG_FEATURES_SIZE){
        return hfp_ag_features[HFP_AG_FEATURES_SIZE];
    }
    return hfp_ag_features[index];
}

int send_str_over_rfcomm(uint16_t cid, char * command){
    // if (!rfcomm_can_send_packet_now(cid)) return 1;
    int err = rfcomm_send_internal(cid, (uint8_t*) command, strlen(command));
    if (err){
        printf("rfcomm_send_internal -> error 0X%02x", err);
    } 
    return err;
}

void hfp_set_codec(hfp_connection_t * context, uint8_t *packet, uint16_t size){
    // parse available codecs
    int pos = 0;
    int i;
    for (i=0; i<size; i++){
        pos+=8;
        if (packet[pos] > context->negotiated_codec){
            context->negotiated_codec = packet[pos];
        }
    }
    printf("Negotiated Codec 0x%02x\n", context->negotiated_codec);
}

// UTILS
int get_bit(uint16_t bitmap, int position){
    return (bitmap >> position) & 1;
}

int store_bit(uint32_t bitmap, int position, uint8_t value){
    if (value){
        bitmap |= 1 << position;
    } else {
        bitmap &= ~ (1 << position);
    }
    return bitmap;
}

int join(char * buffer, int buffer_size, uint8_t * values, int values_nr){
    if (buffer_size < values_nr * 3) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < values_nr-1; i++) {
      offset += snprintf(buffer+offset, buffer_size-offset, "%d,", values[i]); // puts string into buffer
    }
    if (i<values_nr){
        offset += snprintf(buffer+offset, buffer_size-offset, "%d", values[i]);
    }
    return offset;
}

int join_bitmap(char * buffer, int buffer_size, uint32_t values, int values_nr){
    if (buffer_size < values_nr * 3) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < values_nr-1; i++) {
      offset += snprintf(buffer+offset, buffer_size-offset, "%d,", get_bit(values,i)); // puts string into buffer
    }
    if (i<values_nr){
        offset += snprintf(buffer+offset, buffer_size-offset, "%d", get_bit(values,i));
    }
    return offset;
}

void hfp_emit_event(hfp_callback_t callback, uint8_t event_subtype, uint8_t value){
    if (!callback) return;
    uint8_t event[4];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    event[3] = value; // status 0 == OK
    (*callback)(event, sizeof(event));
}


linked_list_t * hfp_get_connections(){
    return (linked_list_t *) &hfp_connections;
} 

hfp_connection_t * get_hfp_connection_context_for_rfcomm_cid(uint16_t cid){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        if (connection->rfcomm_cid == cid){
            return connection;
        }
    }
    return NULL;
}

hfp_connection_t * get_hfp_connection_context_for_bd_addr(bd_addr_t bd_addr){
    linked_list_iterator_t it;  
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        if (memcmp(connection->remote_addr, bd_addr, 6) == 0) {
            return connection;
        }
    }
    return NULL;
}

static hfp_connection_t * get_hfp_connection_context_for_handle(uint16_t handle){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, hfp_get_connections());
    while (linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)linked_list_iterator_next(&it);
        if (connection->con_handle == handle){
            return connection;
        }
    }
    return NULL;
}

static hfp_connection_t * create_hfp_connection_context(){
    hfp_connection_t * context = btstack_memory_hfp_connection_get();
    if (!context) return NULL;
    // init state
    memset(context,0, sizeof(hfp_connection_t));

    context->state = HFP_IDLE;
    context->parser_state = HFP_PARSER_CMD_HEADER;
    context->command = HFP_CMD_NONE;
    context->negotiated_codec = HFP_CODEC_CVSD;
    
    context->enable_status_update_for_ag_indicators = 0xFF;
    context->change_enable_status_update_for_individual_ag_indicators = 0xFF;

    context->generic_status_indicators_nr = hfp_hf_indicators_nr;
    memcpy(context->generic_status_indicators, hfp_hf_indicators, hfp_hf_indicators_nr * sizeof(hfp_generic_status_indicators_t));

    linked_list_add(&hfp_connections, (linked_item_t*)context);
    return context;
}

static void remove_hfp_connection_context(hfp_connection_t * context){
    linked_list_remove(&hfp_connections, (linked_item_t*)context);   
}

hfp_connection_t * provide_hfp_connection_context_for_bd_addr(bd_addr_t bd_addr){
    hfp_connection_t * context = get_hfp_connection_context_for_bd_addr(bd_addr);
    if (context) return  context;
    context = create_hfp_connection_context();
    memcpy(context->remote_addr, bd_addr, 6);
    return context;
}


/* @param suported_features
 * HF bit 0: EC and/or NR function (yes/no, 1 = yes, 0 = no)
 * HF bit 1: Call waiting or three-way calling(yes/no, 1 = yes, 0 = no)
 * HF bit 2: CLI presentation capability (yes/no, 1 = yes, 0 = no)
 * HF bit 3: Voice recognition activation (yes/no, 1= yes, 0 = no)
 * HF bit 4: Remote volume control (yes/no, 1 = yes, 0 = no)
 * HF bit 5: Wide band speech (yes/no, 1 = yes, 0 = no)
 */
 /* Bit position:
 * AG bit 0: Three-way calling (yes/no, 1 = yes, 0 = no)
 * AG bit 1: EC and/or NR function (yes/no, 1 = yes, 0 = no)
 * AG bit 2: Voice recognition function (yes/no, 1 = yes, 0 = no)
 * AG bit 3: In-band ring tone capability (yes/no, 1 = yes, 0 = no)
 * AG bit 4: Attach a phone number to a voice tag (yes/no, 1 = yes, 0 = no)
 * AG bit 5: Wide band speech (yes/no, 1 = yes, 0 = no)
 */


void hfp_create_sdp_record(uint8_t * service, uint16_t service_uuid, int rfcomm_channel_nr, const char * name, uint16_t supported_features){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle);
    de_add_number(service, DE_UINT, DE_SIZE_32, 0x10001);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
    attribute = de_push_sequence(service);
    {
        //  "UUID for Service"
        de_add_number(attribute, DE_UUID, DE_SIZE_16, service_uuid);
        de_add_number(attribute, DE_UUID, DE_SIZE_16, SDP_GenericAudio);
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ProtocolDescriptorList);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, SDP_L2CAPProtocol);
        }
        de_pop_sequence(attribute, l2cpProtocol);
        
        uint8_t* rfcomm = de_push_sequence(attribute);
        {
            de_add_number(rfcomm,  DE_UUID, DE_SIZE_16, SDP_RFCOMMProtocol);  // rfcomm_service
            de_add_number(rfcomm,  DE_UINT, DE_SIZE_8,  rfcomm_channel_nr);  // rfcomm channel
        }
        de_pop_sequence(attribute, rfcomm);
    }
    de_pop_sequence(service, attribute);

    
    // 0x0005 "Public Browse Group"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BrowseGroupList); // public browse group
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, SDP_PublicBrowseGroup);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BluetoothProfileDescriptorList);
    attribute = de_push_sequence(service);
    {
        uint8_t *sppProfile = de_push_sequence(attribute);
        {
            de_add_number(sppProfile,  DE_UUID, DE_SIZE_16, SDP_Handsfree); 
            de_add_number(sppProfile,  DE_UINT, DE_SIZE_16, 0x0107); // Verision 1.7
        }
        de_pop_sequence(attribute, sppProfile);
    }
    de_pop_sequence(service, attribute);

    // 0x0100 "Service Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);
    
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_features);
}

static hfp_connection_t * connection_doing_sdp_query = NULL;
static void handle_query_rfcomm_event(sdp_query_event_t * event, void * context){
    sdp_query_rfcomm_service_event_t * ve;
    sdp_query_complete_event_t * ce;
    hfp_connection_t * connection = connection_doing_sdp_query;
    
    if ( connection->state != HFP_W4_SDP_QUERY_COMPLETE) return;
    
    switch (event->type){
        case SDP_QUERY_RFCOMM_SERVICE:
            ve = (sdp_query_rfcomm_service_event_t*) event;
            if (!connection) {
                log_error("handle_query_rfcomm_event alloc connection for RFCOMM port %u failed", ve->channel_nr);
                return;
            }
            connection->rfcomm_channel_nr = ve->channel_nr;
            break;
        case SDP_QUERY_COMPLETE:
            connection_doing_sdp_query = NULL;
            ce = (sdp_query_complete_event_t*) event;
            
            if (connection->rfcomm_channel_nr > 0){
                connection->state = HFP_W4_RFCOMM_CONNECTED;
                log_info("HFP: SDP_QUERY_COMPLETE context %p, addr %s, state %d", connection, bd_addr_to_str( connection->remote_addr),  connection->state);
                rfcomm_create_channel_internal(NULL, connection->remote_addr, connection->rfcomm_channel_nr); 
                break;
            }
            log_info("rfcomm service not found, status %u.", ce->status);
            break;
        default:
            break;
    }
}

void hfp_handle_hci_event(hfp_callback_t callback, uint8_t packet_type, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    uint16_t rfcomm_cid, handle;
    hfp_connection_t * context = NULL;

    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                printf("BTstack activated, get started .\n");
            }
            break;

        case HCI_EVENT_PIN_CODE_REQUEST:
            // inform about pin code request
            printf("Pin code request - using '0000'\n\r");
            bt_flip_addr(event_addr, &packet[2]);
            hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
            break;
        
        case RFCOMM_EVENT_INCOMING_CONNECTION:
            // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
            bt_flip_addr(event_addr, &packet[2]); 
            context = get_hfp_connection_context_for_bd_addr(event_addr);
            
            if (!context || context->state != HFP_IDLE) return;

            context->rfcomm_cid = READ_BT_16(packet, 9);
            context->state = HFP_W4_RFCOMM_CONNECTED;
            printf("RFCOMM channel %u requested for %s\n", context->rfcomm_cid, bd_addr_to_str(context->remote_addr));
            rfcomm_accept_connection_internal(context->rfcomm_cid);
            break;

        case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
            // data: event(8), len(8), status (8), address (48), handle(16), server channel(8), rfcomm_cid(16), max frame size(16)
            printf("RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);
            bt_flip_addr(event_addr, &packet[3]); 
            context = get_hfp_connection_context_for_bd_addr(event_addr);
            if (!context || context->state != HFP_W4_RFCOMM_CONNECTED) return;
            
            if (packet[2]) {
                hfp_emit_event(callback, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED, packet[2]);
                remove_hfp_connection_context(context);
            } else {
                context->con_handle = READ_BT_16(packet, 9);
                context->rfcomm_cid = READ_BT_16(packet, 12);
                uint16_t mtu = READ_BT_16(packet, 14);
                printf("RFCOMM channel open succeeded. Context %p, RFCOMM Channel ID 0x%02x, max frame size %u\n", context, context->rfcomm_cid, mtu);
                        
                switch (context->state){
                    case HFP_W4_RFCOMM_CONNECTED:
                        context->state = HFP_EXCHANGE_SUPPORTED_FEATURES;
                        break;
                    case HFP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN:
                        context->state = HFP_W2_DISCONNECT_RFCOMM;
                        printf("Shutting down RFCOMM.\n");
                        break;
                    default:
                        break;
                }
            }
            break;
        
        case RFCOMM_EVENT_CHANNEL_CLOSED:
            rfcomm_cid = READ_BT_16(packet,2);
            context = get_hfp_connection_context_for_rfcomm_cid(rfcomm_cid);
            if (!context) break;
            remove_hfp_connection_context(context);
            hfp_emit_event(callback, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED, 0);
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            handle = READ_BT_16(packet,3);
            context = get_hfp_connection_context_for_handle(handle);
            if (!context) break;
            hfp_emit_event(callback, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED, packet[2]);
            remove_hfp_connection_context(context);
            break;

        default:
            break;
    }
}

void update_command(hfp_connection_t * context);

uint32_t fromBinary(char *s) {
    return (uint32_t) strtol(s, NULL, 2);
}

void hfp_parse(hfp_connection_t * context, uint8_t byte){
    int i;
    int value;

    if (byte == ' ') return;
    if (context->line_size == 0 && (byte == '\n' || byte == '\r')) return;
    if ((byte == '\n' || byte == '\r') && (context->parser_state > HFP_PARSER_CMD_SEQUENCE && context->parser_state != HFP_PARSER_CMD_INITITAL_STATE_GENERIC_STATUS_INDICATORS) )return;
   
    switch (context->parser_state){
        case HFP_PARSER_CMD_HEADER: // header
            if (byte == ':' || byte == '='){
                context->parser_state = HFP_PARSER_CMD_SEQUENCE;
                context->line_buffer[context->line_size] = 0;
                context->line_size = 0;
                update_command(context);
                return;
            }
            if (byte == '\n' || byte == '\r'){
                context->line_buffer[context->line_size] = 0;
                if (context->line_size == 2){
                    update_command(context);
                }
                context->line_size = 0;
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
                
                switch (context->command){
                    case HFP_CMD_SUPPORTED_FEATURES:
                        context->remote_supported_features = 0;
                        for (i=0; i<16; i++){
                            if (context->line_buffer[i] == '1'){
                                context->remote_supported_features = store_bit(context->remote_supported_features,15-i,1);
                            } 
                        }
                        printf("Received supported feature %d\n", context->remote_supported_features);
                        context->parser_state = HFP_PARSER_CMD_HEADER;
                        break;
                    case HFP_CMD_AVAILABLE_CODECS:
                        // printf("Received codec %s\n", context->line_buffer);
                        // context->remote_codecs[context->remote_codecs_nr] = (uint16_t)atoi((char*)context->line_buffer);
                        context->remote_codecs[context->parser_item_index] = (uint16_t)atoi((char*)context->line_buffer);
                        context->parser_item_index++;
                        context->remote_codecs_nr = context->parser_item_index;
                        break;
                    case HFP_CMD_INDICATOR_STATUS:
                        // printf("Indicator %d with status: %s\n", context->parser_item_index+1, context->line_buffer);
                        context->ag_indicators[context->parser_item_index].status = atoi((char *) context->line_buffer);
                        context->parser_item_index++;
                        break;
                    case HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE:
                        if (context->parser_item_index == 3){
                            printf("Enable indicators: %s\n", context->line_buffer);
                            value = atoi((char *)&context->line_buffer[0]);
                            context->enable_status_update_for_ag_indicators = (uint8_t) value;
                        } else {
                            context->parser_item_index++;
                        }
                        break;
                    case HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES:
                        //  printf("Support call hold: %s\n", context->line_buffer);
                        if (context->line_size > 2 ) break;
                        strcpy((char *)context->remote_call_services[context->remote_call_services_nr].name,  (char *)context->line_buffer);
                        context->remote_call_services_nr++;
                        break;
                    case HFP_CMD_GENERIC_STATUS_INDICATOR:
                        context->generic_status_indicators[context->parser_item_index].uuid = (uint16_t)atoi((char*)context->line_buffer);
                        context->parser_item_index++;
                        context->generic_status_indicators_nr = context->parser_item_index;
                        break;
                    case HFP_CMD_GENERIC_STATUS_INDICATOR_STATE:
                        // HF parses inital AG gen. ind. state
                        printf("HFP_CMD_GENERIC_STATUS_INDICATOR_STATE %s, %d\n", context->line_buffer, context->command);
                        context->parser_state = HFP_PARSER_CMD_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
                        context->generic_status_indicator_state_index = (uint8_t)atoi((char*)context->line_buffer);
                        break;
                    case HFP_CMD_ENABLE_INDIVIDUAL_INDICATOR_STATUS_UPDATE:
                        // AG parses new gen. ind. state
                        value = atoi((char *)&context->line_buffer[0]);
                        context->generic_status_indicator_state_index = 255;
                        // TODO: match on uuid or index?
                        for (i = 0; i < hfp_hf_indicators_nr; i++){
                            if (hfp_hf_indicators[i].uuid == value ){
                                context->generic_status_indicator_state_index = i;
                                continue;
                            }
                        }
                        if (context->generic_status_indicator_state_index >= 0 && context->generic_status_indicator_state_index < hfp_hf_indicators_nr){
                            printf("Set state of the indicator wiht index %d, ", context->generic_status_indicator_state_index);
                            context->parser_state = HFP_PARSER_CMD_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
                            break;
                        }
                        context->parser_state = HFP_PARSER_CMD_HEADER;
                        break;
                    default:
                        break;
                }
                context->line_size = 0;

                if (byte == '\n' || byte == '\r'){
                    context->parser_state = HFP_PARSER_CMD_HEADER;
                    context->parser_item_index = 0;
                    break;
                }
                if (byte == ')' && context->command == HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES){ // tuple separated mit comma
                    context->parser_state = HFP_PARSER_CMD_HEADER;
                    context->parser_item_index = 0;
                    break;
                }
                break;
            }
            context->line_buffer[context->line_size++] = byte;
            break;
        case HFP_PARSER_CMD_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
            context->line_buffer[context->line_size] = 0;
            if (byte == ',') break;
            if (byte == '\n' || byte == '\r'){
                context->line_buffer[context->line_size] = 0;
                context->line_size = 0;
                context->parser_state = HFP_PARSER_CMD_HEADER;
                printf("to %s [0-dissabled, 1-enabled]\n", context->line_buffer);
                // HF stores inital AG gen. ind. state
                // AG stores new gen. ind. state
                context->generic_status_indicators[context->generic_status_indicator_state_index].state = (uint8_t)atoi((char*)context->line_buffer);
                break;
            }
            context->line_buffer[context->line_size++] = byte;
            break;
        
        case HFP_PARSER_CMD_INDICATOR_NAME: // parse indicator name
            if (byte == '"'){
                context->line_buffer[context->line_size] = 0;
                context->line_size = 0;
                //printf("Indicator %d: %s (", context->ag_indicators_nr+1, context->line_buffer);
                strcpy((char *)context->ag_indicators[context->parser_item_index].name,  (char *)context->line_buffer);
                context->ag_indicators[context->parser_item_index].index = context->parser_item_index+1;
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
                //printf("%d, ", atoi((char *)&context->line_buffer[0]));
                context->ag_indicators[context->parser_item_index].min_range = atoi((char *)context->line_buffer);
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
                //printf("%d)\n", atoi((char *)&context->line_buffer[0]));
                context->ag_indicators[context->parser_item_index].max_range = atoi((char *)context->line_buffer);
                context->line_size = 0;
                context->parser_item_index++;
                context->ag_indicators_nr = context->parser_item_index;
                break;
            }
           
            context->line_buffer[context->line_size++] = byte;
            break;
        
    }
}

void hfp_init(uint16_t rfcomm_channel_nr){
    rfcomm_register_service_internal(NULL, rfcomm_channel_nr, 0xffff);  
    sdp_query_rfcomm_register_callback(handle_query_rfcomm_event, NULL);
}

void hfp_establish_service_level_connection(bd_addr_t bd_addr, uint16_t service_uuid){
    hfp_connection_t * context = provide_hfp_connection_context_for_bd_addr(bd_addr);
    log_info("hfp_connect %s, context %p", bd_addr_to_str(bd_addr), context);
    
    if (!context) {
        log_error("hfp_establish_service_level_connection for addr %s failed", bd_addr_to_str(bd_addr));
        return;
    }
    if (context->state != HFP_IDLE) return;
    
    memcpy(context->remote_addr, bd_addr, 6);
    context->state = HFP_W4_SDP_QUERY_COMPLETE;

    connection_doing_sdp_query = context;
    sdp_query_rfcomm_channel_and_name_for_uuid(context->remote_addr, service_uuid);
}

void hfp_release_service_level_connection(hfp_connection_t * context){
    if (!context) {
        log_error("hfp_release_service_level_connection failed");
        return;
    }
            
    switch (context->state){
        case HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
            context->state = HFP_W2_DISCONNECT_RFCOMM;
            break;
        case HFP_W4_RFCOMM_CONNECTED:
            context->state = HFP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN;
            break;
        default:
            break;
    }
    return;
}


