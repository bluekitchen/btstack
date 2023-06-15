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

#define BTSTACK_FILE__ "media_player_server_test.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "media_player_server_test.h"
#include "btstack.h"

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

typedef enum {
    BAP_APP_SERVER_STATE_IDLE = 0,
    BAP_APP_SERVER_STATE_CONNECTED
} bap_app_server_state_t; 

static bap_app_server_state_t  bap_app_server_state      = BAP_APP_SERVER_STATE_IDLE;
static hci_con_handle_t        bap_app_server_con_handle = HCI_CON_HANDLE_INVALID;
static bd_addr_t               bap_app_client_addr;

// Advertisement

#define APP_AD_FLAGS 0x06

static uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // RSI
    0x07, BLUETOOTH_DATA_TYPE_RESOLVABLE_SET_IDENTIFIER, 0x28, 0x31, 0xB6, 0x4C, 0x39, 0xCC,
    // CSIS
    0x03, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x46, 0x18,
    // Name
    0x04, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'I', 'U', 'T', 
};

static const uint8_t adv_data_len = sizeof(adv_data);
static const uint8_t adv_sid = 0;
static uint8_t       adv_handle = 0;
static le_advertising_set_t le_advertising_set;

static const le_extended_advertising_parameters_t extended_params = {
        .advertising_event_properties = 1,  // connectable
        .primary_advertising_interval_min = 0x4b0, // 750 ms
        .primary_advertising_interval_max = 0x4b0, // 750 ms
        .primary_advertising_channel_map = 7,
        .own_address_type = 0,
        .peer_address_type = 0,
        .peer_address =  { 0 },
        .advertising_filter_policy = 0,
        .advertising_tx_power = 10, // 10 dBm
        .primary_advertising_phy = 1, // LE 1M PHY
        .secondary_advertising_max_skip = 0,
        .secondary_advertising_phy = 1, // LE 1M PHY
        .advertising_sid = adv_sid,
        .scan_request_notification_enable = 0,
};

static void setup_advertising(void);

// Object Transfer Server (OTS)
#define OTS_MAX_CLIENTS_NUM 3
static  object_transfer_service_connection_t ots_clients[OTS_MAX_CLIENTS_NUM];

// Media Player Server (MCS)

#define MCS_MEDIA_PLAYER_TIMEOUT_IN_MS  500

typedef struct {
    int8_t p_value;               // 0xFFFFFFFF unknown, or not set
    float  s_value;               // 0xFFFFFFFF unknown, or not set
} mcs_speed_values_mapping_t;

static mcs_speed_values_mapping_t mcs_speed_values_mapping[] = {
    {-128, 0.25}, {-64, 0.5}, {0, 1.0}, {64, 2.0}, {127, 3.957}
};

typedef struct {
    uint8_t parent_group_object_id[6];
    uint8_t current_group_object_id[6];

    uint8_t tracks_num;
    mcs_track_t * tracks;
} mcs_track_group_t;

typedef struct {
    media_control_service_server_t media_server;
    uint16_t id;
    uint8_t playback_speed_index; 
    uint8_t seeking_speed_index;
    bool    seeking_forward;     

    mcs_media_state_t media_state;

    uint8_t current_track_index;
    uint8_t current_group_index;

    uint8_t track_groups_num;
    mcs_track_group_t * track_groups;
} mcs_media_player_t;

static uint16_t current_media_player_id;
static btstack_timer_source_t mcs_seeking_speed_timer;
    
static void mcs_seeking_speed_timer_stop(uint16_t media_player_id);
static void mcs_seeking_speed_timer_start(uint16_t media_player_id);

static uint32_t previous_track_position_10ms;
static uint32_t previous_track_index;
static uint32_t previous_group_index;

// MCS Test
static mcs_track_t tracksA[] = {
    {18000, 0, {0x11, 0x11, 0x11, 0x11, 0x11, 0x11}, "Track1"}, //{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE}, {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB}},
    {15000, 0, {0x22, 0x22, 0x22, 0x22, 0x22, 0x22}, "Track2"}, //{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE}, {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB}},
    {12000, 0, {0x33, 0x33, 0x33, 0x33, 0x33, 0x33}, "Track3"}, //{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE}, {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB}},
};

static mcs_track_t tracksB[] = {
    {28000, 0, {0x44, 0x44, 0x44, 0x44, 0x44, 0x44}, "Track4"}, //{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE}, {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB}},
    {25000, 0, {0x55, 0x55, 0x55, 0x55, 0x55, 0x55}, "Track5"}, //{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE}, {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB}},
    {22000, 0, {0x66, 0x66, 0x66, 0x66, 0x66, 0x66}, "Track6"}, //{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE}, {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB}},
    {28000, 0, {0x77, 0x77, 0x77, 0x77, 0x77, 0x77}, "Track7"}, //{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE}, {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB}}, 
};

static mcs_track_t tracksC[] = {
    {35000, 0, {0x88, 0x88, 0x88, 0x88, 0x88, 0x88}, "Track8"}, //{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE}, {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB}},
    {32000, 0, {0x99, 0x99, 0x99, 0x99, 0x99, 0x99}, "Track9"}, //{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE}, {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB}},
};

static mcs_track_group_t  track_groups[] = {
    {{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE},{0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A}, 3, tracksA},
    {{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE},{0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B}, 4, tracksB},
    {{0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE},{0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C}, 2, tracksC}
};

static mcs_media_player_t media_player1;

static char * long_string1 = "abcdefghijkabcdefghijkabcdefghijkabcdefghijkabcdefghijk";
static char * long_string2 = "ghijkabcdefghijkabcdefghijkabcdefghijkabcdefghijkabcdefgfgfgf";

