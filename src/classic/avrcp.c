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

#define BTSTACK_FILE__ "avrcp.c"

#include <stdint.h>
#include <string.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "classic/sdp_client.h"
#include "classic/sdp_util.h"
#include "classic/avrcp.h"
#include "classic/avrcp_controller.h"

static void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static const char * default_avrcp_controller_service_name = "BTstack AVRCP Controller Service";
static const char * default_avrcp_controller_service_provider_name = "BTstack AVRCP Controller Service Provider";
static const char * default_avrcp_target_service_name = "BTstack AVRCP Target Service";
static const char * default_avrcp_target_service_provider_name = "BTstack AVRCP Target Service Provider";

static uint16_t  avrcp_cid_counter = 0;

static avrcp_context_t * sdp_query_context;
static uint8_t   attribute_value[1000];
static const unsigned int attribute_value_buffer_size = sizeof(attribute_value);

static btstack_linked_list_t connections;
static btstack_packet_handler_t avrcp_controller_packet_handler;
static btstack_packet_handler_t avrcp_target_packet_handler;
static int l2cap_service_registered = 0;

static const char * avrcp_subunit_type_name[] = {
    "MONITOR", "AUDIO", "PRINTER", "DISC", "TAPE_RECORDER_PLAYER", "TUNER", 
    "CA", "CAMERA", "RESERVED", "PANEL", "BULLETIN_BOARD", "CAMERA_STORAGE", 
    "VENDOR_UNIQUE", "RESERVED_FOR_ALL_SUBUNIT_TYPES",
    "EXTENDED_TO_NEXT_BYTE", "UNIT", "ERROR"
};

const char * avrcp_subunit2str(uint16_t index){
    if (index <= 11) return avrcp_subunit_type_name[index];
    if (index >= 0x1C && index <= 0x1F) return avrcp_subunit_type_name[index - 0x10];
    return avrcp_subunit_type_name[16];
}

static const char * avrcp_event_name[] = {
    "ERROR", "PLAYBACK_STATUS_CHANGED",
    "TRACK_CHANGED", "TRACK_REACHED_END", "TRACK_REACHED_START",
    "PLAYBACK_POS_CHANGED", "BATT_STATUS_CHANGED", "SYSTEM_STATUS_CHANGED",
    "PLAYER_APPLICATION_SETTING_CHANGED", "NOW_PLAYING_CONTENT_CHANGED", 
    "AVAILABLE_PLAYERS_CHANGED", "ADDRESSED_PLAYER_CHANGED", "UIDS_CHANGED", "VOLUME_CHANGED"
};
const char * avrcp_event2str(uint16_t index){
    if (index <= 0x0d) return avrcp_event_name[index];
    return avrcp_event_name[0];
}

static const char * avrcp_operation_name[] = {
    "NOT SUPPORTED", // 0x3B
    "SKIP", "NOT SUPPORTED", "NOT SUPPORTED", "NOT SUPPORTED", "NOT SUPPORTED", 
    "VOLUME_UP", "VOLUME_DOWN", "MUTE", "PLAY", "STOP", "PAUSE", "NOT SUPPORTED",
    "REWIND", "FAST_FORWARD", "NOT SUPPORTED", "FORWARD", "BACKWARD" // 0x4C
};
const char * avrcp_operation2str(uint8_t index){
    if (index >= 0x3B && index <= 0x4C) return avrcp_operation_name[index - 0x3B];
    return avrcp_operation_name[0];
}

static const char * avrcp_media_attribute_id_name[] = {
    "NONE", "TITLE", "ARTIST", "ALBUM", "TRACK", "TOTAL TRACKS", "GENRE", "SONG LENGTH"
};
const char * avrcp_attribute2str(uint8_t index){
    if (index >= 1 && index <= 7) return avrcp_media_attribute_id_name[index];
    return avrcp_media_attribute_id_name[0];
}

static const char * avrcp_play_status_name[] = {
    "STOPPED", "PLAYING", "PAUSED", "FORWARD SEEK", "REVERSE SEEK",
    "ERROR" // 0xFF
};
const char * avrcp_play_status2str(uint8_t index){
    if (index >= 1 && index <= 4) return avrcp_play_status_name[index];
    return avrcp_play_status_name[5];
}

