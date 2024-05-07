/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "bap_service_client_test.c"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack_lc3.h>

#include "btstack.h"
#include "btstack_config.h"

#define BASS_CLIENT_NUM_SOURCES 3
#define ASCS_CLIENT_NUM_STREAMENDPOINTS 4
#define MAX_CHANNELS 2

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static void show_usage(void);

static const char * bap_app_server_addr_string = "C007E8414591";

static bd_addr_t        bap_app_server_addr;
static hci_con_handle_t bap_app_client_con_handle;
static char operation_cmd = 0;

// MCS
#define MCS_CLIENT_CHARACTERISTICS_MAX_NUM 25
static uint16_t mcs_cid;
static mcs_client_connection_t mcs_connection;
static gatt_service_client_characteristic_t mcs_connection_characteristics[MCS_CLIENT_CHARACTERISTICS_MAX_NUM];

typedef enum {
    BAP_APP_CLIENT_STATE_IDLE = 0,
    BAP_APP_CLIENT_STATE_W4_GAP_CONNECTED,
    BAP_APP_CLIENT_STATE_W4_BASS_CONNECTED,
    BAP_APP_CLIENT_STATE_W4_PACS_CONNECTED,
    BAP_APP_CLIENT_STATE_W4_MCS_CONNECTED,
    BAP_APP_CLIENT_STATE_CONNECTED,
    BAP_APP_W4_CIG_COMPLETE,
    BAP_APP_W4_CIS_CREATED,
    BAP_APP_STREAMING,
    BAP_APP_CLIENT_STATE_W4_GAP_DISCONNECTED,
} bap_app_client_state_t; 

static bap_app_client_state_t bap_app_client_state = BAP_APP_CLIENT_STATE_IDLE;

static void bap_client_reset(void){
    bap_app_client_state      = BAP_APP_CLIENT_STATE_IDLE;
    bap_app_client_con_handle = HCI_CON_HANDLE_INVALID;
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    bd_addr_t addr;
    uint8_t event = hci_event_packet_get_type(packet);
    
    switch (event) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)){
                case HCI_STATE_WORKING:
                    bap_client_reset();
                    show_usage();
                    printf("Please select sample frequency and variation\n");
                    break;
                case HCI_STATE_OFF:
                    printf("Goodbye\n");
                    exit(0);
                    break;
                default:
                    break;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            bap_client_reset();
            printf("BAP  Client: disconnected from %s\n", bd_addr_to_str(bap_app_server_addr));
            break;

        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            printf("Display Passkey: %"PRIu32"\n", sm_event_passkey_display_number_get_passkey(packet));
            break;
        case SM_EVENT_IDENTITY_CREATED:
            sm_event_identity_created_get_identity_address(packet, addr);
            printf("Identity created: type %u address %s\n", sm_event_identity_created_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
            sm_event_identity_resolving_succeeded_get_identity_address(packet, addr);
            printf("Identity resolved: type %u address %s\n", sm_event_identity_resolving_succeeded_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_FAILED:
            sm_event_identity_created_get_address(packet, addr);
            printf("Identity resolving failed\n");
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            switch (sm_event_pairing_complete_get_status(packet)){
                case ERROR_CODE_SUCCESS:
                    printf("Pairing complete, success\n");
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    printf("Pairing failed, timeout\n");
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    printf("Pairing faileed, disconnected\n");
                    break;
                case ERROR_CODE_AUTHENTICATION_FAILURE:
                    printf("Pairing failed, reason = %u\n", sm_event_pairing_complete_get_reason(packet));
                    break;
                default:
                    break;
            }
            break;

        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    bap_app_client_con_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    bap_app_client_state      = BAP_APP_CLIENT_STATE_CONNECTED;
                    gap_subevent_le_connection_complete_get_peer_address(packet, bap_app_server_addr);
                    printf("BAP  Client: connection to %s established, con_handle 0x%04x\n", bd_addr_to_str(bap_app_server_addr), bap_app_client_con_handle);
                    break;
                default:
                    return;
            }
            break;

        default:
            break;
    }
}

