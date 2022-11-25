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

#define BTSTACK_FILE__ "coordinated_set_identification_service_server.c"

#include <stdio.h>

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/gatt-service/coordinated_set_identification_service_server.h"
#include "le-audio/le_audio_util.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#define CSIS_TASK_SEND_SIRK                         0x01
#define CSIS_TASK_SEND_COORDINATED_SET_SIZE         0x02
#define CSIS_TASK_SEND_MEMBER_LOCK                  0x04


static att_service_handler_t    coordinated_set_identification_service;
static btstack_packet_handler_t csis_event_callback;
static csis_coordinator_t     * csis_coordinators;
static uint8_t                  csis_coordinators_max_num;

// csis server data
static uint16_t sirk_handle;
static uint16_t sirk_configuration_handle;

static uint8_t           csis_sirk[16];
static csis_sirk_type_t  csis_sirk_type;

static uint8_t  csis_coordinated_set_size;
static uint16_t coordinated_set_size_handle;
static uint16_t coordinated_set_size_configuration_handle;

static csis_member_lock_t   csis_member_lock;

static uint16_t member_lock_handle;
static uint16_t member_lock_configuration_handle;

static uint8_t  csis_member_rank;
static uint16_t member_rank_handle;

static csis_coordinator_t * csis_get_coordinator_for_con_handle(hci_con_handle_t con_handle){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return NULL;
    }
    uint8_t i;
    for (i = 0; i < csis_coordinators_max_num; i++){
        if (csis_coordinators[i].con_handle == con_handle){
            return &csis_coordinators[i];
        }
    }
    return NULL;
}

static csis_coordinator_t * csis_server_add_coordinator(hci_con_handle_t con_handle){
    uint8_t i;
    
    for (i = 0; i < csis_coordinators_max_num; i++){
        if (csis_coordinators[i].con_handle == HCI_CON_HANDLE_INVALID){
            csis_coordinators[i].con_handle = con_handle;
            log_info("added coordinator 0x%02x, index %d", con_handle, i);
            return &csis_coordinators[i];
        } 
    }
    return NULL;
}

static void csis_server_emit_coordinator_disconnected(hci_con_handle_t con_handle){
    btstack_assert(csis_event_callback != NULL);

    uint8_t event[5];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_COORDINATOR_DISCONNECTED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}


static void csis_server_emit_coordinator_connected(hci_con_handle_t con_handle, uint8_t status){
    btstack_assert(csis_event_callback != NULL);

    uint8_t event[6];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_COORDINATOR_CONNECTED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = status;
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static uint16_t coordinated_set_identification_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    csis_coordinator_t * coordinator = csis_get_coordinator_for_con_handle(con_handle);
        
    if (coordinator == NULL){
        coordinator = csis_server_add_coordinator(con_handle);
        
        if (coordinator == NULL){
            csis_server_emit_coordinator_connected(con_handle, ERROR_CODE_CONNECTION_LIMIT_EXCEEDED);
            log_info("There are already %d coordinators connected. No memory for new coordinator.", csis_coordinators_max_num);
            return 0;
        } else {
            csis_server_emit_coordinator_connected(con_handle, ERROR_CODE_SUCCESS);
        }    
    }

    if (attribute_handle == sirk_configuration_handle){
        return att_read_callback_handle_little_endian_16(coordinator->sirk_configuration, offset, buffer, buffer_size);
    }

    if (attribute_handle == coordinated_set_size_configuration_handle){
        return att_read_callback_handle_little_endian_16(coordinator->coordinated_set_size_configuration, offset, buffer, buffer_size);
    }
   
    if (attribute_handle == member_lock_configuration_handle){
        return att_read_callback_handle_little_endian_16(coordinator->member_lock_configuration, offset, buffer, buffer_size);
    }

    if (attribute_handle == sirk_handle){
        return att_read_callback_handle_blob(csis_sirk, sizeof(csis_sirk), offset, buffer, buffer_size);
    }

    if (attribute_handle == coordinated_set_size_handle){
        return att_read_callback_handle_byte(csis_coordinated_set_size, offset, buffer, buffer_size);
    }
   
    if (attribute_handle == member_lock_handle){
        return att_read_callback_handle_byte((uint8_t)csis_member_lock, offset, buffer, buffer_size);
    }

    if (attribute_handle == member_rank_handle){
        return att_read_callback_handle_byte(csis_member_rank, offset, buffer, buffer_size);
    }

    // reset coordinator if no attribute handle was associated with it
    coordinator->con_handle = HCI_CON_HANDLE_INVALID;
    return 0;
}

static csis_coordinator_t * csis_get_lock_owner_coordinator(void){
    uint8_t i;
    for (i = 0; i < csis_coordinators_max_num; i++){
        if (csis_coordinators[i].is_lock_owner){
            return &csis_coordinators[i];
        }
    }
    return NULL;
}

