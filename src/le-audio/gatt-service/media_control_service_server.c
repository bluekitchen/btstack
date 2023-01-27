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

#include "le-audio/gatt-service/media_control_service_server.h"
#include "le-audio/gatt-service/audio_input_control_service_server.h"

typedef enum {
	HANDLE_TYPE_CHARACTERISTIC_VALUE = 0,
	HANDLE_TYPE_CHARACTERISTIC_CCD,
} handle_type_t;

static btstack_linked_list_t    mcs_media_players;
static btstack_packet_handler_t mcs_server_callback;
static uint16_t mcs_media_player_counter = 0;

static uint16_t msc_server_get_next_media_player_id(void){
	mcs_media_player_counter = btstack_next_cid_ignoring_zero(mcs_media_player_counter);
	return mcs_media_player_counter;
}

static media_control_service_server_t * msc_server_media_player_registered(uint16_t start_handle){
	if (start_handle == 0xffff) {
		return NULL;
	}

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
}



static void mcs_server_set_con_handle(media_control_service_server_t * media_player, uint16_t characteristic_index, hci_con_handle_t con_handle, uint16_t configuration){
    media_player->characteristics[characteristic_index].client_configuration = configuration;
    media_player->con_handle = (configuration == 0) ? HCI_CON_HANDLE_INVALID : con_handle;
}

static void mcs_server_reset_values(media_control_service_server_t * media_player){
	if (media_player == NULL){
		return;
	}
    media_player->con_handle = HCI_CON_HANDLE_INVALID;
    
    uint8_t i;
	for (i = 0; i < NUM_MCS_CHARACTERISTICS; i++){
		media_player->characteristics[i].client_configuration = 0;
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
            mcs_server_reset_values(media_player);
            break;
        default:
            break;
    }
}

static uint16_t mcs_server_max_att_value_len(hci_con_handle_t con_handle){
	uint16_t att_mtu = att_server_get_mtu(con_handle);
    return (att_mtu >= 3) ? att_mtu - 3 : 0;
}

static uint16_t mcs_server_max_value_len(hci_con_handle_t con_handle, uint16_t value_len){
	return btstack_min(value_len, mcs_server_max_att_value_len(con_handle));
}

static void mcs_server_can_send_now(void * context){
    media_control_service_server_t * media_player = (media_control_service_server_t *) context;
    
    if (media_player->con_handle == HCI_CON_HANDLE_INVALID){
        media_player->scheduled_tasks = 0;
        return;
    }

    uint16_t value_handle;

	if ((media_player->scheduled_tasks & MEDIA_PLAYER_NAME) != 0){
        media_player->scheduled_tasks &= ~MEDIA_PLAYER_NAME;
        value_handle = media_player->characteristics[MEDIA_PLAYER_NAME].value_handle;

        uint16_t value_len = mcs_server_max_value_len(media_player->con_handle, strlen(media_player->data.name));
        att_server_notify(media_player->con_handle, value_handle, (uint8_t *)media_player->data.name, value_len);
    
    } else if ((media_player->scheduled_tasks & TRACK_CHANGED) != 0){
        media_player->scheduled_tasks &= ~TRACK_CHANGED;
        value_handle = media_player->characteristics[MEDIA_PLAYER_NAME].value_handle;

    	att_server_notify(media_player->con_handle, value_handle, NULL, 0);

    } else if ((media_player->scheduled_tasks & TRACK_TITLE) != 0){
        media_player->scheduled_tasks &= ~TRACK_TITLE;
    	value_handle = media_player->characteristics[TRACK_TITLE].value_handle;

    	uint16_t value_len = mcs_server_max_value_len(media_player->con_handle, strlen(media_player->data.track_title));
        att_server_notify(media_player->con_handle, value_handle, (uint8_t *)media_player->data.track_title, value_len);
    
    }

    if (media_player->scheduled_tasks != 0){
        media_player->scheduled_tasks_callback.callback = &mcs_server_can_send_now;
        media_player->scheduled_tasks_callback.context  = (void*) media_player;
        att_server_register_can_send_now_callback(&media_player->scheduled_tasks_callback, media_player->con_handle);
    }
}

