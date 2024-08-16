/*
 * Copyright (C) 2021 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "media_control_service_server.c"

/**
 * Implementation of the GATT Battery Service Server 
 * To use with your application, add '#import <media_control_service.gatt' to your .gatt file
 */

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/gatt-service/media_control_service_util.h"
#include "le-audio/gatt-service/media_control_service_server.h"
#include "le-audio/gatt-service/audio_input_control_service_server.h"


#define MCS_NOTIFICATION_TASK_MEDIA_PLAYER_NAME                         0x000001                   
#define MCS_NOTIFICATION_TASK_MEDIA_PLAYER_ICON_OBJECT_ID               0x000002
#define MCS_NOTIFICATION_TASK_MEDIA_PLAYER_ICON_URL                     0x000004
#define MCS_NOTIFICATION_TASK_TRACK_CHANGED                             0x000008
#define MCS_NOTIFICATION_TASK_TRACK_TITLE                               0x000010
#define MCS_NOTIFICATION_TASK_TRACK_DURATION                            0x000020
#define MCS_NOTIFICATION_TASK_TRACK_POSITION                            0x000040
#define MCS_NOTIFICATION_TASK_PLAYBACK_SPEED                            0x000080
#define MCS_NOTIFICATION_TASK_SEEKING_SPEED                             0x000100
#define MCS_NOTIFICATION_TASK_CURRENT_TRACK_SEGMENTS_OBJECT_ID          0x000200
#define MCS_NOTIFICATION_TASK_CURRENT_TRACK_OBJECT_ID                   0x000400
#define MCS_NOTIFICATION_TASK_NEXT_TRACK_OBJECT_ID                      0x000800
#define MCS_NOTIFICATION_TASK_PARENT_GROUP_OBJECT_ID                    0x001000
#define MCS_NOTIFICATION_TASK_CURRENT_GROUP_OBJECT_ID                   0x002000
#define MCS_NOTIFICATION_TASK_PLAYING_ORDER                             0x004000
#define MCS_NOTIFICATION_TASK_PLAYING_ORDERS_SUPPORTED                  0x008000
#define MCS_NOTIFICATION_TASK_MEDIA_STATE                               0x010000
#define MCS_NOTIFICATION_TASK_MEDIA_CONTROL_POINT                       0x020000
#define MCS_NOTIFICATION_TASK_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED     0x040000
#define MCS_NOTIFICATION_TASK_SEARCH_RESULTS_OBJECT_ID                  0x080000
#define MCS_NOTIFICATION_TASK_SEARCH_CONTROL_POINT                      0x100000
#define MCS_NOTIFICATION_TASK_CONTENT_CONTROL_ID                        0x200000

typedef enum {
	HANDLE_TYPE_CHARACTERISTIC_VALUE = 0,
	HANDLE_TYPE_CHARACTERISTIC_CCD,
} handle_type_t;

static btstack_linked_list_t    mcs_media_players;
static uint16_t mcs_media_player_counter = 0;
static uint16_t mcs_services_start_handle = 0;

static uint16_t msc_server_get_next_media_player_id(void){
	mcs_media_player_counter = btstack_next_cid_ignoring_zero(mcs_media_player_counter);
	return mcs_media_player_counter;
}


static char * media_state_names[] = {
    "INACTIVE",
    "PLAYING",
    "PAUSED",
    "SEEKING",
    "RFU"
};

static char * media_control_opcode_names[] = {
    "PLAY",
    "PAUSE",
    "FAST_REWIND",
    "FAST_FORWARD",
    "STOP",
    "MOVE_RELATIVE",
    "PREVIOUS_SEGMENT",
    "NEXT_SEGMENT",
    "FIRST_SEGMENT",
    "LAST_SEGMENT",
    "GOTO_SEGMENT",
    "PREVIOUS_TRACK",
    "NEXT_TRACK",
    "FIRST_TRACK",
    "LAST_TRACK",
    "GOTO_TRACK",
    "PREVIOUS_GROUP",
    "NEXT_GROUP",
    "FIRST_GROUP",
    "LAST_GROUP",
    "GOTO_GROUP",
    "RFU"
};

static char * media_control_characteristic_names[] = {
    "MEDIA_PLAYER_NAME",
    "MEDIA_PLAYER_ICON_OBJECT_ID",
    "MEDIA_PLAYER_ICON_URL",
    "TRACK_CHANGED",
    "TRACK_TITLE",
    "TRACK_DURATION",
    "TRACK_POSITION",
    "PLAYBACK_SPEED",
    "SEEKING_SPEED",
    "CURRENT_TRACK_SEGMENTS_OBJECT_ID",
    "CURRENT_TRACK_OBJECT_ID",
    "NEXT_TRACK_OBJECT_ID",
    "PARENT_GROUP_OBJECT_ID",
    "CURRENT_GROUP_OBJECT_ID",
    "PLAYING_ORDER",
    "PLAYING_ORDERS_SUPPORTED",
    "MEDIA_STATE",
    "MEDIA_CONTROL_POINT",
    "MEDIA_CONTROL_POINT_OPCODES_SUPPORTED",
    "SEARCH_RESULTS_OBJECT_ID",
    "SEARCH_CONTROL_POINT",
    "CONTENT_CONTROL_ID",
    "RFU",
};

static void mcs_server_schedule_task(media_control_service_server_t * media_player, msc_characteristic_id_t characteristic_id);

static uint8_t mcs_media_control_point_opcode_index(media_control_point_opcode_t opcode){
    switch (opcode){
        case MEDIA_CONTROL_POINT_OPCODE_PLAY:
            return 0;
        case MEDIA_CONTROL_POINT_OPCODE_PAUSE:           
            return 1;
        case MEDIA_CONTROL_POINT_OPCODE_FAST_REWIND:     
            return 2;
        case MEDIA_CONTROL_POINT_OPCODE_FAST_FORWARD:    
            return 3;
        case MEDIA_CONTROL_POINT_OPCODE_STOP:   
            return 4;
        case MEDIA_CONTROL_POINT_OPCODE_MOVE_RELATIVE:   
            return 5;
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_SEGMENT:
            return 6;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_SEGMENT:
            return 7;
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_SEGMENT:   
            return 8;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_SEGMENT:  
            return 9;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_SEGMENT:   
            return 10;
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_TRACK: 
            return 11;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_TRACK:
            return 12;
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_TRACK:
            return 13;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_TRACK:
            return 14;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_TRACK:
            return 15;
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_GROUP:
            return 16;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_GROUP:
            return 17;
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_GROUP:
            return 18;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_GROUP:
            return 19;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_GROUP:
            return 20;
        default:
            return 21;
    }
}