static void setup_advertising(void) {
    gap_extended_advertising_setup(&le_advertising_set, &extended_params, &adv_handle);
    gap_extended_advertising_set_adv_data(adv_handle, sizeof(adv_data), adv_data);
    gap_extended_advertising_start(adv_handle, 0, 0);
}

static mcs_media_player_t * mcs_get_media_player_for_id(uint16_t media_player_id){
    if (media_player_id == media_player1.id){
        return &media_player1;
    }
    return NULL;
}

static mcs_track_group_t * mcs_get_current_group_for_media_player(mcs_media_player_t * media_player){
    return &media_player->track_groups[media_player->current_group_index];
}

static mcs_track_t * mcs_get_current_track_for_media_player(mcs_media_player_t * media_player){
    if (media_player == NULL){
        return NULL;
    }
    mcs_track_group_t * current_track_group = mcs_get_current_group_for_media_player(media_player);
    
    if (current_track_group == NULL){
        return NULL;
    }

    return &current_track_group->tracks[media_player->current_track_index];
}

static mcs_track_t * mcs_get_current_track_for_media_player_id(uint16_t media_player_id){
    return mcs_get_current_track_for_media_player(mcs_get_media_player_for_id(media_player_id));
}

static void mcs_reset_current_track(mcs_media_player_t * media_player){
    mcs_track_t * current_track = mcs_get_current_track_for_media_player(media_player);
    // reset current track
    current_track->track_position_10ms = 0;
}

static void mcs_change_current_track(mcs_media_player_t * media_player, uint32_t track_index){
    if (media_player->current_track_index == track_index){
        return;
    }
    printf(" * change_current_track: previous %d, current %d\n", media_player->current_track_index, track_index);
    previous_track_index = media_player->current_track_index;
    // reset current
    mcs_reset_current_track(media_player);
    
    // change track
    media_player->current_track_index = track_index;
    mcs_reset_current_track(media_player);
}

static void mcs_change_current_group(mcs_media_player_t * media_player, uint32_t group_index){
    if (media_player->current_group_index == group_index){
        return;
    }
    printf(" * change_current_group: previous %d, current %d\n", media_player->current_group_index, group_index);
    previous_group_index = media_player->current_track_index;
    mcs_reset_current_track(media_player);
    // change group
    media_player->current_group_index = group_index;
    media_player->current_track_index = 0;
    mcs_reset_current_track(media_player);
}

static void mcs_goto_first_track(uint16_t media_player_id) {
    mcs_media_player_t * media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }                            

    if (media_player->current_track_index == 0){
        return;
    }
    mcs_change_current_track(media_player, 0);
}

static void mcs_goto_last_track(uint16_t media_player_id) {
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    mcs_track_group_t * current_track_group = mcs_get_current_group_for_media_player(media_player);
    if (current_track_group == NULL){
        return;
    }

    if (current_track_group->tracks_num == 0){
        return;
    }
    
    mcs_change_current_track(media_player, current_track_group->tracks_num - 1);
}

static void mcs_goto_next_track(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    mcs_track_group_t * current_track_group = mcs_get_current_group_for_media_player(media_player);
    if (current_track_group == NULL){
        return;
    }

    if (current_track_group->tracks_num <= 1){
        return;
    }

    if (media_player->current_track_index >= (current_track_group->tracks_num - 1)){
        return;
    }
    mcs_change_current_track(media_player, media_player->current_track_index + 1);
}

static void mcs_goto_previous_track(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    mcs_track_group_t * current_track_group = mcs_get_current_group_for_media_player(media_player);
    if (current_track_group == NULL){
        return;
    }

    if (current_track_group->tracks_num == 0){
        return;
    }
    
    if (media_player->current_track_index == 0){
        return;
    }
    mcs_change_current_track(media_player, media_player->current_track_index - 1);
}

static void mcs_goto_track(uint16_t media_player_id, int32_t track_index){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    mcs_track_group_t * current_track_group = mcs_get_current_group_for_media_player(media_player);
    if (current_track_group == NULL){
        return;
    }    
    if (current_track_group->tracks_num == 0){
        return;
    }
    
    uint32_t new_track_index = media_player->current_track_index;
        
    if (track_index != 0){
        if (track_index > 0){
            track_index--;
        } 
        
        if (track_index >= 0){
            if (track_index < current_track_group->tracks_num){
                new_track_index = track_index;
            }
        } else {
            uint32_t abs_track_index = -track_index;
            if (abs_track_index <= current_track_group->tracks_num){
                new_track_index = current_track_group->tracks_num - abs_track_index;
            }
        }
    }
    mcs_change_current_track(media_player, new_track_index);
}

static bool mcs_goto_group(uint16_t media_player_id, int32_t group_index){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return NULL;
    }
    if (media_player->track_groups_num == 0){
        return NULL;
    }

    int32_t previous_group_index = media_player->current_group_index;
    if (group_index != 0){

        if (group_index > 0 ){
            group_index--;
        } 

        if (media_player->current_group_index != group_index){
            if (group_index >= 0 ){
                if (group_index < media_player->track_groups_num){
                    mcs_change_current_group(media_player, group_index);
                }
            } else {
                int32_t abs_group_index = -group_index;
                if (abs_group_index <= media_player->track_groups_num){
                    mcs_change_current_group(media_player, media_player->track_groups_num - abs_group_index);
                }
            }
        }
    }

    printf(" * mcs_goto_group: previous %d, current %d\n", previous_group_index, media_player->current_group_index);
    return  previous_group_index != media_player->current_group_index;
}

