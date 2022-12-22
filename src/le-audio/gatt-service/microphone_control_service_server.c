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

#define BTSTACK_FILE__ "microphone_control_service_server.c"

/**
 * Implementation of the GATT Battery Service Server 
 * To use with your application, add '#import <microphone_control.gatt' to your .gatt file
 */

#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"

#include "le-audio/gatt-service/microphone_control_service_server.h"
#include "le-audio/gatt-service/audio_input_control_service_server.h"

static btstack_context_callback_registration_t  mc_mute_callback;
static att_service_handler_t       microphone_control;

static gatt_microphone_control_mute_t mc_mute_state;
static uint16_t mc_mute_state_client_configuration;
static hci_con_handle_t mc_mute_state_client_configuration_connection;

static uint16_t mc_mute_state_handle;
static uint16_t mc_mute_state_handle_client_configuration;

static btstack_packet_handler_t mics_callback;

static audio_input_control_service_server_t aics_services[AICS_MAX_NUM_SERVICES];
static uint8_t aics_services_num;

static void microphone_control_service_server_emit_mute(gatt_microphone_control_mute_t mute_state){
    btstack_assert(mics_callback != NULL);
    uint8_t event[6];

    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_LOCAL_MICS_MUTE;
    little_endian_store_16(event, pos, mc_mute_state_client_configuration_connection);
    pos += 2;
    event[pos++] = (uint8_t)mute_state;
    (*mics_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}


static uint16_t microphone_control_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	UNUSED(con_handle);

	if (attribute_handle == mc_mute_state_handle){
		return att_read_callback_handle_byte((uint8_t)mc_mute_state, offset, buffer, buffer_size);
	}
	if (attribute_handle == mc_mute_state_handle_client_configuration){
		return att_read_callback_handle_little_endian_16(mc_mute_state_client_configuration, offset, buffer, buffer_size);
	}
	return 0;
}

static int microphone_control_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	UNUSED(transaction_mode);
	UNUSED(offset);
	UNUSED(buffer_size);


	if (attribute_handle == mc_mute_state_handle){
		if (buffer_size != 1){
			return ATT_ERROR_VALUE_NOT_ALLOWED;
		} 
		gatt_microphone_control_mute_t mute_state = (gatt_microphone_control_mute_t)buffer[0];
		switch (mute_state){
			case GATT_MICROPHONE_CONTROL_MUTE_OFF:
			case GATT_MICROPHONE_CONTROL_MUTE_ON:
				if (mc_mute_state == GATT_MICROPHONE_CONTROL_MUTE_DISABLED){
					return ATT_ERROR_RESPONSE_MICROPHONE_CONTROL_MUTE_DISABLED;
				}
				mc_mute_state_client_configuration_connection = con_handle;
				microphone_control_service_server_emit_mute(mute_state);
				microphone_control_service_server_set_mute(mute_state);
				break;
			default:
				return ATT_ERROR_VALUE_NOT_ALLOWED;
		}
	}

	if (attribute_handle == mc_mute_state_handle_client_configuration){
		if (buffer_size != 2){
			return ATT_ERROR_VALUE_NOT_ALLOWED;
		} 
		mc_mute_state_client_configuration = little_endian_read_16(buffer, 0);
		mc_mute_state_client_configuration_connection = con_handle;
		// TODO store client in a list 
	}
	return 0;
}

static void microphone_control_service_can_send_now(void * context){
	hci_con_handle_t con_handle = (hci_con_handle_t) (uintptr_t) context;
	uint8_t value = (uint8_t) mc_mute_state;
	att_server_notify(con_handle, mc_mute_state_handle, &value, 1);
}

