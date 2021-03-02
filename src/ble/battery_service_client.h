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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#ifndef BATTERY_SERVICE_CLIENT_H
#define BATTERY_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"


#if defined __cplusplus
extern "C" {
#endif

#ifndef MAX_NUM_BATTERY_SERVICES
#define MAX_NUM_BATTERY_SERVICES 3
#endif

/* API_START */

typedef enum {
    BATTERY_SERVICE_IDLE,
    BATTERY_SERVICE_W2_QUERY_SERVICE,
    BATTERY_SERVICE_W4_SERVICE_RESULT,
    BATTERY_SERVICE_W4_CHARACTERISTIC_RESULT,
    BATTERY_SERVICE_W4_BATTERY_DATA
} battery_service_state_t;

typedef struct {
    btstack_linked_item_t item;
    
    hci_con_handle_t  con_handle;
    uint16_t          cid;
    battery_service_state_t  state;
    btstack_packet_handler_t client_handler;

    uint8_t battery_services_index;
    uint8_t num_battery_services;
    gatt_client_service_t services[MAX_NUM_BATTERY_SERVICES];
} battery_service_client_t;

void battery_service_client_init(void);
void battery_service_client_deinit(void);

uint8_t battery_service_client_connect(hci_con_handle_t con_handle, btstack_packet_handler_t callback, uint16_t * battery_service_cid);
uint8_t battery_service_client_disconnect(uint16_t battery_service_cid);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
