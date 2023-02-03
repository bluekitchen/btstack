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

#define BTSTACK_FILE__ "media_control_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "le-audio/gatt-service/media_control_service_client.h"

#include "btstack_memory.h"
#include "ble/core.h"
#include "ble/gatt_client.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"

// MSC Client
static gatt_service_client_helper_t msc_service_client;

static void msc_service_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// list of uuids
static const gatt_service_client_characteristic_desc16_t mcs_characteristics_desc16[] = {
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_NAME,
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_URL,
    ORG_BLUETOOTH_CHARACTERISTIC_TRACK_CHANGED,
    ORG_BLUETOOTH_CHARACTERISTIC_TRACK_TITLE,
    ORG_BLUETOOTH_CHARACTERISTIC_TRACK_DURATION,
    ORG_BLUETOOTH_CHARACTERISTIC_TRACK_POSITION,
    ORG_BLUETOOTH_CHARACTERISTIC_PLAYBACK_SPEED,
    ORG_BLUETOOTH_CHARACTERISTIC_SEEKING_SPEED,
    ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_SEGMENTS_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_NEXT_TRACK_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_PARENT_GROUP_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_GROUP_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDER,
    ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDERS_SUPPORTED,
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_STATE,
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT,
    ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED,
    ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_RESULTS_OBJECT_ID,
    ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_CONTROL_POINT,
    ORG_BLUETOOTH_CHARACTERISTIC_CONTENT_CONTROL_ID,
};


static void msc_service_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void msc_service_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    hci_con_handle_t con_handle;
    uint8_t status;
    gatt_service_client_connection_helper_t * connection;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
            connection = gatt_service_client_get_connection_for_cid(&msc_service_client, gattservice_subevent_client_connected_get_cid(packet));
            btstack_assert(connection != NULL);
            msc_service_client_replace_subevent_id_and_emit(connection->event_callback, packet, size, GATTSERVICE_SUBEVENT_MCS_CLIENT_CONNECTED);
            break;

        case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
            connection = gatt_service_client_get_connection_for_cid(&msc_service_client, gattservice_subevent_client_disconnected_get_cid(packet));
            btstack_assert(connection != NULL);
            msc_service_client_replace_subevent_id_and_emit(connection->event_callback, packet, size, GATTSERVICE_SUBEVENT_MCS_CLIENT_DISCONNECTED);
            break;

        default:
            break;
    }
}


static void mcs_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_hci_event_handler(&msc_service_client, packet_type, channel, packet, size);
}

uint8_t media_control_service_client_connect(hci_con_handle_t con_handle, mcs_client_connection_t * connection, 
    gatt_service_client_characteristic_t * characteristics, uint8_t characteristics_num,
    btstack_packet_handler_t packet_handler, uint16_t * mcs_cid){

    btstack_assert(msc_service_client.characteristics_desc16_num > 0);
    return gatt_service_client_connect(con_handle,
        &msc_service_client, &connection->basic_connection, 
        characteristics, characteristics_num, packet_handler, mcs_cid);
}

uint8_t media_control_service_client_disconnect(uint16_t mcs_cid){
    return gatt_service_client_disconnect(&msc_service_client, mcs_cid);
}

void media_control_service_client_init(void){
    gatt_service_client_init(&msc_service_client, &mcs_client_packet_handler_trampoline);
    gatt_service_client_register_packet_handler(&msc_service_client, &msc_service_client_packet_handler_internal);

    msc_service_client.service_uuid16      = ORG_BLUETOOTH_SERVICE_MEDIA_CONTROL_SERVICE;

    msc_service_client.characteristics_desc16_num = sizeof(mcs_characteristics_desc16)/sizeof(gatt_service_client_characteristic_desc16_t);
    msc_service_client.characteristics_desc16 = mcs_characteristics_desc16;


}

void media_control_service_client_deinit(void){
    gatt_service_client_deinit(&msc_service_client);
}

