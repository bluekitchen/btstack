/*
 * Copyright (C) 2011-2013 by BlueKitchen GmbH
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
 * 4. This software may not be used in a commercial product
 *    without an explicit license granted by the copyright holder. 
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 */

//*****************************************************************************
//
// BLE Client
//
//*****************************************************************************


// NOTE: Supports only a single connection

#ifndef __BLE_CLIENT_H
#define __BLE_CLIENT_H

#include "btstack-config.h"

#include <btstack/utils.h>
#include "gatt_client.h"

#if defined __cplusplus
extern "C" {
#endif

#define LE_CENTRAL_MAX_INCLUDE_DEPTH 3


//*************** le central client

typedef struct ad_event {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} ad_event_t;

typedef enum{
    P_IDLE,
    P_W2_CONNECT,
    P_W4_CONNECTED,
    P_CONNECTED,
    P_W2_CANCEL_CONNECT,
    P_W4_CONNECT_CANCELLED,
    P_W2_DISCONNECT,
    P_W4_DISCONNECTED
} le_central_state_t;

typedef struct le_central{
    linked_item_t    item;
    le_central_state_t le_central_state;
    
    uint8_t   address_type;
    bd_addr_t address;
    uint16_t handle;
} le_central_t;

typedef struct le_central_connection_complete_event{
    uint8_t   type;
    le_central_t * device;
    uint8_t status;
} le_central_connection_complete_event_t;


//*************** gatt client



void ble_client_init();
void le_central_init();

void le_central_register_handler(void (*le_callback)(le_event_t * event));
void ble_client_register_packet_handler(void (*le_callback)(le_event_t * event));

le_command_status_t le_central_start_scan();
// creates one event per found peripheral device
// { type (8), addr_type (8), addr(48), rssi(8), ad_len(8), ad_data(ad_len*8) }
le_command_status_t le_central_stop_scan();

le_command_status_t  le_central_connect(le_central_t *context, uint8_t addr_type, bd_addr_t addr);
le_command_status_t  le_central_disconnect(le_central_t *context);


#if defined __cplusplus
}
#endif
#endif // __BLE_CLIENT_H