char * mcs_server_media_control_opcode2str(media_control_point_opcode_t opcode){
    uint8_t opcode_index = mcs_media_control_point_opcode_index(opcode);
    return media_control_opcode_names[opcode_index];
}

char * mcs_server_media_state2str(mcs_media_state_t media_state){
    if (media_state >= MCS_MEDIA_STATE_RFU){
        return media_state_names[(uint8_t) MCS_MEDIA_STATE_RFU];
    }
    return media_state_names[(uint8_t)media_state];
}

char * mcs_server_characteristic2str(msc_characteristic_id_t msc_characteristic){
    if (msc_characteristic >= NUM_MCS_CHARACTERISTICS){
        return media_control_characteristic_names[(uint8_t) NUM_MCS_CHARACTERISTICS];
    }
    return media_control_characteristic_names[(uint8_t) msc_characteristic];
}

static bool mcs_media_control_point_opcode_supported(media_control_service_server_t * media_player, media_control_point_opcode_t opcode){
    switch (opcode){
        case MEDIA_CONTROL_POINT_OPCODE_PLAY:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_PLAY) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_PAUSE:           
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_PAUSE) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_FAST_REWIND:     
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_FAST_REWIND) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_FAST_FORWARD:    
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_FAST_FORWARD) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_STOP:   
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_STOP) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_MOVE_RELATIVE:   
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_MOVE_RELATIVE) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_SEGMENT:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_PREVIOUS_SEGMENT) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_SEGMENT:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_NEXT_SEGMENT) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_SEGMENT:   
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_FIRST_SEGMENT) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_SEGMENT:  
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_LAST_SEGMENT) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_SEGMENT:   
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_GOTO_SEGMENT) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_TRACK: 
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_PREVIOUS_TRACK) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_TRACK:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_NEXT_TRACK) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_TRACK:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_FIRST_TRACK) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_TRACK:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_LAST_TRACK) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_TRACK:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_GOTO_TRACK) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_PREVIOUS_GROUP:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_PREVIOUS_GROUP) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_NEXT_GROUP:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_NEXT_GROUP) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_FIRST_GROUP:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_FIRST_GROUP) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_LAST_GROUP:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_LAST_GROUP) != 0;
        case MEDIA_CONTROL_POINT_OPCODE_GOTO_GROUP:
            return ( media_player->data.media_control_point_opcodes_supported & MEDIA_CONTROL_POINT_OPCODE_MASK_GOTO_GROUP) != 0;
        default:
            return false;
    }
}

static void mcs_server_reset_media_player(media_control_service_server_t * media_player){
    media_player->con_handle = HCI_CON_HANDLE_INVALID;
    media_player->scheduled_tasks = 0;

    memset(&media_player->data, 0, sizeof(mcs_media_player_data_t));
    media_player->data.track_duration_10ms = 0xFFFFFFFF;    
    media_player->data.track_position_10ms = 0xFFFFFFFF;       
    
    media_player->data.name_value_changed = false;
    media_player->data.track_title_value_changed = false;
}

static media_control_service_server_t * msc_server_media_player_registered(uint16_t start_handle){
	btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &mcs_media_players);
    while (btstack_linked_list_iterator_has_next(&it)){
        media_control_service_server_t * media_player = (media_control_service_server_t *)btstack_linked_list_iterator_next(&it);
        if (media_player->service.start_handle != start_handle) continue;
        return media_player;
    }
    return NULL;
}

static media_control_service_server_t * msc_server_find_media_player_for_id(uint16_t media_player_id){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &mcs_media_players);
    while (btstack_linked_list_iterator_has_next(&it)){
        media_control_service_server_t * media_player = (media_control_service_server_t *)btstack_linked_list_iterator_next(&it);
        if (media_player_id == media_player->player_id){
        	return media_player;
        } 
    }
    return NULL;
}

static media_control_service_server_t * msc_server_find_media_player_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &mcs_media_players);
    while (btstack_linked_list_iterator_has_next(&it)){
        media_control_service_server_t * media_player = (media_control_service_server_t *)btstack_linked_list_iterator_next(&it);
        if (con_handle == media_player->con_handle){
        	return media_player;
        } 
    }
    return NULL;
}

static media_control_service_server_t * msc_server_find_media_player_for_attribute_handle(uint16_t attribute_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &mcs_media_players);
    while (btstack_linked_list_iterator_has_next(&it)){
        media_control_service_server_t * media_player = (media_control_service_server_t *)btstack_linked_list_iterator_next(&it);
        if ((attribute_handle > media_player->service.start_handle) && (attribute_handle <= media_player->service.end_handle)){
        	return media_player;
        } 
    }
    return NULL;
}

static msc_characteristic_id_t msc_server_find_index_for_attribute_handle(media_control_service_server_t * media_player, uint16_t attribute_handle, handle_type_t * type){
    uint8_t i;
    for (i = 0; i < NUM_MCS_CHARACTERISTICS; i++){
		if (attribute_handle == media_player->characteristics[i].client_configuration_handle){
			*type = HANDLE_TYPE_CHARACTERISTIC_CCD;
			return (msc_characteristic_id_t)i;
		}
		if (attribute_handle == media_player->characteristics[i].value_handle){
			*type = HANDLE_TYPE_CHARACTERISTIC_VALUE;
			return (msc_characteristic_id_t)i;
		}
	}
	btstack_assert(false);
    return -1;
}

