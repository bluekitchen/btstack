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

#include "le-audio/gatt-service/volume_offset_control_service_server.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#define VOCS_TASK_SEND_VOLUME_OFFSET                        0x01
#define VOCS_TASK_SEND_AUDIO_LOCATION                       0x02
#define VOCS_TASK_SEND_AUDIO_OUTPUT_DESCRIPTION             0x04

static btstack_linked_list_t vocs_services;

static vocs_server_connection_t * vocs_server_find_service_for_attribute_handle(uint16_t attribute_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &vocs_services);
    while (btstack_linked_list_iterator_has_next(&it)){
        vocs_server_connection_t * item = (vocs_server_connection_t*) btstack_linked_list_iterator_next(&it);
        if (attribute_handle < item->start_handle) continue;
        if (attribute_handle > item->end_handle)   continue;
        return item;
    }
    return NULL;
}

static uint16_t vocs_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    vocs_server_connection_t * connection = vocs_server_find_service_for_attribute_handle(attribute_handle);
    if (connection == NULL){
        return 0;
    }

    if (attribute_handle == connection->volume_offset_state_value_handle){
        uint8_t value[3];
        little_endian_store_16(value, 0, connection->info->volume_offset);
        value[2] = connection->volume_offset_state_change_counter;
        return att_read_callback_handle_blob(value, sizeof(value), offset, buffer, buffer_size);
    }

    if (attribute_handle == connection->audio_location_value_handle){
        return att_read_callback_handle_little_endian_32(connection->info->audio_location, offset, buffer, buffer_size);
    }

    if (attribute_handle == connection->audio_output_description_value_handle){
        return att_read_callback_handle_blob((uint8_t *)connection->info->audio_output_description, (uint16_t) strlen(connection->info->audio_output_description), offset, buffer, buffer_size);
    }
    
    
    if (attribute_handle == connection->volume_offset_state_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(connection->volume_offset_state_client_configuration, offset, buffer, buffer_size);
    }
    
    if (attribute_handle == connection->audio_location_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(connection->audio_location_client_configuration, offset, buffer, buffer_size);
    }
    
    if (attribute_handle == connection->audio_output_description_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(connection->audio_output_description_client_configuration, offset, buffer, buffer_size);
    }

    return 0;
}

static void vocs_server_update_change_counter(vocs_server_connection_t * connection){
    connection->volume_offset_state_change_counter++;
}

static void vocs_server_can_send_now(void * context){
    vocs_server_connection_t * connection = (vocs_server_connection_t *) context;

    if ((connection->scheduled_tasks & VOCS_TASK_SEND_VOLUME_OFFSET) != 0){
        connection->scheduled_tasks &= ~VOCS_TASK_SEND_VOLUME_OFFSET;
        uint8_t value[3];
        little_endian_store_16(value, 0, (uint16_t)connection->info->volume_offset);
        value[2] = connection->volume_offset_state_change_counter;
        att_server_notify(connection->con_handle, connection->volume_offset_state_value_handle, &value[0], sizeof(value));

    } else if ((connection->scheduled_tasks & VOCS_TASK_SEND_AUDIO_LOCATION) != 0) {
        connection->scheduled_tasks &= ~VOCS_TASK_SEND_AUDIO_LOCATION;
        uint8_t value[4];
        little_endian_store_32(value, 0, connection->info->audio_location);
        att_server_notify(connection->con_handle, connection->audio_location_value_handle, &value[0], sizeof(value));
    } else if ((connection->scheduled_tasks & VOCS_TASK_SEND_AUDIO_OUTPUT_DESCRIPTION) != 0) {
        connection->scheduled_tasks &= ~VOCS_TASK_SEND_AUDIO_OUTPUT_DESCRIPTION;
        att_server_notify(connection->con_handle, connection->audio_output_description_value_handle, (uint8_t *)connection->info->audio_output_description, connection->audio_output_description_len);
    }

    if (connection->scheduled_tasks != 0){
        att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
    } else {
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &vocs_services);
        while (btstack_linked_list_iterator_has_next(&it)){
            vocs_server_connection_t * item = (vocs_server_connection_t*) btstack_linked_list_iterator_next(&it);
            if (item->scheduled_tasks == 0) continue;
            att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
            return;
        }
    }
}

static void vocs_server_set_callback(vocs_server_connection_t * connection, uint8_t task){
    if (connection->con_handle == HCI_CON_HANDLE_INVALID){
        connection->scheduled_tasks &= ~task;
        return;
    }

    uint8_t scheduled_tasks = connection->scheduled_tasks;
    connection->scheduled_tasks |= task;
    if (scheduled_tasks == 0){
        connection->scheduled_tasks_callback.callback = &vocs_server_can_send_now;
        connection->scheduled_tasks_callback.context  = (void*) connection;
        att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
    }
}

