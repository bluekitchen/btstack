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
#include "avrcp.h"

#define AV_REMOTE_CONTROL_TARGET        0x110C
#define AV_REMOTE_CONTROL               0x110E
#define AV_REMOTE_CONTROL_CONTROLLER    0x110F

#define PSM_AVCTP                       0x0017
#define PSM_AVCTP_BROWSING              0xFF17

/* 
Category 1: Player/Recorder
Category 2: Monitor/Amplifier
Category 3: Tuner
Category 4: Menu
*/

/* controller supported features
Bit 0 = Category 1
Bit 1 = Category 2
Bit 2 = Category 3
Bit 3 = Category 4
Bit 4-5 = RFA
Bit 6 = Supports browsing
Bit 7-15 = RFA
The bits for supported categories are set to 1. Others are set to 0.
*/

/* target supported features
Bit 0 = Category 1 
Bit 1 = Category 2 
Bit 2 = Category 3 
Bit 3 = Category 4
Bit 4 = Player Application Settings. Bit 0 should be set for this bit to be set.
Bit 5 = Group Navigation. Bit 0 should be set for this bit to be set.
Bit 6 = Supports browsing*4
Bit 7 = Supports multiple media player applications
Bit 8-15 = RFA
The bits for supported categories are set to 1. Others are set to 0.
*/

// TODO: merge with avdtp_packet_type_t
typedef enum {
    AVRCP_SINGLE_PACKET= 0,
    AVRCP_START_PACKET    ,
    AVRCP_CONTINUE_PACKET ,
    AVRCP_END_PACKET
} avrcp_packet_type_t;

typedef enum {
    AVRCP_COMMAND_FRAME = 0,
    AVRCP_RESPONSE_FRAME    
} avrcp_frame_type_t;

static const char * default_avrcp_controller_service_name = "BTstack AVRCP Controller Service";
static const char * default_avrcp_controller_service_provider_name = "BTstack AVRCP Controller Service Provider";
static const char * default_avrcp_target_service_name = "BTstack AVRCP Target Service";
static const char * default_avrcp_target_service_provider_name = "BTstack AVRCP Target Service Provider";

static btstack_linked_list_t avrcp_connections;
static btstack_packet_handler_t avrcp_callback;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void avrcp_create_sdp_record(uint8_t controller, uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
    attribute = de_push_sequence(service);
    {
        if (controller){
            de_add_number(attribute, DE_UUID, DE_SIZE_16, AV_REMOTE_CONTROL); 
            de_add_number(attribute, DE_UUID, DE_SIZE_16, AV_REMOTE_CONTROL_CONTROLLER); 
        } else {
            de_add_number(attribute, DE_UUID, DE_SIZE_16, AV_REMOTE_CONTROL_TARGET); 
        }
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ProtocolDescriptorList);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, SDP_L2CAPProtocol);
            de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, PSM_AVCTP);  
        }
        de_pop_sequence(attribute, l2cpProtocol);
        
        uint8_t* avctpProtocol = de_push_sequence(attribute);
        {
            de_add_number(avctpProtocol,  DE_UUID, DE_SIZE_16, PSM_AVCTP);  // avctpProtocol_service
            de_add_number(avctpProtocol,  DE_UINT, DE_SIZE_16,  0x0103);    // version
        }
        de_pop_sequence(attribute, avctpProtocol);

        if (browsing){
            de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ProtocolDescriptorList);
            attribute = de_push_sequence(service);
            {
                uint8_t* browsing_l2cpProtocol = de_push_sequence(attribute);
                {
                    de_add_number(browsing_l2cpProtocol,  DE_UUID, DE_SIZE_16, SDP_L2CAPProtocol);
                    de_add_number(browsing_l2cpProtocol,  DE_UINT, DE_SIZE_16, PSM_AVCTP_BROWSING);  
                }
                de_pop_sequence(attribute, browsing_l2cpProtocol);
                
                uint8_t* browsing_avctpProtocol = de_push_sequence(attribute);
                {
                    de_add_number(browsing_avctpProtocol,  DE_UUID, DE_SIZE_16, PSM_AVCTP);  // browsing_avctpProtocol_service
                    de_add_number(browsing_avctpProtocol,  DE_UINT, DE_SIZE_16,  0x0103);    // version
                }
                de_pop_sequence(attribute, browsing_avctpProtocol);
            }
            de_pop_sequence(service, attribute);
        }
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
        uint8_t *avrcProfile = de_push_sequence(attribute);
        {
            de_add_number(avrcProfile,  DE_UUID, DE_SIZE_16, AV_REMOTE_CONTROL); 
            de_add_number(avrcProfile,  DE_UINT, DE_SIZE_16, 0x0105); 
        }
        de_pop_sequence(attribute, avrcProfile);
    }
    de_pop_sequence(service, attribute);


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

