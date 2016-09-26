/*
 * Copyright (C) 2016 BlueKitchen GmbH
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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack.h"
#include "avdtp.h"
#include "avdtp_sink.h"

static const char * default_avdtp_sink_service_name = "BTstack AVDTP Sink Service";
static const char * default_avdtp_sink_service_provider_name = "BTstack AVDTP Sink Service Provider";

static btstack_linked_list_t avdtp_sink_connections = NULL;

static btstack_packet_handler_t avdtp_sink_callback;
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void avdtp_sink_run_for_connection(avdtp_sink_connection_t *connection);

void a2dp_sink_create_sdp_record(uint8_t * service,  uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UUID, DE_SIZE_16, AUDIO_SINK_GROUP); 
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ProtocolDescriptorList);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, SDP_L2CAPProtocol);
            de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, PSM_AVDTP);  
        }
        de_pop_sequence(attribute, l2cpProtocol);
        
        uint8_t* avProtocol = de_push_sequence(attribute);
        {
            de_add_number(avProtocol,  DE_UUID, DE_SIZE_16, PSM_AVDTP);  // avProtocol_service
            de_add_number(avProtocol,  DE_UINT, DE_SIZE_16,  0x0103);  // version
        }
        de_pop_sequence(attribute, avProtocol);
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
        uint8_t *a2dProfile = de_push_sequence(attribute);
        {
            de_add_number(a2dProfile,  DE_UUID, DE_SIZE_16, ADVANCED_AUDIO_DISTRIBUTION); 
            de_add_number(a2dProfile,  DE_UINT, DE_SIZE_16, 0x0103); 
        }
        de_pop_sequence(attribute, a2dProfile);
    }
    de_pop_sequence(service, attribute);


    // 0x0100 "Service Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    if (service_name){
        de_add_data(service,  DE_STRING, strlen(service_name), (uint8_t *) service_name);
    } else {
        de_add_data(service,  DE_STRING, strlen(default_avdtp_sink_service_name), (uint8_t *) default_avdtp_sink_service_name);
    }

    // 0x0100 "Provider Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0102);
    if (service_provider_name){
        de_add_data(service,  DE_STRING, strlen(service_provider_name), (uint8_t *) service_provider_name);
    } else {
        de_add_data(service,  DE_STRING, strlen(default_avdtp_sink_service_provider_name), (uint8_t *) default_avdtp_sink_service_provider_name);
    }

    // 0x0311 "Supported Features"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0311);
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_features);
}


static avdtp_sink_connection_t * create_avdtp_sink_connection_context(bd_addr_t bd_addr){
    avdtp_sink_connection_t * avdtp_connection = btstack_memory_avdtp_sink_connection_get();
    if (!avdtp_connection) return NULL;
    printf("create_avdtp_sink_connection_context %p\n", avdtp_connection);
    
    memset(avdtp_connection,0, sizeof(avdtp_sink_connection_t));
    memcpy(avdtp_connection->remote_addr, bd_addr, 6);
    avdtp_connection->state = AVDTP_SINK_IDLE;

    btstack_linked_list_add(&avdtp_sink_connections, (btstack_linked_item_t*)avdtp_connection);
    return avdtp_connection;
}

static avdtp_sink_connection_t * get_avdtp_sink_connection_context_for_bd_addr(bd_addr_t bd_addr){
    btstack_linked_list_iterator_t it;  
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avdtp_sink_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_sink_connection_t * connection = (avdtp_sink_connection_t *)btstack_linked_list_iterator_next(&it);
        if (memcmp(connection->remote_addr, bd_addr, 6) == 0) {
            return connection;
        }
    }
    return NULL;
}

static avdtp_sink_connection_t * get_avdtp_sink_connection_context_for_l2cap_cid(uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avdtp_sink_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_sink_connection_t * connection = (avdtp_sink_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->l2cap_cid == l2cap_cid){
            return connection;
        }
    }
    return NULL;
}

static void remove_avdtp_sink_connection_context(avdtp_sink_connection_t * connection){
    btstack_linked_list_remove(&avdtp_sink_connections, (btstack_linked_item_t*) connection);   
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    hci_con_handle_t con_handle;
    uint16_t psm;
    uint16_t remote_cid;
    uint16_t local_cid;
    avdtp_sink_connection_t * connection = NULL;

    printf("avdtp_sink packet handler: Received packet type %02x\n", packet_type);
            
    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            // just dump data for now
            printf("source cid %x -- ", channel);
            // printf_hexdump( packet, size );
            break;
            
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    connection = get_avdtp_sink_connection_context_for_bd_addr(event_addr);
                    if (connection){
                        log_error("");
                        break;
                    }
                    
                    connection = create_avdtp_sink_connection_context(event_addr);
                    if (!connection) return;

                    con_handle = l2cap_event_incoming_connection_get_handle(packet); 
                    psm        = l2cap_event_incoming_connection_get_psm(packet); 
                    local_cid  = l2cap_event_incoming_connection_get_local_cid(packet); 
                    remote_cid = l2cap_event_incoming_connection_get_remote_cid(packet); 
                    printf("L2CAP_EVENT_INCOMING_CONNECTION %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                        bd_addr_to_str(event_addr), con_handle, psm, local_cid, remote_cid);
                    
                    connection->state = AVDTP_SINK_W4_L2CAP_CONNECTED;
                    l2cap_accept_connection(local_cid);

                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    connection = get_avdtp_sink_connection_context_for_bd_addr(event_addr);
                    if (!connection || connection->state != AVDTP_SINK_W4_L2CAP_CONNECTED) return;

                    if (l2cap_event_channel_opened_get_status(packet)){
                        printf("L2CAP connection to device %s failed. status code %u\n", 
                            bd_addr_to_str(event_addr), l2cap_event_channel_opened_get_status(packet));
                        break;
                    }
                    psm = l2cap_event_channel_opened_get_psm(packet); 
                    connection->l2cap_cid = l2cap_event_channel_opened_get_local_cid(packet); 
                    connection->acl_handle = l2cap_event_channel_opened_get_handle(packet);

                    printf("Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                           bd_addr_to_str(event_addr), connection->acl_handle, psm, connection->l2cap_cid,  l2cap_event_channel_opened_get_remote_cid(packet));
                    

                    connection->state = AVDTP_SINK_W4_CONFIGURATION_COMPLETE;
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    // data: event (8), len(8), channel (16)
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    connection = get_avdtp_sink_connection_context_for_l2cap_cid(local_cid);
                    if (!connection || connection->state != AVDTP_SINK_W4_L2CAP_DISCONNECTED) return;

                    log_info("L2CAP_EVENT_CHANNEL_CLOSED cid 0x%0x", local_cid);
                    remove_avdtp_sink_connection_context(connection);
                    connection->state = AVDTP_SINK_IDLE;
                    break;

                default:
                    // other event
                    break;
            }
            break;
            
        default:
            // other packet type
            break;
    }
    avdtp_sink_run_for_connection(connection);
}

// TODO: find out which security level is needed, and replace LEVEL_0 in avdtp_sink_init
void avdtp_sink_init(void){
    l2cap_register_service(&packet_handler, PSM_AVDTP, 0xffff, LEVEL_0);
}

void avdtp_sink_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("avdtp_sink_register_packet_handler called with NULL callback");
        return;
    }
    avdtp_sink_callback = callback;
}


static void avdtp_sink_run_for_connection(avdtp_sink_connection_t *connection){
    if (!connection) return;
    // handle disconnect request
    if (connection->release_l2cap_connection){
        if (connection->state > AVDTP_SINK_W4_L2CAP_CONNECTED || 
            connection->state < AVDTP_SINK_W4_L2CAP_DISCONNECTED){
                connection->release_l2cap_connection = 0;
                connection->state = AVDTP_SINK_W4_L2CAP_DISCONNECTED;
                l2cap_disconnect(connection->l2cap_cid, 0);
            return;
        }
    }

    // if (!l2cap_can_send_packet_now(connection->l2cap_cid)) {
    //     log_info("avdtp_sink_run_for_connection: request cannot send for 0x%02x", connection->l2cap_cid);
    //     return;
    // }
    
}

void avdtp_sink_connect(bd_addr_t bd_addr){
    avdtp_sink_connection_t * connection = get_avdtp_sink_connection_context_for_bd_addr(bd_addr);
    if (connection) {
        log_error("...");
        return;
    }
    connection = create_avdtp_sink_connection_context(bd_addr);
    if (!connection){
        log_error("");
        return;
    }

    connection->state = AVDTP_SINK_W4_L2CAP_CONNECTED;
    l2cap_create_channel(packet_handler, connection->remote_addr, PSM_AVDTP, 0xffff, NULL);
}


void avdtp_sink_disconnect(uint16_t l2cap_cid){
    avdtp_sink_connection_t * connection = get_avdtp_sink_connection_context_for_l2cap_cid(l2cap_cid);
    if (!connection) {
        log_error("avdtp_sink_disconnect Connection with l2cap_cid %d not found", l2cap_cid);
        return;
    }

    if (connection->state == AVDTP_SINK_IDLE) return;
    if (connection->state == AVDTP_SINK_W4_L2CAP_DISCONNECTED) return;
    
    connection->release_l2cap_connection = 1;
    avdtp_sink_run_for_connection(connection);
}