static void mcs_goto_previous_group(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    if (media_player->track_groups_num == 0){
        return;
    }
    if (media_player->current_group_index == 0){
        return;
    }
    mcs_change_current_group(media_player, media_player->current_group_index - 1);
 }

static void mcs_goto_first_group(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    if (media_player->track_groups_num == 0){
        return;
    }
    if (media_player->current_group_index == 0){
        return;
    }
    mcs_change_current_group(media_player, 0);
}

static void mcs_goto_next_group(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    if (media_player->track_groups_num == 0){
        return;
    }
    if (media_player->current_group_index >= media_player->track_groups_num){
        return;
    }
    mcs_change_current_group(media_player, media_player->current_group_index + 1);
}

static void mcs_goto_last_group(uint16_t media_player_id){
    mcs_media_player_t *media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }
    if (media_player->track_groups_num == 0){
        return;
    }
    mcs_change_current_group(media_player, media_player->track_groups_num - 1);
}


static void mcs_current_track_apply_relative_offset(uint16_t media_player_id, int32_t offset_10ms){
    mcs_track_t * track = mcs_get_current_track_for_media_player_id(media_player_id);
    if (track == NULL) {
        return;
    }
    uint32_t position_10ms = track->track_position_10ms;
    printf(" * mcs_current_track_apply_relative_offset: previous position %d, offset %d, ", position_10ms, offset_10ms);
    if (offset_10ms >= 0){
        // forward
        if ((position_10ms + offset_10ms) < track->track_duration_10ms){
            position_10ms += offset_10ms;
        } else {
            position_10ms = track->track_duration_10ms;
        }
    } else {
        // backward
        uint32_t abs_offset = (uint32_t)(-offset_10ms);
        if (position_10ms > abs_offset){
            position_10ms -= offset_10ms;
        } else {
            position_10ms = 0;
        }
    }

    track->track_position_10ms = position_10ms;
    printf("current %d\n", track->track_position_10ms);
}

#define MCS_TRACK_SEGMENT_DURATION_10MS 1500
static uint32_t mcs_track_num_segments(mcs_track_t * track) {
    return track->track_duration_10ms/MCS_TRACK_SEGMENT_DURATION_10MS;
}

static uint32_t mcs_track_current_segment(mcs_track_t * track) {
    // uint32_t num_segments = mcs_track_num_segments(track);
    uint32_t current_segment = track->track_position_10ms / MCS_TRACK_SEGMENT_DURATION_10MS + 1;
    return current_segment;
}

static void mcs_current_track_goto_first_segment(mcs_track_t * track) {
    int32_t old_position = track->track_position_10ms;
    track->track_position_10ms = 0;

    printf("mcs_current_track_goto_first_segment: duration %d, num_segments %d, current segment %d, old position %d, new position %d\n", 
        track->track_duration_10ms, mcs_track_num_segments(track), mcs_track_current_segment(track), old_position, 
        track->track_position_10ms);
}

static void mcs_current_track_go_from_backward_for_num_steps(mcs_track_t * track, uint32_t backward_steps_num) {
    uint32_t step_10ms = backward_steps_num *  MCS_TRACK_SEGMENT_DURATION_10MS;
    uint32_t old_position = track->track_position_10ms;
    track->track_position_10ms = track->track_duration_10ms - step_10ms;

    printf("mcs_current_track_go_from_backward_for_num_steps: duration %d, num_segments %d, current segment %d, old position %d, backward_steps_num %d [%d], new position %d\n", 
        track->track_duration_10ms, mcs_track_num_segments(track), mcs_track_current_segment(track), old_position, 
        backward_steps_num, step_10ms, track->track_position_10ms);
}

static void mcs_current_track_go_from_begining_for_num_steps(mcs_track_t * track, uint32_t forward_steps_num) {
    uint32_t step_10ms = forward_steps_num * MCS_TRACK_SEGMENT_DURATION_10MS;
    uint32_t old_position = track->track_position_10ms;
    track->track_position_10ms = step_10ms;

    printf("mcs_current_track_go_from_begining_for_num_steps: duration %d, num_segments %d, current segment %d, old position %d, forward_steps_num %d [%d], new position %d\n", 
        track->track_duration_10ms, mcs_track_num_segments(track), mcs_track_current_segment(track), old_position, 
        forward_steps_num, step_10ms, track->track_position_10ms);
}

static void mcs_current_track_goto_last_segment(mcs_track_t * track) {
    uint32_t old_position = track->track_position_10ms;
    printf("mcs_current_track_goto_last_segment: duration %d, num_segments %d, current segment %d, old position %d, new position %d\n", 
        track->track_duration_10ms, mcs_track_num_segments(track), mcs_track_current_segment(track), old_position, 
        track->track_position_10ms);

    mcs_current_track_go_from_backward_for_num_steps(track, 1);
}

static void mcs_current_track_goto_segment(mcs_track_t * track, int32_t segment_index) {
    if (segment_index > 0) {
        if (segment_index < mcs_track_num_segments(track)){
            printf("    1: ");
            mcs_current_track_go_from_begining_for_num_steps(track, (segment_index - 1));
        } else {
            printf("    2: ");
            mcs_current_track_goto_last_segment(track);
        }
    } else if (segment_index < 0){
        uint32_t abs_index = -segment_index;
        if (abs_index < mcs_track_num_segments(track)){
            printf("    3: ");
            mcs_current_track_go_from_backward_for_num_steps(track, abs_index);
        } else {
            printf("    4: ");
            mcs_current_track_goto_first_segment(track);
        }
    }
}

static void mcs_current_track_goto_next_segment(mcs_track_t * track) {
    mcs_current_track_goto_segment(track, mcs_track_current_segment(track) + 1);
}

