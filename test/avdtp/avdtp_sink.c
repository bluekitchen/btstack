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
#include "avdtp_util.h"
#include "avdtp_initiator.h"
#include "avdtp_acceptor.h"

static const char * default_avdtp_sink_service_name = "BTstack AVDTP Sink Service";
static const char * default_avdtp_sink_service_provider_name = "BTstack AVDTP Sink Service Provider";

static btstack_linked_list_t avdtp_sink_connections = NULL;

static avdtp_sep_t local_seps[MAX_NUM_SEPS];
static uint8_t local_seps_num = 0;

static btstack_packet_handler_t avdtp_sink_callback;
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void avdtp_sink_run_for_connection(avdtp_sink_connection_t *connection);

static void (*handle_media_data)(avdtp_sink_connection_t * connection, uint8_t *packet, uint16_t size);    

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


void avdtp_sink_register_media_transport_category(uint8_t seid){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_MEDIA_TRANSPORT, 1);
    local_seps[seid].registered_service_categories = bitmap;
    printf("registered services AVDTP_MEDIA_TRANSPORT(%d) %02x\n", AVDTP_MEDIA_TRANSPORT, local_seps[seid].registered_service_categories);
}

void avdtp_sink_register_reporting_category(uint8_t seid){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_REPORTING, 1);
    local_seps[seid].registered_service_categories = bitmap;
}

void avdtp_sink_register_delay_reporting_category(uint8_t seid){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_DELAY_REPORTING, 1);
    local_seps[seid].registered_service_categories = bitmap;
}

void avdtp_sink_register_recovery_category(uint8_t seid, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_RECOVERY, 1);
    local_seps[seid].registered_service_categories = bitmap;
    local_seps[seid].capabilities.recovery.recovery_type = 0x01; // 0x01 = RFC2733
    local_seps[seid].capabilities.recovery.maximum_recovery_window_size = maximum_recovery_window_size;
    local_seps[seid].capabilities.recovery.maximum_number_media_packets = maximum_number_media_packets;
}

void avdtp_sink_register_content_protection_category(uint8_t seid, uint8_t cp_type_lsb,  uint8_t cp_type_msb, const uint8_t * cp_type_value, uint8_t cp_type_value_len){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_CONTENT_PROTECTION, 1);
    local_seps[seid].registered_service_categories = bitmap;
    local_seps[seid].capabilities.content_protection.cp_type_lsb = cp_type_lsb;
    local_seps[seid].capabilities.content_protection.cp_type_msb = cp_type_msb;
    local_seps[seid].capabilities.content_protection.cp_type_value = cp_type_value;
    local_seps[seid].capabilities.content_protection.cp_type_value_len = cp_type_value_len;
}

void avdtp_sink_register_header_compression_category(uint8_t seid, uint8_t back_ch, uint8_t media, uint8_t recovery){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_HEADER_COMPRESSION, 1);
    local_seps[seid].registered_service_categories = bitmap;
    local_seps[seid].capabilities.header_compression.back_ch = back_ch;
    local_seps[seid].capabilities.header_compression.media = media;
    local_seps[seid].capabilities.header_compression.recovery = recovery;
}

void avdtp_sink_register_media_codec_category(uint8_t seid, avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, const uint8_t * media_codec_info, uint16_t media_codec_info_len){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_MEDIA_CODEC, 1);
    local_seps[seid].registered_service_categories = bitmap;
    printf("registered services AVDTP_MEDIA_CODEC(%d) %02x\n", AVDTP_MEDIA_CODEC, local_seps[seid].registered_service_categories);
    local_seps[seid].capabilities.media_codec.media_type = media_type;
    local_seps[seid].capabilities.media_codec.media_codec_type = media_codec_type;
    local_seps[seid].capabilities.media_codec.media_codec_information = media_codec_info;
    local_seps[seid].capabilities.media_codec.media_codec_information_len = media_codec_info_len;
}

