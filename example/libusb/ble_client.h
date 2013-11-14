/*
 * Copyright (C) 2011-2013 by Matthias Ringwald
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

#pragma once

#include "config.h"

#include <btstack/utils.h>

#if defined __cplusplus
extern "C" {
#endif


typedef struct le_central_event {
    uint8_t   type;
} le_central_event_t;

typedef struct ad_event {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} ad_event_t;

typedef enum {
    P_IDLE,
    P_W2_CONNECT,
    P_W4_CONNECTED,
    P_CONNECTED
} peripheral_state_t;

typedef struct le_peripheral{
    linked_item_t    item;

    peripheral_state_t state;
    uint8_t   address_type;
    bd_addr_t address;
    uint16_t handle;
} le_peripheral_t;

typedef struct le_characteristic{
    uint8_t  permision;
    uint16_t characteristic_handle;
    uint8_t  has_uuid128;
    uint16_t uuid16;
    uint8_t  uuid128[16];
} le_characteristic_t;


void le_central_init();
void le_central_register_handler(void (*le_callback)(le_central_event_t * event));

void le_central_start_scan();
// creates one event per found peripheral device
// { type (8), addr_type (8), addr(48), rssi(8), ad_len(8), ad_data(ad_len*8) }
void le_central_stop_scan();

void le_central_connect(le_peripheral_t *context, uint8_t addr_type, bd_addr_t addr);
void le_central_cancel_connect(le_peripheral_t *context);

void le_central_get_services(le_peripheral_t *context);
void le_central_get_services_with_uuid16(le_peripheral_t *context,  uint16_t  uuid16);
void le_central_get_services_with_uuid128(le_peripheral_t *context, uint8_t * uuid128);
// { type (8), le_peripheral_t *context, service_handle }

void le_central_get_characteristics_for_service(le_peripheral_t *context, uint16_t service_handle);
void le_central_get_characteristics_for_service_with_uuid16(le_peripheral_t *context, uint16_t service_handle, uint16_t  uuid16);
void le_central_get_characteristics_for_service_with_uuid128(le_peripheral_t *context, uint16_t service_handle, uint8_t * uuid128);
// { type (8), le_peripheral_t *context, service_handle, le_characteristic *}

void le_central_read_value_of_characteristic(le_peripheral_t *context, uint16_t characteristic_handle);
void le_central_write_value_of_characteristic(le_peripheral_t *context, uint16_t characteristic_handle, int length, uint8_t * data);
void le_central_subscribe_to_characteristic(le_peripheral_t *context, uint16_t characteristic_handle);
void le_central_unsubscribe_from_characteristic(le_peripheral_t *context, uint16_t characteristic_handle);
// { read/write/subscribe/unsubscribe confirm/result}

// { type, le_peripheral *, characteristic handle, int len, uint8_t data[]?}

#if defined __cplusplus
}
#endif