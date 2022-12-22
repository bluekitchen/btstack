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

#define BTSTACK_FILE__ "volume_control_service_server.c"

/**
 * Implementation of the GATT Battery Service Server 
 * To use with your application, add '#import <volume_control_service.gatt' to your .gatt file
 */

#include "btstack_defines.h"
#include "btstack_event.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"

#include "le-audio/gatt-service/volume_control_service_server.h"
#include "le-audio/gatt-service/audio_input_control_service_server.h"
#include "le-audio/gatt-service/volume_offset_control_service_server.h"

typedef enum {
    VCS_OPCODE_RELATIVE_VOLUME_DOWN = 0x00,
    VCS_OPCODE_RELATIVE_VOLUME_UP,
    VCS_OPCODE_UNMUTE_RELATIVE_VOLUME_DOWN,
    VCS_OPCODE_UNMUTE_RELATIVE_VOLUME_UP,
    VCS_OPCODE_SET_ABSOLUTE_VOLUME,
    VCS_OPCODE_UNMUTE,
    VCS_OPCODE_MUTE,
    VCS_OPCODE_RFU
} vcs_opcode_t;

#define VCS_TASK_SEND_VOLUME_SETTING                0x01
#define VCS_TASK_SEND_VOLUME_FLAGS                  0x02

static att_service_handler_t volume_control_service;

static btstack_context_callback_registration_t  vcs_callback;
static hci_con_handle_t  vcs_con_handle;
static uint8_t           vcs_tasks;

static uint8_t    vcs_volume_change_step_size;

// characteristic: VOLUME_STATE 
static uint16_t   vcs_volume_state_handle;

static uint16_t   vcs_volume_state_client_configuration_handle;
static uint16_t   vcs_volume_state_client_configuration;

static vcs_mute_t vcs_volume_state_mute;            // 0 - not_muted, 1 - muted
static uint8_t    vcs_volume_state_volume_setting;  // range [0,255]
static uint8_t    vcs_volume_state_change_counter;  // range [0,255], ounter is increased for every change on mute and volume setting

// characteristic: VOLUME_FLAGS
static uint16_t   vcs_volume_flags_handle;

static uint16_t   vcs_volume_flags_client_configuration_handle;
static uint16_t   vcs_volume_flags_client_configuration;

static uint8_t    vcs_volume_flags;

// characteristic: CONTROL_POINT
static uint16_t   vcs_control_point_value_handle;

static btstack_packet_handler_t vcs_event_callback;

static audio_input_control_service_server_t aics_services[AICS_MAX_NUM_SERVICES];
static uint8_t aics_services_num;

static volume_offset_control_service_server_t vocs_services[VOCS_MAX_NUM_SERVICES];
static uint8_t vocs_services_num;


static void volume_control_service_update_change_counter(void){
    vcs_volume_state_change_counter++;
}

static void volume_control_service_volume_up(void){
    if (vcs_volume_state_volume_setting < (255 - vcs_volume_change_step_size)){
        vcs_volume_state_volume_setting += vcs_volume_change_step_size;
    } else {
        vcs_volume_state_volume_setting = 255;
    }
} 

static void volume_control_service_volume_down(void){
    if (vcs_volume_state_volume_setting > vcs_volume_change_step_size){
        vcs_volume_state_volume_setting -= vcs_volume_change_step_size;
    } else {
        vcs_volume_state_volume_setting = 0;
    }
} 

static void volume_control_service_mute(void){
    vcs_volume_state_mute = VCS_MUTE_ON;
}

static void volume_control_service_unmute(void){
    vcs_volume_state_mute = VCS_MUTE_OFF;
}

