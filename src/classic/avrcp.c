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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
// snprintf
#include <stdio.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "classic/avrcp.h"
#include "classic/sdp_client.h"
#include "classic/sdp_util.h"


typedef struct {
    uint8_t  parse_sdp_record;
    uint32_t record_id;
    uint16_t avrcp_cid;
    uint16_t avrcp_l2cap_psm;
    uint16_t avrcp_version;

    uint16_t browsing_l2cap_psm;
    uint16_t browsing_version;
    uint16_t cover_art_l2cap_psm;
} avrcp_sdp_query_context_t; 

static void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void avrcp_start_next_sdp_query(void);

static const char * avrcp_subunit_type_name[] = {
        "MONITOR", "AUDIO", "PRINTER", "DISC", "TAPE_RECORDER_PLAYER", "TUNER",
        "CA", "CAMERA", "RESERVED", "PANEL", "BULLETIN_BOARD", "CAMERA_STORAGE",
        "VENDOR_UNIQUE", "RESERVED_FOR_ALL_SUBUNIT_TYPES",
        "EXTENDED_TO_NEXT_BYTE", "UNIT", "ERROR"
};

// default subunit info: single PANEL subunit
static const uint8_t avrcp_default_subunit_info[] = { AVRCP_SUBUNIT_TYPE_PANEL << 3};

// globals
static bool avrcp_l2cap_service_registered = false;

// connections
static uint16_t                 avrcp_cid_counter;
static btstack_linked_list_t    avrcp_connections;

// higher layer callbacks
static btstack_packet_handler_t avrcp_callback;
static btstack_packet_handler_t avrcp_controller_packet_handler;
static btstack_packet_handler_t avrcp_target_packet_handler;

// sdp query
static btstack_context_callback_registration_t avrcp_sdp_query_registration;
static avrcp_sdp_query_context_t               avrcp_sdp_query_context;
static uint8_t                                 avrcp_sdp_query_attribute_value[45];
static const unsigned int                      avrcp_sdp_query_attribute_value_buffer_size = sizeof(avrcp_sdp_query_attribute_value);

static void (*avrcp_browsing_sdp_query_complete_handler)(avrcp_connection_t * connection, uint8_t status);
#ifdef ENABLE_AVRCP_COVER_ART
static void (*avrcp_cover_art_sdp_query_complete_handler)(avrcp_connection_t * connection, uint8_t status);
#endif

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
    "SKIP", NULL, NULL, NULL, NULL, 
    "VOLUME_UP", "VOLUME_DOWN", "MUTE", "PLAY", "STOP", "PAUSE", NULL,
    "REWIND", "FAST_FORWARD", NULL, "FORWARD", "BACKWARD" // 0x4C
};

const char * avrcp_operation2str(uint8_t operation_id){
    char * name = NULL;
    if ((operation_id >= AVRCP_OPERATION_ID_SKIP) && (operation_id <= AVRCP_OPERATION_ID_BACKWARD)){
        name = (char *)avrcp_operation_name[operation_id - AVRCP_OPERATION_ID_SKIP];
    } 
    if (name == NULL){
        static char buffer[13];
        snprintf(buffer, sizeof(buffer), "Unknown 0x%02x", operation_id);
        buffer[sizeof(buffer)-1] = 0;
        return buffer;
    } else {
        return name;
    }
}

static const char * avrcp_media_attribute_id_name[] = {
    "NONE", "TITLE", "ARTIST", "ALBUM", "TRACK", "TOTAL TRACKS", "GENRE", "SONG LENGTH"
};
const char * avrcp_attribute2str(uint8_t index){
    if (index > 7){
        index = 0;
    }
    return avrcp_media_attribute_id_name[0];
}

