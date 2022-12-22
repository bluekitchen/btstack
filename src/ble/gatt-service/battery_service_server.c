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

#define BTSTACK_FILE__ "battery_service_server.c"

/**
 * Implementation of the GATT Battery Service Server 
 * To use with your application, add '#import <battery_service.gatt' to your .gatt file
 */

#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"


#include "ble/gatt-service/battery_service_server.h"

static btstack_context_callback_registration_t  battery_callback;
static att_service_handler_t       battery_service;

static uint8_t 	battery_value;
static uint16_t battery_value_client_configuration;
static hci_con_handle_t battery_value_client_configuration_connection;

static uint16_t battery_value_handle;
static uint16_t battery_value_client_configuration_handle;


static uint16_t battery_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	UNUSED(con_handle);

	if (attribute_handle == battery_value_handle){
		return att_read_callback_handle_byte(battery_value, offset, buffer, buffer_size);
	}
	if (attribute_handle == battery_value_client_configuration_handle){
		return att_read_callback_handle_little_endian_16(battery_value_client_configuration, offset, buffer, buffer_size);
	}
	return 0;
}

static int battery_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	UNUSED(transaction_mode);
	UNUSED(offset);
	UNUSED(buffer_size);

	if (attribute_handle == battery_value_client_configuration_handle){
		battery_value_client_configuration = little_endian_read_16(buffer, 0);
		battery_value_client_configuration_connection = con_handle;
	}
	return 0;
}

static void battery_service_can_send_now(void * context){
	hci_con_handle_t con_handle = (hci_con_handle_t) (uintptr_t) context;
	att_server_notify(con_handle, battery_value_handle, &battery_value, 1);
}

void battery_service_server_init(uint8_t value){

	battery_value = value;

	// get service handle range
	uint16_t start_handle = 0;
	uint16_t end_handle   = 0xffff;
	int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE, &start_handle, &end_handle);
	btstack_assert(service_found != 0);
	UNUSED(service_found);

	// get characteristic value handle and client configuration handle
	battery_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
	battery_value_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);

	// register service with ATT Server
	battery_service.start_handle   = start_handle;
	battery_service.end_handle     = end_handle;
	battery_service.read_callback  = &battery_service_read_callback;
	battery_service.write_callback = &battery_service_write_callback;
	att_server_register_service_handler(&battery_service);
}

void battery_service_server_set_battery_value(uint8_t value){
	battery_value = value;
	if (battery_value_client_configuration != 0){
		battery_callback.callback = &battery_service_can_send_now;
		battery_callback.context  = (void*) (uintptr_t) battery_value_client_configuration_connection;
		att_server_register_can_send_now_callback(&battery_callback, battery_value_client_configuration_connection);
	}
}