static void vocs_server_set_con_handle(vocs_server_connection_t * connection, hci_con_handle_t con_handle, uint16_t configuration){
    connection->con_handle = (configuration == 0) ? HCI_CON_HANDLE_INVALID : con_handle;
}

static bool vocs_server_set_volume_offset(vocs_server_connection_t * vocs, int16_t volume_offset){
    if (volume_offset < -255) return false;
    if (volume_offset > 255) return false;

    vocs->info->volume_offset = volume_offset;
    return true;
}

static bool vocs_server_set_audio_location(vocs_server_connection_t * connection, uint32_t audio_location){
    if (audio_location == LE_AUDIO_LOCATION_MASK_NOT_ALLOWED) return false;
    if ( (audio_location & LE_AUDIO_LOCATION_MASK_RFU) != 0) return false;

    connection->info->audio_location = audio_location;
    return true;
}

static void vocs_server_emit_volume_offset(vocs_server_connection_t * connection){
    btstack_assert(connection != NULL);
    btstack_assert(connection->info != NULL);
    btstack_assert(connection->info->packet_handler != NULL);
    
    uint8_t event[8];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_VOCS_SERVER_VOLUME_OFFSET;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    event[pos++] = connection->index;
    little_endian_store_16(event, pos, connection->info->volume_offset);
    pos += 2;
    (*connection->info->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void vocs_server_emit_audio_location(vocs_server_connection_t * connection){
    btstack_assert(connection != NULL);
    btstack_assert(connection->info != NULL);
    btstack_assert(connection->info->packet_handler != NULL);
    
    uint8_t event[10];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_VOCS_SERVER_AUDIO_LOCATION;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    event[pos++] = connection->index;
    little_endian_store_32(event, pos, connection->info->audio_location);
    pos += 4;
    (*connection->info->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void vocs_server_emit_audio_output_description(vocs_server_connection_t * connection){
    btstack_assert(connection != NULL);
    btstack_assert(connection->info != NULL);
    btstack_assert(connection->info->packet_handler != NULL);
    
    uint8_t event[7 + VOCS_MAX_AUDIO_OUTPUT_DESCRIPTION_LENGTH];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_VOCS_SERVER_AUDIO_OUTPUT_DESCRIPTION;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    event[pos++] = connection->index;
    event[pos++] = (uint8_t)connection->audio_output_description_len;
    memcpy(&event[pos], (uint8_t *)connection->info->audio_output_description, connection->audio_output_description_len + 1);
    pos += connection->audio_output_description_len;
    event[pos++] = 0;
    (*connection->info->packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}


static int vocs_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);
    
    vocs_server_connection_t * connection = vocs_server_find_service_for_attribute_handle(attribute_handle);
    if (connection == NULL){
        return 0;
    }

    if (attribute_handle == connection->volume_offset_control_point_value_handle){
        if (buffer_size == 0){
            return VOCS_ERROR_CODE_OPCODE_NOT_SUPPORTED;
        }
        vocs_opcode_t opcode = (vocs_opcode_t)buffer[0];

        if (buffer_size == 1){
            return VOCS_ERROR_CODE_INVALID_CHANGE_COUNTER;
        }
        uint8_t change_counter = buffer[1];

        if (change_counter != connection->volume_offset_state_change_counter){
            return VOCS_ERROR_CODE_INVALID_CHANGE_COUNTER;
        }

        int16_t volume_offset = (int16_t) little_endian_read_16(buffer, 2);
        switch (opcode){
            case VOCS_OPCODE_SET_VOLUME_OFFSET:
                if (buffer_size != 4){
                    return VOCS_ERROR_CODE_VALUE_OUT_OF_RANGE;
                }

                if (!vocs_server_set_volume_offset(connection, volume_offset)) {
                    return VOCS_ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
                vocs_server_emit_volume_offset(connection);
                break;

            default:
                return VOCS_ERROR_CODE_OPCODE_NOT_SUPPORTED;
        }

        vocs_server_update_change_counter(connection);
        vocs_server_set_callback(connection, VOCS_TASK_SEND_VOLUME_OFFSET);
    }

    if (attribute_handle == connection->audio_location_value_handle){
        if (buffer_size != 4){
            return 0;
        }
        uint32_t audio_location = little_endian_read_32(buffer, 0);
        if (vocs_server_set_audio_location(connection, audio_location)) {
            vocs_server_emit_audio_location(connection);
            vocs_server_set_callback(connection, VOCS_TASK_SEND_AUDIO_LOCATION);
        }
    }

    if (attribute_handle == connection->audio_output_description_value_handle){
        btstack_strcpy(connection->info->audio_output_description, connection->audio_output_description_len, (char *)buffer);
        connection->audio_output_description_len = (uint8_t) strlen(connection->info->audio_output_description);
        vocs_server_emit_audio_output_description(connection);
        vocs_server_set_callback(connection, VOCS_TASK_SEND_AUDIO_OUTPUT_DESCRIPTION);
    }

    if (attribute_handle == connection->volume_offset_state_client_configuration_handle){
        connection->volume_offset_state_client_configuration = little_endian_read_16(buffer, 0);
        vocs_server_set_con_handle(connection, con_handle, connection->volume_offset_state_client_configuration);
    }
    
    if (attribute_handle == connection->audio_location_client_configuration_handle){
        connection->audio_location_client_configuration = little_endian_read_16(buffer, 0);
        vocs_server_set_con_handle(connection, con_handle, connection->audio_location_client_configuration);
    }
    
    if (attribute_handle == connection->audio_output_description_client_configuration_handle){
        connection->audio_output_description_client_configuration = little_endian_read_16(buffer, 0);
        vocs_server_set_con_handle(connection, con_handle, connection->audio_output_description_client_configuration);
    }

    return 0;
}

static void vocs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
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
            btstack_linked_list_iterator_init(&it, &vocs_services);

            while (btstack_linked_list_iterator_has_next(&it)){
                vocs_server_connection_t * item = (vocs_server_connection_t*) btstack_linked_list_iterator_next(&it);
                if (item->con_handle != con_handle) continue;
                item->con_handle = HCI_CON_HANDLE_INVALID;
            }
            break;
        default:
            break;
    }
}


void volume_offset_control_service_server_init(vocs_server_connection_t * connection){
    btstack_assert(connection != NULL);
    btstack_assert(connection->info->packet_handler != NULL);

    btstack_linked_list_add(&vocs_services, (btstack_linked_item_t *)connection);

    connection->scheduled_tasks = 0;
    connection->con_handle = HCI_CON_HANDLE_INVALID;

    // get characteristic value handle and client configuration handle
    connection->volume_offset_state_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_OFFSET_STATE);
    connection->volume_offset_state_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_OFFSET_STATE);

    connection->audio_location_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_LOCATION);
    connection->audio_location_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_LOCATION);

    connection->volume_offset_control_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_OFFSET_CONTROL_POINT);

    connection->audio_output_description_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_OUTPUT_DESCRIPTION);
    connection->audio_output_description_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(connection->start_handle, connection->end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_OUTPUT_DESCRIPTION);