static void mcs_current_track_goto_previous_segment(mcs_track_t * track) {
    printf("mcs_current_track_goto_previous_segment -> current %d \n", mcs_track_current_segment(track));
    mcs_current_track_goto_segment(track, mcs_track_current_segment(track) - 1);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CAN_SEND_NOW:
            // att_server_notify(con_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
            break;
       case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Accept Just Works\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;

        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    bap_app_server_con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    bap_app_server_state      = BAP_APP_SERVER_STATE_CONNECTED;
                    hci_subevent_le_connection_complete_get_peer_address(packet, bap_app_client_addr);
                    printf("BAP Server: Connection to %s established\n", bd_addr_to_str(bap_app_client_addr));
                    
                    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
                    gap_advertisements_enable(1);
                    break;
                default:
                    return;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            bap_app_server_con_handle = HCI_CON_HANDLE_INVALID;
            bap_app_server_state      = BAP_APP_SERVER_STATE_IDLE;
            printf("BAP Server: Disconnected from %s\n", bd_addr_to_str(bap_app_client_addr));
            
            mcs_seeking_speed_timer_stop(current_media_player_id);
            gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
            gap_advertisements_enable(1);
            break;

        default:
            break;
    }
}

    
static float mcs_playing_speed_step(mcs_media_player_t * media_player){
    mcs_track_t * current_track = mcs_get_current_track_for_media_player(media_player);
    btstack_assert(current_track != NULL);
    
    float playback_speed = mcs_speed_values_mapping[media_player->playback_speed_index].s_value;
    uint32_t track_duration_ms = current_track->track_duration_10ms * 10;
    uint32_t playing_speed_delta = (uint32_t)(((float)track_duration_ms / MCS_MEDIA_PLAYER_TIMEOUT_IN_MS) * playback_speed);
    return playing_speed_delta;
}

static float mcs_seeking_speed_step(mcs_media_player_t * media_player){
    float seeking_speed  = mcs_speed_values_mapping[media_player->seeking_speed_index].s_value;

    uint32_t playing_speed_delta = mcs_playing_speed_step(media_player);
    uint32_t seeking_speed_delta = (uint32_t)((float)playing_speed_delta * seeking_speed);
    return seeking_speed_delta;
}

static bool mcs_next_track_index(mcs_media_player_t * media_player, bool next_forward, uint32_t * track_index){
    // TODO: set new track, respect playing order, and/or stop seeking at the end
    mcs_track_group_t * current_group = mcs_get_current_group_for_media_player(media_player);
    btstack_assert(current_group != NULL);
    
    if (next_forward){
        if ((media_player->current_track_index + 1) < current_group->tracks_num){
            *track_index = media_player->current_track_index + 1;
        } else {
            *track_index = 0;
        }    
    } else {
        if (media_player->current_track_index <= 1){
            *track_index = current_group->tracks_num - 1;
        } else {
            *track_index = media_player->current_track_index - 1;
        }
    }
    return true;
}

static void mcs_move_forward_with_speed_ms(mcs_media_player_t * media_player, uint32_t speed_delta_ms){
    mcs_track_t * current_track = mcs_get_current_track_for_media_player(media_player);
    btstack_assert(current_track != NULL);

    uint32_t track_duration_ms = current_track->track_duration_10ms * 10;
    uint32_t track_position_ms = current_track->track_position_10ms * 10;
    uint32_t remaining_track_ms = track_duration_ms - track_position_ms;

    if (speed_delta_ms > remaining_track_ms){
        // printf("- switch to the next track\n");
        // check if there is next track available
        uint32_t track_index;
        bool track_changed = mcs_next_track_index(media_player, true, &track_index);

        if (track_changed){
            mcs_change_current_track(media_player, track_index);
            current_track = mcs_get_current_track_for_media_player(media_player);
            btstack_assert(current_track != NULL);

            uint32_t remaining_playing_speed_delta_ms = speed_delta_ms - remaining_track_ms;
            current_track->track_position_10ms = remaining_playing_speed_delta_ms/10;
            // printf("- go forward 1, index %d, position %d, remaining %d \n", media_player->current_track_index, track_position_ms, remaining_playing_speed_delta_ms);
        }
    } else {
        // printf("- advance with the same track\n");
        current_track->track_position_10ms += speed_delta_ms/10;
    }
}

static void mcs_seek_forward(mcs_media_player_t * media_player){
    uint32_t seeking_speed_delta_ms = mcs_seeking_speed_step(media_player);
    mcs_move_forward_with_speed_ms(media_player, seeking_speed_delta_ms);
}

static void mcs_play(mcs_media_player_t * media_player){
    uint32_t playing_speed_delta_ms = mcs_playing_speed_step(media_player);
    mcs_move_forward_with_speed_ms(media_player, playing_speed_delta_ms);
}