static const char * avrcp_play_status_name[] = {
    "STOPPED", "PLAYING", "PAUSED", "FORWARD SEEK", "REVERSE SEEK",
    "ERROR" // 0xFF
};
const char * avrcp_play_status2str(uint8_t index){
    if (index > 4){
        index = 5;
    }
    return avrcp_play_status_name[index];
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
static const uint16_t avrcp_ctype_name_num = 16;

const char * avrcp_ctype2str(uint8_t index){
    if (index < avrcp_ctype_name_num){
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

static const char * notification_name[] = {
    "INVALID_INDEX",
    "PLAYBACK_STATUS_CHANGED",
    "TRACK_CHANGED",
    "TRACK_REACHED_END",
    "TRACK_REACHED_START",
    "PLAYBACK_POS_CHANGED",
    "BATT_STATUS_CHANGED",
    "SYSTEM_STATUS_CHANGED",
    "PLAYER_APPLICATION_SETTING_CHANGED",
    "NOW_PLAYING_CONTENT_CHANGED",
    "AVAILABLE_PLAYERS_CHANGED",
    "ADDRESSED_PLAYER_CHANGED",
    "UIDS_CHANGED",
    "VOLUME_CHANGED",
    "MAX_VALUE"
};

const char * avrcp_notification2str(avrcp_notification_event_id_t index){
    if ((index >= AVRCP_NOTIFICATION_EVENT_FIRST_INDEX) && (index <= AVRCP_NOTIFICATION_EVENT_LAST_INDEX)){
        return notification_name[index];
    } 
    return notification_name[0];
}

btstack_linked_list_t avrcp_get_connections(void){
    return avrcp_connections;
}

uint8_t avrcp_cmd_opcode(uint8_t *packet, uint16_t size){
    uint8_t cmd_opcode_index = 5;
    if (cmd_opcode_index > size) return AVRCP_CMD_OPCODE_UNDEFINED;
    return packet[cmd_opcode_index];
}

void avrcp_create_sdp_record(bool controller, uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features,
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
            de_add_number(avctpProtocol,  DE_UINT, DE_SIZE_16,  0x0104);    // version
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
            de_add_number(avrcProfile,  DE_UINT, DE_SIZE_16, 0x0106); 
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
                    de_add_number(browsing_avctpProtocol,  DE_UINT, DE_SIZE_16, 0x0104);                   // version
                }
                de_pop_sequence(des, browsing_avctpProtocol);
            }
            de_pop_sequence(attribute, des);
        }
        de_pop_sequence(service, attribute);
    }


    // 0x0100 "Service Name"
    if (strlen(service_name) > 0){
        de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
        de_add_data(service,  DE_STRING, (uint16_t) strlen(service_name), (uint8_t *) service_name);
    }

    // 0x0100 "Provider Name"
    if (strlen(service_provider_name) > 0){
        de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0102);
        de_add_data(service,  DE_STRING, (uint16_t) strlen(service_provider_name), (uint8_t *) service_provider_name);
    }

    // 0x0311 "Supported Features"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SUPPORTED_FEATURES);
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_features);
}

uint16_t avctp_get_num_bytes_for_header(avctp_packet_type_t avctp_packet_type) {
    switch (avctp_packet_type){
        case AVCTP_SINGLE_PACKET:
            // AVCTP message: transport header (1), pid (2)
            return 3;
        case AVCTP_START_PACKET:
            // AVCTP message: transport header (1), num_packets (1), pid (2)
            return 4;
        default:
            // AVCTP message: transport header (1)
            return 1;
    }
}

uint16_t avrcp_get_num_bytes_for_header(avrcp_command_opcode_t command_opcode, avctp_packet_type_t avctp_packet_type) {
    switch (avctp_packet_type){
        case AVCTP_SINGLE_PACKET:
        case AVCTP_START_PACKET:
            break;
        default:
            return 0;
    }

    uint16_t offset = 3; // AVRCP message: cmd type (1), subunit (1), opcode (1)
    switch (command_opcode){
        case AVRCP_CMD_OPCODE_VENDOR_DEPENDENT:
            offset += 7; // AVRCP message:  company (3), pdu id(1), AVRCP packet type (1), param_len (2)
            break;
        case AVRCP_CMD_OPCODE_PASS_THROUGH:
            offset += 3;  // AVRCP message: operation id (1), param_len (2)
            break;
        default:
            break;
    }
    return offset;
}

static uint16_t avrcp_get_num_free_bytes_for_payload(uint16_t l2cap_mtu, avrcp_command_opcode_t command_opcode, avctp_packet_type_t avctp_packet_type){
    uint16_t max_frame_size = btstack_min(l2cap_mtu, AVRCP_MAX_AV_C_MESSAGE_FRAME_SIZE);
    uint16_t payload_offset = avctp_get_num_bytes_for_header(avctp_packet_type) +
                              avrcp_get_num_bytes_for_header(command_opcode, avctp_packet_type);

    btstack_assert(max_frame_size >= payload_offset);
    return (max_frame_size - payload_offset);
}


avctp_packet_type_t avctp_get_packet_type(avrcp_connection_t * connection, uint16_t * max_payload_size){
    if (connection->l2cap_mtu >= AVRCP_MAX_AV_C_MESSAGE_FRAME_SIZE){
        return AVCTP_SINGLE_PACKET;
    }

    if (connection->data_offset == 0){
        uint16_t max_payload_size_for_single_packet = avrcp_get_num_free_bytes_for_payload(connection->l2cap_mtu,
                                                                 connection->command_opcode,
                                                                 AVCTP_SINGLE_PACKET);
        if (max_payload_size_for_single_packet >= connection->data_len){
            *max_payload_size = max_payload_size_for_single_packet;
            return AVCTP_SINGLE_PACKET;
        } else {
            uint16_t max_payload_size_for_start_packet = max_payload_size_for_single_packet - 1;
            *max_payload_size = max_payload_size_for_start_packet;
            return AVCTP_START_PACKET;
        }
    } else {
        // both packet types have the same single byte AVCTP header
        *max_payload_size = avrcp_get_num_free_bytes_for_payload(connection->l2cap_mtu,
                                                                 connection->command_opcode,
                                                                 AVCTP_CONTINUE_PACKET);
        if ((connection->data_len - connection->data_offset) > *max_payload_size){
            return AVCTP_CONTINUE_PACKET;
        } else {
            return AVCTP_END_PACKET;
        }
    }
}