static void vcs_emit_volume_state(void){
    btstack_assert(vcs_event_callback != NULL);
    
    uint8_t event[8];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_VCS_VOLUME_STATE;
    little_endian_store_16(event, pos, vcs_con_handle);
    pos += 2;
    event[pos++] = vcs_volume_state_volume_setting;
    event[pos++] = vcs_volume_change_step_size;
    event[pos++] = (uint8_t)vcs_volume_state_mute;
    (*vcs_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void vcs_emit_volume_flags(void){
    btstack_assert(vcs_event_callback != NULL);
    
    uint8_t event[6];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_VCS_VOLUME_FLAGS;
    little_endian_store_16(event, pos, vcs_con_handle);
    pos += 2;
    event[pos++] = vcs_volume_flags;
    (*vcs_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static uint16_t volume_control_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);

    if (attribute_handle == vcs_volume_state_handle){
        uint8_t value[3];
        value[0] = vcs_volume_state_volume_setting;
        value[1] = (uint8_t) vcs_volume_state_mute;
        value[2] = vcs_volume_state_change_counter;
        return att_read_callback_handle_blob(value, sizeof(value), offset, buffer, buffer_size);
    }

    if (attribute_handle == vcs_volume_flags_handle){
        return att_read_callback_handle_byte(vcs_volume_flags, offset, buffer, buffer_size);
    }

    if (attribute_handle == vcs_volume_state_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(vcs_volume_state_client_configuration, offset, buffer, buffer_size);
    }
    
    if (attribute_handle == vcs_volume_flags_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(vcs_volume_flags_client_configuration, offset, buffer, buffer_size);
    }

    return 0;
}

static void volume_control_service_can_send_now(void * context){
    UNUSED(context);

    // checks task
    if ((vcs_tasks & VCS_TASK_SEND_VOLUME_SETTING) != 0){
        vcs_tasks &= ~VCS_TASK_SEND_VOLUME_SETTING;
        
        uint8_t buffer[3];
        buffer[0] = vcs_volume_state_volume_setting;
        buffer[1] = (uint8_t)vcs_volume_state_mute;
        buffer[2] = vcs_volume_state_change_counter;
        att_server_notify(vcs_con_handle, vcs_volume_state_handle, &buffer[0], sizeof(buffer));

    } else if ((vcs_tasks & VCS_TASK_SEND_VOLUME_FLAGS) != 0){
        vcs_tasks &= ~VCS_TASK_SEND_VOLUME_FLAGS;
        att_server_notify(vcs_con_handle, vcs_volume_flags_handle, &vcs_volume_flags, 1);
    }

    if (vcs_tasks != 0){
        att_server_register_can_send_now_callback(&vcs_callback, vcs_con_handle);
    }
}

static void volume_control_service_server_set_callback(uint8_t task){
    if (vcs_con_handle == HCI_CON_HANDLE_INVALID){
        vcs_tasks &= ~task;
        return;
    }

    uint8_t scheduled_tasks = vcs_tasks;
    vcs_tasks |= task;
    if (scheduled_tasks == 0){
        vcs_callback.callback = &volume_control_service_can_send_now;
        att_server_register_can_send_now_callback(&vcs_callback, vcs_con_handle);
    }   
}

static void vcs_set_con_handle(hci_con_handle_t con_handle, uint16_t configuration){
    vcs_con_handle = (configuration == 0) ? HCI_CON_HANDLE_INVALID : con_handle;
}

static int volume_control_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);

    if (attribute_handle == vcs_control_point_value_handle){
        if (buffer_size < 2){
            return VOLUME_CONTROL_OPCODE_NOT_SUPPORTED;
        }

        vcs_opcode_t opcode = (vcs_opcode_t)buffer[0];
        uint8_t change_counter = buffer[1];

        if (change_counter != vcs_volume_state_change_counter){
            return VOLUME_CONTROL_INVALID_CHANGE_COUNTER;
        }

        uint8_t    volume_setting = vcs_volume_state_volume_setting;
        vcs_mute_t mute = vcs_volume_state_mute;

        switch (opcode){
            case VCS_OPCODE_RELATIVE_VOLUME_DOWN:  
                volume_control_service_volume_down();
                break;

            case VCS_OPCODE_RELATIVE_VOLUME_UP:
                volume_control_service_volume_up();
                break;

            case VCS_OPCODE_UNMUTE_RELATIVE_VOLUME_DOWN:
                volume_control_service_volume_down();
                volume_control_service_unmute();
                break;

            case VCS_OPCODE_UNMUTE_RELATIVE_VOLUME_UP:     
                volume_control_service_volume_up();
                volume_control_service_unmute();
                break;

            case VCS_OPCODE_SET_ABSOLUTE_VOLUME: 
                if (buffer_size != 3){
                    return VOLUME_CONTROL_OPCODE_NOT_SUPPORTED;
                }
                vcs_volume_state_volume_setting = buffer[2];
                break;
            
            case VCS_OPCODE_UNMUTE:                    
                volume_control_service_unmute();
                break;

            case VCS_OPCODE_MUTE:                          
                volume_control_service_mute();
                break;

            default:
                return VOLUME_CONTROL_OPCODE_NOT_SUPPORTED;
        }
        volume_control_service_server_set_volume_state(volume_setting, mute);
        vcs_emit_volume_state();
        vcs_emit_volume_flags();
    } 

    else if (attribute_handle == vcs_volume_state_client_configuration_handle){
        vcs_volume_state_client_configuration = little_endian_read_16(buffer, 0);
        vcs_set_con_handle(con_handle, vcs_volume_state_client_configuration);
    }

    else if (attribute_handle == vcs_volume_flags_client_configuration_handle){
        vcs_volume_flags_client_configuration = little_endian_read_16(buffer, 0);
        vcs_set_con_handle(con_handle, vcs_volume_flags_client_configuration);
    }

    return 0;
}

