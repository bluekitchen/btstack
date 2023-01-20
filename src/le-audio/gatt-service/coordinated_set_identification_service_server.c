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

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "ble/sm.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"
#include "btstack_crypto.h"

#include "le-audio/le_audio_util.h"
#include "le-audio/gatt-service/coordinated_set_identification_service_server.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#define CSIS_TASK_SEND_SIRK                         0x01
#define CSIS_TASK_SEND_COORDINATED_SET_SIZE         0x02
#define CSIS_TASK_SEND_MEMBER_LOCK                  0x04
#define CSIS_TASK_SEND_MEMBER_RANK                  0x08


static att_service_handler_t    coordinated_set_identification_service;
static btstack_packet_handler_t csis_event_callback;
static csis_coordinator_t     * csis_coordinators;
static uint8_t                  csis_coordinators_max_num;

// csis server data
static uint16_t sirk_handle;
static uint16_t sirk_configuration_handle;

static bool              csis_sirk_exposed_via_oob;
static uint8_t           csis_sirk[16];
static csis_sirk_type_t  csis_sirk_type;

static bool    csis_rsi_calculation_ongoing = true;
static uint8_t csis_prand_data[3];
static uint8_t csis_hash[16];

static uint8_t  csis_coordinated_set_size;
static uint16_t coordinated_set_size_handle;
static uint16_t coordinated_set_size_configuration_handle;

static csis_member_lock_t   csis_member_lock;

static uint16_t member_lock_handle;
static uint16_t member_lock_configuration_handle;

static uint8_t  csis_coordinated_set_member_rank;
static uint16_t coordinated_set_member_rank_handle;

static btstack_crypto_random_t random_request;
static btstack_crypto_aes128_t aes128_request;
static btstack_crypto_aes128_cmac_t aes128_cmac_request;
static uint8_t  s1[16];
static uint8_t   T[16];
static uint8_t  k1[16];
const static uint8_t s1_string[] = { 'S', 'I', 'R', 'K', 'e', 'n', 'c'};

static uint8_t key_ltk[16];

static csis_coordinator_t * active_coordinator;

static void csis_server_trigger_next_sirk_calculation(void);