avrcp_packet_type_t avrcp_get_packet_type(avrcp_connection_t * connection){
    switch (connection->avctp_packet_type) {
        case AVCTP_SINGLE_PACKET:
        case AVCTP_START_PACKET:
            break;
        default:
            return connection->avrcp_packet_type;
    }

    uint16_t payload_offset = avctp_get_num_bytes_for_header(connection->avctp_packet_type) +
                              avrcp_get_num_bytes_for_header(connection->command_opcode, connection->avctp_packet_type);
    uint16_t bytes_to_send = (connection->data_len - connection->data_offset) + payload_offset;

    if (connection->data_offset == 0){
        if (bytes_to_send <= AVRCP_MAX_AV_C_MESSAGE_FRAME_SIZE){
            return AVRCP_SINGLE_PACKET;
        } else {
            return AVRCP_START_PACKET;
        }
    } else {
        if (bytes_to_send > AVRCP_MAX_AV_C_MESSAGE_FRAME_SIZE){
            return AVRCP_CONTINUE_PACKET;
        } else {
            return AVRCP_END_PACKET;
        }
    }
}

avrcp_connection_t * avrcp_get_connection_for_bd_addr_for_role(avrcp_role_t role, bd_addr_t addr){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (memcmp(addr, connection->remote_addr, 6) != 0) continue;
        return connection;
    }
    return NULL;
}

avrcp_connection_t * avrcp_get_connection_for_l2cap_signaling_cid_for_role(avrcp_role_t role, uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->l2cap_signaling_cid != l2cap_cid) continue;
        return connection;
    }
    return NULL;
}

avrcp_connection_t * avrcp_get_connection_for_avrcp_cid_for_role(avrcp_role_t role, uint16_t avrcp_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->avrcp_cid != avrcp_cid) continue;
        return connection;
    }
    return NULL;
}

avrcp_connection_t * avrcp_get_connection_for_browsing_cid_for_role(avrcp_role_t role, uint16_t browsing_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->avrcp_browsing_cid != browsing_cid) continue;
        return connection;
    }
    return NULL;
}

avrcp_connection_t * avrcp_get_connection_for_browsing_l2cap_cid_for_role(avrcp_role_t role, uint16_t browsing_l2cap_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->browsing_connection &&  (connection->browsing_connection->l2cap_browsing_cid != browsing_l2cap_cid)) continue;
        return connection;
    }
    return NULL;
}

avrcp_browsing_connection_t * avrcp_get_browsing_connection_for_l2cap_cid_for_role(avrcp_role_t role, uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->role != role) continue;
        if (connection->browsing_connection && (connection->browsing_connection->l2cap_browsing_cid != l2cap_cid)) continue;
        return connection->browsing_connection;
    }
    return NULL;
}

void avrcp_request_can_send_now(avrcp_connection_t * connection, uint16_t l2cap_cid){
    connection->wait_to_send = true;
    l2cap_request_can_send_now_event(l2cap_cid);
}

uint16_t avrcp_get_next_cid(avrcp_role_t role){
    do {
        if (avrcp_cid_counter == 0xffff) {
            avrcp_cid_counter = 1;
        } else {
            avrcp_cid_counter++;
        }
    } while (avrcp_get_connection_for_avrcp_cid_for_role(role, avrcp_cid_counter) !=  NULL) ;
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

    connection->transaction_id = 0xFF;
    connection->transaction_id_counter = 0;

    connection->controller_max_num_fragments = 0xFF;

    // setup default unit / subunit info
    connection->company_id = 0xffffff;
    connection->target_unit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->target_subunit_info_data_size = sizeof(avrcp_default_subunit_info);
    connection->target_subunit_info_data = avrcp_default_subunit_info;

    log_info("avrcp_create_connection, role %d", role);
    (void)memcpy(connection->remote_addr, remote_addr, 6);
    btstack_linked_list_add_tail(&avrcp_connections, (btstack_linked_item_t *) connection);
    return connection;
}

static void avrcp_finalize_connection(avrcp_connection_t * connection){
    btstack_run_loop_remove_timer(&connection->retry_timer);
    btstack_run_loop_remove_timer(&connection->controller_press_and_hold_cmd_timer);
    btstack_linked_list_remove(&avrcp_connections, (btstack_linked_item_t*) connection);
    btstack_memory_avrcp_connection_free(connection);
}

static void avrcp_emit_connection_established(uint16_t avrcp_cid, bd_addr_t addr, hci_con_handle_t con_handle, uint8_t status){
    btstack_assert(avrcp_callback != NULL);

    uint8_t event[14];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_CONNECTION_ESTABLISHED;
    event[pos++] = status;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, con_handle);
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

uint16_t avrcp_sdp_query_browsing_l2cap_psm(void){
    return avrcp_sdp_query_context.browsing_l2cap_psm;
}

