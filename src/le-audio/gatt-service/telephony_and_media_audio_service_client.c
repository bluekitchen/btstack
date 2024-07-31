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

#define BTSTACK_FILE__ "telephony_and_media_audio_service_client_read.c"

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
#include "le-audio/gatt-service/telephony_and_media_audio_service_client.h"
#include "bluetooth_data_types.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

typedef enum {
    TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_READY = 0,
    TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE,
    TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_READ_FAILED,
    TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT,
} telephony_and_media_audio_service_client_state_t;

static telephony_and_media_audio_service_client_state_t tmas_client_state;
static btstack_packet_handler_t tmas_client_event_callback;
static hci_con_handle_t tmas_client_con_handle;
static btstack_context_callback_registration_t tmas_client_handle_can_send_now;
static uint16_t tmap_supported_roles;


static void tmas_client_emit_supported_roles(hci_con_handle_t con_handle, uint8_t status){
    UNUSED(con_handle);
    btstack_assert(tmas_client_event_callback != NULL);

    uint8_t event[10];
    int pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_TMAS_CLIENT_SUPPORTED_ROLES_BITMAP;
    little_endian_store_16(event, pos, tmas_client_con_handle);
    pos += 2;
    little_endian_store_16(event, pos, tmap_supported_roles);
    pos += 2;
    event[pos++] = status;
    (*tmas_client_event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void tmas_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            if (gatt_event_characteristic_value_query_result_get_value_length(packet) != 2){
                tmas_client_state = TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_READ_FAILED;
                break;
            }
            tmap_supported_roles = little_endian_read_16(gatt_event_characteristic_value_query_result_get_value(packet), 0) & TMAS_ROLE_BITMASK_ALL;
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            status = gatt_event_query_complete_get_att_status(packet);
            
            switch (tmas_client_state){
                case TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_READ_FAILED:
                    tmas_client_state = TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_READY;
                    tmas_client_emit_supported_roles(tmas_client_con_handle, ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE);
                    break;

                case TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT:
                    tmas_client_state = TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_READY;
                    tmas_client_emit_supported_roles(tmas_client_con_handle, status);
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }
}


static void tmas_client_run_for_connection(void * context){
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t)context;

    switch (tmas_client_state){
        case TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            tmas_client_state = TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT;
            (void) gatt_client_read_value_of_characteristics_by_uuid16(
                    &tmas_client_handle_gatt_client_event, con_handle, 1, 0xFFFF,
                    ORG_BLUETOOTH_CHARACTERISTIC_TMAP_ROLE);
            break;
        default:
            break;
    }
}

void telephony_and_media_audio_service_client_init(void){
    tmas_client_state = TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_READY;
    tmas_client_event_callback = NULL;
    tmas_client_con_handle = HCI_CON_HANDLE_INVALID;
    tmas_client_handle_can_send_now.callback = &tmas_client_run_for_connection;
}

uint8_t telephony_and_media_audio_service_client_read_roles(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle){
    btstack_assert(packet_handler != NULL);

    if (tmas_client_state != TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_REPEATED_ATTEMPTS;
    }
    tmas_client_event_callback = packet_handler;
    tmas_client_con_handle = con_handle;
    tmap_supported_roles = 0;
    
    tmas_client_state = TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    
    tmas_client_handle_can_send_now.context = (void *)(uintptr_t)con_handle;
    uint8_t status = gatt_client_request_to_send_gatt_query(&tmas_client_handle_can_send_now, con_handle);
    if (status != ERROR_CODE_SUCCESS){
        tmas_client_state = TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_READY;
    } 
    return status;
}

void telephony_and_media_audio_service_client_deinit(void){
    tmas_client_state = TELEPHONY_AND_MEDIA_AUDIO_SERVICE_CLIENT_STATE_READY;
    tmas_client_event_callback = NULL;
    tmas_client_con_handle = HCI_CON_HANDLE_INVALID;
}