static void volume_control_service_server_reset_values(void){
    vcs_volume_flags = 0;
    vcs_con_handle = HCI_CON_HANDLE_INVALID;
    vcs_tasks = 0;

    vcs_volume_change_step_size = 1;
    vcs_volume_state_volume_setting = 0;
    vcs_volume_state_mute = VCS_MUTE_OFF;
}

static void vcs_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(packet);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }

    hci_con_handle_t con_handle;
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            if (vcs_con_handle == con_handle){
                volume_control_service_server_reset_values();
            }
            break;
        default:
            break;
    }
}

static void volume_control_init_included_aics_services(uint16_t vcs_start_handle, uint16_t vcs_end_handle, uint8_t aics_info_num, aics_info_t * aics_info){
    uint16_t start_handle = vcs_start_handle + 1;
    uint16_t end_handle   = vcs_end_handle - 1;
    aics_services_num = 0;

    // include and enumerate AICS services
    while ((start_handle < end_handle) && (aics_services_num < aics_info_num)) {
        uint16_t included_service_handle;
        uint16_t included_service_start_handle;
        uint16_t included_service_end_handle;

        bool service_found = gatt_server_get_included_service_with_uuid16(start_handle, end_handle, 
            ORG_BLUETOOTH_SERVICE_AUDIO_INPUT_CONTROL, &included_service_handle, &included_service_start_handle, &included_service_end_handle);

        if (!service_found){
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
        
        start_handle = included_service_handle + 1;
        aics_services_num++;
    }
}

static void volume_control_init_included_vocs_services(uint16_t start_handle, uint16_t end_handle, uint8_t vocs_info_num, vocs_info_t * vocs_info){
    vocs_services_num = 0;

    // include and enumerate AICS services
    while ((start_handle < end_handle) && (vocs_services_num < vocs_info_num)) {
        uint16_t included_service_handle;
        uint16_t included_service_start_handle;
        uint16_t included_service_end_handle;

        bool service_found = gatt_server_get_included_service_with_uuid16(start_handle, end_handle, 
            ORG_BLUETOOTH_SERVICE_VOLUME_OFFSET_CONTROL,
            &included_service_handle, &included_service_start_handle, &included_service_end_handle);

        if (!service_found){
            break;
        }
        log_info("Include VOCS service 0x%02x-0x%02x", included_service_start_handle, included_service_end_handle);

        volume_offset_control_service_server_t * service = &vocs_services[vocs_services_num];
        service->start_handle = included_service_start_handle;
        service->end_handle = included_service_end_handle;
        service->index = vocs_services_num;
        
        service->info = &vocs_info[vocs_services_num];
        service->audio_output_description_len = (uint8_t) strlen(vocs_info->audio_output_description);

        volume_offset_control_service_server_init(service);
        
        start_handle = included_service_handle + 1;
        vocs_services_num++;
    }
}

void volume_control_service_server_init(uint8_t volume_setting, vcs_mute_t mute,
    uint8_t aics_info_num, aics_info_t * aics_info, 
    uint8_t vocs_info_num, vocs_info_t * vocs_info){

    volume_control_service_server_reset_values();

    vcs_volume_state_volume_setting = volume_setting;
    vcs_volume_state_mute = mute;

    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_VOLUME_CONTROL, &start_handle, &end_handle);
    btstack_assert(service_found != 0);
    UNUSED(service_found);

    // get characteristic value handle and client configuration handle
    vcs_control_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_CONTROL_POINT);
    
    vcs_volume_state_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_STATE);
    vcs_volume_state_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_STATE);

    vcs_volume_flags_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_FLAGS);
    vcs_volume_flags_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_FLAGS);

    log_info("Found VCS service 0x%02x-0x%02x", start_handle, end_handle);

    volume_control_init_included_aics_services(start_handle, end_handle, aics_info_num, aics_info);
    volume_control_init_included_vocs_services(start_handle, end_handle, vocs_info_num, vocs_info);

    // register service with ATT Server
    volume_control_service.start_handle   = start_handle;
    volume_control_service.end_handle     = end_handle;
    volume_control_service.read_callback  = &volume_control_service_read_callback;
    volume_control_service.write_callback = &volume_control_service_write_callback;
    volume_control_service.packet_handler = vcs_packet_handler;
    att_server_register_service_handler(&volume_control_service);
}

