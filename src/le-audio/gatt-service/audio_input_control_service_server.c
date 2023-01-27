/*
 * Copyright (C) 2022 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "audio_control_input_service_server.c"

#include "btstack_defines.h"
#include "btstack_event.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"

#include "le-audio/gatt-service/audio_input_control_service_server.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#define AICS_TASK_SEND_AUDIO_INPUT_STATE                  0x01
#define AICS_TASK_SEND_AUDIO_INPUT_STATUS                 0x02
#define AICS_TASK_SEND_AUDIO_INPUT_DESCRIPTION            0x04

static btstack_linked_list_t aics_services;

static aics_server_connection_t * aics_server_find_service_for_attribute_handle(uint16_t attribute_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &aics_services);
    while (btstack_linked_list_iterator_has_next(&it)){
        aics_server_connection_t * item = (aics_server_connection_t*) btstack_linked_list_iterator_next(&it);
        if (attribute_handle < item->start_handle) continue;
        if (attribute_handle > item->end_handle)   continue;
        return item;
    }
    return NULL;
}

static uint16_t aics_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    aics_server_connection_t * connection = aics_server_find_service_for_attribute_handle(attribute_handle);
    if (connection == NULL){
        return 0;
    }

    if (attribute_handle == connection->audio_input_state_value_handle){
        uint8_t value[4];
        value[0] = (uint8_t)connection->info->audio_input_state.gain_setting_db;
        value[1] = connection->info->audio_input_state.mute_mode;
        value[2] = connection->info->audio_input_state.gain_mode;
        value[3] = connection->audio_input_state_change_counter;
        return att_read_callback_handle_blob(value, sizeof(value), offset, buffer, buffer_size);
    }


    if (attribute_handle == connection->gain_settings_properties_value_handle){
        uint8_t value[3];

        value[0] = connection->info->gain_settings_properties.gain_settings_units;
        value[1] = (uint8_t)connection->info->gain_settings_properties.gain_settings_minimum;
        value[2] = (uint8_t)connection->info->gain_settings_properties.gain_settings_maximum;
        return att_read_callback_handle_blob(value, sizeof(value), offset, buffer, buffer_size);
    }

    if (attribute_handle == connection->audio_input_type_value_handle){
        return att_read_callback_handle_byte((uint8_t)connection->info->audio_input_type, offset, buffer, buffer_size);
    }

    if (attribute_handle == connection->audio_input_status_value_handle){
        return att_read_callback_handle_byte((uint8_t)connection->audio_input_status, offset, buffer, buffer_size);
    }

    if (attribute_handle == connection->audio_input_description_value_handle){
        return att_read_callback_handle_blob((uint8_t *)connection->info->audio_input_description, connection->audio_input_description_len, offset, buffer, buffer_size);
    }
    
    
    if (attribute_handle == connection->audio_input_state_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(connection->audio_input_state_client_configuration, offset, buffer, buffer_size);
    }
    
    if (attribute_handle == connection->audio_input_status_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(connection->audio_input_status_client_configuration, offset, buffer, buffer_size);
    }
    
    if (attribute_handle == connection->audio_input_description_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(connection->audio_input_description_client_configuration, offset, buffer, buffer_size);
    }

    return 0;
}


static void aics_server_emit_mute_mode(aics_server_connection_t * connection){
    btstack_assert(connection != NULL);
    btstack_assert(connection->info != NULL);
    btstack_assert(connection->info->packet_handler != NULL);
    
    uint8_t event[7];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_AICS_SERVER_MUTE_MODE;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    event[pos++] = connection->index;
    event[pos++] = (uint8_t)connection->info->audio_input_state.mute_mode;
    (*connection->info->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void aics_server_emit_gain_mode(aics_server_connection_t * connection){
    btstack_assert(connection != NULL);
    btstack_assert(connection->info != NULL);
    btstack_assert(connection->info->packet_handler != NULL);
    
    uint8_t event[7];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_AICS_SERVER_GAIN_MODE;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    event[pos++] = connection->index;
    event[pos++] = (uint8_t)connection->info->audio_input_state.gain_mode;
    (*connection->info->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void aics_server_emit_gain(aics_server_connection_t * connection){
    btstack_assert(connection != NULL);
    btstack_assert(connection->info != NULL);
    btstack_assert(connection->info->packet_handler != NULL);
    
    uint8_t event[7];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_AICS_SERVER_GAIN_CHANGED;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    event[pos++] = connection->index;
    event[pos++] = (uint8_t)connection->info->audio_input_state.gain_setting_db;
    (*connection->info->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void aics_server_emit_audio_input_description(aics_server_connection_t * connection){
    btstack_assert(connection != NULL);
    btstack_assert(connection->info != NULL);
    btstack_assert(connection->info->packet_handler != NULL);
    
    uint8_t event[7 + AICS_MAX_AUDIO_INPUT_DESCRIPTION_LENGTH];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_AICS_SERVER_AUDIO_INPUT_DESC_CHANGED;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    event[pos++] = connection->index;
    event[pos++] = connection->audio_input_description_len;
    memcpy(&event[pos], (uint8_t *)connection->info->audio_input_description, connection->audio_input_description_len + 1);
    pos += connection->audio_input_description_len;
    event[pos++] = 0;
    (*connection->info->packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

static bool aics_server_set_gain(aics_server_connection_t * connection, int8_t gain_db){
    if (gain_db < connection->info->gain_settings_properties.gain_settings_minimum) return false;
    if (gain_db > connection->info->gain_settings_properties.gain_settings_maximum) return false;

    connection->info->audio_input_state.gain_setting_db = gain_db;
    return true;
}

static void aics_server_update_change_counter(aics_server_connection_t * connection){
    connection->audio_input_state_change_counter++;
}

static void aics_server_can_send_now(void * context){
    aics_server_connection_t * connection = (aics_server_connection_t *) context;

    if ((connection->scheduled_tasks & AICS_TASK_SEND_AUDIO_INPUT_STATE) != 0){
        connection->scheduled_tasks &= ~AICS_TASK_SEND_AUDIO_INPUT_STATE;
        
        uint8_t value[4];
        value[0] = (uint8_t)connection->info->audio_input_state.gain_setting_db;
        value[1] = connection->info->audio_input_state.mute_mode;
        value[2] = connection->info->audio_input_state.gain_mode;
        value[3] = connection->audio_input_state_change_counter;

        att_server_notify(connection->con_handle, connection->audio_input_state_value_handle, &value[0], sizeof(value));

    } else if ((connection->scheduled_tasks & AICS_TASK_SEND_AUDIO_INPUT_STATUS) != 0){
        connection->scheduled_tasks &= ~AICS_TASK_SEND_AUDIO_INPUT_STATUS;
        uint8_t value = (uint8_t)connection->audio_input_status;
        att_server_notify(connection->con_handle, connection->audio_input_status_value_handle, &value, 1);

    } else if ((connection->scheduled_tasks & AICS_TASK_SEND_AUDIO_INPUT_DESCRIPTION) != 0){
        connection->scheduled_tasks &= ~AICS_TASK_SEND_AUDIO_INPUT_DESCRIPTION;
        att_server_notify(connection->con_handle, connection->audio_input_description_value_handle, (uint8_t *)connection->info->audio_input_description, connection->audio_input_description_len);
    }

    if (connection->scheduled_tasks != 0){
        att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
    } else {
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &aics_services);
        while (btstack_linked_list_iterator_has_next(&it)){
            aics_server_connection_t * item = (aics_server_connection_t*) btstack_linked_list_iterator_next(&it);
            if (item->scheduled_tasks == 0) continue;
            att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
            return;
        }
    }
}

static void aics_server_set_callback(aics_server_connection_t * connection, uint8_t task){
    if (connection->con_handle == HCI_CON_HANDLE_INVALID){
        connection->scheduled_tasks &= ~task;
        return;
    }

    uint8_t scheduled_tasks = connection->scheduled_tasks;
    connection->scheduled_tasks |= task;
    if (scheduled_tasks == 0){
        connection->scheduled_tasks_callback.callback = &aics_server_can_send_now;
        connection->scheduled_tasks_callback.context  = (void*) connection;
        att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
    }
}

static void aics_server_set_con_handle(aics_server_connection_t * connection, hci_con_handle_t con_handle, uint16_t configuration){
    connection->con_handle = (configuration == 0) ? HCI_CON_HANDLE_INVALID : con_handle;
}

static int aics_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);
    
    aics_server_connection_t * connection = aics_server_find_service_for_attribute_handle(attribute_handle);
    if (connection == NULL){
        return 0;
    }

    if (attribute_handle == connection->audio_input_control_value_handle){
        if (buffer_size == 0){
            return AICS_ERROR_CODE_OPCODE_NOT_SUPPORTED;
        }
        aics_opcode_t opcode = (aics_opcode_t)buffer[0];

        if (buffer_size == 1){
            return AICS_ERROR_CODE_INVALID_CHANGE_COUNTER;
        }
        uint8_t change_counter = buffer[1];

        if (change_counter != connection->audio_input_state_change_counter){
            return AICS_ERROR_CODE_INVALID_CHANGE_COUNTER;
        }

        switch (opcode){
            case AICS_OPCODE_SET_GAIN_SETTING:
                if (buffer_size != 3){
                    return AICS_ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
                
                if (!aics_server_set_gain(connection, (int8_t)buffer[2])) {
                    return AICS_ERROR_CODE_VALUE_OUT_OF_RANGE;
                }

                switch (connection->info->audio_input_state.gain_mode){
                    case AICS_GAIN_MODE_MANUAL_ONLY:
                    case AICS_GAIN_MODE_MANUAL:
                        aics_server_emit_gain(connection);
                        break;
                    default:
                        break;
                }
                break;

            case AICS_OPCODE_UMUTE:
                switch (connection->info->audio_input_state.mute_mode){
                    case AICS_MUTE_MODE_DISABLED:
                        aics_server_emit_mute_mode(connection);
                        return AICS_ERROR_CODE_MUTE_DISABLED;
                    case AICS_MUTE_MODE_MUTED:
                        connection->info->audio_input_state.mute_mode = AICS_MUTE_MODE_NOT_MUTED;
                        aics_server_emit_mute_mode(connection);
                        break;
                    default:
                        break;
                }
                break;

            case AICS_OPCODE_MUTE:
                switch (connection->info->audio_input_state.mute_mode){
                    case AICS_MUTE_MODE_DISABLED:
                        aics_server_emit_mute_mode(connection);
                        return AICS_ERROR_CODE_MUTE_DISABLED;
                    case AICS_MUTE_MODE_NOT_MUTED:
                        connection->info->audio_input_state.mute_mode = AICS_MUTE_MODE_MUTED;
                        aics_server_emit_mute_mode(connection);
                        break;
                    default:
                        break;
                }
                break;

            case AICS_OPCODE_SET_MANUAL_GAIN_MODE:
                switch (connection->info->audio_input_state.gain_mode){
                    case AICS_GAIN_MODE_AUTOMATIC:
                        connection->info->audio_input_state.gain_mode = AICS_GAIN_MODE_MANUAL;
                        aics_server_emit_gain_mode(connection);
                        break;
                    default:
                        return AICS_ERROR_CODE_GAIN_MODE_CHANGE_NOT_ALLOWED;
                }
                break;

            case AICS_OPCODE_SET_AUTOMATIC_GAIN_MODE:
                switch (connection->info->audio_input_state.gain_mode){
                    case AICS_GAIN_MODE_MANUAL:
                        connection->info->audio_input_state.gain_mode = AICS_GAIN_MODE_AUTOMATIC;
                        aics_server_emit_gain_mode(connection);
                        break;
                    default:
                        return AICS_ERROR_CODE_GAIN_MODE_CHANGE_NOT_ALLOWED;
                }
                break;

            default:
                return AICS_ERROR_CODE_OPCODE_NOT_SUPPORTED;
        }

        aics_server_update_change_counter(connection);
        aics_server_set_callback(connection, AICS_TASK_SEND_AUDIO_INPUT_STATE);
    }

    if (attribute_handle == connection->audio_input_description_value_handle){
        btstack_strcpy(connection->info->audio_input_description, AICS_MAX_AUDIO_INPUT_DESCRIPTION_LENGTH, (char *)buffer);
        connection->audio_input_description_len = (uint8_t) strlen(connection->info->audio_input_description);
        aics_server_emit_audio_input_description(connection);
        aics_server_set_callback(connection, AICS_TASK_SEND_AUDIO_INPUT_DESCRIPTION);
    }

    if (attribute_handle == connection->audio_input_state_client_configuration_handle){
        connection->audio_input_state_client_configuration = little_endian_read_16(buffer, 0);
        aics_server_set_con_handle(connection, con_handle, connection->audio_input_state_client_configuration);
    }
    
    if (attribute_handle == connection->audio_input_status_client_configuration_handle){
        connection->audio_input_status_client_configuration = little_endian_read_16(buffer, 0);
        aics_server_set_con_handle(connection, con_handle, connection->audio_input_status_client_configuration);
    }
    
    if (attribute_handle == connection->audio_input_description_client_configuration_handle){
        connection->audio_input_description_client_configuration = little_endian_read_16(buffer, 0);
        aics_server_set_con_handle(connection, con_handle, connection->audio_input_description_client_configuration);
    }

    return 0;
}

static void aics_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(packet);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }
    hci_con_handle_t con_handle;
    btstack_linked_list_iterator_t it;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            btstack_linked_list_iterator_init(&it, &aics_services);

            while (btstack_linked_list_iterator_has_next(&it)){
                aics_server_connection_t * connection = (aics_server_connection_t*) btstack_linked_list_iterator_next(&it);
                if (connection->con_handle != con_handle) continue;
                connection->con_handle = HCI_CON_HANDLE_INVALID;
            }
            break;
        default:
            break;
    }
}

void audio_input_control_service_server_init(aics_server_connection_t * connection){
    btstack_assert(connection != NULL);
    btstack_assert(connection->info->packet_handler != NULL);

    btstack_linked_list_add(&aics_services, (btstack_linked_item_t *)connection);

    connection->scheduled_tasks = 0;
    connection->con_handle = HCI_CON_HANDLE_INVALID;

    // get characteristic value handle and client configuration handle
    connection->audio_input_state_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATE);
    connection->audio_input_state_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATE);

    connection->gain_settings_properties_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_GAIN_SETTINGS_ATTRIBUTE);

    connection->audio_input_type_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_TYPE);

    connection->audio_input_status_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATUS);
    connection->audio_input_status_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATUS);

    connection->audio_input_control_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_CONTROL_POINT);

    connection->audio_input_description_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_DESCRIPTION);
    connection->audio_input_description_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_DESCRIPTION);

#ifdef ENABLE_TESTING_SUPPORT
    printf("AICS[%d] 0x%02x - 0x%02x \n", connection->index, connection->start_handle, connection->end_handle);
    printf("    audio_input_state            0x%02x \n", connection->audio_input_state_value_handle);
    printf("    audio_input_state CCC        0x%02x \n", connection->audio_input_state_client_configuration_handle);

    printf("    gain_settings_properties     0x%02x \n", connection->gain_settings_properties_value_handle);
    printf("    audio_input_type             0x%02x \n", connection->audio_input_type_value_handle);

    printf("    audio_input_status           0x%02x \n", connection->audio_input_status_value_handle);
    printf("    audio_input_status CCC       0x%02x \n", connection->audio_input_status_client_configuration_handle);

    printf("    control_value_handle         0x%02x \n", connection->audio_input_control_value_handle);
    printf("    description_value_handle     0x%02x \n", connection->audio_input_description_value_handle);
    printf("    description_value_handle CCC 0x%02x \n", connection->audio_input_description_client_configuration_handle);
#endif

    // register service with ATT Server
    connection->service_handler.start_handle   = connection->start_handle;
    connection->service_handler.end_handle     = connection->end_handle;
    connection->service_handler.read_callback  = &aics_server_read_callback;
    connection->service_handler.write_callback = &aics_server_write_callback;
    connection->service_handler.packet_handler = aics_server_packet_handler;
    att_server_register_service_handler(&connection->service_handler);
}

uint8_t audio_input_control_service_server_set_audio_input_state(aics_server_connection_t * connection, aics_audio_input_state_t * audio_input_state){
    btstack_assert(connection != NULL);
    
    bool valid_range = aics_server_set_gain(connection, audio_input_state->gain_setting_db);
    if (!valid_range){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    connection->info->audio_input_state.mute_mode = audio_input_state->mute_mode;
    connection->info->audio_input_state.gain_mode = audio_input_state->gain_mode;
    aics_server_update_change_counter(connection);

    aics_server_set_callback(connection, AICS_TASK_SEND_AUDIO_INPUT_STATE);
    return ERROR_CODE_SUCCESS;
}

void audio_input_control_service_server_set_audio_input_status(aics_server_connection_t * connection, aics_audio_input_status_t audio_input_status){
    btstack_assert(connection != NULL);
    connection->audio_input_status = audio_input_status;
    aics_server_set_callback(connection, AICS_TASK_SEND_AUDIO_INPUT_STATUS);
}

void audio_input_control_service_server_set_audio_input_description(aics_server_connection_t * connection, const char * audio_input_desc){
    btstack_assert(connection != NULL);
    btstack_strcpy(connection->info->audio_input_description, AICS_MAX_AUDIO_INPUT_DESCRIPTION_LENGTH, (char *)audio_input_desc);
    connection->audio_input_description_len = (uint8_t) strlen(connection->info->audio_input_description);
    aics_server_set_callback(connection, AICS_TASK_SEND_AUDIO_INPUT_DESCRIPTION);
}


