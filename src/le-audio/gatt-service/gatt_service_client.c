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

#define BTSTACK_FILE__ "gatt_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>

#include "le-audio/gatt-service/gatt_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

static gatt_service_client_helper_t gatts_client;

static void gatt_service_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void gatt_service_client_run_for_connection(void * context);

// list of uuids
static const uint16_t gatts_uuid16s[GATT_SERVICE_NUM_CHARACTERISTICS] = {
    ORG_BLUETOOTH_CHARACTERISTIC_GATT_SERVICE_CHANGED
};

typedef enum {
    GATTS_CLIENT_CHARACTERISTIC_INDEX_GATT_SERVICE_CHANGED = 0,
    GATTS_CLIENT_CHARACTERISTIC_INDEX_RFU
} gatt_service_client_characteristic_index_t;

static void gatt_client_emit_service_changed(gatt_service_client_connection_helper_t * connection_helper, const uint8_t *data, uint16_t data_size){
    btstack_assert(connection_helper != NULL);
    btstack_assert(connection_helper->event_callback != NULL);
    btstack_assert(data_size == 4);

    uint8_t event[8];
    int pos = 0;
    event[pos++] = GATT_EVENT_SERVICE_CHANGED;
    event[pos++] = sizeof(event) - 2;
    little_endian_store_16(event, pos, connection_helper->con_handle);
    pos += 2;
    memcpy(&event[pos], data, data_size);
    pos += data_size;

    (*connection_helper->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void gatt_service_client_emit_indication(gatt_service_client_connection_helper_t * connection_helper, uint16_t value_handle, uint8_t att_status, const uint8_t * data, uint16_t data_size){
    uint16_t characteristic_uuid16 = gatt_service_client_helper_characteristic_uuid16_for_value_handle(&gatts_client, connection_helper, value_handle);
    
    switch (characteristic_uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_GATT_SERVICE_CHANGED:
            if (data_size == 4){
                gatt_client_emit_service_changed(connection_helper, data, data_size);
            }
            break;
        default:
            break;
    }
}
static void gatt_service_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    gatt_service_client_connection_helper_t * connection_helper;
    hci_con_handle_t con_handle;;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_INDICATION:
            con_handle = (hci_con_handle_t)gatt_event_indication_get_handle(packet);
            connection_helper = gatt_service_client_get_connection_for_con_handle(&gatts_client, con_handle);

            btstack_assert(connection_helper != NULL);

            gatt_service_client_emit_indication(connection_helper, gatt_event_indication_get_value_handle(packet), ATT_ERROR_SUCCESS,
                                         gatt_event_indication_get_value(packet),gatt_event_indication_get_value_length(packet));
            break;
        default:
            break;
    }
}

static void gatt_service_client_run_for_connection(void * context){
    UNUSED(context);
}

static void gatt_service_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    gatt_service_client_hci_event_handler(&gatts_client, packet_type, channel, packet, size);
}

void gatt_service_changed_client_init(void){
    gatt_service_client_init(&gatts_client, &gatt_service_client_packet_handler_trampoline);
    gatt_service_client_register_packet_handler(&gatts_client, &gatt_service_client_packet_handler_internal);

    gatts_client.characteristics_desc16_num = sizeof(gatts_uuid16s)/sizeof(uint16_t);
    gatts_client.characteristics_desc16 = gatts_uuid16s;
}

uint8_t gatt_service_changed_client_connect(
    hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    gatt_service_client_connection_t * connection){

    connection->gatt_query_can_send_now.callback = &gatt_service_client_run_for_connection;
    connection->gatt_query_can_send_now.context = (void *)(uintptr_t)connection->basic_connection.con_handle;
    connection->state = GATT_SERVICE_CLIENT_STATE_W4_CONNECTED;
    return gatt_service_client_connect(con_handle,
        &gatts_client, &connection->basic_connection,
        ORG_BLUETOOTH_SERVICE_GENERIC_ATTRIBUTE, 0,
         connection->characteristics_storage, GATT_SERVICE_NUM_CHARACTERISTICS, packet_handler, &connection->basic_connection.cid);
}

uint8_t gatt_service_changed_client_disconnect(gatt_service_client_connection_t * connection){
    if (connection == NULL){
        return ERROR_CODE_SUCCESS;
    }
    return gatt_service_client_disconnect(&gatts_client, connection->basic_connection.cid);
}

void gatt_service_changed_client_deinit(void){
    gatt_service_client_deinit(&gatts_client);
}