void volume_control_service_server_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    vcs_event_callback = callback;
}

void volume_control_service_server_set_volume_state(uint8_t volume_setting, vcs_mute_t mute){
    uint8_t    old_volume_setting = vcs_volume_state_volume_setting;
    vcs_mute_t old_mute = vcs_volume_state_mute;

    vcs_volume_state_volume_setting = volume_setting;
    vcs_volume_state_mute = mute;

    if ((old_volume_setting != vcs_volume_state_volume_setting) || (old_mute != vcs_volume_state_mute)) {
        volume_control_service_update_change_counter();
        volume_control_service_server_set_callback(VCS_TASK_SEND_VOLUME_SETTING);
    }

    if (old_volume_setting != vcs_volume_state_volume_setting) {
        if ((vcs_volume_flags & VCS_VOLUME_FLAGS_SETTING_PERSISTED_MASK) == VCS_VOLUME_FLAGS_SETTING_PERSISTED_RESET){
            vcs_volume_flags |= VCS_VOLUME_FLAGS_SETTING_PERSISTED_USER_SET;
            volume_control_service_server_set_callback(VCS_TASK_SEND_VOLUME_FLAGS);
        }
    }
}

void volume_control_service_server_set_volume_change_step(uint8_t volume_change_step){
    vcs_volume_change_step_size = volume_change_step;
}

uint8_t volume_control_service_server_set_audio_input_state_for_aics(uint8_t aics_index, aics_audio_input_state_t * audio_input_state){
    if (aics_index >= aics_services_num){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    return audio_input_control_service_server_set_audio_input_state(&aics_services[aics_index], audio_input_state);
}

uint8_t volume_control_service_server_set_audio_input_description_for_aics(uint8_t aics_index, const char * audio_input_desc){
    if (aics_index >= aics_services_num){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    audio_input_control_service_server_set_audio_input_description(&aics_services[aics_index], audio_input_desc);
    return ERROR_CODE_SUCCESS;
}

uint8_t  volume_control_service_server_set_audio_input_status_for_aics(uint8_t aics_index, aics_audio_input_status_t audio_input_status){
    if (aics_index >= aics_services_num){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    audio_input_control_service_server_set_audio_input_status(&aics_services[aics_index], audio_input_status);
    return ERROR_CODE_SUCCESS;
}


uint8_t volume_control_service_server_set_volume_offset_for_vocs(uint8_t voics_index, int16_t volume_offset){
    if (voics_index >= vocs_services_num){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    volume_offset_control_service_server_set_volume_offset(&vocs_services[voics_index], volume_offset);
    return ERROR_CODE_SUCCESS;
}

uint8_t volume_control_service_server_set_audio_location_for_vocs(uint8_t vocs_index, uint32_t audio_location){
    if (vocs_index >= vocs_services_num){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    volume_offset_control_service_server_set_audio_location(&vocs_services[vocs_index], audio_location);
    return ERROR_CODE_SUCCESS;
}

uint8_t volume_control_service_server_set_audio_output_description_for_vocs(uint8_t vocs_index, const char * audio_output_desc){
    if (vocs_index >= vocs_services_num){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    volume_offset_control_service_server_set_audio_output_description(&vocs_services[vocs_index], audio_output_desc);
    return ERROR_CODE_SUCCESS;
}