#ifdef ENABLE_TESTING_SUPPORT
    printf("VOCS[%d] 0x%02x - 0x%02x \n", connection->index, connection->start_handle, connection->end_handle);
    printf("    volume_offset_state            0x%02x \n", connection->volume_offset_state_value_handle);
    printf("    volume_offset_state CCC        0x%02x \n", connection->volume_offset_state_client_configuration_handle);
    
    printf("    audio_location                 0x%02x \n", connection->audio_location_value_handle);
    printf("    audio_location CCC             0x%02x \n", connection->audio_location_client_configuration_handle);

    printf("    control_point_value_handle     0x%02x \n", connection->volume_offset_control_point_value_handle);

    printf("    description_value_handle     0x%02x \n", connection->audio_output_description_value_handle);
    printf("    description_value_handle CCC 0x%02x \n", connection->audio_output_description_client_configuration_handle);
#endif

    // register service with ATT Server
    connection->service_handler.start_handle   = connection->start_handle;
    connection->service_handler.end_handle     = connection->end_handle;
    connection->service_handler.read_callback  = &vocs_server_read_callback;
    connection->service_handler.write_callback = &vocs_server_write_callback;
    connection->service_handler.packet_handler = vocs_server_packet_handler;
    att_server_register_service_handler(&connection->service_handler);
}

uint8_t volume_offset_control_service_server_set_volume_offset(vocs_server_connection_t * connection, int16_t volume_offset){
    btstack_assert(connection != NULL);
    connection->info->volume_offset = volume_offset;
    vocs_server_update_change_counter(connection);
    vocs_server_set_callback(connection, VOCS_TASK_SEND_VOLUME_OFFSET);
    return ERROR_CODE_SUCCESS;
}

uint8_t volume_offset_control_service_server_set_audio_location(vocs_server_connection_t * connection, uint32_t audio_location){
    btstack_assert(connection != NULL);
    connection->info->audio_location = audio_location;
    vocs_server_set_callback(connection, VOCS_TASK_SEND_AUDIO_LOCATION);
    return ERROR_CODE_SUCCESS;
}

void volume_offset_control_service_server_set_audio_output_description(vocs_server_connection_t * connection, const char * audio_output_desc){
    btstack_assert(connection != NULL);
    btstack_strcpy(connection->info->audio_output_description, connection->audio_output_description_len, (char *)audio_output_desc);
    connection->audio_output_description_len = (uint8_t) strlen(connection->info->audio_output_description);
    vocs_server_set_callback(connection, VOCS_TASK_SEND_AUDIO_OUTPUT_DESCRIPTION);
}