static void mcs_seek_backward(mcs_media_player_t * media_player){
    mcs_track_t * current_track = mcs_get_current_track_for_media_player(media_player);
    btstack_assert(current_track != NULL);

    uint32_t seeking_speed_delta_ms = mcs_seeking_speed_step(media_player);

    // uint32_t track_duration_ms = current_track->track_duration_10ms * 10;
    uint32_t track_position_ms = current_track->track_position_10ms * 10;
    uint32_t remaining_track_ms = track_position_ms;

    // printf("try seek backward, index %d, duration %d, position %d, step %d, remaining %d \n", media_player->current_track_index, track_duration_ms, track_position_ms, seeking_speed_delta_ms, remaining_track_ms);
    if (seeking_speed_delta_ms > remaining_track_ms){
         // printf("- switch to the previous track\n");
        
        mcs_reset_current_track(media_player);

        uint32_t track_index;
        bool track_changed = mcs_next_track_index(media_player, true, &track_index);

        if (track_changed){
            mcs_change_current_track(media_player, track_index);
            current_track = mcs_get_current_track_for_media_player(media_player);
            btstack_assert(current_track != NULL);

            uint32_t remaining_seeking_speed_delta_ms = seeking_speed_delta_ms - remaining_track_ms;
            current_track->track_position_10ms = current_track->track_duration_10ms - remaining_seeking_speed_delta_ms/10;
            // printf("- go backward, index %d, duration %d, position %d, remaining %d \n", 
                // media_player->current_track_index, current_track->track_duration_10ms, 
                // current_track->track_position_10ms, 
                // remaining_seeking_speed_delta_ms);
        }        
    } else {
        // printf("- go backward with the same track\n");
        current_track->track_position_10ms -= seeking_speed_delta_ms/10;
    } 
}

static void mcs_server_trigger_notifications_for_opcode(mcs_media_player_t * media_player, media_control_point_opcode_t opcode){
    mcs_track_t * track = mcs_get_current_track_for_media_player(media_player);
    btstack_assert(track != NULL);

    bool mcs_track_changed = (previous_group_index != media_player->current_group_index) || (previous_track_index != media_player->current_track_index);
    bool mcs_position_changed = (previous_track_index == media_player->current_track_index) && (previous_track_position_10ms != track->track_position_10ms);

    bool notify_track_change = mcs_track_changed;
    bool notify_track_position_change = mcs_position_changed;
    
    switch (opcode){
        // track operation
        case MEDIA_CONTROL_POINT_OPCODE_MOVE_RELATIVE:
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_TRACK:
        case MEDIA_CONTROL_POINT_OPCODE_LAST_TRACK:
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_TRACK:
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_TRACK:
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_TRACK:
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_SEGMENT:
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_SEGMENT:
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_SEGMENT:
        case MEDIA_CONTROL_POINT_OPCODE_LAST_SEGMENT:
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_SEGMENT:
            switch (media_player->media_state){
                case MCS_MEDIA_STATE_SEEKING:
                case MCS_MEDIA_STATE_PAUSED:
                    notify_track_position_change = true;
                    break;
                default:
                    notify_track_position_change = false;
                    break;
            }
            break;

        // group command
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_GROUP:
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_GROUP:
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_GROUP:
        case MEDIA_CONTROL_POINT_OPCODE_LAST_GROUP:
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_GROUP:
            switch (media_player->media_state){
                case MCS_MEDIA_STATE_SEEKING:
                case MCS_MEDIA_STATE_PAUSED:
                    notify_track_position_change = true;
                    break;
                default:
                    notify_track_position_change = false;
                    break;
            }
            break;

        case MEDIA_CONTROL_POINT_OPCODE_PAUSE:
        case MEDIA_CONTROL_POINT_OPCODE_STOP:
            notify_track_position_change = true;
            break;
        default:
            break;
    }

    // printf("Update track info, title %s, length %d, pos %d\n", track->title, track->track_duration_10ms, track->track_position_10ms);
    media_control_service_server_update_current_track_info(media_player->id, track);

    if (notify_track_change){
        media_control_service_server_set_media_track_changed(media_player->id); 
        media_control_service_server_set_track_duration(media_player->id, track->track_duration_10ms);
        media_control_service_server_set_track_title(media_player->id, track->title);
    } 

    if (notify_track_position_change){
        media_control_service_server_set_track_position(media_player->id, track->track_position_10ms);
    }
}

static void mcs_server_trigger_notifications(mcs_media_player_t * media_player){
    mcs_server_trigger_notifications_for_opcode(media_player, MEDIA_CONTROL_POINT_OPCODE_RFU);
}

static void mcs_seeking_speed_timer_timeout_handler(btstack_timer_source_t * timer){
    uint16_t media_player_id = (uint16_t)(uintptr_t) btstack_run_loop_get_timer_context(timer);
    mcs_media_player_t * media_player = mcs_get_media_player_for_id(media_player_id);

    switch (media_player->media_state){
        case MCS_MEDIA_STATE_SEEKING:
            if (media_player->seeking_forward) {
                mcs_seek_forward(media_player);
            } else {
                mcs_seek_backward(media_player);
            }
            break;

        case MCS_MEDIA_STATE_PLAYING:
            mcs_play(media_player);
            break;

        default:
            break;
    }

    mcs_server_trigger_notifications(media_player);
    log_info("timer restart");

    // enforce fixed interval
#if 0
    btstack_run_loop_set_timer(&mcs_seeking_speed_timer, MCS_MEDIA_PLAYER_TIMEOUT_IN_MS); 
#else
#ifdef ENABLE_TESTING_SUPPORT_REPLAY
        mcs_seeking_speed_timer.timeout += MCS_MEDIA_PLAYER_TIMEOUT_IN_MS * 1000;
#else
        mcs_seeking_speed_timer.timeout += MCS_MEDIA_PLAYER_TIMEOUT_IN_MS;
#endif    
#endif
    btstack_run_loop_add_timer(&mcs_seeking_speed_timer);
    
}

static void mcs_seeking_speed_timer_start(uint16_t media_player_id){
    mcs_media_player_t * media_player = mcs_get_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return;
    }

    btstack_run_loop_set_timer_handler(&mcs_seeking_speed_timer, mcs_seeking_speed_timer_timeout_handler);
    btstack_run_loop_set_timer_context(&mcs_seeking_speed_timer, (void *)(uintptr_t)media_player_id);

    btstack_run_loop_set_timer(&mcs_seeking_speed_timer, MCS_MEDIA_PLAYER_TIMEOUT_IN_MS); 
    btstack_run_loop_add_timer(&mcs_seeking_speed_timer);
    log_info("timer start");
}