static void mcs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    uint32_t duration_ms;
    uint32_t position_ms;
    int speed;
    int seeking_speed;
    uint8_t subevent = hci_event_leaudio_meta_get_subevent_code(packet);
    uint8_t status;

    switch (subevent){
        case LEAUDIO_SUBEVENT_MCS_CLIENT_CONNECTED:
            if (bap_app_client_con_handle != leaudio_subevent_mcs_client_connected_get_con_handle(packet)){
                printf("MCS Client: expected con handle 0x%04x, received 0x%04x\n", bap_app_client_con_handle, leaudio_subevent_mcs_client_connected_get_con_handle(packet));
                return;
            }

            bap_app_client_state = BAP_APP_CLIENT_STATE_CONNECTED;

            if (leaudio_subevent_mcs_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                printf("MCS client: connection failed, cid 0x%04x, con_handle 0x%04x, status 0x%02x\n", mcs_cid, bap_app_client_con_handle, 
                    leaudio_subevent_mcs_client_connected_get_status(packet));
                return;
            }
            printf("MCS client: connected, cid 0x%04x\n", mcs_cid);
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_CONNECTED:
            status = leaudio_subevent_ots_client_connected_get_att_status(packet);

            switch (status) {
                case ERROR_CODE_SUCCESS:
                    printf("OTS Client Test: connected\n");
                    bap_app_client_state = BAP_APP_CLIENT_STATE_CONNECTED;
                    break;
                default:
                    printf("OTS Client Test: connection failed, err 0x%02x.\n", status);
                    break;
            }
            break;


        case LEAUDIO_SUBEVENT_OTS_CLIENT_DISCONNECTED:
            printf("MTS: OTS Server disconnected\n");
            break;

        case LEAUDIO_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_NAME:
            printf("MCS Client: Media Player Name \"%s\"\n", (char *) leaudio_subevent_mcs_client_media_player_name_get_value(packet));
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_ICON_OBJECT_ID:
            printf("MCS Client: Media Player Icon Object ID \"%s\"\n", (char *) leaudio_subevent_mcs_client_media_player_icon_object_id_get_value(packet));
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_ICON_URI:
            printf("MCS Client: Media Player Icon URI\"%s\"\n", (char *) leaudio_subevent_mcs_client_media_player_icon_uri_get_value(packet));
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_TRACK_CHANGED:
            printf("MCS Client: Media Player Track Changed\n");
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_TRACK_TITLE:
            printf("MCS Client: Media Player Name \"%s\"\n", (char *) leaudio_subevent_mcs_client_track_title_get_value(packet));
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_TRACK_DURATION:
            duration_ms = leaudio_subevent_mcs_client_track_duration_get_duration_10ms(packet) * 10;
            printf("MCS Client: Media Player Track Duration %d ms\n", duration_ms * 10);
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_TRACK_POSITION:
            position_ms = leaudio_subevent_mcs_client_track_position_get_position_10ms(packet) * 10;
            printf("MCS Client: Media Player Track Position %d ms\n", position_ms);
            break;

        case LEAUDIO_SUBEVENT_MCS_CLIENT_PLAYBACK_SPEED:
            speed = (int)leaudio_subevent_mcs_client_playback_speed_get_speed(packet);
            printf("MCS Client: Media Player Playback Speed %d \n", speed);
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_SEEKING_SPEED:
            seeking_speed = (int)leaudio_subevent_mcs_client_seeking_speed_get_multiplier(packet);
            printf("MCS Client: Media Player Seeking Speed %d \n", seeking_speed);
            break;

        case LEAUDIO_SUBEVENT_MCS_CLIENT_CURRENT_TRACK_SEGMENTS_OBJECT_ID:
            printf("MCS Client: Current Track Segments Object ID \"%s\"\n", (char *) leaudio_subevent_mcs_client_current_track_segments_object_id_get_value(packet));
            break;

        case LEAUDIO_SUBEVENT_MCS_CLIENT_CURRENT_TRACK_OBJECT_ID:
            printf("MCS Client: Current Track Object ID \"%s\"\n", (char *) leaudio_subevent_mcs_client_current_track_object_id_get_value(packet));
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_NEXT_TRACK_OBJECT_ID:
            printf("MCS Client: Next Track Object ID \"%s\"\n", (char *) leaudio_subevent_mcs_client_next_track_object_id_get_value(packet));
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_PARENT_GROUP_OBJECT_ID:
            printf("MCS Client: Parent Group Object ID \"%s\"\n", (char *) leaudio_subevent_mcs_client_parent_group_object_id_get_value(packet));
            break;

        case LEAUDIO_SUBEVENT_MCS_CLIENT_CURRENT_GROUP_OBJECT_ID:
            printf("MCS Client: Current Group Object ID \"%s\"\n", (char *) leaudio_subevent_mcs_client_current_group_object_id_get_value(packet));
            break;

        case LEAUDIO_SUBEVENT_MCS_CLIENT_PLAYING_ORDER:
            printf("MCS Client: Playing Order 0x%02x\n", leaudio_subevent_mcs_client_playing_order_get_order(packet));
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_PLAYING_ORDER_SUPPORTED:
            printf("MCS Client: Playing Order Supported 0x%04x\n", leaudio_subevent_mcs_client_playing_order_supported_get_bitmap(packet));
            break;  

        case LEAUDIO_SUBEVENT_MCS_CLIENT_MEDIA_STATE:
            printf("MCS Client: Media State 0x%02x\n", leaudio_subevent_mcs_client_media_state_get_state(packet));
            break;

        case LEAUDIO_SUBEVENT_MCS_CLIENT_CONTROL_POINT_OPCODES_SUPPORTED:
            printf("MCS Client: Control Point Opcodes Supported 0x%04x\n", leaudio_subevent_mcs_client_control_point_opcodes_supported_get_bitmap(packet));
            break;

        case LEAUDIO_SUBEVENT_MCS_CLIENT_MEDIA_CONTROL_POINT_NOTIFICATION_RESULT:
            printf("MCS Client: Control Point Notification Opcode 0x%02x, Result Code 0x%02x\n",
                   leaudio_subevent_mcs_client_media_control_point_notification_result_get_opcode(packet),
                   leaudio_subevent_mcs_client_media_control_point_notification_result_get_result_code(packet));
            break;

        case LEAUDIO_SUBEVENT_MCS_CLIENT_SEARCH_CONTROL_POINT_NOTIFICATION_RESULT:
            printf("MCS Client: Search Control Point Notification, Result Code 0x%02x\n",
                   leaudio_subevent_mcs_client_search_control_point_notification_result_get_result_code(packet));
                break;

        case LEAUDIO_SUBEVENT_MCS_CLIENT_SEARCH_RESULT_OBJECT_ID:
            printf("MCS Client: Search Result Object ID \"%s\"\n", (char *) leaudio_subevent_mcs_client_search_result_object_id_get_value(packet));
            printf_hexdump(leaudio_subevent_mcs_client_search_result_object_id_get_value(packet), leaudio_subevent_mcs_client_search_result_object_id_get_value_len(packet));
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_CONTENT_CONTROL_ID:
            printf("MCS Client: Content Control ID 0x%02x\n", leaudio_subevent_mcs_client_content_control_id_get_ccid(packet));
            break;
        
        case LEAUDIO_SUBEVENT_MCS_CLIENT_DISCONNECTED:
            mcs_cid = 0;
            printf("MCS Client: disconnected\n");
            break;
        default:
            printf("MCS Client: subevent 0x%02X\n", subevent);
            break;
    }
}

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- BAP Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("a   - connect to %s\n", bap_app_server_addr_string);    
    printf("A   - disconnect from %s\n", bap_app_server_addr_string);       
    
    printf("\n--- MCS Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("0   - connect to GMCS server %s\n", bap_app_server_addr_string);
    printf("b   - connect to MCS  server %s\n", bap_app_server_addr_string);
    printf("B   - get media player name\n");
    printf("c   - get icon object ID\n");
    printf("C   - get icon URI\n");
    printf("d   - get track title\n");
    printf("D   - get track duration\n");
    printf("e/E - get/set track position\n");
    printf("f/F - get/set playback speed\n");
    printf("g   - get seeking speed\n");
    printf("G   - get current track segments object id\n");
    printf("h/H - get/set current track object id\n");
    printf("i/I - get/set next track object id\n");
    printf("j   - get parent group object id\n");
    printf("k/K - get/set current group object id\n");
    printf("l/L - get/set playing order\n");
    printf("J   - get playing order supported\n");
    printf("m   - get media state\n");
    printf("M   - get media control point opcodes supported\n");
    printf("n   - get media search results object id\n");
    printf("N   - get media content control id\n");
    printf("\n");
    printf("o   - play\n");
    printf("O   - pause\n");
    printf("p   - fast rewind\n");
    printf("P   - fast forward\n");
    printf("q   - stop\n");
    printf("Q   - move relative\n");
    printf("r   - previous segment\n");
    printf("R   - next segment\n");
    printf("s   - first segment\n");
    printf("S   - last segment\n");
    printf("t   - goto segment\n");
    printf("T   - previous track\n");
    printf("u   - next track\n");
    printf("U   - first track\n");
    printf("v   - last track\n");
    printf("V   - goto track\n");
    printf("z   - previous group\n");
    printf("Z   - next group\n");
    printf("x   - first group\n");
    printf("X   - last group\n");
    printf("y   - goto group\n");
    printf("\n");
    printf("1   - search track name\n");
    printf("2   - search artist_name\n");
    printf("3   - search album_name\n");
    printf("4   - search group_name\n");
    printf("5   - search earliest_year\n");
    printf("6   - search latest_year\n");
    printf("7   - search genre\n");
    printf("8   - search only_tracks\n");
    printf("9   - search only_groups\n");

    printf("\n");
    printf(" \n");
    printf(" \n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    operation_cmd = cmd;
    char * search_data;

    switch (cmd){
        case 'a':
            printf("Connecting...\n");
            bap_app_client_state = BAP_APP_CLIENT_STATE_W4_GAP_CONNECTED;
            status = gap_connect(bap_app_server_addr, 0);
            break;
        
        case 'A':
            printf("Disconnect...\n");
            bap_app_client_state = BAP_APP_CLIENT_STATE_W4_GAP_DISCONNECTED;
            status = gap_disconnect(bap_app_client_con_handle);
            break;

        case '0':
            if (bap_app_client_state < BAP_APP_CLIENT_STATE_CONNECTED){
                printf("Not connected yet. Call first \"a\"\n");
                break;
            }
            printf("MCS: connect to GMCP Server 0x%02x\n", bap_app_client_con_handle);
            bap_app_client_state = BAP_APP_CLIENT_STATE_W4_MCS_CONNECTED;
            status = media_control_service_client_connect_generic_player(
                    bap_app_client_con_handle, &mcs_client_event_handler,
                    &mcs_connection, mcs_connection_characteristics, MCS_CLIENT_CHARACTERISTICS_MAX_NUM,
                    &mcs_cid);
            break;

        case 'b':
            if (bap_app_client_state < BAP_APP_CLIENT_STATE_CONNECTED){
                printf("Not connected yet. Call first \"a\"\n");
                break;
            }
            printf("MCS: connect to MCP Server 0x%02x\n", bap_app_client_con_handle);
            bap_app_client_state = BAP_APP_CLIENT_STATE_W4_MCS_CONNECTED;
            status = media_control_service_client_connect_media_player(
                bap_app_client_con_handle, 0, &mcs_client_event_handler,
                &mcs_connection, mcs_connection_characteristics, MCS_CLIENT_CHARACTERISTICS_MAX_NUM,
                &mcs_cid);
            break;

        case 'B':
            printf("MCS: get - Media Player Name\n");
            media_control_service_client_get_media_player_name(mcs_cid);
            break;

        case 'c':
            printf("MCS: get - Media Player Icon Object ID\n");
            media_control_service_client_get_media_player_icon_object_id(mcs_cid);
            break;

        case 'C':
            printf("MCS: get - Media Player Icon URI\n");
            media_control_service_client_get_media_player_icon_uri(mcs_cid);
            break;

        case 'd':
            printf("MCS: get - Media Player Track Title\n");
            media_control_service_client_get_track_title(mcs_cid);
            break;

        case 'D':
            printf("MCS: get - Media Player Track Duration\n");
            media_control_service_client_get_track_duration(mcs_cid);
            break;
        
        case 'e':
            printf("MCS: get - Media Player Track Position\n");
            media_control_service_client_get_track_position(mcs_cid);
            break;

        case 'E':
            printf("MCS: set - Media Player Track Position\n");
            media_control_service_client_set_track_position(mcs_cid, 10000);
            break;

        case 'f':
            printf("MCS: get - Media Player Playback Speed\n");
            media_control_service_client_get_playback_speed(mcs_cid);
            break;

        case 'F':
            printf("MCS: set - Media Player Playback Speed\n");
            media_control_service_client_set_playback_speed(mcs_cid, 64);
            break;
        
        case 'g':
            printf("MCS: get - Media Player Seeking Speed\n");
            media_control_service_client_get_seeking_speed(mcs_cid);
            break;

        case 'G':
            printf("MCS: get - Current Track Segments Object ID\n");
            media_control_service_client_get_current_track_segments_object_id(mcs_cid);
            break;

        case 'h':
            printf("MCS: get - Current Track Object ID\n");
            media_control_service_client_get_current_track_object_id(mcs_cid);
            break;

        case 'H':
            printf("MCS: set - Current Track Object ID\n");
            media_control_service_client_set_current_track_object_id(mcs_cid, "12345");
            break;

        case 'i':
            printf("MCS: get - Next Track Object ID\n");
            media_control_service_client_get_next_track_object_id(mcs_cid);
            break;

        case 'I':
            printf("MCS: set - Next Track Object ID\n");
            media_control_service_client_set_next_track_object_id(mcs_cid, "12345");
            break;

        case 'j':
            printf("MCS: get - Parent Group Object ID\n");
            media_control_service_client_get_parent_group_object_id(mcs_cid);
            break;

        case 'k':
            printf("MCS: get - Current Group Object ID\n");
            media_control_service_client_get_current_group_object_id(mcs_cid);
            break;

        case 'K':
            printf("MCS: set - Current Group Object ID\n");
            media_control_service_client_set_current_group_object_id(mcs_cid, "12345");
            break;  

        case 'l':
            printf("MCS: get - playing order\n");
            media_control_service_client_get_playing_order(mcs_cid);
            break;

        case 'L':
            printf("MCS: set - playing order\n");
            media_control_service_client_set_playing_order(mcs_cid, 0x02);
            break;
        
        case 'J':
            printf("MCS: get - playing order supported\n");
            media_control_service_client_get_playing_orders_supported(mcs_cid);
            break;

        case 'm':
            printf("MCS: get - media state\n");
            media_control_service_client_get_media_state(mcs_cid);
            break;

        case 'M':
            printf("MCS: get - media control point opcodes supported\n");
            media_control_service_client_get_media_control_point_opcodes_supported(mcs_cid);
            break;

        case 'n':
            printf("MCS: get - media search results object id\n");
            media_control_service_client_get_search_results_object_id(mcs_cid);
            break;

        case 'N':
            printf("MCS: get - media content control id\n");
            media_control_service_client_get_content_control_id(mcs_cid);
            break;

        case 'o':
           printf("MCS: play\n");
           media_control_service_client_command_play(mcs_cid);
           break;
        case 'O':
           printf("MCS: pause\n");
           media_control_service_client_command_pause(mcs_cid);
           break;
        case 'p':
           printf("MCS: fast rewind\n");
           media_control_service_client_command_fast_rewind(mcs_cid);
           break;
        case 'P':
           printf("MCS: fast forward\n");
           media_control_service_client_command_fast_forward(mcs_cid);
           break;
        case 'q':
           printf("MCS: stop\n");
           media_control_service_client_command_stop(mcs_cid);
           break;
        case 'Q':
           printf("MCS: move relative\n");
           media_control_service_client_command_move_relative(mcs_cid, 0);
           break;
        case 'r':
           printf("MCS: previous segment\n");
           media_control_service_client_command_previous_segment(mcs_cid);
           break;
        case 'R':
           printf("MCS: next segment\n");
           media_control_service_client_command_next_segment(mcs_cid);
           break;
        case 's':
           printf("MCS: first segment\n");
           media_control_service_client_command_first_segment(mcs_cid);
           break;
        case 'S':
           printf("MCS: last segment\n");
           media_control_service_client_command_last_segment(mcs_cid);
           break;
        case 't':
           printf("MCS: goto segment\n");
           media_control_service_client_command_goto_segment(mcs_cid, 2);
           break;
        case 'T':
           printf("MCS: previous track\n");
           media_control_service_client_command_previous_track(mcs_cid);
           break;
        case 'u':
           printf("MCS: next track\n");
           media_control_service_client_command_next_track(mcs_cid);
           break;
        case 'U':
           printf("MCS: first track\n");
           media_control_service_client_command_first_track(mcs_cid);
           break;
        case 'v':
           printf("MCS: last track\n");
           media_control_service_client_command_last_track(mcs_cid);
           break;
        case 'V':
           printf("MCS: goto track\n");
           media_control_service_client_command_goto_track(mcs_cid, 2);
           break;
        case 'z':
           printf("MCS: previous group\n");
           media_control_service_client_command_previous_group(mcs_cid);
           break;
        case 'Z':
           printf("MCS: next group\n");
           media_control_service_client_command_next_group(mcs_cid);
           break;
        case 'x':
           printf("MCS: first group\n");
           media_control_service_client_command_first_group(mcs_cid);
           break;
        case 'X':
           printf("MCS: last group\n");
           media_control_service_client_command_last_group(mcs_cid);
           break;
        case 'y':
           printf("MCS: goto group\n");
           media_control_service_client_command_goto_group(mcs_cid, 2);
           break;

        case '1':
            search_data = "Track";
            printf("MCS: search track name %s\n", search_data);
            media_control_service_client_search_control_command_init(mcs_cid);
            media_control_service_client_search_control_command_add(mcs_cid, SEARCH_CONTROL_POINT_TYPE_TRACK_NAME, (const char *) search_data);
            media_control_service_client_search_control_command_execute(mcs_cid);
            break;
        
        case '2':
            search_data = "Artist";
            printf("MCS: search artist name %s\n", search_data);
            media_control_service_client_search_control_command_init(mcs_cid);
            media_control_service_client_search_control_command_add(mcs_cid, SEARCH_CONTROL_POINT_TYPE_ARTIST_NAME, (const char *) search_data);
            media_control_service_client_search_control_command_execute(mcs_cid);
            break;

        case '3':
            search_data = "Album";
            printf("MCS: search album name %s\n", search_data);
            media_control_service_client_search_control_command_init(mcs_cid);
            media_control_service_client_search_control_command_add(mcs_cid, SEARCH_CONTROL_POINT_TYPE_ALBUM_NAME, (const char *) search_data);
            media_control_service_client_search_control_command_execute(mcs_cid);
            break;

        case '4':
            search_data = "Group";
            printf("MCS: search group name %s\n", search_data);
            media_control_service_client_search_control_command_init(mcs_cid);
            media_control_service_client_search_control_command_add(mcs_cid, SEARCH_CONTROL_POINT_TYPE_GROUP_NAME, (const char *) search_data);
            media_control_service_client_search_control_command_execute(mcs_cid);
            break;

        case '5':
            search_data = "1998";
            printf("MCS: search earliest year %s\n", search_data);
            media_control_service_client_search_control_command_init(mcs_cid);
            media_control_service_client_search_control_command_add(mcs_cid, SEARCH_CONTROL_POINT_TYPE_EARLIEST_YEAR, (const char *) search_data);
            media_control_service_client_search_control_command_execute(mcs_cid);
            break;

        case '6':
            search_data = "2022";
            printf("MCS: search latest year %s\n", search_data);
            media_control_service_client_search_control_command_init(mcs_cid);
            media_control_service_client_search_control_command_add(mcs_cid, SEARCH_CONTROL_POINT_TYPE_LATEST_YEAR, (const char *) search_data);
            media_control_service_client_search_control_command_execute(mcs_cid);
            break;

        case '7':
            search_data = "Genre";
            printf("MCS: search genre %s\n", search_data);
            media_control_service_client_search_control_command_init(mcs_cid);
            media_control_service_client_search_control_command_add(mcs_cid, SEARCH_CONTROL_POINT_TYPE_GENRE, (const char *) search_data);
            media_control_service_client_search_control_command_execute(mcs_cid);
            break;

        case '8':
            search_data = "";
            printf("MCS: search only tracks %s\n", search_data);
            media_control_service_client_search_control_command_init(mcs_cid);
            media_control_service_client_search_control_command_add(mcs_cid, SEARCH_CONTROL_POINT_TYPE_ONLY_TRACKS, (const char *) search_data);
            media_control_service_client_search_control_command_execute(mcs_cid);
            break;

        case '9':
            search_data = "";
            printf("MCS: search only groups %s\n", search_data);
            media_control_service_client_search_control_command_init(mcs_cid);
            media_control_service_client_search_control_command_add(mcs_cid, SEARCH_CONTROL_POINT_TYPE_ONLY_GROUPS, (const char *) search_data);
            media_control_service_client_search_control_command_execute(mcs_cid);
            break;

        case '\n':
        case '\r':
            break;

        default:
            show_usage();
            break;
    }


    if (status != ERROR_CODE_SUCCESS){
        printf("Command '%c' failed with status 0x%02x\n", cmd, status);
    }
}


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;
    
    l2cap_init();

    gatt_client_init();

    le_device_db_init();
    
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION);
    sm_event_callback_registration.callback = &hci_event_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    media_control_service_client_init();
    object_transfer_service_client_init();

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // parse human readable Bluetooth address
    sscanf_bd_addr(bap_app_server_addr_string, bap_app_server_addr);
    btstack_stdin_setup(stdin_process);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */
