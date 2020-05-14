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
static avrcp_context_t avrcp_context;

static btstack_packet_handler_t avrcp_callback;

static uint8_t   attribute_value[45];
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
    if ((index >= 0x1C) && (index <= 0x1F)) return avrcp_subunit_type_name[index - 0x10];
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
    if ((index >= 0x3B) && (index <= 0x4C)) return avrcp_operation_name[index - 0x3B];
    return avrcp_operation_name[0];
}

static const char * avrcp_media_attribute_id_name[] = {
    "NONE", "TITLE", "ARTIST", "ALBUM", "TRACK", "TOTAL TRACKS", "GENRE", "SONG LENGTH"
};
const char * avrcp_attribute2str(uint8_t index){
    if ((index >= 1) && (index <= 7)) return avrcp_media_attribute_id_name[index];
    return avrcp_media_attribute_id_name[0];
}

static const char * avrcp_play_status_name[] = {
    "STOPPED", "PLAYING", "PAUSED", "FORWARD SEEK", "REVERSE SEEK",
    "ERROR" // 0xFF
};
const char * avrcp_play_status2str(uint8_t index){
    if ((index >= 1) && (index <= 4)) return avrcp_play_status_name[index];
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
    if ((index >= 1) && (index <= 3)) return avrcp_shuffle_mode_name[index-1];
    return "NONE";
}

static const char * avrcp_repeat_mode_name[] = {
    "REPEAT OFF",
    "REPEAT SINGLE TRACK",
    "REPEAT ALL TRACKS",
    "REPEAT GROUP"
};

