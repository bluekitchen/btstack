/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "tx_power_service_server.c"


/**
 * Implementation of the GATT TX Power Service Server 
 * To use with your application, add '#import <tx_power_service.gatt' to your .gatt file
 */

#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"


#include "ble/gatt-service/tx_power_service_server.h"

static att_service_handler_t       tx_power_service_handler;

static int8_t 	tx_power_level_value;
static uint16_t tx_power_level_value_handle;


static uint16_t tx_power_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	UNUSED(con_handle);

	if (attribute_handle == tx_power_level_value_handle){
		return att_read_callback_handle_byte((uint8_t)tx_power_level_value, offset, buffer, buffer_size);
	}
	return 0;
}

void tx_power_service_server_init(int8_t tx_power_level){
	tx_power_level_value = tx_power_level;

	// get service handle range
	uint16_t start_handle = 0;
	uint16_t end_handle   = 0xfff;
	bool service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_TX_POWER, &start_handle, &end_handle);
	btstack_assert(service_found);
	UNUSED(service_found);

	// get characteristic value handle and client configuration handle
	tx_power_level_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_TX_POWER_LEVEL);

	// register service with ATT Server
	tx_power_service_handler.start_handle   = start_handle;
	tx_power_service_handler.end_handle     = end_handle;
	tx_power_service_handler.read_callback  = &tx_power_service_read_callback;
	tx_power_service_handler.write_callback = NULL;
	att_server_register_service_handler(&tx_power_service_handler);
}

void tx_power_service_server_set_level(int8_t tx_power_level){
	tx_power_level_value = tx_power_level;
}