static void csis_server_can_send_now(void * context){
    csis_coordinator_t * coordinator = (csis_coordinator_t *) context;

    if ((coordinator->scheduled_tasks & CSIS_TASK_SEND_MEMBER_LOCK) != 0) {
        coordinator->scheduled_tasks &= ~CSIS_TASK_SEND_MEMBER_LOCK;
        uint8_t value[1];
        value[0] = csis_member_lock;
        att_server_notify(coordinator->con_handle, member_lock_handle, &value[0], sizeof(value));
    }

    if (coordinator->scheduled_tasks != 0){
        coordinator->scheduled_tasks_callback.callback = &csis_server_can_send_now;
        coordinator->scheduled_tasks_callback.context  = coordinator;
        att_server_register_can_send_now_callback(&coordinator->scheduled_tasks_callback, coordinator->con_handle);
    }
}

static void csis_server_set_callback(uint8_t task){
    uint8_t i;
    for (i = 0; i < csis_coordinators_max_num; i++){
        csis_coordinator_t * coordinator = &csis_coordinators[i];

        if (coordinator->con_handle == HCI_CON_HANDLE_INVALID){
            coordinator->scheduled_tasks &= ~task;
            continue;
        }
    
        if ((task == CSIS_TASK_SEND_MEMBER_LOCK) && coordinator->is_lock_owner){
            continue;
        }

        uint8_t scheduled_tasks = coordinator->scheduled_tasks;
        coordinator->scheduled_tasks |= task;
        if (scheduled_tasks == 0){
            coordinator->scheduled_tasks_callback.callback = &csis_server_can_send_now;
            coordinator->scheduled_tasks_callback.context  = coordinator;
            att_server_register_can_send_now_callback(&coordinator->scheduled_tasks_callback, coordinator->con_handle);
        }
    }
}

static void csis_lock_timer_timeout_handler(btstack_timer_source_t * timer){
    csis_coordinator_t * coordinator = (csis_coordinator_t *) btstack_run_loop_get_timer_context(timer);

    coordinator->is_lock_owner = false;
    csis_member_lock = CSIS_MEMBER_UNLOCKED;
    csis_server_set_callback(CSIS_TASK_SEND_MEMBER_LOCK);
    btstack_run_loop_remove_timer(&coordinator->lock_timer);
}

static void csis_lock_timer_start(csis_coordinator_t * coordinator){
    if ( ((csis_coordinator_t *) btstack_run_loop_get_timer_context(&coordinator->lock_timer)) != NULL ){
        return;
    }

    btstack_run_loop_set_timer_handler(&coordinator->lock_timer, csis_lock_timer_timeout_handler);
    btstack_run_loop_set_timer_context(&coordinator->lock_timer, coordinator);
    btstack_run_loop_set_timer(&coordinator->lock_timer, CSIS_LOCK_TIMEOUT_MS); 
    btstack_run_loop_add_timer(&coordinator->lock_timer);
}

static int coordinated_set_identification_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    // printf("coordinated_set_identification_service_write_callback con_handle 0x%02x, attr_handle 0x%02x \n", con_handle, attribute_handle);
    csis_coordinator_t * coordinator = csis_get_coordinator_for_con_handle(con_handle);
    if (coordinator == NULL){
        coordinator = csis_server_add_coordinator(con_handle);
        
        if (coordinator == NULL){
            csis_server_emit_coordinator_connected(con_handle, ERROR_CODE_CONNECTION_LIMIT_EXCEEDED);
            log_info("There are already %d coordinators connected. No memory for new coordinator.", csis_coordinators_max_num);
            return 0;
        } else {
            coordinator->con_handle = con_handle;
            csis_server_emit_coordinator_connected(con_handle, ERROR_CODE_SUCCESS);
        }    
    }

    if (attribute_handle == sirk_configuration_handle){
        coordinator->sirk_configuration = little_endian_read_16(buffer, 0);
        return 0;
    }

    if (attribute_handle == coordinated_set_size_configuration_handle){
        coordinator->coordinated_set_size_configuration = little_endian_read_16(buffer, 0);
        return 0;
    }
   
    if (attribute_handle == member_lock_configuration_handle){
        coordinator->coordinated_set_size_configuration = little_endian_read_16(buffer, 0);
        return 0;
    }

    if (attribute_handle == member_lock_handle){
        csis_member_lock_t lock = (csis_member_lock_t) buffer[0];
        if (csis_member_lock == lock){
            return CSIS_LOCK_ALREADY_GRANTED;
        }
        
        csis_coordinator_t * lock_owner = csis_get_lock_owner_coordinator();

        switch (lock){
            case CSIS_MEMBER_UNLOCKED:
                if ((lock_owner == NULL) || (lock_owner != coordinator)){
                    return CSIS_LOCK_RELEASE_NOT_ALLOWED;
                }
                csis_member_lock = lock;
                // coordinator wrote this value, and as long as it is owner, 
                // csis_server_set_callback will set notification to all
                // other coordinators
                csis_server_set_callback(CSIS_TASK_SEND_MEMBER_LOCK);
                coordinator->is_lock_owner = false;
                break;
            
            case CSIS_MEMBER_LOCKED:
                if (lock_owner != coordinator){
                    return CSIS_LOCK_DENIED;
                }
                csis_member_lock = lock;
                coordinator->is_lock_owner = true;
                csis_lock_timer_start(coordinator);
                csis_server_set_callback(CSIS_TASK_SEND_MEMBER_LOCK);
                break;

            default:
                return CSIS_INVALID_LOCK_VALUE;
        }
        return 0;
    }
    
    if (attribute_handle == coordinated_set_size_handle){
        csis_coordinated_set_size = buffer[0];
        csis_server_set_callback(CSIS_TASK_SEND_COORDINATED_SET_SIZE);
        return 0;
    }
    
    return 0;
}