void avdtp_sink_register_multiplexing_category(uint8_t seid, uint8_t fragmentation){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_MULTIPLEXING, 1);
    local_seps[seid].registered_service_categories = bitmap;
    local_seps[seid].capabilities.multiplexing_mode.fragmentation = fragmentation;
}
// // media, reporting. recovery
// void avdtp_sink_register_media_transport_identifier_for_multiplexing_category(uint8_t seid, uint8_t fragmentation){

// }


static avdtp_sink_connection_t * create_avdtp_sink_connection_context(bd_addr_t bd_addr){
    avdtp_sink_connection_t * connection = btstack_memory_avdtp_sink_connection_get();
    if (!connection) return NULL;
    memset(connection,0, sizeof(avdtp_sink_connection_t));
    memcpy(connection->remote_addr, bd_addr, 6);
    connection->avdtp_state = AVDTP_IDLE;
    connection->initiator_config_state = AVDTP_INITIATOR_STREAM_CONFIG_IDLE;
    connection->acceptor_config_state = AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE;

    btstack_linked_list_add(&avdtp_sink_connections, (btstack_linked_item_t*)connection);
    return connection;
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
        if (connection->l2cap_signaling_cid == l2cap_cid){
            return connection;
        }
        if (connection->l2cap_media_cid == l2cap_cid){
            return connection;
        }
        if (connection->l2cap_reporting_cid == l2cap_cid){
            return connection;
        }   
    }
    return NULL;
}

static void avdtp_sink_remove_connection_context(avdtp_sink_connection_t * connection){
    btstack_linked_list_remove(&avdtp_sink_connections, (btstack_linked_item_t*) connection);   
}