static const char * avrcp_ctype_name[] = {
    "CONTROL",
    "STATUS",
    "SPECIFIC_INQUIRY",
    "NOTIFY",
    "GENERAL_INQUIRY",
    "RESERVED5",
    "RESERVED6",
    "RESERVED7",
    "NOT IMPLEMENTED IN REMOTE",
    "ACCEPTED BY REMOTE",
    "REJECTED BY REMOTE",
    "IN_TRANSITION", 
    "IMPLEMENTED_STABLE",
    "CHANGED_STABLE",
    "RESERVED",
    "INTERIM"            
};
const char * avrcp_ctype2str(uint8_t index){
    if (index < sizeof(avrcp_ctype_name)){
        return avrcp_ctype_name[index];
    }
    return "NONE";
}

static const char * avrcp_shuffle_mode_name[] = {
    "SHUFFLE OFF",
    "SHUFFLE ALL TRACKS",
    "SHUFFLE GROUP"
};

const char * avrcp_shuffle2str(uint8_t index){
    if (index >= 1 && index <= 3) return avrcp_shuffle_mode_name[index-1];
    return "NONE";
}

static const char * avrcp_repeat_mode_name[] = {
    "REPEAT OFF",
    "REPEAT SINGLE TRACK",
    "REPEAT ALL TRACKS",
    "REPEAT GROUP"
};

const char * avrcp_repeat2str(uint8_t index){
    if (index >= 1 && index <= 4) return avrcp_repeat_mode_name[index-1];
    return "NONE";
}

uint8_t avrcp_cmd_opcode(uint8_t *packet, uint16_t size){
    uint8_t cmd_opcode_index = 5;
    if (cmd_opcode_index > size) return AVRCP_CMD_OPCODE_UNDEFINED;
    return packet[cmd_opcode_index];
}

void avrcp_create_sdp_record(uint8_t controller, uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, 
    const char * service_name, const char * service_provider_name){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        if (controller){
            de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL); 
            de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_CONTROLLER); 
        } else {
            de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_TARGET); 
        }
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
            de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, BLUETOOTH_PSM_AVCTP);  
        }
        de_pop_sequence(attribute, l2cpProtocol);
        
        uint8_t* avctpProtocol = de_push_sequence(attribute);
        {
            de_add_number(avctpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_AVCTP);  // avctpProtocol_service
            de_add_number(avctpProtocol,  DE_UINT, DE_SIZE_16,  0x0103);    // version
        }
        de_pop_sequence(attribute, avctpProtocol);
    }
    de_pop_sequence(service, attribute);

    // 0x0005 "Public Browse Group"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BROWSE_GROUP_LIST); // public browse group
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t *avrcProfile = de_push_sequence(attribute);
        {
            de_add_number(avrcProfile,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL); 
            de_add_number(avrcProfile,  DE_UINT, DE_SIZE_16, 0x0105); 
        }
        de_pop_sequence(attribute, avrcProfile);
    }
    de_pop_sequence(service, attribute);

    // 0x000d "Additional Bluetooth Profile Descriptor List"
    if (browsing){
        de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS);
        attribute = de_push_sequence(service);
        {
            uint8_t * des = de_push_sequence(attribute);
            {
                uint8_t* browsing_l2cpProtocol = de_push_sequence(des);
                {
                    de_add_number(browsing_l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
                    de_add_number(browsing_l2cpProtocol,  DE_UINT, DE_SIZE_16, BLUETOOTH_PSM_AVCTP_BROWSING);  
                }
                de_pop_sequence(des, browsing_l2cpProtocol);
                
                uint8_t* browsing_avctpProtocol = de_push_sequence(des);
                {
                    de_add_number(browsing_avctpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_AVCTP);  // browsing_avctpProtocol_service
                    de_add_number(browsing_avctpProtocol,  DE_UINT, DE_SIZE_16,  0x0103);    // version
                }
                de_pop_sequence(des, browsing_avctpProtocol);
            }
            de_pop_sequence(attribute, des);
        }
        de_pop_sequence(service, attribute);
    }


    // 0x0100 "Service Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    if (service_name){
        de_add_data(service,  DE_STRING, strlen(service_name), (uint8_t *) service_name);
    } else {
        if (controller){
            de_add_data(service,  DE_STRING, strlen(default_avrcp_controller_service_name), (uint8_t *) default_avrcp_controller_service_name);
        } else {
            de_add_data(service,  DE_STRING, strlen(default_avrcp_target_service_name), (uint8_t *) default_avrcp_target_service_name);
        }
    }

    // 0x0100 "Provider Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0102);
    if (service_provider_name){
        de_add_data(service,  DE_STRING, strlen(service_provider_name), (uint8_t *) service_provider_name);
    } else {
        if (controller){
            de_add_data(service,  DE_STRING, strlen(default_avrcp_controller_service_provider_name), (uint8_t *) default_avrcp_controller_service_provider_name);
        } else {
            de_add_data(service,  DE_STRING, strlen(default_avrcp_target_service_provider_name), (uint8_t *) default_avrcp_target_service_provider_name);
        }
    }

    // 0x0311 "Supported Features"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0311);
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_features);
}