void avrcp_handle_sdp_client_query_attribute_value(uint8_t *packet){
    des_iterator_t des_list_it;

    des_iterator_t additional_protocol_descriptor_list_it;
    des_iterator_t protocol_descriptor_list_it;
    des_iterator_t protocol_it;
    uint8_t protocol_descriptor_id;

    // Handle new SDP record
    if (sdp_event_query_attribute_byte_get_record_id(packet) != avrcp_sdp_query_context.record_id) {
        avrcp_sdp_query_context.record_id = sdp_event_query_attribute_byte_get_record_id(packet);
        avrcp_sdp_query_context.parse_sdp_record = 0;
        // log_info("SDP Record: Nr: %d", record_id);
    }

    if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= avrcp_sdp_query_attribute_value_buffer_size) {
        avrcp_sdp_query_attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);

        if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {
            switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                case BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST:
                    if (de_get_element_type(avrcp_sdp_query_attribute_value) != DE_DES) break;
                    for (des_iterator_init(&des_list_it, avrcp_sdp_query_attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {
                        uint8_t * element = des_iterator_get_element(&des_list_it);
                        if (de_get_element_type(element) != DE_UUID) continue;
                        uint32_t uuid = de_get_uuid32(element);
                        switch (uuid){
                            case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_TARGET:
                            case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL:
                            case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_CONTROLLER:
                                avrcp_sdp_query_context.parse_sdp_record = 1;
                                break;
                            default:
                                break;
                        }
                    }
                    break;

                case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
                    if (!avrcp_sdp_query_context.parse_sdp_record) break;

                    for (des_iterator_init(&protocol_descriptor_list_it, avrcp_sdp_query_attribute_value); des_iterator_has_more(&protocol_descriptor_list_it); des_iterator_next(&protocol_descriptor_list_it)) {

                        if (des_iterator_get_type(&protocol_descriptor_list_it) != DE_DES) continue;
                        uint8_t * protocol_descriptor_list_element = des_iterator_get_element(&protocol_descriptor_list_it);

                        des_iterator_init(&protocol_it, protocol_descriptor_list_element);
                        uint8_t * protocol_element = des_iterator_get_element(&protocol_it);

                        if (de_get_element_type(protocol_element) != DE_UUID) continue;

                        uint32_t uuid = de_get_uuid32(protocol_element);
                        des_iterator_next(&protocol_it);
                        switch (uuid){
                            case BLUETOOTH_PROTOCOL_L2CAP:
                                if (!des_iterator_has_more(&protocol_it)) continue;
                                de_element_get_uint16(des_iterator_get_element(&protocol_it), &avrcp_sdp_query_context.avrcp_l2cap_psm);
                                break;
                            case BLUETOOTH_PROTOCOL_AVCTP:
                                if (!des_iterator_has_more(&protocol_it)) continue;
                                de_element_get_uint16(des_iterator_get_element(&protocol_it), &avrcp_sdp_query_context.avrcp_version);
                                break;
                            default:
                                break;
                        }
                    }
                    break;

                case BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS:
                    if (!avrcp_sdp_query_context.parse_sdp_record) break;

                    protocol_descriptor_id = 0;

                    for ( des_iterator_init(&additional_protocol_descriptor_list_it, avrcp_sdp_query_attribute_value);
                          des_iterator_has_more(&additional_protocol_descriptor_list_it);
                          des_iterator_next(&additional_protocol_descriptor_list_it)) {

                        if (des_iterator_get_type(&additional_protocol_descriptor_list_it) != DE_DES) continue;
                        uint8_t *additional_protocol_descriptor_element = des_iterator_get_element(&additional_protocol_descriptor_list_it);

                        for ( des_iterator_init(&protocol_descriptor_list_it,additional_protocol_descriptor_element);
                              des_iterator_has_more(&protocol_descriptor_list_it);
                              des_iterator_next(&protocol_descriptor_list_it)) {

                            if (des_iterator_get_type(&protocol_descriptor_list_it) != DE_DES) continue;

                            uint8_t * protocol_descriptor_list_element = des_iterator_get_element(&protocol_descriptor_list_it);

                            des_iterator_init(&protocol_it, protocol_descriptor_list_element);
                            uint8_t * protocol_element = des_iterator_get_element(&protocol_it);

                            if (de_get_element_type(protocol_element) != DE_UUID) continue;

                            uint32_t uuid = de_get_uuid32(protocol_element);
                            des_iterator_next(&protocol_it);
                            switch (uuid) {
                                case BLUETOOTH_PROTOCOL_L2CAP:
                                    if (!des_iterator_has_more(&protocol_it)) continue;
                                    switch (protocol_descriptor_id) {
                                        case 0:
                                            de_element_get_uint16(des_iterator_get_element(&protocol_it),
                                                                  &avrcp_sdp_query_context.browsing_l2cap_psm);
                                            break;
                                        case 1:
                                            de_element_get_uint16(des_iterator_get_element(&protocol_it),
                                                                  &avrcp_sdp_query_context.cover_art_l2cap_psm);
                                            break;
                                        default:
                                            break;
                                    }
                                    break;
                                case BLUETOOTH_PROTOCOL_AVCTP:
                                    if (!des_iterator_has_more(&protocol_it)) continue;
                                    de_element_get_uint16(des_iterator_get_element(&protocol_it),
                                                          &avrcp_sdp_query_context.browsing_version);
                                    break;
                                default:
                                    break;
                            }
                        }
                        protocol_descriptor_id++;
                    }
                    break;

                default:
                    break;
            }
        }
    } else {
        log_error("SDP attribute value buffer size exceeded: available %d, required %d", avrcp_sdp_query_attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
    }
}