static void mcs_server_emit_media_value_changed(media_control_service_server_t * media_player, msc_characteristic_id_t characteristic_id){
    btstack_assert(media_player->event_callback != NULL);
    
    uint8_t event[9];
    memset(event, 0, sizeof(event));

    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_MCS_SERVER_VALUE_CHANGED;
    little_endian_store_16(event, pos, media_player->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, media_player->player_id);
    pos += 2;
    event[pos++] = media_player->data.media_state;
    event[pos++] = (uint8_t)characteristic_id;

    (*media_player->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void mcs_server_emit_media_control_notification_task(media_control_service_server_t * media_player, media_control_point_opcode_t opcode, uint8_t * buffer, uint16_t buffer_size){
    btstack_assert(media_player->event_callback != NULL);
    
    uint8_t event[13];
    memset(event, 0, sizeof(event));

    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_MCS_SERVER_MEDIA_CONTROL_POINT_NOTIFICATION_TASK;
    little_endian_store_16(event, pos, media_player->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, media_player->player_id);
    pos += 2;
    event[pos++] = media_player->data.media_state;
    event[pos++] = (uint8_t)opcode;

    if (buffer_size == 4){
        memcpy(&event[pos], buffer, 4);
    }

    (*media_player->event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void mcs_server_emit_search_control_notification_task(media_control_service_server_t * media_player, uint8_t * buffer, uint16_t buffer_size){
    btstack_assert(media_player->event_callback != NULL);
    
    uint8_t event[9 + MCS_SEARCH_CONTROL_POINT_COMMAND_MAX_LENGTH];
    memset(event, 0, sizeof(event));

    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_MCS_SERVER_SEARCH_CONTROL_POINT_NOTIFICATION_TASK;
    little_endian_store_16(event, pos, media_player->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, media_player->player_id);
    pos += 2;
    event[pos++] = media_player->data.media_state;
    event[pos++] = buffer_size;
    memcpy(&event[pos], buffer, buffer_size);
    pos += buffer_size;

    (*media_player->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void mcs_server_set_con_handle(media_control_service_server_t * media_player, uint16_t characteristic_index, hci_con_handle_t con_handle, uint16_t configuration){
    media_player->characteristics[characteristic_index].client_configuration = configuration;
    if (configuration == 0){
        media_player->con_handle = HCI_CON_HANDLE_INVALID;
    } else {
        media_player->con_handle = con_handle;
        //mcs_server_schedule_task(media_player, characteristic_index);
    }
}

static void mcs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
	UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }

    hci_con_handle_t con_handle;
    media_control_service_server_t * media_player;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            media_player = msc_server_find_media_player_for_con_handle(con_handle);
            if (media_player != NULL){
                mcs_server_reset_media_player(media_player);
            }
            break;
        default:
            break;
    }
}

static uint16_t mcs_server_max_att_value_len(hci_con_handle_t con_handle){
	uint16_t att_mtu = att_server_get_mtu(con_handle);
    return (att_mtu >= 3) ? (att_mtu - 3) : 0;
}

static uint16_t mcs_server_max_value_len(hci_con_handle_t con_handle, uint16_t value_len){
	return btstack_min(mcs_server_max_att_value_len(con_handle), value_len);
}

static void mcs_server_can_send_now(void * context){
    media_control_service_server_t * media_player = (media_control_service_server_t *) context;

    if (media_player->con_handle == HCI_CON_HANDLE_INVALID){
        media_player->scheduled_tasks = 0;
        return;
    }

    // uint32_t scheduled_tasks = media_player->scheduled_tasks;

    if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_MEDIA_PLAYER_NAME) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_MEDIA_PLAYER_NAME;

        att_server_notify(media_player->con_handle, 
            media_player->characteristics[MEDIA_PLAYER_NAME].value_handle, 
            (uint8_t *)media_player->data.name, 
            mcs_server_max_value_len(media_player->con_handle, strlen(media_player->data.name)));
    
    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_TRACK_CHANGED) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_TRACK_CHANGED;
        
    	att_server_notify(media_player->con_handle, 
            media_player->characteristics[TRACK_CHANGED].value_handle,
            NULL, 0);

    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_TRACK_TITLE) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_TRACK_TITLE;

        att_server_notify(media_player->con_handle, 
            media_player->characteristics[TRACK_TITLE].value_handle, 
            (uint8_t *)media_player->data.track_title, 
            mcs_server_max_value_len(media_player->con_handle, strlen(media_player->data.track_title)));
    
    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_TRACK_DURATION) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_TRACK_DURATION;
        uint8_t value[4];
        little_endian_store_32(value, 0, media_player->data.track_duration_10ms);
        
        att_server_notify(media_player->con_handle, 
            media_player->characteristics[TRACK_DURATION].value_handle, 
            (uint8_t *)value, sizeof(value));
    
    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_PLAYBACK_SPEED) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_PLAYBACK_SPEED;
        uint8_t value[4];
        little_endian_store_32(value, 0, media_player->data.playback_speed);
        
        att_server_notify(media_player->con_handle, 
            media_player->characteristics[PLAYBACK_SPEED].value_handle, 
            (uint8_t *)value, sizeof(value));

    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_TRACK_POSITION) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_TRACK_POSITION;
        uint8_t value[4];
        little_endian_store_32(value, 0, media_player->data.track_position_10ms);
        
        att_server_notify(media_player->con_handle, 
            media_player->characteristics[TRACK_POSITION].value_handle, 
            (uint8_t *)value, sizeof(value));

    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_PLAYBACK_SPEED) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_PLAYBACK_SPEED;
        uint8_t value[1];
        value[0] = media_player->data.playback_speed;
        
        att_server_notify(media_player->con_handle, 
            media_player->characteristics[PLAYBACK_SPEED].value_handle, 
            (uint8_t *)value, sizeof(value));

    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_SEEKING_SPEED) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_SEEKING_SPEED;
        uint8_t value[1];
        value[0] = media_player->data.seeking_speed;

        att_server_notify(media_player->con_handle, 
            media_player->characteristics[SEEKING_SPEED].value_handle, 
            (uint8_t *)value, sizeof(value));
    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_CURRENT_TRACK_OBJECT_ID) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_CURRENT_TRACK_OBJECT_ID;

        uint8_t value[6];
        reverse_48(media_player->data.current_track_object_id, value);

        att_server_notify(media_player->con_handle, 
            media_player->characteristics[CURRENT_TRACK_OBJECT_ID].value_handle, 
            value,
            sizeof(media_player->data.current_track_object_id));

    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_NEXT_TRACK_OBJECT_ID) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_NEXT_TRACK_OBJECT_ID;
        uint8_t value[6];
        reverse_48(media_player->data.next_track_object_id, value);

        att_server_notify(media_player->con_handle, 
            media_player->characteristics[NEXT_TRACK_OBJECT_ID].value_handle, 
            value,
            sizeof(media_player->data.next_track_object_id));
    
    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_CURRENT_TRACK_OBJECT_ID) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_CURRENT_TRACK_OBJECT_ID;
        
        att_server_notify(media_player->con_handle, 
            media_player->characteristics[CURRENT_TRACK_OBJECT_ID].value_handle, 
            media_player->data.current_track_object_id, 
            sizeof(media_player->data.current_track_object_id));
    
    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_PARENT_GROUP_OBJECT_ID) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_PARENT_GROUP_OBJECT_ID;
        uint8_t value[6];
        reverse_48(media_player->data.parent_group_object_id, value);

        att_server_notify(media_player->con_handle, 
            media_player->characteristics[PARENT_GROUP_OBJECT_ID].value_handle,
                          value,
            sizeof(media_player->data.parent_group_object_id));
    
    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_CURRENT_GROUP_OBJECT_ID) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_CURRENT_GROUP_OBJECT_ID;

        uint8_t value[6];
        reverse_48(media_player->data.current_group_object_id, value);
        att_server_notify(media_player->con_handle, 
            media_player->characteristics[CURRENT_GROUP_OBJECT_ID].value_handle,
                          value,
            sizeof(media_player->data.current_group_object_id));
    
    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_SEARCH_RESULTS_OBJECT_ID) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_SEARCH_RESULTS_OBJECT_ID;
        uint8_t value[6];
        reverse_48(media_player->data.search_results_object_id, value);
        att_server_notify(media_player->con_handle, 
            media_player->characteristics[SEARCH_RESULTS_OBJECT_ID].value_handle,
                          value, media_player->data.search_results_object_id_len);
    
    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_PLAYING_ORDER) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_PLAYING_ORDER;
        uint8_t value[1];
        value[0] = (uint8_t)media_player->data.playing_order;
        
        att_server_notify(media_player->con_handle, 
            media_player->characteristics[PLAYING_ORDER].value_handle, 
            (uint8_t *)value, sizeof(value));

    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_MEDIA_STATE) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_MEDIA_STATE;
        uint8_t value[1];
        value[0] = (uint8_t)media_player->data.media_state;

        att_server_notify(media_player->con_handle, 
            media_player->characteristics[MEDIA_STATE].value_handle, 
            (uint8_t *)value, sizeof(value));

    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED;
        uint8_t value[2];
        little_endian_store_16(value, 0, media_player->data.media_control_point_opcodes_supported);
        
        att_server_notify(media_player->con_handle, 
            media_player->characteristics[MEDIA_CONTROL_POINT_OPCODES_SUPPORTED].value_handle, 
            (uint8_t *)value, sizeof(value));
    
    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_MEDIA_CONTROL_POINT) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_MEDIA_CONTROL_POINT;
        uint8_t value[2];
        value[0] = media_player->data.media_control_point_requested_opcode;
        value[1] = media_player->data.media_control_point_result_code;

        att_server_notify(media_player->con_handle, 
            media_player->characteristics[MEDIA_CONTROL_POINT].value_handle, 
            (uint8_t *)value, sizeof(value));

    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_SEARCH_CONTROL_POINT) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_SEARCH_CONTROL_POINT;
        uint8_t value[1];
        value[0] = media_player->data.search_control_point_result_code;

        att_server_notify(media_player->con_handle, 
            media_player->characteristics[SEARCH_CONTROL_POINT].value_handle, 
            (uint8_t *)value, sizeof(value));

    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_MEDIA_PLAYER_ICON_OBJECT_ID) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_MEDIA_PLAYER_ICON_OBJECT_ID;
        uint8_t value[6];
        reverse_48(media_player->data.icon_object_id, value);

        att_server_notify(media_player->con_handle,
                          media_player->characteristics[MEDIA_PLAYER_ICON_OBJECT_ID].value_handle,
                          (uint8_t *)value, sizeof(value));

    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_CURRENT_TRACK_SEGMENTS_OBJECT_ID) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_CURRENT_TRACK_SEGMENTS_OBJECT_ID;
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_CURRENT_TRACK_SEGMENTS_OBJECT_ID;
        uint8_t value[6];
        reverse_48(media_player->data.current_track_segments_object_id, value);

        att_server_notify(media_player->con_handle,
                          media_player->characteristics[CURRENT_TRACK_SEGMENTS_OBJECT_ID].value_handle,
                          (uint8_t *)value, sizeof(value));

    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_CONTENT_CONTROL_ID) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_CONTENT_CONTROL_ID;
        // TODO
    } else if ((media_player->scheduled_tasks & MCS_NOTIFICATION_TASK_MEDIA_PLAYER_ICON_URL) != 0){
        media_player->scheduled_tasks &= ~MCS_NOTIFICATION_TASK_MEDIA_PLAYER_ICON_URL;
        // TODO
    }

    if (media_player->scheduled_tasks != 0){
        att_server_register_can_send_now_callback(&media_player->scheduled_tasks_callback, media_player->con_handle);
    }
}

