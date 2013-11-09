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


typedef struct gatt_client_event {
    uint8_t   type;
} gatt_client_event_t;

typedef struct ad_event {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} ad_event_t;


void (*gatt_client_callback)(gatt_client_event_t * event);

void gatt_client_init();
void gatt_client_start_scan();
// creates one event per found peripheral device
// EVENT: type (8), addr_type (8), addr(48), rssi(8), ad_len(8), ad_data(ad_len*8)
void gatt_client_stop_scan();


/*

typedef struct service_uuid{
    uint8_t lenght;
    uint8_t * uuid;
} service_uuid_t;

typedef struct peripheral{

} peripheral_t;



// GATT Client API

void gatt_client_register_handler( btstack_packet_handler_t handler);

uint16_t gatt_client_connect(bt_addr_t *dev);
void gatt_client_cancel_connect(peripheral_id peripheral);

void get_services_for_peripheral(peripheral_id peripheral);
// EVENT: type (8), peripheral_id (16), service_id 

void get_characteristics_for_service(peripheral_id, service_id);
// EVENT: type (8), peripheral_id (16), service_id (16), ...
*/


#if defined __cplusplus
}
#endif