static void avrcp_signaling_handle_sdp_query_complete(avrcp_connection_t * connection, uint8_t status){

    // l2cap available?
    if (status == ERROR_CODE_SUCCESS){
        if (avrcp_sdp_query_context.avrcp_l2cap_psm == 0){
            status = SDP_SERVICE_NOT_FOUND;
        }
    }

    if (status == ERROR_CODE_SUCCESS){
        // ready to connect
        connection->state = AVCTP_CONNECTION_W2_L2CAP_CONNECT;

        // check if both events have been handled
        avrcp_connection_t * connection_with_opposite_role;
        switch (connection->role){
            case AVRCP_CONTROLLER:
                connection_with_opposite_role = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, connection->avrcp_cid);
                break;
            case AVRCP_TARGET:
                connection_with_opposite_role = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, connection->avrcp_cid);
                break;
            default:
                btstack_assert(false);
                return;
        }
        if (connection_with_opposite_role->state == AVCTP_CONNECTION_W2_L2CAP_CONNECT){
            connection->state                    = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
            connection_with_opposite_role->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
            l2cap_create_channel(&avrcp_packet_handler, connection->remote_addr, connection->avrcp_l2cap_psm, l2cap_max_mtu(), NULL);
        }
    } else {
        log_info("AVRCP: SDP query failed with status 0x%02x.", status);
        avrcp_emit_connection_established(connection->avrcp_cid, connection->remote_addr, connection->con_handle, status);
        avrcp_finalize_connection(connection);
    }
}

static void avrcp_handle_sdp_query_completed(avrcp_connection_t * connection, uint8_t status){
    btstack_assert(connection != NULL);

    // cache SDP result on success
    if (status == ERROR_CODE_SUCCESS){
        connection->avrcp_l2cap_psm = avrcp_sdp_query_context.avrcp_l2cap_psm;
        connection->browsing_version = avrcp_sdp_query_context.browsing_version;
        connection->browsing_l2cap_psm = avrcp_sdp_query_context.browsing_l2cap_psm;
#ifdef ENABLE_AVRCP_COVER_ART
        connection->cover_art_psm = avrcp_sdp_query_context.cover_art_l2cap_psm;
#endif
    }

    // SDP Signaling Query?
    if (connection->state == AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE){
        avrcp_signaling_handle_sdp_query_complete(connection, status);
        return;
    }
    // Browsing SDP <- Browsing Connection <- Existing AVRCP Connection => it wasn't an SDP query for signaling
    if (avrcp_browsing_sdp_query_complete_handler != NULL){
        (*avrcp_browsing_sdp_query_complete_handler)(connection, status);
    }
#ifdef ENABLE_AVRCP_COVER_ART
    // Cover Art SDP <- Cover Art Connection <- Existing AVRCP Connection => it wasn't an SDP query for signaling
    if (avrcp_cover_art_sdp_query_complete_handler != NULL){
        (*avrcp_cover_art_sdp_query_complete_handler)(connection, status);
    }
#endif
}

static void avrcp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    avrcp_connection_t * avrcp_target_connection     = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET,     avrcp_sdp_query_context.avrcp_cid);
    avrcp_connection_t * avrcp_controller_connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_sdp_query_context.avrcp_cid);
    bool state_ok = (avrcp_target_connection != NULL) && (avrcp_controller_connection != NULL);

    if (!state_ok){
        // something wrong, nevertheless, start next sdp query if this one is complete
        if (hci_event_packet_get_type(packet) == SDP_EVENT_QUERY_COMPLETE){
            avrcp_sdp_query_context.avrcp_cid = 0;
            avrcp_start_next_sdp_query();
        }
        return;
    }

    uint8_t status;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            avrcp_handle_sdp_client_query_attribute_value(packet);
            return;
            
        case SDP_EVENT_QUERY_COMPLETE:
            // handle result
            status = sdp_event_query_complete_get_status(packet);
            avrcp_handle_sdp_query_completed(avrcp_controller_connection, status);
            avrcp_handle_sdp_query_completed(avrcp_target_connection, status);

            // query done, start next one
            avrcp_sdp_query_context.avrcp_cid = 0;
            avrcp_start_next_sdp_query();
            break;
       
        default:
            return;
    }

}

static void avrcp_handle_start_sdp_client_query(void * context){
    UNUSED(context);

    avrcp_connection_t * avrcp_target_connection     = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET,     avrcp_sdp_query_context.avrcp_cid);
    avrcp_connection_t * avrcp_controller_connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_sdp_query_context.avrcp_cid);
    bool state_ok = (avrcp_target_connection != NULL) && (avrcp_controller_connection != NULL);
    if (state_ok == false){
        // connection seems to got finalized in the meantime, just trigger next query
        avrcp_start_next_sdp_query();
        return;
    }

    // prevent triggering SDP query twice (for each role once)
    avrcp_target_connection->trigger_sdp_query = false;
    avrcp_controller_connection->trigger_sdp_query = false;

    sdp_client_query_uuid16(&avrcp_handle_sdp_client_query_result, avrcp_target_connection->remote_addr, BLUETOOTH_PROTOCOL_AVCTP);
}