void avrcp_controller_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    avrcp_create_sdp_record(1, service, service_record_handle, browsing, supported_features, service_name, service_provider_name);
}

void avrcp_target_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    avrcp_create_sdp_record(0, service, service_record_handle, browsing, supported_features, service_name, service_provider_name);
}

static void avrcp_emit_connection_established(btstack_packet_handler_t callback, uint16_t con_handle, uint16_t local_cid, bd_addr_t addr, uint8_t status){
    if (!callback) return;
    uint8_t event[14];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_CONNECTION_ESTABLISHED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    little_endian_store_16(event, pos, local_cid);
    pos += 2;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    event[pos++] = status;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_emit_connection_closed(btstack_packet_handler_t callback, uint16_t con_handle){
    if (!callback) return;
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static avrcp_connection_t * get_avrcp_connection_for_bd_addr(bd_addr_t addr){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (memcmp(addr, connection->remote_addr, 6) != 0) continue;
        return connection;
    }
    return NULL;
}

static avrcp_connection_t * get_avrcp_connection_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        return connection;
    }
    return NULL;
}

static avrcp_connection_t * get_avrcp_connection_for_l2cap_signaling_cid(uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avrcp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->l2cap_signaling_cid != l2cap_cid) continue;
        return connection;
    }
    return NULL;
}

static void avrcp_request_can_send_now(avrcp_connection_t * connection, uint16_t l2cap_cid){
    connection->wait_to_send = 1;
    l2cap_request_can_send_now_event(l2cap_cid);
}

