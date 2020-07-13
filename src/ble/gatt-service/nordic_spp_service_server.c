/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "nordic_spp_service_server.c"

/**
 * Implementation of the Nordic SPP-like profile
 *
 * To use with your application, add '#import <nordic_spp_service.gatt' to your .gatt file
 * and call all functions below. All strings and blobs need to stay valid after calling the functions.
 */

#include <stdint.h>
#include <string.h>

#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"

#include "ble/gatt-service/nordic_spp_service_server.h"

//
static att_service_handler_t  nordic_spp_service;
static void (*client_callback)(hci_con_handle_t con_handle, const uint8_t * data, uint16_t size);

static uint16_t nordic_spp_rx_value_handle;
static uint16_t nordic_spp_tx_value_handle;
static uint16_t nordic_spp_tx_client_configuration_handle;
static uint16_t nordic_spp_tx_client_configuration_value;

static uint16_t nordic_spp_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	UNUSED(con_handle);
	UNUSED(offset);
	UNUSED(buffer_size);
	
	if (attribute_handle == nordic_spp_tx_client_configuration_handle){
		if (buffer){
			little_endian_store_16(buffer, 0, nordic_spp_tx_client_configuration_value);
		}
		return 2;
	}
	return 0;
}

static int nordic_spp_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	UNUSED(transaction_mode);
	UNUSED(offset);
	UNUSED(buffer_size);
	
	if (attribute_handle == nordic_spp_rx_value_handle){
		client_callback(con_handle, &buffer[0], buffer_size);
	}
	if (attribute_handle == nordic_spp_tx_client_configuration_handle){
		nordic_spp_tx_client_configuration_value = little_endian_read_16(buffer, 0);
		if (nordic_spp_tx_client_configuration_value){
			client_callback(con_handle, NULL, 0);
		}
	}

	return 0;
}

/**
 * @brief Init Nordic SPP Service Server with ATT DB
 * @param callback for tx data from peer
 */
void nordic_spp_service_server_init(void (*callback)(hci_con_handle_t con_handle, const uint8_t * data, uint16_t size)){

    static const uint8_t nordic_spp_profile_uuid128[] = { 0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E };
    static const uint8_t nordic_spp_rx_uuid128[] 	  = { 0x6E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E };
    static const uint8_t nordic_spp_tx_uuid128[] 	  = { 0x6E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E };

    client_callback = callback;

	// get service handle range
	uint16_t start_handle = 0;
	uint16_t end_handle   = 0xffff;
	int service_found = gatt_server_get_get_handle_range_for_service_with_uuid128(nordic_spp_profile_uuid128, &start_handle, &end_handle);
	if (!service_found) return;

	// get characteristic value handle and client configuration handle
	nordic_spp_rx_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(start_handle, end_handle, nordic_spp_rx_uuid128);
	nordic_spp_tx_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(start_handle, end_handle, nordic_spp_tx_uuid128);
	nordic_spp_tx_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(start_handle, end_handle, nordic_spp_tx_uuid128);

	log_info("nordic_spp_rx_value_handle 					0x%02x", nordic_spp_rx_value_handle);
	log_info("nordic_spp_tx_value_handle 					0x%02x", nordic_spp_tx_value_handle);
	log_info("nordic_spp_tx_client_configuration_handle 	0x%02x", nordic_spp_tx_client_configuration_handle);
	
	// register service with ATT Server
	nordic_spp_service.start_handle   = start_handle;
	nordic_spp_service.end_handle     = end_handle;
	nordic_spp_service.read_callback  = &nordic_spp_service_read_callback;
	nordic_spp_service.write_callback = &nordic_spp_service_write_callback;
	att_server_register_service_handler(&nordic_spp_service);
}

/** 
 * @brief Queue send request. When called, one packet can be send via nordic_spp_service_send below
 * @param request
 * @param con_handle
 */
void nordic_spp_service_server_request_can_send_now(btstack_context_callback_registration_t * request, hci_con_handle_t con_handle){
	att_server_request_to_send_notification(request, con_handle);
}

/**
 * @brief Send data
 * @param con_handle
 * @param data
 * @param size
 */
int nordic_spp_service_server_send(hci_con_handle_t con_handle, const uint8_t * data, uint16_t size){
	return att_server_notify(con_handle, nordic_spp_tx_value_handle, data, size);
}