static void avrcp_start_next_sdp_query(void) {
    if (avrcp_sdp_query_context.avrcp_cid != 0) {
        return;
    }
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &avrcp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->trigger_sdp_query == false) continue;

        // we're ready => setup avrcp_sdp_query_context and request sdp query
        avrcp_sdp_query_context.avrcp_cid = connection->avrcp_cid;
        avrcp_sdp_query_context.avrcp_l2cap_psm = 0;
        avrcp_sdp_query_context.avrcp_version  = 0;
        avrcp_sdp_query_registration.callback = &avrcp_handle_start_sdp_client_query;
        uint8_t status = sdp_client_register_query_callback(&avrcp_sdp_query_registration);
        btstack_assert(status == ERROR_CODE_SUCCESS);
        UNUSED(status);
        break;
    }
}

static avrcp_connection_t * avrcp_handle_incoming_connection_for_role(avrcp_role_t role, avrcp_connection_t * connection, bd_addr_t event_addr, hci_con_handle_t con_handle, uint16_t local_cid, uint16_t avrcp_cid){
    if (connection == NULL){
        connection = avrcp_create_connection(role, event_addr);
    }
    if (connection) {
        connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
        connection->l2cap_signaling_cid = local_cid;
        connection->avrcp_cid = avrcp_cid;
        connection->con_handle = con_handle;
        btstack_run_loop_remove_timer(&connection->retry_timer);
    } 
    return connection;
}

static void avrcp_handle_open_connection(avrcp_connection_t * connection, hci_con_handle_t con_handle, uint16_t local_cid, uint16_t l2cap_mtu){
    connection->l2cap_signaling_cid = local_cid;
    connection->l2cap_mtu = l2cap_mtu;
    connection->con_handle = con_handle;
    connection->incoming_declined = false;
    connection->target_song_length_ms = 0xFFFFFFFF;
    connection->target_song_position_ms = 0xFFFFFFFF;
    memset(connection->target_track_id, 0xFF, 8);
    connection->target_track_selected = false;
    connection->target_track_changed = false;
    connection->target_playback_status = AVRCP_PLAYBACK_STATUS_STOPPED;
    connection->state = AVCTP_CONNECTION_OPENED;
    
    log_info("L2CAP_EVENT_CHANNEL_OPENED avrcp_cid 0x%02x, l2cap_signaling_cid 0x%02x, role %d, state %d", connection->avrcp_cid, connection->l2cap_signaling_cid, connection->role, connection->state);
}

static void avrcp_retry_timer_timeout_handler(btstack_timer_source_t * timer){
    uint16_t avrcp_cid = (uint16_t)(uintptr_t) btstack_run_loop_get_timer_context(timer);
    avrcp_connection_t * connection_controller = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (connection_controller == NULL) return;
    avrcp_connection_t * connection_target = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (connection_target == NULL) return;

    if (connection_controller->state == AVCTP_CONNECTION_W2_L2CAP_RETRY){
        connection_controller->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
        connection_target->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
        l2cap_create_channel(&avrcp_packet_handler, connection_controller->remote_addr, connection_controller->avrcp_l2cap_psm, l2cap_max_mtu(), NULL);
    } 
}

static void avrcp_retry_timer_start(avrcp_connection_t * connection){
    btstack_run_loop_set_timer_handler(&connection->retry_timer, avrcp_retry_timer_timeout_handler);
    btstack_run_loop_set_timer_context(&connection->retry_timer, (void *)(uintptr_t)connection->avrcp_cid);

    // add some jitter/randomness to reconnect delay
    uint32_t timeout = 100 + (btstack_run_loop_get_time_ms() & 0x7F);
    btstack_run_loop_set_timer(&connection->retry_timer, timeout); 
    
    btstack_run_loop_add_timer(&connection->retry_timer);
}

static avrcp_frame_type_t avrcp_get_frame_type(uint8_t header){
    return (avrcp_frame_type_t)((header & 0x02) >> 1);
}