static void mcs_seeking_speed_timer_stop(uint16_t media_player_id){
    btstack_run_loop_remove_timer(&mcs_seeking_speed_timer);
    log_info("timer stop");
}

static void mcs_server_execute_track_operation(mcs_media_player_t * media_player, 
    media_control_point_opcode_t opcode, uint8_t * packet, uint16_t packet_size){

    int32_t value_int32;

    mcs_track_t * track = mcs_get_current_track_for_media_player_id(media_player->id);
    if (track == NULL) {
        return;
    }
    mcs_seeking_speed_timer_stop(media_player->id);
    
    switch (opcode){
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_TRACK:
            mcs_goto_first_track(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_TRACK:
            mcs_goto_last_track(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_TRACK:
            mcs_goto_previous_track(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_TRACK:
            mcs_goto_next_track(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_TRACK:
            value_int32 = (int32_t) gattservice_subevent_mcs_server_media_control_point_notification_task_get_data(packet);
            mcs_goto_track(media_player->id, value_int32);
            break;

        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_SEGMENT:
            printf(" - Goto previous segment, current %d\n", mcs_track_current_segment(track));
            mcs_current_track_goto_previous_segment(track);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_SEGMENT:
            printf(" - Goto next segment, current %d\n", mcs_track_current_segment(track));
            mcs_current_track_goto_next_segment(track);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_SEGMENT:
            printf(" - Goto first segment, current %d\n", mcs_track_current_segment(track));
            mcs_current_track_goto_first_segment(track);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_SEGMENT:
            printf(" - Goto last segment, current %d\n", mcs_track_current_segment(track));
            mcs_current_track_goto_last_segment(track);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_SEGMENT:
            value_int32 = (int32_t) gattservice_subevent_mcs_server_media_control_point_notification_task_get_data(packet);
            printf(" - Goto %d segment, current %d\n", value_int32, mcs_track_current_segment(track));
            mcs_current_track_goto_segment(track, value_int32);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_GROUP:
            mcs_goto_previous_group(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_GROUP:
            mcs_goto_next_group(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_GROUP:
            mcs_goto_first_group(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_GROUP:
            mcs_goto_last_group(media_player->id);
            break;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_GROUP:
            value_int32 = (int32_t) gattservice_subevent_mcs_server_media_control_point_notification_task_get_data(packet);
            mcs_goto_group(media_player->id, value_int32);
            break;
        
        case MEDIA_CONTROL_POINT_OPCODE_MOVE_RELATIVE:
            value_int32 = (int32_t) gattservice_subevent_mcs_server_media_control_point_notification_task_get_data(packet);
            mcs_current_track_apply_relative_offset(media_player->id, value_int32);
            break;

        case MEDIA_CONTROL_POINT_OPCODE_STOP:
            track->track_position_10ms = 0;
            break;

        default:
            break;
    }
    mcs_server_trigger_notifications_for_opcode(media_player, opcode);

    switch (media_player->media_state){
        case MCS_MEDIA_STATE_PLAYING:
        case MCS_MEDIA_STATE_SEEKING:
            mcs_seeking_speed_timer_start(media_player->id);
            break;
        default:
            break;
    } 
}

static void ots_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
}

static void mcs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    uint8_t characteristic_id;
    media_control_point_opcode_t opcode;
    uint16_t media_player_id;
    mcs_media_player_t * media_player;
    uint8_t status;

    const uint8_t * search_data;
    uint8_t search_data_len;
    uint8_t pos = 0;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_MCS_SERVER_VALUE_CHANGED:
            characteristic_id = gattservice_subevent_mcs_server_value_changed_get_characteristic_id(packet);
            printf("MCS Server App: changed value, characteristic_id %d\n", characteristic_id);
            break;

        case GATTSERVICE_SUBEVENT_MCS_SERVER_SEARCH_CONTROL_POINT_NOTIFICATION_TASK:
            media_player_id = gattservice_subevent_mcs_server_search_control_point_notification_task_get_media_player_id(packet);
            media_player = mcs_get_media_player_for_id(media_player_id);
            
            if (media_player == NULL){
                return;
            }
            search_data     = gattservice_subevent_mcs_server_search_control_point_notification_task_get_data(packet);
            search_data_len = gattservice_subevent_mcs_server_search_control_point_notification_task_get_data_length(packet);
            
            printf("MCS Server App: Search Notification, data len %d\n", search_data_len);
            
            pos = 0;
            while (pos < search_data_len){
                printf_hexdump(search_data + pos, search_data_len - pos);
                
                uint8_t search_field_length = search_data[pos++] - 2;
                search_control_point_type_t search_field_type = (search_control_point_type_t)search_data[pos++];

                const char * search_field_data;
                if (search_field_length > 0){
                    search_field_data = (const char *)&search_data[pos];
                }

                printf("  ** Search field len %d, type %d, data %s\n",  search_field_length, search_field_type, search_field_length > 0 ? search_field_data : "");
                pos += search_field_length + 2;
            }
            media_control_service_server_search_control_point_response(media_player_id, tracksA[0].object_id);

            break;

        case GATTSERVICE_SUBEVENT_MCS_SERVER_MEDIA_CONTROL_POINT_NOTIFICATION_TASK:
            opcode = (media_control_point_opcode_t)gattservice_subevent_mcs_server_media_control_point_notification_task_get_opcode(packet);
            media_player_id = gattservice_subevent_mcs_server_media_control_point_notification_task_get_media_player_id(packet);
            media_player = mcs_get_media_player_for_id(media_player_id);
            
            if (media_player == NULL){
                return;
            }

            printf("MCS Server App: Control Notification, opcode %s, state %s\n", 
                mcs_server_media_control_opcode2str(opcode), 
                mcs_server_media_state2str(media_player->media_state));

            if (media_player->media_state == MCS_MEDIA_STATE_INACTIVE){
                switch (opcode){
                    case MEDIA_CONTROL_POINT_OPCODE_PLAY:
                    case MEDIA_CONTROL_POINT_OPCODE_PAUSE:
                        // the application should deal with this state below
                        break; 
                    default:
                        media_control_service_server_media_control_point_response(media_player_id, opcode, MEDIA_CONTROL_POINT_ERROR_CODE_MEDIA_PLAYER_INACTIVE);
                        return;
                }
            }

            // accept command
            media_control_service_server_media_control_point_response(media_player_id, opcode, MEDIA_CONTROL_POINT_ERROR_CODE_SUCCESS);
            
            switch (opcode){
                case MEDIA_CONTROL_POINT_OPCODE_FAST_REWIND:
                case MEDIA_CONTROL_POINT_OPCODE_FAST_FORWARD:
                    mcs_seeking_speed_timer_stop(media_player_id);
                    
                    if (opcode == MEDIA_CONTROL_POINT_OPCODE_FAST_FORWARD){
                        media_player->seeking_forward = true;
                    } else {
                        media_player->seeking_forward = false;
                    }

                    status = media_control_service_server_set_seeking_speed(media_player_id, 64);
                    if (status != ERROR_CODE_SUCCESS){
                        return;
                    }
                    
                    status = media_control_service_server_set_media_state(media_player_id, MCS_MEDIA_STATE_SEEKING);
                    if (status == ERROR_CODE_SUCCESS){
                        media_player->media_state = MCS_MEDIA_STATE_SEEKING;
                        mcs_seeking_speed_timer_start(media_player_id);
                    }
                    return;

                case MEDIA_CONTROL_POINT_OPCODE_PLAY:
                    if (media_player->media_state == MCS_MEDIA_STATE_PLAYING){
                        // ignore command
                        return;
                    }
                    mcs_seeking_speed_timer_stop(media_player_id);
                    status = media_control_service_server_set_media_state(media_player_id, MCS_MEDIA_STATE_PLAYING);
                    if (status == ERROR_CODE_SUCCESS){
                        media_player->media_state = MCS_MEDIA_STATE_PLAYING;
                        mcs_seeking_speed_timer_start(media_player_id);
                    }
                    return;

                case MEDIA_CONTROL_POINT_OPCODE_PAUSE:
                    if (media_player->media_state == MCS_MEDIA_STATE_PAUSED){
                        // ignore command
                        return;
                    }
                    status = media_control_service_server_set_media_state(media_player_id, MCS_MEDIA_STATE_PAUSED);
                    if (status == ERROR_CODE_SUCCESS){
                         media_player->media_state = MCS_MEDIA_STATE_PAUSED;
                         mcs_seeking_speed_timer_stop(media_player_id);
                    }
                    mcs_server_trigger_notifications_for_opcode(media_player, opcode);
                    return;
                        
                case MEDIA_CONTROL_POINT_OPCODE_STOP:
                    status = media_control_service_server_set_media_state(media_player_id, MCS_MEDIA_STATE_PAUSED);
                    if (status == ERROR_CODE_SUCCESS){
                         media_player->media_state = MCS_MEDIA_STATE_PAUSED;
                    }
                    mcs_server_execute_track_operation(media_player, opcode, packet, size);
                    return;

                case MEDIA_CONTROL_POINT_OPCODE_MOVE_RELATIVE:
                    mcs_server_execute_track_operation(media_player, opcode, packet, size);
                    break;

                case MEDIA_CONTROL_POINT_OPCODE_FIRST_TRACK:
                case MEDIA_CONTROL_POINT_OPCODE_LAST_TRACK:
                case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_TRACK:
                case MEDIA_CONTROL_POINT_OPCODE_NEXT_TRACK:
                case MEDIA_CONTROL_POINT_OPCODE_GOTO_TRACK:
                case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_SEGMENT:
                case MEDIA_CONTROL_POINT_OPCODE_NEXT_SEGMENT:
                case MEDIA_CONTROL_POINT_OPCODE_FIRST_SEGMENT:
                case MEDIA_CONTROL_POINT_OPCODE_LAST_SEGMENT:
                case MEDIA_CONTROL_POINT_OPCODE_GOTO_SEGMENT:
                    mcs_server_execute_track_operation(media_player, opcode, packet, size);
                    return;

                case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_GROUP:
                case MEDIA_CONTROL_POINT_OPCODE_NEXT_GROUP:
                case MEDIA_CONTROL_POINT_OPCODE_FIRST_GROUP:
                case MEDIA_CONTROL_POINT_OPCODE_LAST_GROUP:
                case MEDIA_CONTROL_POINT_OPCODE_GOTO_GROUP:
                    if (media_player->media_state == MCS_MEDIA_STATE_SEEKING){
                        status = media_control_service_server_set_media_state(media_player_id, MCS_MEDIA_STATE_PAUSED);
                        if (status == ERROR_CODE_SUCCESS){
                             mcs_seeking_speed_timer_stop(media_player_id);
                             media_player->media_state = MCS_MEDIA_STATE_PAUSED;
                        }
                    }

                    mcs_server_execute_track_operation(media_player, opcode, packet, size);
                    return;

                default:
                    break;
            }
            
            break;

        default:
            break;
    }
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);

    return 0;
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    return 0;
}

static void show_usage(void){
    uint8_t iut_address_type;
    bd_addr_t      iut_address;
    gap_le_get_own_address(&iut_address_type, iut_address);

    printf("IUT: addr type %u, addr %s", iut_address_type, bd_addr_to_str(iut_address));

    char * current_media_state = mcs_server_media_state2str(media_control_service_server_get_media_state(current_media_player_id));
    
    printf("\n## MCS\n");
    printf("0 - set INACTIVE state, current %s\n", current_media_state);
    printf("1 - set PLAYING  state, current %s\n", current_media_state);
    printf("2 - set PAUSED   state, current %s\n", current_media_state);
    printf("3 - set SEEKING  state, current %s\n", current_media_state);

    printf("4 - goto 5th segment\n");
    printf("5 - goto last song\n");
    printf("6 - goto second group\n");

    printf("j - set long media player name 1\n");
    printf("J - set long media player name 2\n");
    printf("k - set long track title 1\n");
    printf("K - set long track title 2\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;

    printf("MCS Server App: Command %c, media player ID %d, state %s\n", cmd, current_media_player_id, 
        mcs_server_media_state2str(media_control_service_server_get_media_state(current_media_player_id)));
    
    mcs_media_player_t * media_player = mcs_get_media_player_for_id(current_media_player_id);
    btstack_assert(media_player != NULL);

    switch (cmd){
        case '0':
            printf(" - Set INACTIVE state\n");
            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_INACTIVE);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_INACTIVE;
            }
            break;

        case '1':
            printf(" - Set PLAYING state\n");
            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_PLAYING);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_PLAYING;
            }
            break;

        case '2':
            printf(" - Set PAUSED state\n");
            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_PAUSED);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_PAUSED;
            }
            break;

        case '3':
            printf(" - Set SEEKING state\n");
            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_SEEKING);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_SEEKING;
            }
            break;

        case '4':
            printf(" - Goto the 5th segment\n");

            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_PLAYING);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_PLAYING;
            }
            mcs_current_track_goto_segment(mcs_get_current_track_for_media_player_id(current_media_player_id), 5);
            break;
            
        case '5':
            printf(" - Goto the last song\n");

            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_PLAYING);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_PLAYING;
            }
            mcs_goto_last_track(current_media_player_id);
            break;
        
        case '6':
            printf(" - Goto the second group\n");

            status = media_control_service_server_set_media_state(current_media_player_id, MCS_MEDIA_STATE_PLAYING);
            if (status == ERROR_CODE_SUCCESS){
                media_player->media_state = MCS_MEDIA_STATE_PLAYING;
            }
            mcs_goto_group(current_media_player_id, 2);
            break;

        case 'j':
            status = media_control_service_server_set_media_player_name(current_media_player_id, long_string1);
            break;
        case 'J':
            status = media_control_service_server_set_media_player_name(current_media_player_id, long_string2);
            break;
        case 'k':
            status = media_control_service_server_set_track_title(current_media_player_id, long_string1);
            break;
        case 'K':
            status = media_control_service_server_set_track_title(current_media_player_id, long_string2);
            break;
        
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf(" - Command '%c' could not be performed, status 0x%02x\n", cmd, status);
    }
}