static bool mcs_server_can_schedule_task(media_control_service_server_t * media_player, uint8_t task_id){
	switch (task_id){
    	case TRACK_DURATION:
    		if ((media_player->data.media_state == MCS_MEDIA_STATE_PLAYING) && (media_player->data.playback_speed == 1)){
    			// too avoid an excessive number of notifications, the Track Position should not be notified when the Media State is set to “Playing” and playback happens at a constant speed
				return false;
    		}
    		break;
		
		case MEDIA_STATE:
			switch (media_player->data.media_state){
				case MCS_MEDIA_STATE_PAUSED:
					if (media_player->data.track_duration_10ms != 0xFFFFFFFF){
						media_player->scheduled_tasks |= TRACK_DURATION;
					}
					break;
				case MCS_MEDIA_STATE_SEEKING:
					// TODO start timer for track_duration_10ms
					if (media_player->data.track_duration_10ms != 0xFFFFFFFF){
						media_player->scheduled_tasks |= TRACK_DURATION;
					}
					break;
				default:
					break;
			}
			
			break;
		
		case PLAYBACK_SPEED:
			if ((media_player->data.media_state != MCS_MEDIA_STATE_PLAYING) || (media_player->data.playback_speed != 1)){
				media_player->scheduled_tasks |= TRACK_DURATION;
			}
			break;
		default:
			break;
    }
    return true;
}

static void mcs_server_schedule_task(media_control_service_server_t * media_player, uint8_t task_id){
    // skip if already scheduled
    if ( (media_player->scheduled_tasks & task_id) != 0){
        return;
    }
    if (media_player->con_handle == HCI_CON_HANDLE_INVALID){
        return;
    }
    if (media_player->characteristics[task_id].client_configuration == 0){
        return;
    }

    uint16_t scheduled_tasks = media_player->scheduled_tasks;
    media_player->scheduled_tasks |= task_id;

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

	handle_type_t type;
	msc_characteristic_id_t characteristic_id = msc_server_find_index_for_attribute_handle(media_player, attribute_handle, &type);

	switch (type){
		case HANDLE_TYPE_CHARACTERISTIC_CCD:
			return att_read_callback_handle_little_endian_16(media_player->characteristics[characteristic_id].client_configuration, offset, buffer, buffer_size);
		default:
			break;
	}

	uint16_t value_size = 0;
	switch (characteristic_id){
		case MEDIA_PLAYER_NAME: 
			value_size = btstack_min(strlen(media_player->data.name), mcs_server_max_att_value_len(con_handle));
        	return att_read_callback_handle_blob((const uint8_t *)media_player->data.name, value_size, offset, buffer, buffer_size);
		
		case MEDIA_PLAYER_ICON_OBJECT_ID:
			return att_read_callback_handle_blob(media_player->data.icon_object_id, 6, offset, buffer, buffer_size);

		case MEDIA_PLAYER_ICON_URL:
			value_size = strlen(media_player->data.icon_url);
			return att_read_callback_handle_blob((const uint8_t *)media_player->data.icon_url, value_size, offset, buffer, buffer_size);

        default:
			break;
	}
	return value_size;
}

static int mcs_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	UNUSED(transaction_mode);
	UNUSED(offset);
	UNUSED(buffer_size);
	
	media_control_service_server_t * media_player = msc_server_find_media_player_for_attribute_handle(attribute_handle);
	if (media_player == NULL) {
		return 0;
	}
	
	handle_type_t type;
	msc_characteristic_id_t characteristic_id = msc_server_find_index_for_attribute_handle(media_player, attribute_handle, &type);

	switch (type){
		case HANDLE_TYPE_CHARACTERISTIC_CCD:
			mcs_server_set_con_handle(media_player, (uint16_t)characteristic_id, con_handle, little_endian_read_16(buffer, 0));
			return 0;
		default:
			break;
	}

	switch (characteristic_id){
		case MEDIA_PLAYER_NAME: 
			btstack_strcpy(media_player->data.name, sizeof(media_player->data.name), (const char *)buffer);
			
			if (mcs_server_can_schedule_task(media_player, characteristic_id)){
				mcs_server_schedule_task(media_player, characteristic_id);
			}
			break;
		default:
			break;
	}
	return 0;
}

void media_control_service_server_init(void){
	mcs_media_player_counter = 0;
	// get service handle range
	mcs_media_players = NULL;
}

void media_control_service_server_register_packet_handler(btstack_packet_handler_t packet_handler){
    btstack_assert(packet_handler != NULL);
    mcs_server_callback = packet_handler;
}

static void mcs_server_reset_media_player(media_control_service_server_t * media_player){
	media_player->con_handle = HCI_CON_HANDLE_INVALID;
	
	memset(&media_player->data, 0, sizeof(mcs_media_player_data_t));
	media_player->data.track_duration_10ms = 0xFFFFFFFF;
}