const char * avrcp_repeat2str(uint8_t index){
    if ((index >= 1) && (index <= 4)) return avrcp_repeat_mode_name[index-1];
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

avrcp_connection_t * get_avrcp_connection_for_bd_addr_for_role(avrcp_role_t role, bd_addr_t addr){
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

avrcp_connection_t * get_avrcp_connection_for_l2cap_signaling_cid_for_role(avrcp_role_t role, uint16_t l2cap_cid){
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

avrcp_connection_t * get_avrcp_connection_for_avrcp_cid_for_role(avrcp_role_t role, uint16_t avrcp_cid){
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

avrcp_connection_t * get_avrcp_connection_for_browsing_cid_for_role(avrcp_role_t role, uint16_t browsing_cid){
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

avrcp_connection_t * get_avrcp_connection_for_browsing_l2cap_cid_for_role(avrcp_role_t role, uint16_t browsing_l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->browsing_connection &&  (connection->browsing_connection->l2cap_browsing_cid != browsing_l2cap_cid)) continue;
        return connection;
    }
    return NULL;
}

avrcp_browsing_connection_t * get_avrcp_browsing_connection_for_l2cap_cid_for_role(avrcp_role_t role, uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->browsing_connection && (connection->browsing_connection->l2cap_browsing_cid != l2cap_cid)) continue;
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
    } while (get_avrcp_connection_for_avrcp_cid_for_role(role, avrcp_cid_counter) !=  NULL) ;
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
    log_info("avrcp_create_connection, role %d, avrcp cid 0x%02x", role, connection->avrcp_cid);
    (void)memcpy(connection->remote_addr, remote_addr, 6);
    btstack_linked_list_add(&connections, (btstack_linked_item_t *) connection);
    return connection;
}

static void avrcp_finalize_connection(avrcp_connection_t * connection){
    btstack_run_loop_remove_timer(&connection->reconnect_timer);
    btstack_linked_list_remove(&connections, (btstack_linked_item_t*) connection);
    btstack_memory_avrcp_connection_free(connection);
}

static void avrcp_emit_connection_established(uint16_t avrcp_cid, bd_addr_t addr, uint8_t status){
    btstack_assert(avrcp_callback != NULL);

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
    (*avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_emit_connection_closed(uint16_t avrcp_cid){
    btstack_assert(avrcp_callback != NULL);

    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_CONNECTION_RELEASED;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    (*avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_handle_sdp_client_query_attribute_value(uint8_t *packet){
    des_iterator_t des_list_it;
    des_iterator_t prot_it;

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
                                de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context->browsing_l2cap_psm);
                                break;
                            case BLUETOOTH_PROTOCOL_AVCTP:
                                if (!des_iterator_has_more(&prot_it)) continue;
                                de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context->browsing_version);
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
}

static void avrcp_handle_sdp_query_failed(avrcp_connection_t * connection, uint8_t status){
    if (connection == NULL) return;
    log_info("AVRCP: SDP query failed with status 0x%02x.", status);
    avrcp_emit_connection_established(connection->avrcp_cid, connection->remote_addr, status);
    avrcp_finalize_connection(connection);
}

static void avrcp_handle_sdp_query_succeeded(avrcp_connection_t * connection){
    if (connection == NULL) return;
    connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
    connection->avrcp_l2cap_psm = sdp_query_context->avrcp_l2cap_psm;
    connection->browsing_version = sdp_query_context->browsing_version;
    connection->browsing_l2cap_psm = sdp_query_context->browsing_l2cap_psm;
}

static void avrcp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    avrcp_connection_t * avrcp_target_connection = get_avrcp_connection_for_bd_addr_for_role(AVRCP_TARGET, sdp_query_context->remote_addr);
    avrcp_connection_t * avrcp_controller_connection = get_avrcp_connection_for_bd_addr_for_role(AVRCP_CONTROLLER, sdp_query_context->remote_addr);
    
    uint8_t status;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            avrcp_handle_sdp_client_query_attribute_value(packet);
            break;
            
        case SDP_EVENT_QUERY_COMPLETE:
            status = sdp_event_query_complete_get_status(packet);
    
            if (status != ERROR_CODE_SUCCESS){
                avrcp_handle_sdp_query_failed(avrcp_controller_connection, status);
                avrcp_handle_sdp_query_failed(avrcp_target_connection, status);
                break;
            }

            if (!sdp_query_context->avrcp_l2cap_psm){
                avrcp_handle_sdp_query_failed(avrcp_controller_connection, SDP_SERVICE_NOT_FOUND);
                avrcp_handle_sdp_query_failed(avrcp_target_connection, SDP_SERVICE_NOT_FOUND);
                break;                
            } 
            
            avrcp_handle_sdp_query_succeeded(avrcp_controller_connection);
            avrcp_handle_sdp_query_succeeded(avrcp_target_connection);

            l2cap_create_channel(&avrcp_packet_handler, sdp_query_context->remote_addr, sdp_query_context->avrcp_l2cap_psm, l2cap_max_mtu(), NULL);
            break;
       
        default:
            break;

    }
}


static avrcp_connection_t * avrcp_handle_incoming_connection_for_role(avrcp_role_t role, avrcp_connection_t * connection, bd_addr_t event_addr, uint16_t local_cid, uint16_t avrcp_cid){
    if (connection == NULL){
        connection = avrcp_create_connection(role, event_addr);
    }
    if (connection) {
        connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
        connection->l2cap_signaling_cid = local_cid;
        connection->avrcp_cid = avrcp_cid;
        btstack_run_loop_remove_timer(&connection->reconnect_timer);
    } 
    return connection;
}

static void avrcp_handle_open_connection_for_role( avrcp_connection_t * connection, uint16_t local_cid, uint16_t l2cap_mtu){
    connection->l2cap_signaling_cid = local_cid;
    connection->l2cap_mtu = l2cap_mtu;
    connection->incoming_declined = false;
    connection->song_length_ms = 0xFFFFFFFF;
    connection->song_position_ms = 0xFFFFFFFF;
    connection->playback_status = AVRCP_PLAYBACK_STATUS_ERROR;
    connection->state = AVCTP_CONNECTION_OPENED;
    
    log_info("L2CAP_EVENT_CHANNEL_OPENED avrcp_cid 0x%02x, l2cap_signaling_cid 0x%02x, role %d", connection->avrcp_cid, connection->l2cap_signaling_cid, connection->role);
}

static void avrcp_reconnect_timer_timeout_handler(btstack_timer_source_t * timer){
    uint16_t avrcp_cid = (uint16_t)(uintptr_t) btstack_run_loop_get_timer_context(timer);
    avrcp_connection_t * controller_connection = get_avrcp_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (controller_connection == NULL) return;
    avrcp_connection_t * target_connection = get_avrcp_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (target_connection == NULL) return;

    controller_connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
    target_connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
    
    l2cap_create_channel(&avrcp_packet_handler, controller_connection->remote_addr, controller_connection->avrcp_l2cap_psm, l2cap_max_mtu(), NULL);
}

static void avrcp_reconnect_timer_start(avrcp_connection_t * connection){
    btstack_run_loop_set_timer_handler(&connection->reconnect_timer, avrcp_reconnect_timer_timeout_handler);
    btstack_run_loop_set_timer_context(&connection->reconnect_timer, (void *)(uintptr_t)connection->avrcp_cid);

    // add some jitter/randomness to reconnect delay
    uint32_t timeout = 100 + (btstack_run_loop_get_time_ms() & 0x7F);
    btstack_run_loop_set_timer(&connection->reconnect_timer, timeout); 
    
    btstack_run_loop_add_timer(&connection->reconnect_timer);
}

static void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint16_t l2cap_mtu;
    uint8_t  status;
    avrcp_frame_type_t frame_type;
    bool decline_connection;
    bool outoing_active;

    avrcp_connection_t * connection_controller;
    avrcp_connection_t * connection_target;
    
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {

                case L2CAP_EVENT_INCOMING_CONNECTION:
                    btstack_assert(avrcp_controller_packet_handler != NULL);
                    btstack_assert(avrcp_target_packet_handler != NULL);

                    log_info("incoming connection");

                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    outoing_active = false;
                    
                    connection_target = get_avrcp_connection_for_bd_addr_for_role(AVRCP_TARGET, event_addr);
                    if (connection_target != NULL){
                        if (connection_target->state == AVCTP_CONNECTION_W4_L2CAP_CONNECTED){
                            outoing_active = true;
                            connection_target->incoming_declined = true;
                        }
                    }
                    
                    connection_controller = get_avrcp_connection_for_bd_addr_for_role(AVRCP_CONTROLLER, event_addr);
                    if (connection_controller != NULL){
                        if (connection_controller->state == AVCTP_CONNECTION_W4_L2CAP_CONNECTED) {
                            outoing_active = true;
                            connection_controller->incoming_declined = true;
                        }
                    }

                    decline_connection = outoing_active;
                    if (decline_connection == false){
                        uint16_t avrcp_cid;
                        if ((connection_controller == NULL) || (connection_target == NULL)){
                            avrcp_cid = avrcp_get_next_cid(AVRCP_CONTROLLER);
                        } else {
                            avrcp_cid = connection_controller->avrcp_cid;
                        }
                        // create two connection objects (both)
                        connection_target     = avrcp_handle_incoming_connection_for_role(AVRCP_TARGET, connection_target, event_addr, local_cid, avrcp_cid);
                        connection_controller = avrcp_handle_incoming_connection_for_role(AVRCP_CONTROLLER, connection_controller, event_addr, local_cid, avrcp_cid);
                        if ((connection_target == NULL) || (connection_controller == NULL)){
                            decline_connection = true;
                            if (connection_target) {
                                avrcp_finalize_connection(connection_target);
                            } 
                            if (connection_controller) {
                                avrcp_finalize_connection(connection_controller);
                            }
                        }
                    }
                    if (decline_connection){
                        l2cap_decline_connection(local_cid);
                    } else {
                        log_info("AVRCP: L2CAP_EVENT_INCOMING_CONNECTION avrcp_cid 0x%02x", local_cid);
                        l2cap_accept_connection(local_cid);
                    }
                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    status = l2cap_event_channel_opened_get_status(packet);
                    local_cid = l2cap_event_channel_opened_get_local_cid(packet);
                    l2cap_mtu = l2cap_event_channel_opened_get_remote_mtu(packet);

                    connection_controller = get_avrcp_connection_for_bd_addr_for_role(AVRCP_CONTROLLER, event_addr);
                    connection_target = get_avrcp_connection_for_bd_addr_for_role(AVRCP_TARGET, event_addr);

                    // incoming: structs are already created in L2CAP_EVENT_INCOMING_CONNECTION
                    // outgoing: structs are cteated in avrcp_connect()
                    if ((connection_controller == NULL) || (connection_target == NULL)) {
                        break;
                    }

                    switch (status){
                        case ERROR_CODE_SUCCESS:
                            avrcp_handle_open_connection_for_role(connection_target, local_cid, l2cap_mtu);
                            avrcp_handle_open_connection_for_role(connection_controller, local_cid, l2cap_mtu);
                            avrcp_emit_connection_established(connection_controller->avrcp_cid, event_addr, status);
                            return;
                        case L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES: 
                            if (connection_controller->incoming_declined == true){
                                log_info("Incoming connection was declined, and the outgoing failed");
                                connection_controller->state = AVCTP_CONNECTION_W2_L2CAP_RECONNECT;
                                connection_controller->incoming_declined = false;
                                connection_target->state = AVCTP_CONNECTION_W2_L2CAP_RECONNECT;
                                connection_target->incoming_declined = false;
                                avrcp_reconnect_timer_start(connection_controller);
                                return;
                            } 
                            break;
                        default:
                            break;
                    }
                    log_info("L2CAP connection to connection %s failed. status code 0x%02x", bd_addr_to_str(event_addr), status);
                    avrcp_emit_connection_established(connection_controller->avrcp_cid, event_addr, status);
                    avrcp_finalize_connection(connection_controller);
                    avrcp_finalize_connection(connection_target);
                    
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    
                    connection_controller = get_avrcp_connection_for_l2cap_signaling_cid_for_role(AVRCP_CONTROLLER, local_cid);
                    connection_target = get_avrcp_connection_for_l2cap_signaling_cid_for_role(AVRCP_TARGET, local_cid);
                    if ((connection_controller == NULL) || (connection_target == NULL)) {
                        break;
                    }
                    avrcp_emit_connection_closed(connection_controller->avrcp_cid);
                    avrcp_finalize_connection(connection_controller);

                    avrcp_emit_connection_closed(connection_target->avrcp_cid);
                    avrcp_finalize_connection(connection_target);
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    local_cid = l2cap_event_can_send_now_get_local_cid(packet);
                    
                    connection_target = get_avrcp_connection_for_l2cap_signaling_cid_for_role(AVRCP_TARGET, local_cid);
                    if (connection_target && connection_target->wait_to_send){
                        connection_target->wait_to_send = 0;
                        (*avrcp_target_packet_handler)(HCI_EVENT_PACKET, channel, packet, size);
                        break;    
                    }

                    connection_controller = get_avrcp_connection_for_l2cap_signaling_cid_for_role(AVRCP_CONTROLLER, local_cid);
                    if (connection_controller && connection_controller->wait_to_send){
                        connection_controller->wait_to_send = 0;
                        (*avrcp_controller_packet_handler)(HCI_EVENT_PACKET, channel, packet, size);
                        break;    
                    }
                    break;

                default:
                    break;
            }
            break;

        case L2CAP_DATA_PACKET:
            frame_type = (avrcp_frame_type_t)((packet[0] & 0x02) >> 1);

            switch (frame_type){
                case AVRCP_RESPONSE_FRAME:
                    (*avrcp_controller_packet_handler)(packet_type, channel, packet, size);
                    break;
                case AVRCP_COMMAND_FRAME:
                default:    // make compiler happy
                    (*avrcp_target_packet_handler)(packet_type, channel, packet, size);
                    break;
            }
            break;

        default:
            break;
    }
}