static void mcs_server_schedule_task(media_control_service_server_t * media_player, msc_characteristic_id_t characteristic_id){
    if (media_player->characteristics[characteristic_id].client_configuration == 0){
        return;
    }

    if (media_player->con_handle == HCI_CON_HANDLE_INVALID){
        return;
    }
    
    uint32_t task_bit_mask = 1 << ((uint8_t)characteristic_id);

    // skip if already scheduled
    if ((media_player->scheduled_tasks & task_bit_mask) != 0){
        return;
    }

    uint32_t scheduled_tasks = media_player->scheduled_tasks;
    media_player->scheduled_tasks |= task_bit_mask;

    if (scheduled_tasks == 0){
        media_player->scheduled_tasks_callback.callback = &mcs_server_can_send_now;
        media_player->scheduled_tasks_callback.context  = (void*) media_player;
        att_server_register_can_send_now_callback(&media_player->scheduled_tasks_callback, media_player->con_handle);
    }
}

static uint16_t mcs_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	UNUSED(con_handle);

    media_control_service_server_t * media_player = msc_server_find_media_player_for_attribute_handle(attribute_handle);
	if (media_player == NULL) {
		return 0;
	}

	handle_type_t type = -1;
	msc_characteristic_id_t characteristic_id = msc_server_find_index_for_attribute_handle(media_player, attribute_handle, &type);

	switch (type){
		case HANDLE_TYPE_CHARACTERISTIC_CCD:
			return att_read_callback_handle_little_endian_16(media_player->characteristics[characteristic_id].client_configuration, offset, buffer, buffer_size);
		default:
			break;
	}
    uint8_t value[6];
	switch (characteristic_id){
		case MEDIA_PLAYER_NAME:
            if (buffer == NULL){
                // get len and check if we have up to date value
                if ((offset != 0) && media_player->data.name_value_changed){
                    return (uint16_t)ATT_READ_ERROR_CODE_OFFSET + (uint16_t)MCS_SEARCH_CONTROL_POINT_ATT_ERROR_RESPONSE_VALUE_CHANGED_DURING_READ_LONG;
                }
            } else {
                // actual read (after everything was validated)
                if (offset == 0){
                    media_player->data.name_value_changed = false;
                }
            }
            return att_read_callback_handle_blob((const uint8_t *)media_player->data.name, strlen(media_player->data.name), offset, buffer, buffer_size);
		
        case TRACK_TITLE:
             if (buffer == NULL){
                // get len and check if we have up to date value
                if ((offset != 0) && media_player->data.track_title_value_changed){
                    return (uint16_t)ATT_READ_ERROR_CODE_OFFSET + (uint16_t)MCS_SEARCH_CONTROL_POINT_ATT_ERROR_RESPONSE_VALUE_CHANGED_DURING_READ_LONG;
                }
            } else {
                // actual read (after everything was validated)
                if (offset == 0){
                    media_player->data.track_title_value_changed = false;
                }
            }
            return att_read_callback_handle_blob((const uint8_t *)media_player->data.track_title, strlen(media_player->data.track_title), offset, buffer, buffer_size);
        
        case MEDIA_PLAYER_ICON_OBJECT_ID:
            reverse_48(media_player->data.icon_object_id, value);
            return att_read_callback_handle_blob(value, OTS_OBJECT_ID_LEN, offset, buffer, buffer_size);

		case MEDIA_PLAYER_ICON_URL:
			return att_read_callback_handle_blob((const uint8_t *)media_player->data.icon_url, strlen(media_player->data.icon_url), offset, buffer, buffer_size);

        case TRACK_DURATION:
            return att_read_callback_handle_little_endian_32(media_player->data.track_duration_10ms, offset, buffer, buffer_size);
    
        case TRACK_POSITION:
            return att_read_callback_handle_little_endian_32(media_player->data.track_position_10ms, offset, buffer, buffer_size);

        case PLAYBACK_SPEED:
            return att_read_callback_handle_byte(media_player->data.playback_speed, offset, buffer, buffer_size);

        case SEEKING_SPEED:
            return att_read_callback_handle_byte(media_player->data.seeking_speed, offset, buffer, buffer_size);

        case CURRENT_TRACK_SEGMENTS_OBJECT_ID:
            reverse_48(media_player->data.current_track_segments_object_id, value);
            return att_read_callback_handle_blob((const uint8_t *)value, sizeof(value), offset, buffer, buffer_size);

        case CURRENT_TRACK_OBJECT_ID:
            reverse_48(media_player->data.current_track_object_id, value);
            return att_read_callback_handle_blob((const uint8_t *)value, sizeof(value), offset, buffer, buffer_size);

        case NEXT_TRACK_OBJECT_ID:
            reverse_48(media_player->data.next_track_object_id, value);
            return att_read_callback_handle_blob((const uint8_t *)value, sizeof(value), offset, buffer, buffer_size);

        case PARENT_GROUP_OBJECT_ID:
            reverse_48(media_player->data.parent_group_object_id, value);
            return att_read_callback_handle_blob((const uint8_t *)value, sizeof(value), offset, buffer, buffer_size);

        case CURRENT_GROUP_OBJECT_ID:
            reverse_48(media_player->data.current_group_object_id, value);
            return att_read_callback_handle_blob((const uint8_t *)value, sizeof(value), offset, buffer, buffer_size);

        case PLAYING_ORDER:
            return att_read_callback_handle_byte(media_player->data.playing_order, offset, buffer, buffer_size);

        case PLAYING_ORDERS_SUPPORTED:
            return att_read_callback_handle_little_endian_16(media_player->data.playing_orders_supported, offset, buffer, buffer_size);

        case MEDIA_STATE:
            return att_read_callback_handle_byte((uint8_t)media_player->data.media_state, offset, buffer, buffer_size);

        case MEDIA_CONTROL_POINT_OPCODES_SUPPORTED:
            return att_read_callback_handle_little_endian_32(media_player->data.media_control_point_opcodes_supported, offset, buffer, buffer_size);

        case SEARCH_RESULTS_OBJECT_ID:
            return att_read_callback_handle_blob((const uint8_t *)media_player->data.search_results_object_id, media_player->data.search_results_object_id_len, offset, buffer, buffer_size);

        case CONTENT_CONTROL_ID:
            return att_read_callback_handle_byte(media_player->data.content_control_id, offset, buffer, buffer_size);

        default:
			break;
	}
	return 0;
}