void microphone_control_service_server_init(gatt_microphone_control_mute_t mute_state, uint8_t aics_info_num, aics_info_t * aics_info){
	btstack_assert(aics_info_num <= AICS_MAX_NUM_SERVICES);
	btstack_assert((aics_info_num == 0)|| (aics_info != NULL));

	mc_mute_state = mute_state;
	// get service handle range
	uint16_t start_handle = 0;
	uint16_t end_handle   = 0xffff;
	int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_MICROPHONE_CONTROL, &start_handle, &end_handle);
	btstack_assert(service_found != 0);
	UNUSED(service_found);

	// get characteristic value handle and client configuration handle
	mc_mute_state_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_MUTE);
	mc_mute_state_handle_client_configuration = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_MUTE);

	log_info("Found MICS service 0x%02x-0x%02x", start_handle, end_handle);

    uint16_t aics_start_handle = start_handle + 1;
	uint16_t aics_end_handle   = end_handle - 1;
    aics_services_num = 0;

    // include and enumerate AICS services
	while ((aics_start_handle < aics_end_handle) && (aics_services_num < aics_info_num)) {
		uint16_t included_service_handle;
    	uint16_t included_service_start_handle;
    	uint16_t included_service_end_handle;

		bool aics_service_found = gatt_server_get_included_service_with_uuid16(aics_start_handle, aics_end_handle, 
			ORG_BLUETOOTH_SERVICE_AUDIO_INPUT_CONTROL, &included_service_handle, &included_service_start_handle, &included_service_end_handle);

		if (!aics_service_found){
			break;
		}
		log_info("Include AICS service 0x%02x-0x%02x", included_service_start_handle, included_service_end_handle);

		audio_input_control_service_server_t * service = &aics_services[aics_services_num];
		service->start_handle = included_service_start_handle;
		service->end_handle = included_service_end_handle;
		service->index = aics_services_num;
		
		service->info = &aics_info[aics_services_num];
		service->audio_input_description_len = (uint8_t) strlen(aics_info->audio_input_description);

		audio_input_control_service_server_init(service);
    	
		aics_start_handle = included_service_handle + 1;
		aics_services_num++;
	}
	
	// register service with ATT Server
	microphone_control.start_handle   = start_handle;
	microphone_control.end_handle     = end_handle;
	microphone_control.read_callback  = &microphone_control_service_read_callback;
	microphone_control.write_callback = &microphone_control_service_write_callback;
	att_server_register_service_handler(&microphone_control);
}

void microphone_control_service_server_register_packet_handler(btstack_packet_handler_t callback){
	btstack_assert(callback != NULL);
	mics_callback = callback;
}

void microphone_control_service_server_set_mute(gatt_microphone_control_mute_t mute_state){
	if (mc_mute_state == mute_state){
		return;
	}
	mc_mute_state = mute_state;
	// TODO extend send to all clients that registered for notify,
    // Current: notification is sent to the last one that enabled notification
	
	if (mc_mute_state_client_configuration != 0){
		mc_mute_callback.callback = &microphone_control_service_can_send_now;
		mc_mute_callback.context  = (void*) (uintptr_t) mc_mute_state_client_configuration_connection;
		att_server_register_can_send_now_callback(&mc_mute_callback, mc_mute_state_client_configuration_connection);
	}
}

uint8_t microphone_control_service_server_set_audio_input_state_for_aics(uint8_t aics_index, aics_audio_input_state_t * audio_input_state){
    if (aics_index >= aics_services_num){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    return audio_input_control_service_server_set_audio_input_state(&aics_services[aics_index], audio_input_state);
}

uint8_t microphone_control_service_server_set_audio_input_description_for_aics(uint8_t aics_index, const char * audio_input_desc){
    if (aics_index >= aics_services_num){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    audio_input_control_service_server_set_audio_input_description(&aics_services[aics_index], audio_input_desc);
    return ERROR_CODE_SUCCESS;
}

uint8_t  microphone_control_service_server_set_audio_input_status_for_aics(uint8_t aics_index, aics_audio_input_status_t audio_input_status){
    if (aics_index >= aics_services_num){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    audio_input_control_service_server_set_audio_input_status(&aics_services[aics_index], audio_input_status);
    return ERROR_CODE_SUCCESS;
}