static void csis_server_emit_rsi(const uint8_t * rsi){
    btstack_assert(csis_event_callback != NULL);

    uint8_t event[9];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_RSI;
    reverse_48(rsi, &event[pos]);
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_server_handle_csis_hash(void * arg){
    UNUSED(arg);
    uint8_t rsi[6];
    memcpy(&rsi[0], csis_prand_data, 3);
    memcpy(&rsi[3], &csis_hash[13] , 3);
    
    csis_server_emit_rsi(rsi);
    csis_rsi_calculation_ongoing = false;
}

static void csis_handle_prand_provisioner(void * arg){
    UNUSED(arg);

    // CSIS 4.8: The two most significant bits (MSBs) of prand shall be equal to 0 and 1
    csis_prand_data[0] &= 0x7F;
    csis_prand_data[0] |= 0x40;

    // TEST data
    // csis_prand_data[0] = 0x69;
    // csis_prand_data[1] = 0xf5;
    // csis_prand_data[2] = 0x63;
    
    uint8_t prand_prime[16];
    memset(prand_prime, 0, 16);
    memcpy(&prand_prime[13], csis_prand_data, 3);

    btstack_crypto_aes128_encrypt(&aes128_request, csis_sirk, prand_prime, csis_hash, &csis_server_handle_csis_hash, NULL);
}

uint8_t coordinated_set_identification_service_server_generate_rsi(void){
    if (csis_rsi_calculation_ongoing){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    csis_rsi_calculation_ongoing = true;
    btstack_crypto_random_generate(&random_request, csis_prand_data, 3, &csis_handle_prand_provisioner, NULL);
    return ERROR_CODE_SUCCESS;
}

static csis_coordinator_t * csis_server_get_next_coordinator_for_sirk_calculation(void){
    uint8_t i;
    for (i = 0; i < csis_coordinators_max_num; i++){
        if (csis_coordinators[i].con_handle == HCI_CON_HANDLE_INVALID){
            continue;
        }
        if (csis_coordinators[i].encrypted_sirk_state == CSIS_SIRK_CALCULATION_W2_START){
            return &csis_coordinators[i];
        }
    }
    return NULL;
}

static void csis_server_handle_k1(void * context){
    if (active_coordinator == NULL){
        return;
    }

    uint8_t i;
    for(i = 0; i < sizeof(k1); i++){
        active_coordinator->encrypted_sirk[i] = k1[i] ^ csis_sirk[i];
    }

    active_coordinator->encrypted_sirk_state = CSIS_SIRK_CALCULATION_STATE_READY;
    (void)att_server_response_ready(active_coordinator->con_handle);
    active_coordinator = NULL;

    csis_server_trigger_next_sirk_calculation();
}

static void csis_server_handle_T(void * context){
    if (active_coordinator == NULL){
        return;
    }
    const static uint8_t csis_string[] = { 'c', 's', 'i', 's'};
    btstack_crypto_aes128_cmac_message(&aes128_cmac_request, T, sizeof(csis_string), csis_string, k1, csis_server_handle_k1, NULL);
}

static void csis_server_handle_s1(void * context){
    if (active_coordinator == NULL){
        return;
    }

    sm_get_ltk(active_coordinator->con_handle, key_ltk);
    btstack_crypto_aes128_cmac_message(&aes128_cmac_request, s1, sizeof(key_ltk), key_ltk, T, csis_server_handle_T, NULL);
}

static void csis_server_trigger_next_sirk_calculation(void){
    if (active_coordinator != NULL){
        return;
    }

    active_coordinator = csis_server_get_next_coordinator_for_sirk_calculation();
    if (active_coordinator == NULL){
        return;
    }

    active_coordinator->encrypted_sirk_state = CSIS_SIRK_CALCULATION_ACTIVE;
    btstack_crypto_aes128_cmac_zero(&aes128_cmac_request, sizeof(s1_string), s1_string, s1, &csis_server_handle_s1, NULL);
}

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

static bool csis_coordinator_bonded(csis_coordinator_t * coordinator){
    return sm_le_device_index(coordinator->con_handle) != -1; 
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

static void csis_server_emit_lock(hci_con_handle_t con_handle){
    btstack_assert(csis_event_callback != NULL);

    uint8_t event[6];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_COORDINATED_SET_MEMBER_LOCK;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = (uint8_t)csis_member_lock;
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_server_emit_set_size(hci_con_handle_t con_handle){
    btstack_assert(csis_event_callback != NULL);

    uint8_t event[6];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_COORDINATED_SET_SIZE;
    little_endian_store_16(event, pos, con_handle);
    event[pos++] = csis_coordinated_set_member_rank;
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
        if (csis_sirk_exposed_via_oob){
            return ATT_READ_ERROR_CODE_OFFSET + CSIS_OOB_SIRK_ONLY;
        }

        uint8_t value[17];
        value[0] = (uint8_t)csis_sirk_type;

        switch (csis_sirk_type){
            case CSIS_SIRK_TYPE_ENCRYPTED:
                switch (coordinator->encrypted_sirk_state){
                    case CSIS_SIRK_CALCULATION_STATE_READY:
                        reverse_128(coordinator->encrypted_sirk, &value[1]);
                        break;
                    default:
                        return ATT_READ_RESPONSE_PENDING;
                }
                break;
            default:
                reverse_128(csis_sirk, &value[1]);
                break;
        }
        return att_read_callback_handle_blob(value, sizeof(value), offset, buffer, buffer_size);
    }
    
    if (attribute_handle == ATT_READ_RESPONSE_PENDING){
        switch (coordinator->encrypted_sirk_state){
            case CSIS_SIRK_CALCULATION_ACTIVE:
                return 0;
            default:
                coordinator->encrypted_sirk_state = CSIS_SIRK_CALCULATION_W2_START;
                break;
        }
        csis_server_trigger_next_sirk_calculation();
        return 0;
    }

    if (attribute_handle == coordinated_set_size_handle){
        return att_read_callback_handle_byte(csis_coordinated_set_size, offset, buffer, buffer_size);
    }
   
    if (attribute_handle == member_lock_handle){
        return att_read_callback_handle_byte((uint8_t)csis_member_lock, offset, buffer, buffer_size);
    }

    if (attribute_handle == coordinated_set_member_rank_handle){
        return att_read_callback_handle_byte(csis_coordinated_set_member_rank, offset, buffer, buffer_size);
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
    
    } else if ((coordinator->scheduled_tasks & CSIS_TASK_SEND_COORDINATED_SET_SIZE) != 0) {
        coordinator->scheduled_tasks &= ~CSIS_TASK_SEND_COORDINATED_SET_SIZE;
        uint8_t value[1];
        value[0] = csis_coordinated_set_size;
        att_server_notify(coordinator->con_handle, member_lock_handle, &value[0], sizeof(value));
    
    } else if ((coordinator->scheduled_tasks & CSIS_TASK_SEND_MEMBER_RANK) != 0) {
        coordinator->scheduled_tasks &= ~CSIS_TASK_SEND_MEMBER_RANK;
        uint8_t value[1];
        value[0] = csis_coordinated_set_member_rank;
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
    csis_server_emit_lock(coordinator->con_handle); 
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

static uint8_t csis_set_member_lock(csis_coordinator_t * coordinator, csis_member_lock_t lock){
    csis_coordinator_t * lock_owner = csis_get_lock_owner_coordinator();

    if ( (lock_owner != NULL) && (lock_owner != coordinator)){
        if (lock == CSIS_MEMBER_LOCKED){
            return CSIS_LOCK_DENIED;
        }
        return CSIS_LOCK_RELEASE_NOT_ALLOWED;
    }

    if (lock >= CSIS_MEMBER_PROHIBITED){
        return CSIS_INVALID_LOCK_VALUE;
    }    

    if (csis_member_lock == lock){
       return CSIS_LOCK_ALREADY_GRANTED;
    }

    csis_member_lock = lock;
    coordinator->is_lock_owner = true;
    csis_server_emit_lock(coordinator->con_handle);
        
    if (lock == CSIS_MEMBER_LOCKED){
        csis_lock_timer_start(coordinator);
    }
    csis_server_set_callback(CSIS_TASK_SEND_MEMBER_LOCK);
    return ERROR_CODE_SUCCESS;
}

static int coordinated_set_identification_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
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
        if ((coordinator->sirk_configuration != 0) && csis_coordinator_bonded(coordinator)) {
            if (!csis_sirk_exposed_via_oob){
                csis_server_set_callback(CSIS_TASK_SEND_SIRK);
            }
        }
        return 0;
    }

    if (attribute_handle == coordinated_set_size_configuration_handle){
        coordinator->coordinated_set_size_configuration = little_endian_read_16(buffer, 0);
        if ((coordinator->coordinated_set_size_configuration != 0) && csis_coordinator_bonded(coordinator)) {
            csis_server_set_callback(CSIS_TASK_SEND_COORDINATED_SET_SIZE);
        }
        return 0;
    }
   
    if (attribute_handle == member_lock_configuration_handle){
        coordinator->member_lock_configuration = little_endian_read_16(buffer, 0);
        if ((coordinator->member_lock_configuration != 0) && csis_coordinator_bonded(coordinator)) {
            csis_server_set_callback(CSIS_TASK_SEND_MEMBER_LOCK);
        }
        return 0;
    }

    if (attribute_handle == member_lock_handle){
        uint8_t status = csis_set_member_lock(coordinator, (csis_member_lock_t) buffer[0]);

        if (status == ERROR_CODE_SUCCESS){
            csis_server_set_callback(CSIS_TASK_SEND_MEMBER_LOCK);
        }
        return status;
    }
    
    if (attribute_handle == coordinated_set_size_handle){
        csis_coordinated_set_size = buffer[0];
        csis_server_emit_set_size(coordinator->con_handle);
        csis_server_set_callback(CSIS_TASK_SEND_COORDINATED_SET_SIZE);
        return 0;
    }

    return 0;
}

static void csis_coordinator_reset(csis_coordinator_t * coordinator){
    if (!csis_coordinator_bonded(coordinator) && csis_get_lock_owner_coordinator() == coordinator){
        csis_member_lock = CSIS_MEMBER_UNLOCKED;
        csis_server_emit_lock(coordinator->con_handle);
        btstack_run_loop_remove_timer(&coordinator->lock_timer);
        memset(coordinator, 0, sizeof(csis_coordinator_t));
    }
    if (active_coordinator == coordinator){
        active_coordinator = NULL;
    }
    coordinator->encrypted_sirk_state = CSIS_SIRK_CALCULATION_STATE_IDLE;
    coordinator->scheduled_tasks = 0;
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
    uint8_t coordianted_set_size, uint8_t coordinated_set_member_rank){
    
    btstack_assert(coordinators_num != 0);
    btstack_assert(coordinators     != NULL);
    btstack_assert(coordianted_set_size    != 0);
    btstack_assert(coordinated_set_member_rank > 0);

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

    coordinated_set_member_rank_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_RANK_CHARACTERISTIC);
    
    log_info("Found CSIS service 0x%02x-0x%02x", start_handle, end_handle);

#ifdef ENABLE_TESTING_SUPPORT
    printf("Found CSIS service 0x%02x-0x%02x\n", start_handle, end_handle);
    printf("sirk_handle             0x%02x\n", sirk_handle);
    printf("sirk_handle CCC         0x%02x\n", sirk_configuration_handle);
    printf("set_size_handle         0x%02x\n", coordinated_set_size_handle);
    printf("set_size_handle CCC     0x%02x\n", coordinated_set_size_configuration_handle);
    printf("member_lock_handle      0x%02x\n", member_lock_handle);
    printf("member_lock_handle CCC  0x%02x\n", member_lock_configuration_handle);
    printf("coordinated_set_member_rank_handle      0x%02x\n", coordinated_set_member_rank_handle); 
#endif

    csis_rsi_calculation_ongoing = false;
    memset(csis_sirk, 0, sizeof(csis_sirk));

    csis_coordinated_set_size = coordianted_set_size;
    csis_coordinated_set_member_rank = coordinated_set_member_rank;
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

void coordinated_set_identification_service_server_register_packet_handler(btstack_packet_handler_t packet_handler){
    btstack_assert(packet_handler != NULL);
    csis_event_callback = packet_handler;
}

void coordinated_set_identification_service_server_set_sirk(csis_sirk_type_t sirk_type, const uint8_t * sirk, bool exposed_via_oob){
    btstack_assert(sirk != NULL);
    btstack_assert(sirk_type < CSIS_SIRK_TYPE_PROHIBITED);
    
    csis_sirk_exposed_via_oob = exposed_via_oob;
    csis_sirk_type = sirk_type;
    memcpy(csis_sirk, sirk, sizeof(csis_sirk));
    csis_server_set_callback(CSIS_TASK_SEND_SIRK);
}

void coordinated_set_identification_service_server_set_size(uint8_t coordinated_set_size){
    btstack_assert(coordinated_set_size > 0u);
    csis_coordinated_set_size = coordinated_set_size;
    csis_server_set_callback(CSIS_TASK_SEND_COORDINATED_SET_SIZE);
}

void coordinated_set_identification_service_server_set_rank(uint8_t coordinated_set_member_rank){
    btstack_assert(coordinated_set_member_rank > 0u);
    csis_coordinated_set_member_rank = coordinated_set_member_rank;
    csis_server_set_callback(CSIS_TASK_SEND_MEMBER_RANK);
}

uint8_t coordinated_set_identification_service_server_simulate_set_lock(hci_con_handle_t con_handle, csis_member_lock_t coordinated_set_member_lock){
    csis_coordinator_t * coordinator = csis_get_coordinator_for_con_handle(con_handle);
    if (coordinator == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    return csis_set_member_lock(coordinator, coordinated_set_member_lock);
}

void coordinated_set_identification_service_server_deinit(void){
    memset(csis_sirk, 0, sizeof(csis_sirk));
    csis_member_lock = CSIS_MEMBER_UNLOCKED;
    active_coordinator = NULL;
    csis_event_callback = NULL;
    csis_rsi_calculation_ongoing = false;
}

uint8_t coordinated_set_identification_service_server_simulate_member_connected(hci_con_handle_t con_handle){
    csis_coordinator_t * coordinator = csis_get_coordinator_for_con_handle(con_handle);
    if (coordinator != NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    } 
    
    coordinator = csis_server_add_coordinator(con_handle);
    if (coordinator == NULL){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }
    return ERROR_CODE_SUCCESS;
}