static bool mcs_search_control_buffer_is_valid(uint8_t * search_data, uint16_t search_data_len){
    if (search_data_len > MCS_SEARCH_CONTROL_POINT_COMMAND_MAX_LENGTH){
        return false;
    }
    if (search_data_len < 2){
        return false;
    }

    uint8_t pos = 0;
    while (pos < search_data_len){
        if ( (pos + 2) >= search_data_len ){
            return false;        
        }
        uint8_t search_field_length = search_data[pos];
        search_control_point_type_t search_field_type = (search_control_point_type_t)search_data[pos+1];
        
        if ((search_field_type < SEARCH_CONTROL_POINT_TYPE_FIRST_FIELD) ||
            (search_field_type >= SEARCH_CONTROL_POINT_TYPE_RFU)){
            return false;
        }
        pos += search_field_length + 1;
    }
    return (pos == search_data_len);
}

static int mcs_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	UNUSED(transaction_mode);
	UNUSED(offset);
	UNUSED(buffer_size);
	
	media_control_service_server_t * media_player = msc_server_find_media_player_for_attribute_handle(attribute_handle);
	if (media_player == NULL) {
		return 0;
	}
	
	handle_type_t type = -1;
	msc_characteristic_id_t characteristic_id = msc_server_find_index_for_attribute_handle(media_player, attribute_handle, &type);
    media_control_point_opcode_t      media_control_point_opcode;
    media_control_point_error_code_t  media_control_point_result_code;
    uint8_t media_control_poind_data_length;

	switch (type){
		case HANDLE_TYPE_CHARACTERISTIC_CCD:
            mcs_server_set_con_handle(media_player, (uint16_t)characteristic_id, con_handle, little_endian_read_16(buffer, 0));
			return 0;
		default:
			break;
	}

    playing_order_t playing_order;
    int32_t value;

    switch (characteristic_id){
		case MEDIA_PLAYER_NAME: 
			btstack_strcpy(media_player->data.name, sizeof(media_player->data.name), (const char *)buffer);
			mcs_server_emit_media_value_changed(media_player, characteristic_id);
			break;

        case TRACK_POSITION:
            if (buffer_size != 4){
                break;
            }
            value = (int32_t) little_endian_read_32(buffer, 0);
            if (value < 0){
                value = -value;
                if (value <= (int32_t)media_player->data.track_duration_10ms) {
                    media_player->data.track_position_10ms = media_player->data.track_duration_10ms - value;
                    mcs_server_emit_media_value_changed(media_player, characteristic_id);
                    mcs_server_schedule_task(media_player, characteristic_id);
                }
            } else {
                if (value <= (int32_t)media_player->data.track_duration_10ms){
                    media_player->data.track_position_10ms = (int32_t) little_endian_read_32(buffer, 0);
                    mcs_server_emit_media_value_changed(media_player, characteristic_id);
                    mcs_server_schedule_task(media_player, characteristic_id);
                }
            }

            break;
        
        case PLAYBACK_SPEED:
            if (buffer_size != 1){
                break;
            }
            media_player->data.playback_speed = (int8_t)buffer[0]; 
            mcs_server_emit_media_value_changed(media_player, characteristic_id);
            break;

        case CURRENT_TRACK_OBJECT_ID:
            if (buffer_size != 6){
                break;
            }
            reverse_48(buffer, media_player->data.current_track_object_id);
            mcs_server_emit_media_value_changed(media_player, characteristic_id);
            break;
        
        case NEXT_TRACK_OBJECT_ID:
            if (buffer_size != 6){
                break;
            }
            reverse_48(buffer, media_player->data.next_track_object_id);
            mcs_server_emit_media_value_changed(media_player, characteristic_id);
            break;

        case CURRENT_GROUP_OBJECT_ID:
            if (buffer_size != 6){
                break;
            }
            reverse_48(buffer, media_player->data.current_group_object_id);
            mcs_server_emit_media_value_changed(media_player, characteristic_id);
            break;

        case PLAYING_ORDER:
            if (buffer_size != 1){
                break;
            }
            playing_order = (playing_order_t)buffer[0]; 

            if (playing_order == PLAYING_ORDER_START_GROUP || playing_order >= PLAYING_ORDER_RFU){
                break;
            }
            media_player->data.playing_order = playing_order;
            mcs_server_emit_media_value_changed(media_player, characteristic_id);
            break;

        case SEARCH_CONTROL_POINT:
            if (mcs_search_control_buffer_is_valid(buffer, buffer_size)){
                media_player->data.search_control_point_result_code = SEARCH_CONTROL_POINT_ERROR_CODE_SUCCESS;
                mcs_server_schedule_task(media_player, characteristic_id);
                mcs_server_emit_search_control_notification_task(media_player, buffer, buffer_size);
            } else {
                media_player->data.search_control_point_result_code = SEARCH_CONTROL_POINT_ERROR_CODE_FAILURE;
                media_player->data.search_results_object_id_len = 0;
                memset(media_player->data.search_results_object_id, 0, sizeof(media_player->data.search_results_object_id));
                mcs_server_schedule_task(media_player, characteristic_id);
            }
            return 0;
        
        case MEDIA_CONTROL_POINT:
            if (buffer_size == 0){
                return 0;
            }
            media_control_point_opcode      = (media_control_point_opcode_t) buffer[0];
            media_control_point_result_code = MEDIA_CONTROL_POINT_ERROR_CODE_SUCCESS;

            media_control_poind_data_length = 0;

            if ( (media_control_point_opcode < MEDIA_CONTROL_POINT_OPCODE_START_GROUP) || 
                (media_control_point_opcode >= MEDIA_CONTROL_POINT_OPCODE_RFU)  ||
                mcs_media_control_point_opcode_supported(media_player, media_control_point_opcode) == false){

                    media_control_point_result_code = MEDIA_CONTROL_POINT_ERROR_CODE_OPCODE_NOT_SUPPORTED;

            } else if (media_player->data.media_state >= MCS_MEDIA_STATE_RFU){
                media_control_point_result_code = MEDIA_CONTROL_POINT_ERROR_CODE_COMMAND_CANNOT_BE_COMPLETED;
            } else {
                switch (media_control_point_opcode){
                    case MEDIA_CONTROL_POINT_OPCODE_MOVE_RELATIVE:
                    case MEDIA_CONTROL_POINT_OPCODE_GOTO_SEGMENT:
                    case MEDIA_CONTROL_POINT_OPCODE_GOTO_TRACK:
                    case MEDIA_CONTROL_POINT_OPCODE_GOTO_GROUP:
                        if ((buffer == NULL) || (buffer_size != 5)){
                            media_control_point_result_code = MEDIA_CONTROL_POINT_ERROR_CODE_COMMAND_CANNOT_BE_COMPLETED;
                        }
                        media_control_poind_data_length = 4;

                        break;
                    default:
                        if (buffer_size != 1){
                            media_control_point_result_code = MEDIA_CONTROL_POINT_ERROR_CODE_COMMAND_CANNOT_BE_COMPLETED;
                        }
                        break;
                }
            }
            
            if (media_control_point_result_code != MEDIA_CONTROL_POINT_ERROR_CODE_SUCCESS){
                media_player->data.media_control_point_requested_opcode = media_control_point_opcode;
                media_player->data.media_control_point_result_code      = media_control_point_result_code;
                mcs_server_schedule_task(media_player, characteristic_id);
            } else {
                mcs_server_emit_media_control_notification_task(media_player, media_control_point_opcode, &buffer[1], media_control_poind_data_length);
            }
            break;
		default:
			break;
	}
	return 0;
}