static void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint16_t l2cap_mtu;
    uint8_t  status;
    bool decline_connection;
    bool outoing_active;
    bool connection_already_established;
    hci_con_handle_t con_handle;

    avrcp_connection_t * connection_controller;
    avrcp_connection_t * connection_target;
    bool can_send;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {

                case L2CAP_EVENT_INCOMING_CONNECTION:
                    btstack_assert(avrcp_controller_packet_handler != NULL);
                    btstack_assert(avrcp_target_packet_handler != NULL);

                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    con_handle = l2cap_event_incoming_connection_get_handle(packet);

                    outoing_active = false;
                    connection_already_established = false;

                    connection_target = avrcp_get_connection_for_bd_addr_for_role(AVRCP_TARGET, event_addr);
                    if (connection_target != NULL){
                        if (connection_target->state == AVCTP_CONNECTION_W4_L2CAP_CONNECTED){
                            outoing_active = true;
                            connection_target->incoming_declined = true;
                        }
                        if (connection_target->state >= AVCTP_CONNECTION_OPENED){
                            connection_already_established = true;
                        }
                    }
                    
                    connection_controller = avrcp_get_connection_for_bd_addr_for_role(AVRCP_CONTROLLER, event_addr);
                    if (connection_controller != NULL){
                        if (connection_controller->state == AVCTP_CONNECTION_W4_L2CAP_CONNECTED) {
                            outoing_active = true;
                            connection_controller->incoming_declined = true;
                        }
                        if (connection_controller->state >= AVCTP_CONNECTION_OPENED){
                            connection_already_established = true;
                        }
                    }

                    decline_connection = outoing_active || connection_already_established;
                    if (decline_connection == false){
                        uint16_t avrcp_cid;
                        if ((connection_controller == NULL) || (connection_target == NULL)){
                            avrcp_cid = avrcp_get_next_cid(AVRCP_CONTROLLER);
                        } else {
                            avrcp_cid = connection_controller->avrcp_cid;
                        }
                        // create two connection objects (both)
                        connection_target     = avrcp_handle_incoming_connection_for_role(AVRCP_TARGET, connection_target, event_addr, con_handle, local_cid, avrcp_cid);
                        connection_controller = avrcp_handle_incoming_connection_for_role(AVRCP_CONTROLLER, connection_controller, event_addr, con_handle, local_cid, avrcp_cid);
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
                        log_info("Decline connection 0x%04x: outgoing active %u, connection already established: %u", local_cid, outoing_active, connection_already_established);
                        l2cap_decline_connection(local_cid);
                    } else {
                        log_info("AVRCP: L2CAP_EVENT_INCOMING_CONNECTION local cid 0x%04x, state %d", local_cid, connection_controller->state);
                        l2cap_accept_connection(local_cid);
                    }
                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    status = l2cap_event_channel_opened_get_status(packet);
                    local_cid = l2cap_event_channel_opened_get_local_cid(packet);
                    l2cap_mtu = l2cap_event_channel_opened_get_remote_mtu(packet);
                    con_handle = l2cap_event_channel_opened_get_handle(packet);

                    connection_controller = avrcp_get_connection_for_bd_addr_for_role(AVRCP_CONTROLLER, event_addr);
                    connection_target = avrcp_get_connection_for_bd_addr_for_role(AVRCP_TARGET, event_addr);

                    // incoming: structs are already created in L2CAP_EVENT_INCOMING_CONNECTION
                    // outgoing: structs are cteated in avrcp_connect()
                    if ((connection_controller == NULL) || (connection_target == NULL)) {
                        break;
                    }

                    switch (status){
                        case ERROR_CODE_SUCCESS:
                            avrcp_handle_open_connection(connection_target, con_handle, local_cid, l2cap_mtu);
                            avrcp_handle_open_connection(connection_controller, con_handle, local_cid, l2cap_mtu);
                            avrcp_emit_connection_established(connection_controller->avrcp_cid, event_addr, con_handle, status);
                            return;
                        case L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES: 
                            if (connection_controller->incoming_declined == true){
                                log_info("Incoming connection was declined, and the outgoing failed");
                                connection_controller->state = AVCTP_CONNECTION_W2_L2CAP_RETRY;
                                connection_controller->incoming_declined = false;
                                connection_target->state = AVCTP_CONNECTION_W2_L2CAP_RETRY;
                                connection_target->incoming_declined = false;
                                avrcp_retry_timer_start(connection_controller);
                                return;
                            } 
                            break;
                        default:
                            break;
                    }
                    log_info("L2CAP connection to connection %s failed. status code 0x%02x", bd_addr_to_str(event_addr), status);
                    avrcp_emit_connection_established(connection_controller->avrcp_cid, event_addr, con_handle, status);
                    avrcp_finalize_connection(connection_controller);
                    avrcp_finalize_connection(connection_target);
                    
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    
                    connection_controller = avrcp_get_connection_for_l2cap_signaling_cid_for_role(AVRCP_CONTROLLER, local_cid);
                    connection_target = avrcp_get_connection_for_l2cap_signaling_cid_for_role(AVRCP_TARGET, local_cid);
                    if ((connection_controller == NULL) || (connection_target == NULL)) {
                        break;
                    }
                    avrcp_emit_connection_closed(connection_controller->avrcp_cid);
                    avrcp_finalize_connection(connection_controller);
                    avrcp_finalize_connection(connection_target);
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    local_cid = l2cap_event_can_send_now_get_local_cid(packet);
                    can_send = true;

                    connection_target = avrcp_get_connection_for_l2cap_signaling_cid_for_role(AVRCP_TARGET, local_cid);
                    if ((connection_target != NULL) && connection_target->wait_to_send){
                        connection_target->wait_to_send = false;
                        (*avrcp_target_packet_handler)(HCI_EVENT_PACKET, channel, packet, size);
                        can_send = false;
                    }

                    connection_controller = avrcp_get_connection_for_l2cap_signaling_cid_for_role(AVRCP_CONTROLLER, local_cid);
                    if ((connection_controller != NULL) && connection_controller->wait_to_send){
                        if (can_send){
                            connection_controller->wait_to_send = false;
                            (*avrcp_controller_packet_handler)(HCI_EVENT_PACKET, channel, packet, size);
                        } else {
                            l2cap_request_can_send_now_event(local_cid);
                        }
                    }
                    break;

                default:
                    break;
            }
            break;

        case L2CAP_DATA_PACKET:
            switch (avrcp_get_frame_type(packet[0])){
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

void avrcp_init(void){
    avrcp_connections = NULL;
    if (avrcp_l2cap_service_registered) return;

    int status = l2cap_register_service(&avrcp_packet_handler, BLUETOOTH_PSM_AVCTP, 0xffff, gap_get_security_level());
    if (status != ERROR_CODE_SUCCESS) return;
    avrcp_l2cap_service_registered = true;
}

void avrcp_register_controller_packet_handler(btstack_packet_handler_t callback){
    // note: called by avrcp_controller_init
    avrcp_controller_packet_handler = callback;
}

void avrcp_register_target_packet_handler(btstack_packet_handler_t callback){
    // note: called by avrcp_target_init
    avrcp_target_packet_handler = callback;
}

void avrcp_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    avrcp_callback = callback;
}