avrcp_connection_t * get_avrcp_connection_for_bd_addr(avrcp_role_t role, bd_addr_t addr){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (memcmp(addr, connection->remote_addr, 6) != 0) continue;
        return connection;
    }
    return NULL;
}

avrcp_connection_t * get_avrcp_connection_for_l2cap_signaling_cid(avrcp_role_t role, uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->l2cap_signaling_cid != l2cap_cid) continue;
        return connection;
    }
    return NULL;
}

avrcp_connection_t * get_avrcp_connection_for_avrcp_cid(avrcp_role_t role, uint16_t avrcp_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->avrcp_cid != avrcp_cid) continue;
        return connection;
    }
    return NULL;
}

avrcp_connection_t * get_avrcp_connection_for_browsing_cid(avrcp_role_t role, uint16_t browsing_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->avrcp_browsing_cid != browsing_cid) continue;
        return connection;
    }
    return NULL;
}

avrcp_connection_t * get_avrcp_connection_for_browsing_l2cap_cid(avrcp_role_t role, uint16_t browsing_l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->browsing_connection &&  connection->browsing_connection->l2cap_browsing_cid != browsing_l2cap_cid) continue;
        return connection;
    }
    return NULL;
}

avrcp_browsing_connection_t * get_avrcp_browsing_connection_for_l2cap_cid(avrcp_role_t role, uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->browsing_connection && connection->browsing_connection->l2cap_browsing_cid != l2cap_cid) continue;
        return connection->browsing_connection;
    }
    return NULL;
}

void avrcp_request_can_send_now(avrcp_connection_t * connection, uint16_t l2cap_cid){
    // printf("AVRCP: avrcp_request_can_send_now, role %d\n", connection->role);
    connection->wait_to_send = 1;
    l2cap_request_can_send_now_event(l2cap_cid);
}


uint16_t avrcp_get_next_cid(avrcp_role_t role){
    do {
        if (avrcp_cid_counter == 0xffff) {
            avrcp_cid_counter = 1;
        } else {
            avrcp_cid_counter++;
        }
    } while (get_avrcp_connection_for_avrcp_cid(role, avrcp_cid_counter) !=  NULL) ;
    return avrcp_cid_counter;
}


static avrcp_connection_t * avrcp_create_connection(avrcp_role_t role, bd_addr_t remote_addr){
    avrcp_connection_t * connection = btstack_memory_avrcp_connection_get();
    if (!connection){
        log_error("Not enough memory to create connection for role %d", role);
        return NULL;
    }
    
    connection->state = AVCTP_CONNECTION_IDLE;
    connection->role = role;
    connection->transaction_label = 0xFF;
    connection->max_num_fragments = 0xFF;
    connection->avrcp_cid = avrcp_get_next_cid(role);
    log_info("avrcp_create_connection, role %d, avrcp cid 0x%02x", role, connection->avrcp_cid);
    memcpy(connection->remote_addr, remote_addr, 6);
    btstack_linked_list_add(&connections, (btstack_linked_item_t *) connection);
    return connection;
}

static void avrcp_finalize_connection(avrcp_connection_t * connection){
    btstack_linked_list_remove(&connections, (btstack_linked_item_t*) connection);
    btstack_memory_avrcp_connection_free(connection);
}