int btstack_main(void);
int btstack_main(void)
{
    
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
    sm_allow_ltk_reconstruction_without_le_device_db_entry(0);
    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    

    // setup OTS
    object_transfer_service_server_init(0x3FF, 0x0F, OTS_MAX_CLIENTS_NUM, ots_clients);
    object_transfer_service_server_register_packet_handler(&ots_server_packet_handler);

    // setup MCS
    media_control_service_server_init();

    
    uint8_t icon_object_id[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    char * icon_url = "https://www.bluetooth.com/";

    previous_track_position_10ms = 0;
    previous_track_index = 1;
    previous_group_index = 3;

    media_player1.playback_speed_index = 2;            // 1
    media_player1.seeking_speed_index  = 3;            // 2.0 
    media_player1.current_track_index = previous_track_index;
    media_player1.track_groups = track_groups;
    media_player1.track_groups_num = previous_group_index;
    media_player1.seeking_forward = true;

    mcs_track_t * current_track = mcs_get_current_track_for_media_player(&media_player1);
    btstack_assert(current_track != NULL);

    media_control_service_server_register_media_player(&media_player1.media_server, 
        &mcs_server_packet_handler, 0x1FFFFF, 
        &media_player1.id);
    media_control_service_server_set_media_player_name(media_player1.id, "BK Player1");
    media_control_service_server_set_icon_object_id(media_player1.id, icon_object_id, sizeof(icon_object_id));
    media_control_service_server_set_icon_url(media_player1.id, icon_url);

    media_control_service_server_set_track_title(media_player1.id, current_track->title);
    media_control_service_server_set_track_duration(media_player1.id, current_track->track_duration_10ms);
    media_control_service_server_set_track_position(media_player1.id, current_track->track_position_10ms);
    
    media_control_service_server_set_playing_orders_supported(media_player1.id, 0x3FF);
    media_control_service_server_set_playing_order(media_player1.id, PLAYING_ORDER_IN_ORDER_ONCE);

    current_media_player_id = media_player1.id;
    mcs_seeking_speed_timer_start(current_media_player_id);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);


    // setup advertisements
    setup_advertising();
    gap_periodic_advertising_sync_transfer_set_default_parameters(2, 0, 0x2000, 0);

    btstack_stdin_setup(stdin_process);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