uint8_t avrcp_disconnect(uint16_t avrcp_cid){
    avrcp_connection_t * connection_controller = get_avrcp_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection_controller){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    avrcp_connection_t * connection_target = get_avrcp_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection_target){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection_controller->browsing_connection){
        l2cap_disconnect(connection_controller->browsing_connection->l2cap_browsing_cid, 0);
    }
    l2cap_disconnect(connection_controller->l2cap_signaling_cid, 0);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_connect(bd_addr_t remote_addr, uint16_t * avrcp_cid){
    btstack_assert(avrcp_controller_packet_handler != NULL);
    btstack_assert(avrcp_target_packet_handler != NULL);

    // TODO: implement delayed SDP query
    if (sdp_client_ready() == 0){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 
    
    avrcp_connection_t * connection_controller = get_avrcp_connection_for_bd_addr_for_role(AVRCP_CONTROLLER, remote_addr);
    if (connection_controller){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    avrcp_connection_t * connection_target = get_avrcp_connection_for_bd_addr_for_role(AVRCP_TARGET, remote_addr);
    if (connection_target){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint16_t cid = avrcp_get_next_cid(AVRCP_CONTROLLER);
    
    connection_controller = avrcp_create_connection(AVRCP_CONTROLLER, remote_addr);
    if (!connection_controller) return BTSTACK_MEMORY_ALLOC_FAILED;
    
    connection_target = avrcp_create_connection(AVRCP_TARGET, remote_addr);
    if (!connection_target){
        avrcp_finalize_connection(connection_controller);
        return BTSTACK_MEMORY_ALLOC_FAILED;
    } 

    if (avrcp_cid != NULL){
        *avrcp_cid = cid;
    }

    connection_controller->state = AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE;
    connection_controller->avrcp_cid = cid;

    connection_target->state     = AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE;
    connection_target->avrcp_cid = cid;

    sdp_query_context = &avrcp_context;
    sdp_query_context->avrcp_l2cap_psm = 0;
    sdp_query_context->avrcp_version  = 0;
    sdp_query_context->avrcp_cid = cid;
    memcpy(sdp_query_context->remote_addr, remote_addr, 6);

    sdp_client_query_uuid16(&avrcp_handle_sdp_client_query_result, remote_addr, BLUETOOTH_PROTOCOL_AVCTP);
    return ERROR_CODE_SUCCESS;
}

void avrcp_init(void){
    connections = NULL;
    if (l2cap_service_registered) return;

    int status = l2cap_register_service(&avrcp_packet_handler, BLUETOOTH_PSM_AVCTP, 0xffff, gap_get_security_level());
    if (status != ERROR_CODE_SUCCESS) return;
    l2cap_service_registered = 1;
}

void avrcp_register_controller_packet_handler(btstack_packet_handler_t callback){
    avrcp_controller_packet_handler = callback;
}

void avrcp_register_target_packet_handler(btstack_packet_handler_t callback){
    avrcp_target_packet_handler = callback;
}

void avrcp_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    avrcp_callback = callback;
}

// AVRCP Browsing Service functions
avrcp_browsing_connection_t * avrcp_browsing_create_connection(avrcp_connection_t * avrcp_connection){
    avrcp_browsing_connection_t * connection = btstack_memory_avrcp_browsing_connection_get();
    connection->state = AVCTP_CONNECTION_IDLE;
    connection->transaction_label = 0xFF;
    avrcp_connection->avrcp_browsing_cid = avrcp_get_next_cid(avrcp_connection->role);
    avrcp_connection->browsing_connection = connection;
    return connection;
}

void avrcp_emit_browsing_connection_established(btstack_packet_handler_t callback, uint16_t browsing_cid, bd_addr_t addr, uint8_t status){
    btstack_assert(callback != NULL);
    
    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED;
    event[pos++] = status;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void avrcp_emit_incoming_browsing_connection(btstack_packet_handler_t callback, uint16_t browsing_cid, bd_addr_t addr){
    btstack_assert(callback != NULL);
    
    uint8_t event[11];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_INCOMING_BROWSING_CONNECTION;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void avrcp_emit_browsing_connection_closed(btstack_packet_handler_t callback, uint16_t browsing_cid){
    btstack_assert(callback != NULL);
    
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_CONNECTION_RELEASED;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void avrcp_browser_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avrcp_context_t * context){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status;
    avrcp_browsing_connection_t * browsing_connection = NULL;
    avrcp_connection_t * avrcp_connection = NULL;
    
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            avrcp_emit_browsing_connection_closed(context->browsing_avrcp_callback, 0);
            break;
        case L2CAP_EVENT_INCOMING_CONNECTION:
            l2cap_event_incoming_connection_get_address(packet, event_addr);
            local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
            avrcp_connection = get_avrcp_connection_for_bd_addr_for_role(context->role, event_addr);
            if (!avrcp_connection) {
                log_error("No previously created AVRCP controller connections");
                l2cap_decline_connection(local_cid);
                break;
            }
            browsing_connection = avrcp_browsing_create_connection(avrcp_connection);
            browsing_connection->l2cap_browsing_cid = local_cid;
            browsing_connection->state = AVCTP_CONNECTION_W4_ERTM_CONFIGURATION;
            log_info("Emit AVRCP_SUBEVENT_INCOMING_BROWSING_CONNECTION browsing_cid 0x%02x, l2cap_signaling_cid 0x%02x\n", avrcp_connection->avrcp_browsing_cid, browsing_connection->l2cap_browsing_cid);
            avrcp_emit_incoming_browsing_connection(context->browsing_avrcp_callback, avrcp_connection->avrcp_browsing_cid, event_addr);
            break;
            
        case L2CAP_EVENT_CHANNEL_OPENED:
            l2cap_event_channel_opened_get_address(packet, event_addr);
            status = l2cap_event_channel_opened_get_status(packet);
            local_cid = l2cap_event_channel_opened_get_local_cid(packet);
            
            avrcp_connection = get_avrcp_connection_for_bd_addr_for_role(context->role, event_addr);
            if (!avrcp_connection){
                log_error("Failed to find AVRCP connection for bd_addr %s", bd_addr_to_str(event_addr));
                avrcp_emit_browsing_connection_established(context->browsing_avrcp_callback, local_cid, event_addr, L2CAP_LOCAL_CID_DOES_NOT_EXIST);
                l2cap_disconnect(local_cid, 0); // reason isn't used
                break;
            }

            browsing_connection = avrcp_connection->browsing_connection;
            if (status != ERROR_CODE_SUCCESS){
                log_info("L2CAP connection to connection %s failed. status code 0x%02x", bd_addr_to_str(event_addr), status);
                avrcp_emit_browsing_connection_established(context->browsing_avrcp_callback, avrcp_connection->avrcp_browsing_cid, event_addr, status);
                btstack_memory_avrcp_browsing_connection_free(browsing_connection);
                avrcp_connection->browsing_connection = NULL;
                break;
            }
            if (browsing_connection->state != AVCTP_CONNECTION_W4_L2CAP_CONNECTED) break;
            
            browsing_connection->l2cap_browsing_cid = local_cid;

            log_info("L2CAP_EVENT_CHANNEL_OPENED browsing cid 0x%02x, l2cap cid 0x%02x", avrcp_connection->avrcp_browsing_cid, browsing_connection->l2cap_browsing_cid);
            browsing_connection->state = AVCTP_CONNECTION_OPENED;
            avrcp_emit_browsing_connection_established(context->browsing_avrcp_callback, avrcp_connection->avrcp_browsing_cid, event_addr, ERROR_CODE_SUCCESS);
            break;
        
        case L2CAP_EVENT_CHANNEL_CLOSED:
            local_cid = l2cap_event_channel_closed_get_local_cid(packet);
            avrcp_connection = get_avrcp_connection_for_browsing_l2cap_cid_for_role(context->role, local_cid);
            
            if (avrcp_connection && avrcp_connection->browsing_connection){
                avrcp_emit_browsing_connection_closed(context->browsing_avrcp_callback, avrcp_connection->avrcp_browsing_cid);
                // free connection
                btstack_memory_avrcp_browsing_connection_free(avrcp_connection->browsing_connection);
                avrcp_connection->browsing_connection = NULL;
                break;
            }
            break;
        default:
            break;
    }
}

uint8_t avrcp_browsing_connect(bd_addr_t remote_addr, avrcp_role_t avrcp_role, btstack_packet_handler_t avrcp_browsing_packet_handler, uint8_t * ertm_buffer, uint32_t ertm_buffer_size, l2cap_ertm_config_t * ertm_config, uint16_t * avrcp_browsing_cid){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_bd_addr_for_role(avrcp_role, remote_addr);
    
    if (!avrcp_connection){
        log_error("avrcp: there is no previously established AVRCP controller connection.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (connection){
        log_error(" avrcp_browsing_connect connection exists.");
        return ERROR_CODE_SUCCESS;
    }
    
    connection = avrcp_browsing_create_connection(avrcp_connection);
    if (!connection){
        log_error("avrcp: could not allocate connection struct.");
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
    
    if (avrcp_browsing_cid){
        *avrcp_browsing_cid = avrcp_connection->avrcp_browsing_cid; 
    }

    connection->ertm_buffer = ertm_buffer;
    connection->ertm_buffer_size = ertm_buffer_size;
    avrcp_connection->browsing_connection = connection;
    avrcp_connection->browsing_connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
    (void)memcpy(&connection->ertm_config, ertm_config,
                 sizeof(l2cap_ertm_config_t));

    return l2cap_create_ertm_channel(avrcp_browsing_packet_handler, remote_addr, avrcp_connection->browsing_l2cap_psm, 
                    &connection->ertm_config, connection->ertm_buffer, connection->ertm_buffer_size, NULL);

}

uint8_t avrcp_browsing_disconnect(uint16_t avrcp_browsing_cid, avrcp_role_t avrcp_role){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid_for_role(avrcp_role, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_disconnect: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (avrcp_connection->browsing_connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    l2cap_disconnect(avrcp_connection->browsing_connection->l2cap_browsing_cid, 0);
    return ERROR_CODE_SUCCESS;
}