void avrcp_emit_connection_established(btstack_packet_handler_t callback, uint16_t avrcp_cid, bd_addr_t addr, uint8_t status){
    if (!callback) return;
    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_CONNECTION_ESTABLISHED;
    event[pos++] = status;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void avrcp_emit_connection_closed(btstack_packet_handler_t callback, uint16_t avrcp_cid){
    if (!callback) return;
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_CONNECTION_RELEASED;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void avrcp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(sdp_query_context->role, sdp_query_context->avrcp_cid);
    if (!connection) return;
    if (connection->state != AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE) return; 
    
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;
    des_iterator_t des_list_it;
    des_iterator_t prot_it;
    // uint32_t avdtp_remote_uuid    = 0;
    
    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            // Handle new SDP record 
            if (sdp_event_query_attribute_byte_get_record_id(packet) != sdp_query_context->record_id) {
                sdp_query_context->record_id = sdp_event_query_attribute_byte_get_record_id(packet);
                sdp_query_context->parse_sdp_record = 0;
                // log_info("SDP Record: Nr: %d", record_id);
            }

            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= attribute_value_buffer_size) {
                attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
                
                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {
                    switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        case BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST:
                            if (de_get_element_type(attribute_value) != DE_DES) break;
                            for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {
                                uint8_t * element = des_iterator_get_element(&des_list_it);
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uint32_t uuid = de_get_uuid32(element);
                                switch (uuid){
                                    case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_TARGET:
                                        if (sdp_query_context->role == AVRCP_CONTROLLER) {
                                            sdp_query_context->parse_sdp_record = 1;
                                            break;
                                        }
                                        break;
                                    case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL:
                                    case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_CONTROLLER:
                                        if (sdp_query_context->role == AVRCP_TARGET) {
                                            sdp_query_context->parse_sdp_record = 1;
                                            break;
                                        }
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST: {
                                if (!sdp_query_context->parse_sdp_record) break;
                                // log_info("SDP Attribute: 0x%04x", sdp_event_query_attribute_byte_get_attribute_id(packet));
                                for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {                                    
                                    uint8_t       *des_element;
                                    uint8_t       *element;
                                    uint32_t       uuid;

                                    if (des_iterator_get_type(&des_list_it) != DE_DES) continue;

                                    des_element = des_iterator_get_element(&des_list_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);
                                    
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    
                                    uuid = de_get_uuid32(element);
                                    des_iterator_next(&prot_it);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context->avrcp_l2cap_psm);
                                            break;
                                        case BLUETOOTH_PROTOCOL_AVCTP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context->avrcp_version);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS: {
                                // log_info("SDP Attribute: 0x%04x", sdp_event_query_attribute_byte_get_attribute_id(packet));
                                if (!sdp_query_context->parse_sdp_record) break;
                                if (de_get_element_type(attribute_value) != DE_DES) break;

                                des_iterator_t des_list_0_it;
                                uint8_t       *element_0; 

                                des_iterator_init(&des_list_0_it, attribute_value);
                                element_0 = des_iterator_get_element(&des_list_0_it);

                                for (des_iterator_init(&des_list_it, element_0); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {                                    
                                    uint8_t       *des_element;
                                    uint8_t       *element;
                                    uint32_t       uuid;

                                    if (des_iterator_get_type(&des_list_it) != DE_DES) continue;

                                    des_element = des_iterator_get_element(&des_list_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);

                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    
                                    uuid = de_get_uuid32(element);
                                    des_iterator_next(&prot_it);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &connection->browsing_l2cap_psm);
                                            break;
                                        case BLUETOOTH_PROTOCOL_AVCTP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &connection->browsing_version);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }
            } else {
                log_error("SDP attribute value buffer size exceeded: available %d, required %d", attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
            }
            break;
            
        case SDP_EVENT_QUERY_COMPLETE:{
            status = sdp_event_query_complete_get_status(packet);
    
            if (status != ERROR_CODE_SUCCESS){
                log_info("AVRCP: SDP query failed with status 0x%02x.", status);
                avrcp_emit_connection_established(sdp_query_context->avrcp_callback, connection->avrcp_cid, connection->remote_addr, status);
                avrcp_finalize_connection(connection);
                break;
            }

            if (!sdp_query_context->avrcp_l2cap_psm){
                connection->state = AVCTP_CONNECTION_IDLE;
                log_info("AVRCP: no suitable service found");
                avrcp_emit_connection_established(sdp_query_context->avrcp_callback, connection->avrcp_cid, connection->remote_addr, SDP_SERVICE_NOT_FOUND);
                avrcp_finalize_connection(connection);
                break;                
            } 
            connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
            l2cap_create_channel(&avrcp_packet_handler, connection->remote_addr, sdp_query_context->avrcp_l2cap_psm, l2cap_max_mtu(), NULL);
            break;
        }
    }
}

static btstack_packet_handler_t avrcp_packet_handler_for_role(avrcp_role_t role){
    switch (role){
        case AVRCP_CONTROLLER:
            return avrcp_controller_packet_handler;
        case AVRCP_TARGET:
            return avrcp_target_packet_handler;
    }
    return NULL;
}

static int avrcp_handle_incoming_connection_for_role(avrcp_role_t role, bd_addr_t event_addr, uint16_t local_cid){
    if (!avrcp_packet_handler_for_role(role)) {
        // printf("AVRCP: avrcp_handle_incoming_connection_for_role %d, PH not defined\n", role);
        return 0;
    }
    
    avrcp_connection_t * connection = avrcp_create_connection(role, event_addr);
    if (!connection) {
        // printf("AVRCP: avrcp_handle_incoming_connection_for_role %d, no connection created\n", role);
        return 0 ;  
    } 
    
    // printf("AVRCP: AVCTP_CONNECTION_W4_L2CAP_CONNECTED, role %d\n", role);
    connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
    connection->l2cap_signaling_cid = local_cid;
    return 1;
}

static void avrcp_handle_open_connection_for_role(avrcp_role_t role, bd_addr_t event_addr, uint16_t local_cid, uint16_t l2cap_mtu, uint8_t status){
    // printf("AVRCP: avrcp_handle_open_connection_for_role %d\n", role);

    btstack_packet_handler_t packet_handler = avrcp_packet_handler_for_role(role);
    if (!packet_handler) {
        // printf("AVRCP: avrcp_handle_open_connection_for_role %d, PH not defined\n", role);
        return;
    }
    avrcp_connection_t * connection = get_avrcp_connection_for_bd_addr(role, event_addr);
    if (!connection) {
        // printf("AVRCP: avrcp_handle_open_connection_for_role %d, no connection found\n", role);
        return;  
    } 

    if (connection->state == AVCTP_CONNECTION_OPENED) return;

    if (status != ERROR_CODE_SUCCESS){
        log_info("L2CAP connection to connection %s failed. status code 0x%02x", bd_addr_to_str(event_addr), status);
        avrcp_emit_connection_established(packet_handler, connection->avrcp_cid, event_addr, status);
        avrcp_finalize_connection(connection);
        return;
    }

    connection->l2cap_signaling_cid = local_cid;
    connection->l2cap_mtu = l2cap_mtu;
    connection->song_length_ms = 0xFFFFFFFF;
    connection->song_position_ms = 0xFFFFFFFF;
    connection->playback_status = AVRCP_PLAYBACK_STATUS_ERROR;
    
    log_info("L2CAP_EVENT_CHANNEL_OPENED avrcp_cid 0x%02x, l2cap_signaling_cid 0x%02x, role %d", connection->avrcp_cid, connection->l2cap_signaling_cid, connection->role);
    connection->state = AVCTP_CONNECTION_OPENED;
    // emit twice for each role
    avrcp_emit_connection_established(packet_handler, connection->avrcp_cid, event_addr, ERROR_CODE_SUCCESS);
}

static void avrcp_handle_close_connection_for_role(avrcp_role_t role,uint16_t local_cid){
    // printf("AVRCP: avrcp_handle_close_connection_for_role %d\n", role);
    btstack_packet_handler_t packet_handler = avrcp_packet_handler_for_role(role);
    if (!packet_handler) return;
    
    avrcp_connection_t * connection = get_avrcp_connection_for_l2cap_signaling_cid(role, local_cid);
    if (!connection) return;

    // printf("avrcp_handle_close_connection_for_role avrcp_cid 0x%02x, l2cap_signaling_cid 0x%02x, role %d\n", connection->avrcp_cid, connection->l2cap_signaling_cid, role);
   
    avrcp_emit_connection_closed(packet_handler, connection->avrcp_cid);
    avrcp_finalize_connection(connection);
}

static avrcp_role_t avrcp_role_from_transport_header(uint8_t transport_header){
    avrcp_frame_type_t frame_type = (avrcp_frame_type_t)((transport_header & 0x02) >> 1);
    switch (frame_type){
        case AVRCP_COMMAND_FRAME:
            return AVRCP_TARGET;
        default: // AVRCP_RESPONSE_FRAME - make compiler happy
            return AVRCP_CONTROLLER;
    }
}

static void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint16_t l2cap_mtu;
    uint8_t  status;
    avrcp_connection_t * connection = NULL;
    avrcp_role_t role;
    btstack_packet_handler_t packet_handler;
    uint8_t status_target;
    uint8_t status_controller;

    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            role = avrcp_role_from_transport_header(packet[0]);
            packet_handler = avrcp_packet_handler_for_role(role);
            if (!packet_handler) return;
            
            (*packet_handler)(packet_type, channel, packet, size);
            break;

        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {

                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    // create two connection objects (both)
                    status_target = avrcp_handle_incoming_connection_for_role(AVRCP_TARGET, event_addr, local_cid);
                    status_controller = avrcp_handle_incoming_connection_for_role(AVRCP_CONTROLLER, event_addr, local_cid);

                    if (!status_target && !status_controller){
                        l2cap_decline_connection(local_cid);
                        break;
                    }

                    log_info("AVRCP: L2CAP_EVENT_INCOMING_CONNECTION avrcp_cid 0x%02x", local_cid);
                    l2cap_accept_connection(local_cid);
                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    // printf("AVRCP: L2CAP_EVENT_CHANNEL_OPENED \n");
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    status = l2cap_event_channel_opened_get_status(packet);
                    local_cid = l2cap_event_channel_opened_get_local_cid(packet);
                    l2cap_mtu = l2cap_event_channel_opened_get_remote_mtu(packet);
                    
                    avrcp_handle_open_connection_for_role(AVRCP_TARGET, event_addr, local_cid, l2cap_mtu, status);
                    avrcp_handle_open_connection_for_role(AVRCP_CONTROLLER, event_addr, local_cid, l2cap_mtu, status);
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    // data: event (8), len(8), channel (16)
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    avrcp_handle_close_connection_for_role(AVRCP_TARGET, local_cid);
                    avrcp_handle_close_connection_for_role(AVRCP_CONTROLLER, local_cid);
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    local_cid = l2cap_event_can_send_now_get_local_cid(packet);
                    connection = get_avrcp_connection_for_l2cap_signaling_cid(AVRCP_TARGET, local_cid);
                    if (connection && connection->wait_to_send){
                        // printf("AVRCP: L2CAP_EVENT_CAN_SEND_NOW target\n");
                        connection->wait_to_send = 0;
                        (*avrcp_target_packet_handler)(HCI_EVENT_PACKET, channel, packet, size);
                        break;    
                    }

                    connection = get_avrcp_connection_for_l2cap_signaling_cid(AVRCP_CONTROLLER, local_cid);
                    if (connection && connection->wait_to_send){
                        // printf("AVRCP: L2CAP_EVENT_CAN_SEND_NOW controller\n");
                        connection->wait_to_send = 0;
                        (*avrcp_controller_packet_handler)(HCI_EVENT_PACKET, channel, packet, size);
                        break;    
                    }
                    break;

                default:
                    break;
            }
            break;
        default:
            break;
    }
}

uint8_t avrcp_connect(avrcp_role_t role, bd_addr_t bd_addr, avrcp_context_t * context, uint16_t * avrcp_cid){
    avrcp_connection_t * connection = get_avrcp_connection_for_bd_addr(context->role, bd_addr);
    if (connection) return ERROR_CODE_COMMAND_DISALLOWED;

    if (!sdp_client_ready()) return ERROR_CODE_COMMAND_DISALLOWED;
    
    btstack_packet_handler_t packet_handler = avrcp_packet_handler_for_role(role);
    if (!packet_handler) return 0;

    connection = avrcp_create_connection(role, bd_addr);
    if (!connection) return BTSTACK_MEMORY_ALLOC_FAILED;
    
    if (avrcp_cid){
        *avrcp_cid = connection->avrcp_cid;
    }
    context->avrcp_l2cap_psm = 0;
    context->avrcp_version = 0;
    context->avrcp_cid = connection->avrcp_cid;
    connection->browsing_l2cap_psm = 0;
    connection->state = AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE;
    sdp_query_context = context;
    sdp_client_query_uuid16(&avrcp_handle_sdp_client_query_result, connection->remote_addr, BLUETOOTH_PROTOCOL_AVCTP);
    return ERROR_CODE_SUCCESS;
}

void avrcp_init(void){
    connections = NULL;
    if (l2cap_service_registered) return;

    int status = l2cap_register_service(&avrcp_packet_handler, BLUETOOTH_PSM_AVCTP, 0xffff, LEVEL_2);
    if (status != ERROR_CODE_SUCCESS) return;
    l2cap_service_registered = 1;
}

void avrcp_register_controller_packet_handler(btstack_packet_handler_t callback){
    avrcp_controller_packet_handler = callback;
}

void avrcp_register_target_packet_handler(btstack_packet_handler_t callback){
    avrcp_target_packet_handler = callback;
}