void avrcp_register_browsing_sdp_query_complete_handler(void (*callback)(avrcp_connection_t * connection, uint8_t status)){
    btstack_assert(callback != NULL);
    avrcp_browsing_sdp_query_complete_handler = callback;
}

#ifdef ENABLE_AVRCP_COVER_ART
void avrcp_register_cover_art_sdp_query_complete_handler(void (*callback)(avrcp_connection_t * connection, uint8_t status)){
    btstack_assert(callback != NULL);
    avrcp_cover_art_sdp_query_complete_handler = callback;
}
#endif

void avrcp_trigger_sdp_query(avrcp_connection_t *connection_controller, avrcp_connection_t *connection_target) {
    connection_controller->trigger_sdp_query = true;
    connection_target->trigger_sdp_query     = true;

    avrcp_start_next_sdp_query();
}

uint8_t avrcp_connect(bd_addr_t remote_addr, uint16_t * avrcp_cid){
    btstack_assert(avrcp_controller_packet_handler != NULL);
    btstack_assert(avrcp_target_packet_handler != NULL);

    avrcp_connection_t * connection_controller = avrcp_get_connection_for_bd_addr_for_role(AVRCP_CONTROLLER, remote_addr);
    bool setup_active = false;
    if (connection_controller){
        // allow to call avrcp_connect after signaling connection was triggered remotely
        // @note this also allows to call avrcp_connect again before SLC is complete
        if (connection_controller->state < AVCTP_CONNECTION_OPENED){
            setup_active = true;
        } else {
            return ERROR_CODE_COMMAND_DISALLOWED;
        }
    }
    avrcp_connection_t * connection_target = avrcp_get_connection_for_bd_addr_for_role(AVRCP_TARGET, remote_addr);
    if (connection_target){
        if (connection_target->state < AVCTP_CONNECTION_OPENED){
            setup_active = true;
        } else {
            return ERROR_CODE_COMMAND_DISALLOWED;
        }
    }
    if (setup_active){
        return ERROR_CODE_SUCCESS;
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

    connection_controller->avrcp_cid = cid;
    connection_target->avrcp_cid     = cid;

    connection_controller->state = AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE;
    connection_target->state     = AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE;

    avrcp_trigger_sdp_query(connection_controller, connection_target);

    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_disconnect(uint16_t avrcp_cid){
    avrcp_connection_t * connection_controller = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection_controller){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    avrcp_connection_t * connection_target = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection_target){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection_controller->browsing_connection){
        l2cap_disconnect(connection_controller->browsing_connection->l2cap_browsing_cid);
    }
    l2cap_disconnect(connection_controller->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

void avrcp_deinit(void){
    avrcp_l2cap_service_registered = false;

    avrcp_cid_counter = 0;
    avrcp_connections = NULL;

    avrcp_callback = NULL;
    avrcp_controller_packet_handler = NULL;
    avrcp_target_packet_handler = NULL;

    (void) memset(&avrcp_sdp_query_registration, 0, sizeof(avrcp_sdp_query_registration));
    (void) memset(&avrcp_sdp_query_context, 0, sizeof(avrcp_sdp_query_context_t));
    (void) memset(avrcp_sdp_query_attribute_value, 0, sizeof(avrcp_sdp_query_attribute_value));
}
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#define FUZZ_CID 0x44
#define FUZZ_CON_HANDLE 0x0001
static bd_addr_t remote_addr = { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33 };
void avrcp_init_fuzz(void){
    // setup avrcp connections for cid
    avrcp_connection_t * connection_controller = avrcp_create_connection(AVRCP_CONTROLLER, remote_addr);
    avrcp_connection_t * connection_target     = avrcp_create_connection(AVRCP_TARGET, remote_addr);
    avrcp_handle_open_connection(connection_controller, FUZZ_CON_HANDLE, FUZZ_CID, 999);
    avrcp_handle_open_connection(connection_target, FUZZ_CON_HANDLE, FUZZ_CID, 999);
}
void avrcp_packet_handler_fuzz(uint8_t *packet, uint16_t size){
    avrcp_packet_handler(L2CAP_DATA_PACKET, FUZZ_CID, packet, size);
}
#endif