uint8_t media_control_service_server_register_media_player(media_control_service_server_t * media_player, uint16_t * media_player_id){
	// search service with global start handle
	btstack_assert(media_player != NULL);
	

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

	uint16_t start_handle = 0;
	uint16_t end_handle   = 0xffff;

	while (start_handle < 0xffff) {
        int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_MEDIA_CONTROL_SERVICE, &start_handle, &end_handle);
		
        if (!service_found){
            return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
        }

        media_control_service_server_t * media_player_registered = msc_server_media_player_registered(start_handle);
        if (media_player_registered == NULL){
			log_info("Found MCS service 0x%02x-0x%02x", start_handle, end_handle);

#ifdef ENABLE_TESTING_SUPPORT
        	printf("Found MCS service 0x%02x-0x%02x\n", start_handle, end_handle);
#endif
      		mcs_server_reset_media_player(media_player);
			
        	media_player->service.start_handle   = start_handle;
			media_player->service.end_handle     = end_handle;
        	
        	// get characteristic value handles
			uint16_t i;
			for (i = 0; i < NUM_MCS_CHARACTERISTICS; i++){
				media_player->characteristics[i].value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, msc_characteristic_uuids[i]);
				media_player->characteristics[i].client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, msc_characteristic_uuids[i]);

				if (media_player->characteristics[i].client_configuration_handle != 0){
					start_handle = media_player->characteristics[i].client_configuration_handle + 1;
				} else {
					start_handle = media_player->characteristics[i].value_handle + 1;
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

			// register service with ATT Server
			media_player->player_id = player_id;
			media_player->service.read_callback  = &mcs_server_read_callback;
			media_player->service.write_callback = &mcs_server_write_callback;
			media_player->service.packet_handler = mcs_server_packet_handler;
			att_server_register_service_handler(&media_player->service);
			// add to media player list
            start_handle = media_player->service.end_handle;
			return btstack_linked_list_add(&mcs_media_players, (btstack_linked_item_t *)media_player);
        }

        // prepare for next service
        start_handle = end_handle;
        end_handle = 0xffff;
    }
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_media_player_name(uint16_t media_player_id, char * name){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}
	btstack_strcpy(media_player->data.name, sizeof(media_player->data.name), name);
	
	if (mcs_server_can_schedule_task(media_player, MEDIA_PLAYER_NAME)){
		mcs_server_schedule_task(media_player, MEDIA_PLAYER_NAME);
	}
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_icon_object_id(uint16_t media_player_id, 
    const uint8_t * icon_object_id, uint8_t icon_object_id_len){
	
	if (icon_object_id_len != 6){
		return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
	}

	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}
    media_player->data.icon_object_id_len = icon_object_id_len;
	memcpy(media_player->data.icon_object_id, icon_object_id, media_player->data.icon_object_id_len);
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
	
	if (mcs_server_can_schedule_task(media_player, TRACK_CHANGED)){
		mcs_server_schedule_task(media_player, TRACK_CHANGED);
	}
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_track_title(uint16_t media_player_id, const char * track_title){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}
	btstack_strcpy(media_player->data.track_title, sizeof(media_player->data.track_title), track_title);
	
	if (mcs_server_can_schedule_task(media_player, TRACK_TITLE)){
		mcs_server_schedule_task(media_player, TRACK_TITLE);
	}
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_track_duration(uint16_t media_player_id, uint32_t track_duration_10ms){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}

	media_player->data.track_duration_10ms = track_duration_10ms;
	
	if (mcs_server_can_schedule_task(media_player, TRACK_DURATION)){
		mcs_server_schedule_task(media_player, TRACK_DURATION);
	}
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_track_position_offset(uint16_t media_player_id, int32_t track_position_offset_10ms){
	UNUSED(track_position_offset_10ms);
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}
	// media_player->data.track_position_10ms = track_position_10ms;
	// TODO
	if (mcs_server_can_schedule_task(media_player, TRACK_POSITION)){
		mcs_server_schedule_task(media_player, TRACK_POSITION);
	}
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
	
	if (mcs_server_can_schedule_task(media_player, PLAYBACK_SPEED)){
		mcs_server_schedule_task(media_player, PLAYBACK_SPEED);
	}
	return ERROR_CODE_SUCCESS;
}

uint8_t media_control_service_server_set_seeking_speed(uint16_t media_player_id, int8_t seeking_speed){
	media_control_service_server_t * media_player = msc_server_find_media_player_for_id(media_player_id);
	if (media_player == NULL){
		return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
	}
	media_player->data.seeking_speed = seeking_speed;
	
	if (mcs_server_can_schedule_task(media_player, SEEKING_SPEED)){
		mcs_server_schedule_task(media_player, SEEKING_SPEED);
	}
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
	
	if (mcs_server_can_schedule_task(media_player, MEDIA_STATE)){
		mcs_server_schedule_task(media_player, MEDIA_STATE);
	}
	return ERROR_CODE_SUCCESS;
}


uint8_t media_control_service_server_unregister_media_player(media_control_service_server_t * media_player){
	if (media_player == NULL){
		return ERROR_CODE_SUCCESS;
	}
	return btstack_linked_list_remove(&mcs_media_players, (btstack_linked_item_t *)media_player);
}