static void handle_l2cap_signaling_data_packet(avdtp_sink_connection_t * connection, uint8_t *packet, uint16_t size){
    if (size < 2) {
        log_error("l2cap data packet too small");
        return;
    }
    
    avdtp_signaling_packet_header_t signaling_header;
    avdtp_read_signaling_header(&signaling_header, packet, size);

    // printf("SIGNALING HEADER: tr_label %d, packet_type %d, msg_type %d, signal_identifier %02x\n", 
    //     signaling_header.transaction_label, signaling_header.packet_type, signaling_header.message_type, signaling_header.signal_identifier);

    if (connection->avdtp_state == AVDTP_CONFIGURATION_SUBSTATEMACHINE){
        if (avdtp_initiator_stream_config_subsm_is_done(connection) || avdtp_acceptor_stream_config_subsm_is_done(connection)){
            printf("AVDTP_CONFIGURATION_SUBSTATEMACHINE -> AVDTP_CONFIGURED\n");
            connection->avdtp_state = AVDTP_CONFIGURED;
        }
    }

    switch (connection->avdtp_state){
        case AVDTP_CONFIGURATION_SUBSTATEMACHINE:
            if (signaling_header.message_type == AVDTP_CMD_MSG){
                avdtp_acceptor_stream_config_subsm(connection, packet, size);
                break;
            } 
            avdtp_initiator_stream_config_subsm(connection, packet, size);
            break;
        case AVDTP_CONFIGURED:
            switch (signaling_header.signal_identifier){
                case AVDTP_SI_OPEN:
                    printf("AVDTP_CONFIGURED -> AVDTP_W2_ANSWER_OPEN_STREAM %d\n", signaling_header.transaction_label);
                    connection->local_seid  = packet[2] >> 2;
                    connection->avdtp_state = AVDTP_W2_ANSWER_OPEN_STREAM;
                    connection->acceptor_transaction_label = signaling_header.transaction_label;
                    l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
                    break;
                default:
                    printf("AVDTP_CONFIGURED -> NOT IMPLEMENTED signal_identifier %d, state %d\n", signaling_header.signal_identifier, connection->avdtp_state);
                    break;
            }
            break;
        case AVDTP_OPEN:
            switch (signaling_header.signal_identifier){
                case AVDTP_SI_START:
                    printf("AVDTP_OPEN -> AVDTP_W2_ANSWER_START_SINGLE_STREA %d\n", signaling_header.transaction_label);
                    connection->local_seid  = packet[2] >> 2;
                    connection->avdtp_state = AVDTP_W2_ANSWER_START_SINGLE_STREAM;
                    connection->acceptor_transaction_label = signaling_header.transaction_label;
                    l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
                    break;
                default:
                    printf("AVDTP_OPEN -> NOT IMPLEMENTED signal_identifier %d\n", signaling_header.signal_identifier);
                    break;
            }
            break;
        default:
            printf("handle_l2cap_signaling_data_packet: state %d -> NOT IMPLEMENTED signal_identifier %d\n", connection->avdtp_state, signaling_header.signal_identifier);
            // printf_hexdump( packet, size );
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
                log_error("avdtp packet handler L2CAP_DATA_PACKET: connection for local cid 0x%02x not found", channel);
                break;
            }
            if (channel == connection->l2cap_signaling_cid){
                handle_l2cap_signaling_data_packet(connection, packet, size);
            } else if (channel == connection->l2cap_media_cid){
                (*handle_media_data)(connection, packet, size);
            } else if (channel == connection->l2cap_reporting_cid){
                // TODO
            } else {
                log_error("avdtp packet handler L2CAP_DATA_PACKET: local cid 0x%02x not found", channel);
            }
            break;
            
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    connection = create_avdtp_sink_connection_context(event_addr);
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
                    
                    l2cap_accept_connection(local_cid);
                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    connection = get_avdtp_sink_connection_context_for_bd_addr(event_addr);
                    if (!connection){
                        log_error("avdtp sink: L2CAP_EVENT_CHANNEL_OPENED: connection for bd addr %s cannot be found", bd_addr_to_str(event_addr));
                        return;
                    }

                    if (l2cap_event_channel_opened_get_status(packet)){
                        log_error("L2CAP connection to device %s failed. status code 0x%02x", 
                            bd_addr_to_str(event_addr), l2cap_event_channel_opened_get_status(packet));
                        break;
                    }
                    psm = l2cap_event_channel_opened_get_psm(packet); 
                    if (psm != PSM_AVDTP){
                        log_error("unexpected PSM - Not implemented yet, avdtp sink: L2CAP_EVENT_CHANNEL_OPENED");
                        return;
                    }
                    
                    con_handle = l2cap_event_channel_opened_get_handle(packet);
                    printf("avdtp_sink: Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x, context %p\n",
                           bd_addr_to_str(event_addr), con_handle, psm, connection->l2cap_signaling_cid,  l2cap_event_channel_opened_get_remote_cid(packet),
                           connection);

                    if (connection->avdtp_state == AVDTP_W4_L2CAP_CONNECTED){
                        printf("AVDTP_W4_L2CAP_CONNECTED ->  AVDTP_CONFIGURATION_SUBSTATEMACHINE\n");
                        connection->avdtp_state = AVDTP_CONFIGURATION_SUBSTATEMACHINE;
                        avdtp_initiator_stream_config_subsm_init(connection);
                        avdtp_acceptor_stream_config_subsm_init(connection);
                        
                        connection->l2cap_signaling_cid = l2cap_event_channel_opened_get_local_cid(packet); 
                        connection->initiator_transaction_label++;
                        l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
                    } else if (connection->avdtp_state <= AVDTP_W4_STREAMING_CONNECTION_OPEN){
                        printf("AVDTP_W4_STREAMING_CONNECTION_OPEN ->  AVDTP_STREAMING\n");
                        connection->avdtp_state = AVDTP_STREAMING;
                        connection->l2cap_media_cid = l2cap_event_channel_opened_get_local_cid(packet);
                        // l2cap_request_can_send_now_event(connection->l2cap_media_cid);
                        printf("avdtp_sink: L2CAP_EVENT_CHANNEL_OPENED: Media \n");
                    } else {
                        printf("avdtp_sink: unexpected connection state: Not implemented yet remote state %d\n", connection->acceptor_config_state);
                        return;
                    }
                
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    // data: event (8), len(8), channel (16)
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    connection = get_avdtp_sink_connection_context_for_l2cap_cid(local_cid);
                    if (!connection || connection->avdtp_state != AVDTP_W4_L2CAP_DISCONNECTED) return;

                    log_info("L2CAP_EVENT_CHANNEL_CLOSED cid 0x%0x", local_cid);
                    avdtp_sink_remove_connection_context(connection);
                    connection->avdtp_state = AVDTP_IDLE;
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    break;
                case L2CAP_EVENT_CAN_SEND_NOW:
                    break;
                default:
                    printf("unknown HCI event type %02x\n", hci_event_packet_get_type(packet));
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

void avdtp_sink_register_media_handler(void (*callback)(avdtp_sink_connection_t * connection, uint8_t *packet, uint16_t size)){
    if (callback == NULL){
        log_error("avdtp_sink_register_media_handler called with NULL callback");
        return;
    }
    handle_media_data = callback;
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
    if (connection->release_l2cap_connection){
        if (connection->avdtp_state > AVDTP_W4_L2CAP_CONNECTED || 
            connection->avdtp_state < AVDTP_W4_L2CAP_DISCONNECTED){
            
            connection->release_l2cap_connection = 0;
            connection->avdtp_state = AVDTP_W4_L2CAP_DISCONNECTED;
            l2cap_disconnect(connection->l2cap_signaling_cid, 0);
            return;
        }
    }

    if (!l2cap_can_send_packet_now(connection->l2cap_signaling_cid)) {
        log_info("avdtp_sink_run_for_connection: request cannot send for 0x%02x", connection->l2cap_signaling_cid);
        return;
    }
    
    if (connection->avdtp_state == AVDTP_CONFIGURATION_SUBSTATEMACHINE){
        if (avdtp_initiator_stream_config_subsm_is_done(connection) || avdtp_acceptor_stream_config_subsm_is_done(connection)){
            printf("AVDTP_CONFIGURATION_SUBSTATEMACHINE -> AVDTP_CONFIGURED\n");
            connection->avdtp_state = AVDTP_CONFIGURED;
        }
    }

    switch (connection->avdtp_state){
        case AVDTP_CONFIGURATION_SUBSTATEMACHINE:
            if (!avdtp_initiator_stream_config_subsm_run_for_connection(connection)) {
                avdtp_acceptor_stream_config_subsm_run_for_connection(connection, local_seps, local_seps_num);
            }
            break;
        case AVDTP_W2_ANSWER_OPEN_STREAM:
            printf("AVDTP_W2_ANSWER_OPEN_STREAM -> AVDTP_OPEN use %d\n", connection->acceptor_transaction_label);
            connection->avdtp_state = AVDTP_OPEN;
            avdtp_acceptor_send_accept_response(connection->l2cap_signaling_cid, AVDTP_SI_OPEN, connection->acceptor_transaction_label);
            break;
        case AVDTP_W2_ANSWER_START_SINGLE_STREAM:
            printf("AVDTP_W2_ANSWER_START_SINGLE_STREAM -> AVDTP_W4_STREAMING_CONNECTION_OPEN use %d\n", connection->acceptor_transaction_label);
            connection->avdtp_state = AVDTP_W4_STREAMING_CONNECTION_OPEN;
            avdtp_acceptor_send_accept_response(connection->l2cap_signaling_cid, AVDTP_SI_START, connection->acceptor_transaction_label);
            break;
        default:
            printf("avdtp_sink_run_for_connection: state %d -> NOT IMPLEMENTED\n", connection->avdtp_state);
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

    connection->avdtp_state = AVDTP_W4_L2CAP_CONNECTED;
    l2cap_create_channel(packet_handler, connection->remote_addr, PSM_AVDTP, 0xffff, NULL);
}

void avdtp_sink_disconnect(uint16_t l2cap_cid){
    avdtp_sink_connection_t * connection = get_avdtp_sink_connection_context_for_l2cap_cid(l2cap_cid);
    if (!connection) {
        log_error("avdtp_sink_disconnect: Connection with l2cap_cid %d not found", l2cap_cid);
        return;
    }

    if (connection->avdtp_state == AVDTP_IDLE) return;
    if (connection->avdtp_state == AVDTP_W4_L2CAP_DISCONNECTED) return;
    
    connection->release_l2cap_connection = 1;
    avdtp_sink_run_for_connection(connection);
}