void media_control_service_server_init(void){
    mcs_services_start_handle = 0;
    mcs_media_player_counter = 0;
	// get service handle range
	mcs_media_players = NULL;
}

static uint8_t mcs_server_register_player(uint16_t service_uuid, media_control_service_server_t * media_player,
    btstack_packet_handler_t packet_handler, uint32_t media_control_point_opcodes_supported, uint16_t * media_player_id){
    // search service with global start handle
    btstack_assert(media_player != NULL);
    btstack_assert(packet_handler != NULL);
    

    const uint16_t msc_characteristic_uuids[] = {
        ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_NAME                    ,
        ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_OBJECT_ID          ,
        ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_URL                ,
        ORG_BLUETOOTH_CHARACTERISTIC_TRACK_CHANGED                        ,
        ORG_BLUETOOTH_CHARACTERISTIC_TRACK_TITLE                          ,
        ORG_BLUETOOTH_CHARACTERISTIC_TRACK_DURATION                       ,
        ORG_BLUETOOTH_CHARACTERISTIC_TRACK_POSITION                       ,
        ORG_BLUETOOTH_CHARACTERISTIC_PLAYBACK_SPEED                       ,
        ORG_BLUETOOTH_CHARACTERISTIC_SEEKING_SPEED                        ,
        ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_SEGMENTS_OBJECT_ID     ,
        ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_OBJECT_ID              ,
        ORG_BLUETOOTH_CHARACTERISTIC_NEXT_TRACK_OBJECT_ID                 ,
        ORG_BLUETOOTH_CHARACTERISTIC_PARENT_GROUP_OBJECT_ID               ,
        ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_GROUP_OBJECT_ID              ,
        ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDER                        ,
        ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDERS_SUPPORTED             ,
        ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_STATE                          ,
        ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT                  ,
        ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED,
        ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_RESULTS_OBJECT_ID             ,
        ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_CONTROL_POINT                 ,
        ORG_BLUETOOTH_CHARACTERISTIC_CONTENT_CONTROL_ID                   
    };

#ifdef ENABLE_TESTING_SUPPORT
    const char * msc_characteristic_uuid_names[]= {
        "media_player_name                    ",
        "media_player_icon_object_id          ",
        "media_player_icon_url                ",
        "track_changed                        ",
        "track_title                          ",
        "track_duration                       ",
        "track_position                       ",
        "playback_speed                       ",
        "seeking_speed                        ",
        "current_track_segments_object_id     ",
        "current_track_object_id              ",
        "next_track_object_id                 ",
        "parent_group_object_id               ",
        "current_group_object_id              ",
        "playing_order                        ",
        "playing_orders_supported             ",
        "media_state                          ",
        "media_control_point                  ",
        "media_control_point_opcodes_supported",
        "search_results_object_id             ",
        "search_control_point                 ",
        "content_control_id                   "
    };
#endif
    
    if (mcs_services_start_handle == 0xffff) {
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }
    
    uint16_t mcs_services_end_handle   = 0xffff;
    bool     service_found = gatt_server_get_handle_range_for_service_with_uuid16(service_uuid, &mcs_services_start_handle, &mcs_services_end_handle);
    
    if (!service_found){
        mcs_services_start_handle = 0xffff;
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    media_control_service_server_t * media_player_registered = msc_server_media_player_registered(mcs_services_start_handle);
    if (media_player_registered != NULL){
        return ERROR_CODE_REPEATED_ATTEMPTS;
    }

    log_info("Found %s service 0x%02x-0x%02x", service_uuid == ORG_BLUETOOTH_SERVICE_MEDIA_CONTROL_SERVICE ? "MCS":"GMCS", mcs_services_start_handle, mcs_services_end_handle);

#ifdef ENABLE_TESTING_SUPPORT
    printf("Found %s service 0x%02x-0x%02x\n", service_uuid == ORG_BLUETOOTH_SERVICE_MEDIA_CONTROL_SERVICE ? "MCS":"GMCS", mcs_services_start_handle, mcs_services_end_handle);
#endif
    mcs_server_reset_media_player(media_player);
    
    media_player->service.start_handle   = mcs_services_start_handle;
    media_player->service.end_handle     = mcs_services_end_handle;
    media_player->data.media_control_point_opcodes_supported = media_control_point_opcodes_supported;
    media_player->data.name = "";
    // get characteristic value handles
    uint16_t i;
    for (i = 0; i < NUM_MCS_CHARACTERISTICS; i++){
        media_player->characteristics[i].value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(mcs_services_start_handle, mcs_services_end_handle, msc_characteristic_uuids[i]);
        media_player->characteristics[i].client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(mcs_services_start_handle, mcs_services_end_handle, msc_characteristic_uuids[i]);

        if (media_player->characteristics[i].client_configuration_handle != 0){
            mcs_services_start_handle = media_player->characteristics[i].client_configuration_handle + 1;
        } else {
            mcs_services_start_handle = media_player->characteristics[i].value_handle + 1;
        }

#ifdef ENABLE_TESTING_SUPPORT
        printf("    %s      0x%02x\n", msc_characteristic_uuid_names[i], media_player->characteristics[i].value_handle);
        if (media_player->characteristics[i].client_configuration_handle != 0){
            printf("    %s CCC  0x%02x\n", msc_characteristic_uuid_names[i], media_player->characteristics[i].client_configuration_handle);
        }
#endif
    }
    
    uint16_t player_id = msc_server_get_next_media_player_id();
    if (media_player_id != NULL) {
        *media_player_id = player_id;
    }

    media_player->event_callback = packet_handler;

    // register service with ATT Server
    media_player->player_id = player_id;
    media_player->service.read_callback  = &mcs_server_read_callback;
    media_player->service.write_callback = &mcs_server_write_callback;
    media_player->service.packet_handler = mcs_server_packet_handler;
    att_server_register_service_handler(&media_player->service);
    
    // add to media player list
    btstack_linked_list_add(&mcs_media_players, (btstack_linked_item_t *)media_player);

    // prepare for the next service
    mcs_services_start_handle = media_player->service.end_handle;
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_register_generic_player(
    media_control_service_server_t * generic_player,
    btstack_packet_handler_t packet_handler, 
    uint32_t media_control_point_opcodes_supported, 
    uint16_t * out_generic_player_id){
    
    return mcs_server_register_player(ORG_BLUETOOTH_SERVICE_GENERIC_MEDIA_CONTROL_SERVICE,
                                      generic_player, packet_handler, media_control_point_opcodes_supported, out_generic_player_id);
}

uint8_t media_control_service_server_register_player(
    media_control_service_server_t * player,
    btstack_packet_handler_t packet_handler, 
    uint32_t media_control_point_opcodes_supported, 
    uint16_t * out_player_id){

    return mcs_server_register_player(ORG_BLUETOOTH_SERVICE_MEDIA_CONTROL_SERVICE,
                                      player, packet_handler, media_control_point_opcodes_supported, out_player_id);
}

uint8_t media_control_service_server_set_media_player_name(uint16_t media_player_id, char * name){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}

    media_player->data.name_value_changed = true;
	media_player->data.name = name;
	mcs_server_schedule_task(media_player, MEDIA_PLAYER_NAME);
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_icon_object_id(uint16_t media_player_id, const ots_object_id_t * object_id){
	btstack_assert(object_id != NULL);
    
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}
	memcpy(media_player->data.icon_object_id, object_id, OTS_OBJECT_ID_LEN);
    mcs_server_schedule_task(media_player, MEDIA_PLAYER_ICON_OBJECT_ID);
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_parent_group_object_id(uint16_t media_player_id, const ots_object_id_t * object_id){
    btstack_assert(object_id != NULL);

    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    memcpy(media_player->data.parent_group_object_id, object_id, OTS_OBJECT_ID_LEN);
    mcs_server_schedule_task(media_player, PARENT_GROUP_OBJECT_ID);
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_current_group_object_id(uint16_t media_player_id, const ots_object_id_t * object_id){
    btstack_assert(object_id != NULL);
    
    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    memcpy(media_player->data.current_group_object_id, object_id, OTS_OBJECT_ID_LEN);
    mcs_server_schedule_task(media_player, CURRENT_GROUP_OBJECT_ID);
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_current_track_id(uint16_t media_player_id, const ots_object_id_t * object_id){
    btstack_assert(object_id != NULL);

    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    memcpy(media_player->data.current_track_object_id, object_id, OTS_OBJECT_ID_LEN);
    mcs_server_schedule_task(media_player, CURRENT_TRACK_OBJECT_ID);
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_next_track_id(uint16_t media_player_id, const ots_object_id_t * object_id){
    btstack_assert(object_id != NULL);

    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    memcpy(media_player->data.next_track_object_id, object_id, OTS_OBJECT_ID_LEN);
    mcs_server_schedule_task(media_player, NEXT_TRACK_OBJECT_ID);
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_current_track_segments_id(uint16_t media_player_id, const ots_object_id_t * object_id){
    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (object_id != NULL){
        memcpy(media_player->data.current_track_segments_object_id, object_id, OTS_OBJECT_ID_LEN);
    } else {
        ots_object_id_t null_object_id = {0,0,0,0,0,0};
        memcpy(media_player->data.current_track_segments_object_id, &null_object_id, OTS_OBJECT_ID_LEN);
    }
    mcs_server_schedule_task(media_player, CURRENT_TRACK_SEGMENTS_OBJECT_ID);
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_icon_url(uint16_t media_player_id, const char * icon_url){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}
	btstack_strcpy(media_player->data.icon_url, sizeof(media_player->data.icon_url), icon_url);
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_media_track_changed(uint16_t media_player_id){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}
	
	mcs_server_schedule_task(media_player, TRACK_CHANGED);
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_track_title(uint16_t media_player_id, const char * track_title){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}

    media_player->data.track_title_value_changed = true;
	btstack_strcpy(media_player->data.track_title, sizeof(media_player->data.track_title), track_title);
	
	mcs_server_schedule_task(media_player, TRACK_TITLE);
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_track_duration(uint16_t media_player_id, uint32_t track_duration_10ms){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}

	media_player->data.track_duration_10ms = track_duration_10ms;
	
	mcs_server_schedule_task(media_player, TRACK_DURATION);
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_track_position(uint16_t media_player_id, int32_t track_position_10ms){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}
	media_player->data.track_position_10ms = track_position_10ms;
	mcs_server_schedule_task(media_player, TRACK_POSITION);
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_playback_speed(uint16_t media_player_id, int8_t playback_speed){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}
	if (media_player->data.playback_speed == playback_speed){
		return ERROR_CODE_SUCCESS;
	}
	media_player->data.playback_speed = playback_speed;
	
	mcs_server_schedule_task(media_player, PLAYBACK_SPEED);
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_seeking_speed(uint16_t media_player_id, int8_t seeking_speed){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}
	media_player->data.seeking_speed = seeking_speed;
	mcs_server_schedule_task(media_player, SEEKING_SPEED);
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_playing_orders_supported(uint16_t media_player_id, uint16_t playing_orders_supported){
    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if ((playing_orders_supported & 0xFC00) > 0){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    media_player->data.playing_orders_supported = playing_orders_supported;
    mcs_server_schedule_task(media_player, PLAYING_ORDERS_SUPPORTED);
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_support_playing_order(uint16_t media_player_id, playing_order_t playing_order){
    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if ((playing_order == PLAYING_ORDER_START_GROUP) || (playing_order >= PLAYING_ORDER_RFU)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    media_player->data.playing_orders_supported |= 1 << ((uint8_t)playing_order - 1);

    mcs_server_schedule_task(media_player, PLAYING_ORDERS_SUPPORTED);
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_playing_order(uint16_t media_player_id, playing_order_t playing_order){
    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if ((playing_order == PLAYING_ORDER_START_GROUP) || (playing_order >= PLAYING_ORDER_RFU)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    if ((media_player->data.playing_orders_supported & (1 << ((uint8_t)playing_order - 1))) == 0){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    media_player->data.playing_order = playing_order;

    mcs_server_schedule_task(media_player, PLAYING_ORDERS_SUPPORTED);
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_media_state(uint16_t media_player_id, mcs_media_state_t media_state){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}

	if (media_player->data.media_state == media_state){
		return ERROR_CODE_SUCCESS;
	}
	    
    media_player->data.media_state = media_state;
	mcs_server_schedule_task(media_player, MEDIA_STATE);
	return ERROR_CODE_SUCCESS;
}

mcs_media_state_t media_control_service_server_get_media_state(uint16_t media_player_id){
    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    btstack_assert(media_player != NULL);
    return media_player->data.media_state;
}

uint8_t media_control_service_server_respond_to_media_control_point_command(
    uint16_t media_player_id, 
    media_control_point_opcode_t      media_control_point_opcode,
    media_control_point_error_code_t  media_control_point_result_code){

    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    media_player->data.media_control_point_requested_opcode = media_control_point_opcode;
    media_player->data.media_control_point_result_code      = media_control_point_result_code;
    mcs_server_schedule_task(media_player, MEDIA_CONTROL_POINT);

    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_search_control_point_response(
    uint16_t media_player_id, 
    uint8_t * search_results_object_id){

    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (search_results_object_id != NULL){
        media_player->data.search_results_object_id_len = 6;
        memcpy(media_player->data.search_results_object_id, search_results_object_id, 6);
    } else {
        media_player->data.search_results_object_id_len = 0;
        memset(media_player->data.search_results_object_id, 0, sizeof(media_player->data.search_results_object_id));
    }
    mcs_server_schedule_task(media_player, SEARCH_RESULTS_OBJECT_ID);
    return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_unregister_media_player(media_control_service_server_t * media_player){
	if (media_player == NULL){
		return ERROR_CODE_SUCCESS;
	}
	return btstack_linked_list_remove(&mcs_media_players, (btstack_linked_item_t *)media_player);
}

uint8_t media_control_service_server_update_current_track_info(uint16_t media_player_id, mcs_track_t * track){
    btstack_assert(track != NULL);
    
    media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
    if (media_player == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    media_player->data.track_position_10ms = track->track_position_10ms;
    media_player->data.track_duration_10ms = track->track_duration_10ms;
    btstack_strcpy(media_player->data.track_title, sizeof(media_player->data.track_title), track->title);
    return ERROR_CODE_SUCCESS;
}
