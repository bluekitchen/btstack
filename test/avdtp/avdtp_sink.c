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

static avdtp_sep_t local_seps[MAX_NUM_SEPS];
static uint8_t local_seps_num = 0;

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

uint8_t avdtp_sink_register_stream_end_point(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type){
    if (local_seps_num >= MAX_NUM_SEPS){
        log_error("avdtp_sink_register_sep: excedeed max sep number %d", MAX_NUM_SEPS);
        return 255;
    }
    uint8_t seid = local_seps_num;
    avdtp_sep_t entry = {
        seid,
        0,
        media_type,
        sep_type
    };

    local_seps[local_seps_num] = entry;
    local_seps_num++;
    return seid;
}

static int get_bit16(uint16_t bitmap, int position){
    return (bitmap >> position) & 1;
}

static uint8_t store_bit16(uint16_t bitmap, int position, uint8_t value){
    if (value){
        bitmap |= 1 << position;
    } else {
        bitmap &= ~ (1 << position);
    }
    return bitmap;
}

void avdtp_sink_register_media_transport_category(uint8_t seid){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    store_bit16(local_seps[seid].registered_service_categories, AVDTP_MEDIA_TRANSPORT, 1);
}

void avdtp_sink_register_reporting_category(uint8_t seid){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    store_bit16(local_seps[seid].registered_service_categories, AVDTP_REPORTING, 1);
}

void avdtp_sink_register_delay_reporting_category(uint8_t seid){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    store_bit16(local_seps[seid].registered_service_categories, AVDTP_DELAY_REPORTING, 1);
}

void avdtp_sink_register_recovery_category(uint8_t seid, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    store_bit16(local_seps[seid].registered_service_categories, AVDTP_RECOVERY, 1);
}

void avdtp_sink_register_content_protection_category(uint8_t seid, uint8_t cp_type_lsb,  uint8_t cp_type_msb){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    store_bit16(local_seps[seid].registered_service_categories, AVDTP_CONTENT_PROTECTION, 1);
}

void avdtp_sink_register_header_compression_category(uint8_t seid, uint8_t back_ch, uint8_t media, uint8_t recovery){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    store_bit16(local_seps[seid].registered_service_categories, AVDTP_HEADER_COMPRESSION, 1);
}

void avdtp_sink_register_media_codec_category(uint8_t seid, uint8_t media_type, uint8_t media_codec_type){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    store_bit16(local_seps[seid].registered_service_categories, AVDTP_MEDIA_CODEC, 1);
}

void avdtp_sink_register_multiplexing_category(uint8_t seid, uint8_t fragmentation, uint8_t transport_identifiers_num, uint8_t * transport_session_identifiers, uint8_t * transport_c_identifiers){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    store_bit16(local_seps[seid].registered_service_categories, AVDTP_MULTIPLEXING, 1);
}

static void audio_sink_generate_next_transaction_label(avdtp_sink_connection_t * connection){
    connection->local_transaction_label++;
}

static avdtp_sink_connection_t * create_avdtp_sink_connection_context(bd_addr_t bd_addr){
    avdtp_sink_connection_t * avdtp_connection = btstack_memory_avdtp_sink_connection_get();
    if (!avdtp_connection) return NULL;
    memset(avdtp_connection,0, sizeof(avdtp_sink_connection_t));
    memcpy(avdtp_connection->remote_addr, bd_addr, 6);
    avdtp_connection->local_state = AVDTP_SINK_IDLE;

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

static void avdtp_sink_remove_connection_context(avdtp_sink_connection_t * connection){
    btstack_linked_list_remove(&avdtp_sink_connections, (btstack_linked_item_t*) connection);   
}

static inline uint8_t avdtp_header(uint8_t tr_label, avdtp_packet_type_t packet_type, avdtp_message_type_t msg_type){
    return (tr_label<<4) | ((uint8_t)packet_type<<2) | (uint8_t)msg_type;
}

static int avdtp_sink_send_signaling_cmd(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t transaction_label){
    uint8_t command[2];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
    command[1] = (uint8_t)identifier;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_sink_send_seps_response(uint16_t cid, uint8_t transaction_label, avdtp_sep_t * seps, int seps_num){
    uint8_t command[2+2*MAX_NUM_SEPS];
    int pos = 0;
    command[pos++] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_ACCEPT_MSG);
    command[pos++] = (uint8_t)AVDTP_DISCOVER;
    int i = 0;
    for (i=0; i<seps_num; i++){
        command[pos++] = (seps[i].seid << 2) | (seps[i].in_use<<1);
        command[pos++] = (seps[i].media_type << 4) | (seps[i].type << 3);
    }
    return l2cap_send(cid, command, pos);
}

static int avdtp_sink_send_get_capabilities_cmd(uint16_t cid, uint8_t sep_id){
    return 0;
}

static int avdtp_pack_service_category(uint8_t * buffer, int size, avdtp_sep_t sep, avdtp_service_category_t category){
    int pos = 0;
    switch(category){
        case AVDTP_MEDIA_TRANSPORT:
            break;
        case AVDTP_REPORTING:
            break;
        case AVDTP_RECOVERY:
            break;
        case AVDTP_CONTENT_PROTECTION:
            break;
        case AVDTP_HEADER_COMPRESSION:
            break;
        case AVDTP_MULTIPLEXING:
            break;
        case AVDTP_MEDIA_CODEC:
            break;
        case AVDTP_DELAY_REPORTING:
            break;
    }
    return pos;
}

static int avdtp_sink_send_capabilities_response(uint16_t cid, uint8_t transaction_label, avdtp_sep_t sep){
    uint8_t command[2+2*MAX_NUM_SEPS];
    int pos = 0;
    command[pos++] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_ACCEPT_MSG);
    command[pos++] = (uint8_t)AVDTP_GET_CAPABILITIES;
    int i = 0;
    for (i = 1; i < 9; i++){
        if (get_bit16(sep.registered_service_categories, i)){
            pos += avdtp_pack_service_category(command+pos, sizeof(command)-pos, sep, (avdtp_service_category_t)i);
        }
    }
    return l2cap_send(cid, command, pos);
}

