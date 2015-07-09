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

static hfp_callback_t hfp_callback;
static linked_list_t hfp_connections = NULL;

static hfp_connection_t * get_hfp_connection_context_for_handle(uint16_t handle){
    linked_item_t *it;
    for (it = (linked_item_t *) &hfp_connections; it ; it = it->next){
        hfp_connection_t * connection = (hfp_connection_t *) it;
        if (connection->con_handle == handle){
            return connection;
        }
    }
    return NULL;
}

static hfp_connection_t * get_hfp_connection_context_for_bd_addr(bd_addr_t bd_addr){
    linked_item_t *it;
    for (it = (linked_item_t *) &hfp_connections; it ; it = it->next){
        hfp_connection_t * connection = (hfp_connection_t *) it;
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
    linked_list_add(&hfp_connections, (linked_item_t*)context);
    return context;
}


static hfp_connection_t * provide_hfp_connection_context_for_bd_addr(bd_addr_t bd_addr){
    hfp_connection_t * context = get_hfp_connection_context_for_bd_addr(bd_addr);
    if (context) return  context;
    context = create_hfp_connection_context();
    memcpy(context->remote_addr, bd_addr, 6);
    return context;
}

hfp_connection_t * provide_hfp_connection_context_for_conn_handle(uint16_t con_handle){
    hfp_connection_t * context = get_hfp_connection_context_for_handle(con_handle);
    if (context) return  context;
    context = create_hfp_connection_context();
    context->con_handle = con_handle;
    return context;
}

void hfp_register_packet_handler(hfp_callback_t callback){
    if (callback == NULL){
        log_error("hfp_register_packet_handler called with NULL callback");
        return;
    }
    hfp_callback = callback;
}

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
    /* Bit position:
     * 0: EC and/or NR function (yes/no, 1 = yes, 0 = no)
     * 1: Call waiting or three-way calling(yes/no, 1 = yes, 0 = no)
     * 2: CLI presentation capability (yes/no, 1 = yes, 0 = no)
     * 3: Voice recognition activation (yes/no, 1= yes, 0 = no)
     * 4: Remote volume control (yes/no, 1 = yes, 0 = no)
     * 5: Wide band speech (yes/no, 1 = yes, 0 = no)
     */
}

static hfp_connection_t * connection_doing_sdp_query = NULL;
static void handle_query_rfcomm_event(sdp_query_event_t * event, void * context){
    sdp_query_rfcomm_service_event_t * ve;
    sdp_query_complete_event_t * ce;
    hfp_connection_t * connection = connection_doing_sdp_query;

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
                rfcomm_create_channel_internal(NULL, connection->remote_addr, connection->rfcomm_channel_nr); 
                break;
            }
            log_info("rfcomm service not found, status %u.", ce->status);
            break;
        default:
            break;
    }
}

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    // printf("packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);
    if (packet_type == RFCOMM_DATA_PACKET){
    }

    if (packet_type != HCI_EVENT_PACKET) return;
}


void hfp_init(uint16_t rfcomm_channel_nr){
    rfcomm_register_service_internal(NULL, rfcomm_channel_nr, 0xffff);  
    
    l2cap_register_packet_handler(packet_handler);
    rfcomm_register_packet_handler(packet_handler);

    sdp_query_rfcomm_register_callback(handle_query_rfcomm_event, NULL);
}

void hfp_connect(bd_addr_t bd_addr){
    hfp_connection_t * connection = provide_hfp_connection_context_for_bd_addr(bd_addr);
    if (!connection) {
        log_error("hfp_hf_connect for addr %s failed", bd_addr_to_str(bd_addr));
        return;
    }
    
    if (connection->state != HFP_IDLE) return;
    connection->state = HFP_SDP_QUERY_RFCOMM_CHANNEL;
    memcpy(connection->remote_addr, bd_addr, 6);
    connection_doing_sdp_query = connection;
    // @TODO trigger SDP query
}
