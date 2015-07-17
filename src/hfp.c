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
#include "hfp_ag.h"

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

static hfp_callback_t hfp_callback;
static linked_list_t hfp_connections = NULL;

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
    } else {
        printf("\nSent %s", command);
    }
    return err;
}

void join(char * buffer, int buffer_size, uint8_t * values, int values_nr){
    if (buffer_size < values_nr * 3) return;
    int i;
    int offset = 0;
    for (i = 0; i < values_nr-1; i++) {
      offset += snprintf(buffer+offset, buffer_size-offset, "%d,", values[i]); // puts string into buffer
    }
    if (i<values_nr){
        offset += snprintf(buffer+offset, buffer_size-offset, "%d", values[i]);
    }

    offset += snprintf(buffer+offset, buffer_size-offset, "\r\n");
    buffer[offset] = 0;
}


static void hfp_emit_event(hfp_callback_t callback, uint8_t event_subtype, uint8_t value){
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

static hfp_connection_t * get_hfp_connection_context_for_bd_addr(bd_addr_t bd_addr){
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

static hfp_connection_t * create_hfp_connection_context(){
    hfp_connection_t * context = btstack_memory_hfp_connection_get();
    if (!context) return NULL;
    // init state
    context->state = HFP_IDLE;
    context->line_size = 0;
    
    context->negotiated_codec = HFP_CODEC_CVSD;
    context->remote_supported_features = 0;
    context->remote_indicators_update_enabled = 0;
    context->remote_indicators_nr = 0;
    context->remote_indicators_status = 0;

    linked_list_add(&hfp_connections, (linked_item_t*)context);
    return context;
}


hfp_connection_t * provide_hfp_connection_context_for_bd_addr(bd_addr_t bd_addr){
    hfp_connection_t * context = get_hfp_connection_context_for_bd_addr(bd_addr);
    if (context) return  context;
    context = create_hfp_connection_context();
    memcpy(context->remote_addr, bd_addr, 6);
    return context;
}

hfp_connection_t * provide_hfp_connection_context_for_rfcomm_cid(uint16_t rfcomm_cid){
    hfp_connection_t * context = get_hfp_connection_context_for_rfcomm_cid(rfcomm_cid);
    if (context) return  context;
    context = create_hfp_connection_context();
    printf("Set RFCOMM cid %p %u\n", context, context->rfcomm_cid);
    context->rfcomm_cid = rfcomm_cid;
    return context;
}


void hfp_register_packet_handler(hfp_callback_t callback){
    if (callback == NULL){
        log_error("hfp_register_packet_handler called with NULL callback");
        return;
    }
    hfp_callback = callback;
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


void hfp_create_service(uint8_t * service, uint16_t service_uuid, int rfcomm_channel_nr, const char * name, uint16_t supported_features){
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

static void hfp_reset_state(hfp_connection_t * connection){
    if (!connection) return;
    connection->state = HFP_IDLE;
}


hfp_connection_t * hfp_handle_hci_event(uint8_t packet_type, uint8_t *packet, uint16_t size){
    
    bd_addr_t event_addr;
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
            context = provide_hfp_connection_context_for_bd_addr(event_addr);
            
            if (!context || context->state != HFP_IDLE) return context;

            context->rfcomm_cid = READ_BT_16(packet, 9);
            context->state = HFP_W4_RFCOMM_CONNECTED;
            printf("RFCOMM channel %u requested for %s\n", context->rfcomm_cid, bd_addr_to_str(context->remote_addr));
            rfcomm_accept_connection_internal(context->rfcomm_cid);
            break;

        case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
            // data: event(8), len(8), status (8), address (48), handle(16), server channel(8), rfcomm_cid(16), max frame size(16)
            printf("RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);
            bt_flip_addr(event_addr, &packet[3]); 
            context = provide_hfp_connection_context_for_bd_addr(event_addr);
            if (!context || context->state != HFP_W4_RFCOMM_CONNECTED) return context;
            
            if (packet[2]) {
                hfp_reset_state(context);
                // hfp_emit_event(context->callback, HFP_SUBEVENT_CONNECTION_COMPLETE, packet[2]);
            } else {
                context->con_handle = READ_BT_16(packet, 9);
                context->rfcomm_cid = READ_BT_16(packet, 12);
                uint16_t mtu = READ_BT_16(packet, 14);
                context->state = HFP_EXCHANGE_SUPPORTED_FEATURES;
                printf("RFCOMM channel open succeeded. Context %p, RFCOMM Channel ID 0x%02x, max frame size %u\n", context, context->rfcomm_cid, mtu);
            }
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("HCI_EVENT_DISCONNECTION_COMPLETE \n");
            break;
        case RFCOMM_EVENT_CHANNEL_CLOSED:
            printf(" RFCOMM_EVENT_CHANNEL_CLOSED\n");
            // hfp_emit_event(context->callback, HFP_SUBEVENT_DISCONNECTION_COMPLETE,0);
            break;
        case RFCOMM_EVENT_CREDITS:
            context = provide_hfp_connection_context_for_rfcomm_cid(READ_BT_16(packet, 2));
            break;
        default:
            break;
    }
    return context;
}

void hfp_init(uint16_t rfcomm_channel_nr){
    rfcomm_register_service_internal(NULL, rfcomm_channel_nr, 0xffff);  
    sdp_query_rfcomm_register_callback(handle_query_rfcomm_event, NULL);
}

void hfp_connect(bd_addr_t bd_addr, uint16_t service_uuid){
    hfp_connection_t * connection = provide_hfp_connection_context_for_bd_addr(bd_addr);
    log_info("hfp_connect %s, context %p", bd_addr_to_str(bd_addr), connection);
    
    if (!connection) {
        log_error("hfp_hf_connect for addr %s failed", bd_addr_to_str(bd_addr));
        return;
    }
    if (connection->state != HFP_IDLE) return;
    
    memcpy(connection->remote_addr, bd_addr, 6);
    connection->state = HFP_W4_SDP_QUERY_COMPLETE;

    connection_doing_sdp_query = connection;
    sdp_query_rfcomm_channel_and_name_for_uuid(connection->remote_addr, service_uuid);
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