static void handle_l2cap_data_packet(avdtp_sink_connection_t * connection, uint8_t *packet, uint16_t size){
    if (size < 2) {
        log_error("l2cap data packet too small");
        return;
    }
    uint8_t tr_label = packet[0] >> 4;
    avdtp_packet_type_t packet_type = (avdtp_packet_type_t)((packet[0] >> 2) & 0x03);
    avdtp_message_type_t msg_type = (avdtp_message_type_t) (packet[0] & 0x03);
    uint8_t signal_identifier = packet[1] & 0x3f;

    printf("\nheader: tr_label %d, packet_type %d, msg_type %d, signal_identifier %02x\n", tr_label, packet_type, msg_type, signal_identifier);
    
    int i = 2;
    avdtp_sep_t sep;

    if (msg_type == AVDTP_CMD_MSG){
        // remote
        printf("command from remote\n");
        switch (signal_identifier){
            case AVDTP_DISCOVER:
                connection->remote_transaction_label = tr_label;
                connection->remote_state = AVDTP_SINK_W2_DISCOVER_SEPS;
                l2cap_request_can_send_now_event(connection->l2cap_cid);
                break;
            case AVDTP_GET_CAPABILITIES:
                sep.seid = packet[2] >> 2;
                connection->remote_transaction_label = tr_label;
                connection->requested_local_seid = sep.seid;
                connection->remote_state = AVDTP_SINK_W2_GET_CAPABILITIES;
                l2cap_request_can_send_now_event(connection->l2cap_cid);
                break;
            default:
                break;
        }
        return;
    }

    switch (connection->local_state){
        case AVDTP_SINK_W4_SEPS_DISCOVERED:
            if (tr_label != connection->local_transaction_label){
                log_error("unexpected transaction label, got %d, expected %d", tr_label, connection->local_transaction_label);
                return;
            }
            if (signal_identifier != AVDTP_DISCOVER) {
                log_error("unexpected signal identifier ...");
                return;
            }
            if (msg_type != AVDTP_RESPONSE_ACCEPT_MSG){
                log_error("request rejected...");
                return;   
            }
            if (size == 3){
                printf("ERROR code %02x\n", packet[2]);
                return;
            }
            for (i = 2; i<size; i+=2){
                sep.seid = packet[i] >> 2;
                if (sep.seid < 0x01 || sep.seid > 0x3E){
                    log_error("invalid sep id");
                    return;
                }
                sep.in_use = (packet[i] >> 1) & 0x01;
                sep.media_type = (avdtp_media_type_t)(packet[i+1] >> 4);
                sep.type = (avdtp_sep_type_t)((packet[i+1] >> 3) & 0x01);
                connection->remote_seps[connection->remote_seps_num++] = sep;
                printf("found sep: seid %u, in_use %d, media type %d, sep type %d (1-SNK)\n", 
                    sep.seid, sep.in_use, sep.media_type, sep.type);
            }
            // connection->local_state = AVDTP_SINK_W2_GET_CAPABILITIES;
            // connection->local_transaction_label++;
            // l2cap_request_can_send_now_event(connection->l2cap_cid);
            break;
        default:
            printf_hexdump( packet, size );
            break;
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    hci_con_handle_t con_handle;
    uint16_t psm;
    uint16_t remote_cid;
    uint16_t local_cid;
    avdtp_sink_connection_t * connection = NULL;
        
    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            connection = get_avdtp_sink_connection_context_for_l2cap_cid(channel);
            if (!connection){
                log_error("avdtp packet handler L2CAP_EVENT_INCOMING_CONNECTION: connection for local cid 0x%02x not found", channel);
                break;
            }
            handle_l2cap_data_packet(connection, packet, size);
            break;
            
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    connection = get_avdtp_sink_connection_context_for_bd_addr(event_addr);
                    if (!connection){
                        log_error("avdtp packet handler L2CAP_EVENT_INCOMING_CONNECTION: connection for bd address %s not found", bd_addr_to_str(event_addr));
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
                    
                    connection->local_state = AVDTP_SINK_W4_L2CAP_CONNECTED;
                    l2cap_accept_connection(local_cid);

                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    connection = get_avdtp_sink_connection_context_for_bd_addr(event_addr);
                    if (!connection || connection->local_state != AVDTP_SINK_W4_L2CAP_CONNECTED) return;

                    if (l2cap_event_channel_opened_get_status(packet)){
                        printf("L2CAP connection to device %s failed. status code %u\n", 
                            bd_addr_to_str(event_addr), l2cap_event_channel_opened_get_status(packet));
                        break;
                    }
                    psm = l2cap_event_channel_opened_get_psm(packet); 
                    connection->l2cap_cid = l2cap_event_channel_opened_get_local_cid(packet); 
                    connection->acl_handle = l2cap_event_channel_opened_get_handle(packet);

                    printf("Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x, context %p\n",
                           bd_addr_to_str(event_addr), connection->acl_handle, psm, connection->l2cap_cid,  l2cap_event_channel_opened_get_remote_cid(packet),
                           connection);
                    

                    connection->local_state = AVDTP_SINK_W2_DISCOVER_SEPS;
                    audio_sink_generate_next_transaction_label(connection);
                    l2cap_request_can_send_now_event(connection->l2cap_cid);
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    // data: event (8), len(8), channel (16)
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    connection = get_avdtp_sink_connection_context_for_l2cap_cid(local_cid);
                    if (!connection || connection->local_state != AVDTP_SINK_W4_L2CAP_DISCONNECTED) return;

                    log_info("L2CAP_EVENT_CHANNEL_CLOSED cid 0x%0x", local_cid);
                    avdtp_sink_remove_connection_context(connection);
                    connection->local_state = AVDTP_SINK_IDLE;
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
        if (connection->local_state > AVDTP_SINK_W4_L2CAP_CONNECTED || 
            connection->local_state < AVDTP_SINK_W4_L2CAP_DISCONNECTED){
            
            connection->release_l2cap_connection = 0;
            connection->local_state = AVDTP_SINK_W4_L2CAP_DISCONNECTED;
            l2cap_disconnect(connection->l2cap_cid, 0);
            return;
        }
    }

    if (!l2cap_can_send_packet_now(connection->l2cap_cid)) {
        log_info("avdtp_sink_run_for_connection: request cannot send for 0x%02x", connection->l2cap_cid);
        return;
    }

    switch (connection->remote_state){
        case AVDTP_SINK_W2_DISCOVER_SEPS:
            connection->local_state = AVDTP_SINK_W4_SEPS_DISCOVERED;
            avdtp_sink_send_seps_response(connection->l2cap_cid, connection->remote_transaction_label, local_seps, local_seps_num);
            return;
        case AVDTP_SINK_W2_GET_CAPABILITIES:
            connection->local_state = AVDTP_SINK_W4_CAPABILITIES;
            avdtp_sink_send_capabilities_response(connection->l2cap_cid, connection->remote_transaction_label, local_seps[connection->requested_local_seid]);
            break;
        default:
            break;
    }

    switch (connection->local_state){
        case AVDTP_SINK_W2_DISCOVER_SEPS:
            connection->local_state = AVDTP_SINK_W4_SEPS_DISCOVERED;
            avdtp_sink_send_signaling_cmd(connection->l2cap_cid, AVDTP_DISCOVER, connection->local_transaction_label);
            return;
        case AVDTP_SINK_W2_GET_CAPABILITIES:
            connection->local_state = AVDTP_SINK_W4_CAPABILITIES;
            avdtp_sink_send_get_capabilities_cmd(connection->l2cap_cid, 0);
            return;
        default:
            break;
    }    
}

void avdtp_sink_connect(bd_addr_t bd_addr){
    avdtp_sink_connection_t * connection = get_avdtp_sink_connection_context_for_bd_addr(bd_addr);
    if (connection) {
        log_error("avdtp_sink_connect: connection for bd address %s not found", bd_addr_to_str(bd_addr));
        return;
    }
    connection = create_avdtp_sink_connection_context(bd_addr);
    if (!connection){
        log_error("avdtp_sink_connect: cannot create connection for bd address %s", bd_addr_to_str(bd_addr));
        return;
    }

    connection->local_state = AVDTP_SINK_W4_L2CAP_CONNECTED;
    l2cap_create_channel(packet_handler, connection->remote_addr, PSM_AVDTP, 0xffff, NULL);
}

void avdtp_sink_disconnect(uint16_t l2cap_cid){
    avdtp_sink_connection_t * connection = get_avdtp_sink_connection_context_for_l2cap_cid(l2cap_cid);
    if (!connection) {
        log_error("avdtp_sink_disconnect: Connection with l2cap_cid %d not found", l2cap_cid);
        return;
    }

    if (connection->local_state == AVDTP_SINK_IDLE) return;
    if (connection->local_state == AVDTP_SINK_W4_L2CAP_DISCONNECTED) return;
    
    connection->release_l2cap_connection = 1;
    avdtp_sink_run_for_connection(connection);
}