static void csis_coordinator_reset(csis_coordinator_t * coordinator){
    if (csis_get_lock_owner_coordinator() == coordinator){
        csis_member_lock = CSIS_MEMBER_UNLOCKED;
        btstack_run_loop_remove_timer(&coordinator->lock_timer);
    }

    memset(coordinator, 0, sizeof(csis_coordinator_t));
    coordinator->con_handle = HCI_CON_HANDLE_INVALID;
}

static void coordinated_set_identification_service_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(packet);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }

    hci_con_handle_t con_handle;
    csis_coordinator_t * coordinator;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            
            coordinator = csis_get_coordinator_for_con_handle(con_handle);
            if (coordinator == NULL){
                break;
            }
            csis_coordinator_reset(coordinator);
            csis_server_emit_coordinator_disconnected(con_handle);
            break;
        default:
            break;
    }
}

void coordinated_set_identification_service_server_init(
    const uint8_t coordinators_num, csis_coordinator_t * coordinators, 
    uint8_t coordianted_set_size, uint8_t member_rank){
    
    btstack_assert(coordinators_num != 0);
    btstack_assert(coordinators     != NULL);
    btstack_assert(coordianted_set_size    != 0);
    btstack_assert(member_rank > 0);

     // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_COORDINATED_SET_IDENTIFICATION_SERVICE, &start_handle, &end_handle);
    btstack_assert(service_found != 0);
    UNUSED(service_found);

    // get characteristic value handle and client configuration handle
    sirk_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SET_IDENTITY_RESOLVING_KEY_CHARACTERISTIC);
    sirk_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SET_IDENTITY_RESOLVING_KEY_CHARACTERISTIC);

    coordinated_set_size_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SIZE_CHARACTERISTIC);
    coordinated_set_size_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SIZE_CHARACTERISTIC);
    
    member_lock_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_LOCK_CHARACTERISTIC);
    member_lock_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_LOCK_CHARACTERISTIC);

    member_rank_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_RANK_CHARACTERISTIC);
    
    log_info("Found SIRK service 0x%02x-0x%02x", start_handle, end_handle);

    csis_coordinated_set_size = coordianted_set_size;
    csis_member_rank = member_rank;
    csis_member_lock = CSIS_MEMBER_UNLOCKED;
    
    csis_coordinators_max_num = coordinators_num;
    csis_coordinators = coordinators;
    
    uint8_t i;
    for (i = 0; i < csis_coordinators_max_num; i++){
        csis_coordinator_reset(&csis_coordinators[i]);
    }
    // register service with ATT Server
    coordinated_set_identification_service.start_handle   = start_handle;
    coordinated_set_identification_service.end_handle     = end_handle;
    coordinated_set_identification_service.read_callback  = &coordinated_set_identification_service_read_callback;
    coordinated_set_identification_service.write_callback = &coordinated_set_identification_service_write_callback;
    coordinated_set_identification_service.packet_handler = coordinated_set_identification_service_packet_handler;
    att_server_register_service_handler(&coordinated_set_identification_service);
}

void coordinated_set_identification_service_server_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    csis_event_callback = callback;
}

uint8_t coordinated_set_identification_service_server_set_sirk(csis_sirk_type_t type, uint8_t * sirk){
    if (type >= CSIS_SIRK_TYPE_PROHIBITED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    csis_sirk_type = type;
    memcpy(csis_sirk, sirk, sizeof(csis_sirk));
    return ERROR_CODE_SUCCESS;
}

void coordinated_set_identification_service_server_deinit(void){
    csis_member_lock = CSIS_MEMBER_UNLOCKED;
    csis_event_callback = NULL;
}