static void avrcp_press_and_hold_timeout_handler(btstack_timer_source_t * timer){
    UNUSED(timer);
    avrcp_connection_t * connection = btstack_run_loop_get_timer_context(timer);
    btstack_run_loop_set_timer(&connection->press_and_hold_cmd_timer, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(&connection->press_and_hold_cmd_timer);
    connection->state = AVCTP_W2_SEND_PRESS_COMMAND;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
}

static void avrcp_press_and_hold_timer_start(avrcp_connection_t * connection){
    btstack_run_loop_remove_timer(&connection->press_and_hold_cmd_timer);
    btstack_run_loop_set_timer_handler(&connection->press_and_hold_cmd_timer, avrcp_press_and_hold_timeout_handler);
    btstack_run_loop_set_timer_context(&connection->press_and_hold_cmd_timer, connection);
    btstack_run_loop_set_timer(&connection->press_and_hold_cmd_timer, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(&connection->press_and_hold_cmd_timer);
}

static void avrcp_press_and_hold_timer_stop(avrcp_connection_t * connection){
    btstack_run_loop_remove_timer(&connection->press_and_hold_cmd_timer);
} 

static void request_pass_through_release_control_cmd(avrcp_connection_t * connection){
    connection->state = AVCTP_W2_SEND_RELEASE_COMMAND;
    switch (connection->cmd_operands[0]){
        case AVRCP_OPERATION_ID_REWIND:
        case AVRCP_OPERATION_ID_FAST_FORWARD:
            avrcp_press_and_hold_timer_stop(connection);
            break;
        default:
            break;
    }
    connection->cmd_operands[0] = 0x80 | connection->cmd_operands[0];
    connection->transaction_label++;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
}

static void request_pass_through_press_control_cmd(uint16_t con_handle, avrcp_operation_id_t opid, uint16_t playback_speed){
    avrcp_connection_t * connection = get_avrcp_connection_for_con_handle(con_handle);
    if (!connection){
        log_error("avrcp: coud not find a connection.");
        return;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return;
    connection->state = AVCTP_W2_SEND_PRESS_COMMAND;
    connection->cmd_to_send =  AVRCP_CMD_OPCODE_PASS_THROUGH;
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL; 
    connection->subunit_id =   0;
    connection->cmd_operands_lenght = 0;

    connection->cmd_operands_lenght = 2;
    connection->cmd_operands[0] = opid;
    if (playback_speed > 0){
        connection->cmd_operands[2] = playback_speed;
        connection->cmd_operands_lenght++;
    }
    connection->cmd_operands[1] = connection->cmd_operands_lenght - 2;
    
    switch (connection->cmd_operands[0]){
        case AVRCP_OPERATION_ID_REWIND:
        case AVRCP_OPERATION_ID_FAST_FORWARD:
            avrcp_press_and_hold_timer_start(connection);
            break;
        default:
            break;
    }
    connection->transaction_label++;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
}


static int avrcp_send_cmd(uint16_t cid, avrcp_connection_t * connection){
    uint8_t command[20];
    int pos = 0; 
    // transport header
    // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    command[pos++] = (connection->transaction_label << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_COMMAND_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    command[pos++] = AV_REMOTE_CONTROL >> 8;
    command[pos++] = AV_REMOTE_CONTROL & 0x00FF;

    // command_type
    command[pos++] = connection->command_type;
    // subunit_type | subunit ID
    command[pos++] = (connection->subunit_type << 3) | connection->subunit_id;
    // opcode
    command[pos++] = (uint8_t)connection->cmd_to_send;
    // operands
    memcpy(command+pos, connection->cmd_operands, connection->cmd_operands_lenght);
    pos += connection->cmd_operands_lenght;

    return l2cap_send(cid, command, pos);
}


static void avrcp_handle_l2cap_data_packet_for_signaling_connection(avrcp_connection_t * connection, uint8_t *packet, uint16_t size){
    switch (connection->state){
        case AVCTP_W2_RECEIVE_PRESS_RESPONSE:
            switch (connection->cmd_operands[0]){
                case AVRCP_OPERATION_ID_REWIND:
                case AVRCP_OPERATION_ID_FAST_FORWARD:
                    connection->state = AVCTP_W4_STOP;
                    break;
                default:
                    connection->state = AVCTP_W2_SEND_RELEASE_COMMAND;
                    break;
            }
            break;
        case AVCTP_W2_RECEIVE_RESPONSE:
            connection->state = AVCTP_CONNECTION_OPENED;
            break;
        default:
            return;
    }
    
    avrcp_command_type_t ctype;
    avrcp_subunit_type_t subunit_type;
    avrcp_subunit_type_t subunit_id;

    uint8_t operands[5];
    uint8_t opcode;
    uint8_t value;
    int pos = 0;

    uint8_t transport_header = packet[pos++];
    uint8_t transaction_label = transport_header >> 4;
    uint8_t packet_type = (transport_header & 0x0F) >> 2;
    uint8_t frame_type = (transport_header & 0x03) >> 1;
    uint8_t ipid = transport_header & 0x01;
    value = packet[pos++];
    uint16_t pid = (value << 8) | packet[pos++];

    printf("L2CAP DATA, response: ");
    printf_hexdump(packet, size);

    printf("    Transport header 0x%02x (transaction_label %d, packet_type %d, frame_type %d, ipid %d), pid 0x%4x\n", 
        transport_header, transaction_label, packet_type, frame_type, ipid, pid);
    switch (connection->cmd_to_send){
        case AVRCP_CMD_OPCODE_UNIT_INFO:{
            ctype = packet[pos++];
            value = packet[pos++];
            subunit_type = value >> 3;
            subunit_id = value & 0x07;
            opcode = packet[pos++];
            
            // operands:
            memcpy(operands, packet+pos, 5);
            uint8_t unit_type = operands[1] >> 3;
            uint8_t unit = operands[1] & 0x07;
            uint32_t company_id = operands[2] << 16 | operands[3] << 8 | operands[4];
            printf("    UNIT INFO response: ctype 0x%02x (0C), subunit_type 0x%02x (1F), subunit_id 0x%02x (07), opcode 0x%02x (30), unit_type 0x%02x, unit %d, company_id 0x%06x\n",
                ctype, subunit_type, subunit_id, opcode, unit_type, unit, company_id );
            break;
        }
        case AVRCP_CMD_OPCODE_VENDOR_DEPENDENT:
            break;
        case AVRCP_CMD_OPCODE_PASS_THROUGH:
            if (connection->state == AVCTP_W2_SEND_RELEASE_COMMAND){
                request_pass_through_release_control_cmd(connection);
            }
            break;
        default:
            break;
    }
}

static void avrcp_handle_can_send_now(avrcp_connection_t * connection){
    switch (connection->state){
        case AVCTP_W2_SEND_PRESS_COMMAND:
            connection->state = AVCTP_W2_RECEIVE_PRESS_RESPONSE;
            break;
        case AVCTP_W2_SEND_COMMAND:
        case AVCTP_W2_SEND_RELEASE_COMMAND:
            connection->state = AVCTP_W2_RECEIVE_RESPONSE;
            break;
        default:
            return;
    }
    avrcp_send_cmd(connection->l2cap_signaling_cid, connection);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    hci_con_handle_t con_handle;
    uint16_t psm;
    uint16_t local_cid;
    avrcp_connection_t * connection = NULL;
   
    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            connection = get_avrcp_connection_for_l2cap_signaling_cid(channel);
            if (!connection) break;
            avrcp_handle_l2cap_data_packet_for_signaling_connection(connection, packet, size);
            break;

        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    
                    if (l2cap_event_channel_opened_get_status(packet)){
                        log_error("L2CAP connection to connection %s failed. status code 0x%02x", 
                            bd_addr_to_str(event_addr), l2cap_event_channel_opened_get_status(packet));
                        break;
                    }
                    psm = l2cap_event_channel_opened_get_psm(packet); 
                    if (psm != PSM_AVCTP){
                        log_error("unexpected PSM - Not implemented yet: L2CAP_EVENT_CHANNEL_OPENED");
                        return;
                    }
                    
                    connection = get_avrcp_connection_for_bd_addr(event_addr);
                    if (!connection) break;

                    con_handle = l2cap_event_channel_opened_get_handle(packet);
                    local_cid = l2cap_event_channel_opened_get_local_cid(packet);
                    // printf("L2CAP_EVENT_CHANNEL_OPENED: Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                    //        bd_addr_to_str(event_addr), con_handle, psm, local_cid,  l2cap_event_channel_opened_get_remote_cid(packet));
                    
                    if (connection->l2cap_signaling_cid == 0) {
                        connection->l2cap_signaling_cid = local_cid;
                        connection->con_handle = con_handle;
                        connection->state = AVCTP_CONNECTION_OPENED;
                        avrcp_emit_connection_established(avrcp_callback, con_handle, local_cid, event_addr, 0);
                        break;
                    }
                    break;
                
                case L2CAP_EVENT_CAN_SEND_NOW:
                    connection = get_avrcp_connection_for_l2cap_signaling_cid(channel);
                    if (!connection) break;
                    avrcp_handle_can_send_now(connection);
                    break;

                case L2CAP_EVENT_CHANNEL_CLOSED:
                    // data: event (8), len(8), channel (16)
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    connection = get_avrcp_connection_for_l2cap_signaling_cid(local_cid);
                    printf(" -> L2CAP_EVENT_CHANNEL_CLOSED cid 0x%0x\n", local_cid);
                    
                    if (connection){
                        printf("connection closed\n");
                        avrcp_emit_connection_closed(avrcp_callback, connection->con_handle);
                        btstack_linked_list_remove(&avrcp_connections, (btstack_linked_item_t*) connection); 
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

void avrcp_init(void){
    avrcp_connections = NULL;
    l2cap_register_service(&packet_handler, PSM_AVCTP, 0xffff, LEVEL_0);
}

void avrcp_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("avrcp_register_packet_handler called with NULL callback");
        return;
    }
    avrcp_callback = callback;
}

static avrcp_connection_t * avrcp_create_connection(bd_addr_t remote_addr){
    avrcp_connection_t * connection = btstack_memory_avrcp_connection_get();
    memset(connection, 0, sizeof(avrcp_connection_t));
    connection->state = AVCTP_CONNECTION_IDLE;
    connection->transaction_label = 0xFF;
    memcpy(connection->remote_addr, remote_addr, 6);
    btstack_linked_list_add(&avrcp_connections, (btstack_linked_item_t *) connection);
    return connection;
}

void avrcp_connect(bd_addr_t bd_addr){
    avrcp_connection_t * connection = get_avrcp_connection_for_bd_addr(bd_addr);
    if (!connection){
        connection = avrcp_create_connection(bd_addr);
    }
    if (!connection){
        log_error("avrcp: coud not find or create a connection.");
        return;
    }
    if (connection->state != AVCTP_CONNECTION_IDLE) return;
    connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
    l2cap_create_channel(packet_handler, connection->remote_addr, PSM_AVCTP, 0xffff, NULL);
}

void avrcp_unit_info(uint16_t con_handle){
    avrcp_connection_t * connection = get_avrcp_connection_for_con_handle(con_handle);
    if (!connection){
        log_error("avrcp_unit_info: coud not find a connection.");
        return;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return;
    connection->state = AVCTP_W2_SEND_COMMAND;
    
    connection->transaction_label++;
    connection->cmd_to_send = AVRCP_CMD_OPCODE_UNIT_INFO;
    connection->command_type = AVRCP_CTYPE_STATUS;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_UNIT; //vendor unique
    connection->subunit_id =   AVRCP_SUBUNIT_ID_IGNORE;
    memset(connection->cmd_operands, 0xFF, connection->cmd_operands_lenght);
    connection->cmd_operands_lenght = 5;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
}

void avrcp_get_capabilities(uint16_t con_handle){
    avrcp_connection_t * connection = get_avrcp_connection_for_con_handle(con_handle);
    if (!connection){
        log_error("avrcp_unit_info: coud not find a connection.");
        return;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return;
    connection->state = AVCTP_W2_SEND_COMMAND;
    
    connection->transaction_label++;
    connection->cmd_to_send = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_STATUS;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = 0;
    big_endian_store_24(connection->cmd_operands, 0, BT_SIG_COMPANY_ID);
    connection->cmd_operands[3] = 0x10;
    connection->cmd_operands[4] = 0;
    big_endian_store_16(connection->cmd_operands, 5, 1);
    connection->cmd_operands[7] = 0x02;
    connection->cmd_operands_lenght = 8;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
}


void avrcp_play(uint16_t con_handle){
    request_pass_through_press_control_cmd(con_handle, AVRCP_OPERATION_ID_PLAY, 0);
}

void avrcp_stop(uint16_t con_handle){
    request_pass_through_press_control_cmd(con_handle, AVRCP_OPERATION_ID_STOP, 0);
}

void avrcp_pause(uint16_t con_handle){
    request_pass_through_press_control_cmd(con_handle, AVRCP_OPERATION_ID_PAUSE, 0);
}

void avrcp_forward(uint16_t con_handle){
    request_pass_through_press_control_cmd(con_handle, AVRCP_OPERATION_ID_FORWARD, 0);
} 

void avrcp_backward(uint16_t con_handle){
    request_pass_through_press_control_cmd(con_handle, AVRCP_OPERATION_ID_BACKWARD, 0);
}

void avrcp_start_rewind(uint16_t con_handle){
    request_pass_through_press_control_cmd(con_handle, AVRCP_OPERATION_ID_REWIND, 0);
}

void avrcp_stop_rewind(uint16_t con_handle){
    avrcp_connection_t * connection = get_avrcp_connection_for_con_handle(con_handle);
    if (!connection){
        log_error("avrcp_unit_info: coud not find a connection.");
        return;
    }
    if (connection->state != AVCTP_W4_STOP) return;
    request_pass_through_release_control_cmd(connection);
}

void avrcp_start_fast_forward(uint16_t con_handle){
    request_pass_through_press_control_cmd(con_handle, AVRCP_OPERATION_ID_FAST_FORWARD, 0);
}

void avrcp_stop_fast_forward(uint16_t con_handle){
    avrcp_connection_t * connection = get_avrcp_connection_for_con_handle(con_handle);
    if (!connection){
        log_error("avrcp_unit_info: coud not find a connection.");
        return;
    }
    if (connection->state != AVCTP_W4_STOP) return;
    request_pass_through_release_control_cmd(connection);
}
