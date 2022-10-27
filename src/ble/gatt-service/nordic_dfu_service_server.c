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

#define BTSTACK_FILE__ "nordic_dfu_service_server.c"

/**
 * Implementation of the Nordic DFU profile
 *
 * To use with your application, add '#import <nordic_dfu_service.gatt' to your .gatt file
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

#include "ble/gatt-service/nordic_dfu_service_server.h"

typedef struct {
	uint16_t attribute_handle;
	uint16_t attribute_cccd_value;
	hci_con_handle_t con_handle;
	uint8_t *data;
	uint32_t size;
} nordic_dfu_request_context_t;


static btstack_packet_handler_t client_packet_handler;
static att_service_handler_t  nordic_dfu_service;
static btstack_context_callback_registration_t send_request;
static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;

static uint16_t dfu_ctrl_point_value_handle;
static uint16_t dfu_data_point_value_handle;
static uint16_t dfu_buttonless_value_handle;
static uint16_t dfu_ctrl_point_cccd_handle;
static uint16_t dfu_buttonless_cccd_handle;
static uint16_t dfu_ctrl_point_cccd_value;
static uint16_t dfu_buttonless_cccd_value;

static uint8_t  dfu_ctrl_point_value[32]  = {0};
static uint8_t  dfu_data_point_value[256] = {0};
static uint8_t  dfu_buttonless_value[16]  = {0};

static void nordic_dfu_service_emit_state(hci_con_handle_t con_handle, bool enabled){
	uint8_t event[5];
	uint8_t pos = 0;
	event[pos++] = HCI_EVENT_GATTSERVICE_META;
	event[pos++] = sizeof(event) - 2;
	event[pos++] = enabled ? GATTSERVICE_SUBEVENT_SPP_SERVICE_CONNECTED : GATTSERVICE_SUBEVENT_SPP_SERVICE_DISCONNECTED;
	little_endian_store_16(event,pos, (uint16_t) con_handle);
	pos += 2;
	(*client_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

static void nordic_dfu_service_notify_request_cb(void *context) {
	if (NULL == context)
		return;

	nordic_dfu_request_context_t *req_context = (nordic_dfu_request_context_t *)context;
	if (req_context->attribute_cccd_value & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION)
		att_server_notify(req_context->con_handle, req_context->attribute_handle, req_context->data, req_context->size);
}

static void nordic_dfu_service_indicate_request_cb(void *context) {
	if (NULL == context)
		return;

	nordic_dfu_request_context_t *req_context = (nordic_dfu_request_context_t *)context;
	if (req_context->attribute_cccd_value & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION)
		att_server_indicate(req_context->con_handle, req_context->attribute_handle, req_context->data, req_context->size);
}

static uint16_t nordic_dfu_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	UNUSED(con_handle);
	UNUSED(offset);
	UNUSED(buffer_size);

	if (attribute_handle == dfu_ctrl_point_cccd_handle){
		if (buffer != NULL){
			little_endian_store_16(buffer, 0, dfu_ctrl_point_cccd_value);
		}
		return 2;
	} else if (attribute_handle == dfu_buttonless_cccd_handle) {
		if (buffer != NULL){
			little_endian_store_16(buffer, 0, dfu_buttonless_cccd_value);
		}
		return 2;
	}

	return 0;
}

static int nordic_dfu_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	UNUSED(transaction_mode);
	UNUSED(offset);
	UNUSED(buffer_size);


	if (attribute_handle == dfu_ctrl_point_value_handle) {

	} else if (attribute_handle == dfu_ctrl_point_cccd_handle) {
		dfu_ctrl_point_cccd_value = little_endian_read_16(buffer, 0);
	} else if (attribute_handle == dfu_data_point_value_handle) {
		(*client_packet_handler)(RFCOMM_DATA_PACKET, (uint16_t) con_handle, buffer, buffer_size);
	} else if (attribute_handle == dfu_buttonless_value_handle) {

	} else if (attribute_handle == dfu_buttonless_cccd_handle) {
		dfu_buttonless_cccd_value = little_endian_read_16(buffer, 0);
	}

	return 0;
}

/**
 * @brief Init Nordic DFU Service Server with ATT DB
 * @param callback for events and data from dfu controller
 */
void nordic_dfu_service_server_init(btstack_packet_handler_t packet_handler) {
	static const uint16_t dfu_profile_uuid16      = 0xFE59;
	static const uint8_t dfu_ctrl_point_uuid128[] = { 0x8E, 0xC9, 0x00, 0x01, 0xF3, 0x15, 0x4F, 0x60, 0x9F, 0xB8, 0x83, 0x88, 0x30, 0xDA, 0xEA, 0x50 };
	static const uint8_t dfU_data_point_uuid128[] = { 0x8E, 0xC9, 0x00, 0x02, 0xF3, 0x15, 0x4F, 0x60, 0x9F, 0xB8, 0x83, 0x88, 0x30, 0xDA, 0xEA, 0x50 };
	static const uint8_t dfu_buttonless_uuid128[] = { 0x8E, 0xC9, 0x00, 0x03, 0xF3, 0x15, 0x4F, 0x60, 0x9F, 0xB8, 0x83, 0x88, 0x30, 0xDA, 0xEA, 0x50 };

	client_packet_handler = packet_handler;

	// get service handle range
	uint16_t start_handle = 0;
	uint16_t end_handle   = 0xffff;
	int service_found = gatt_server_get_handle_range_for_service_with_uuid16(dfu_profile_uuid16, &start_handle, &end_handle);
	btstack_assert(service_found != 0);
	UNUSED(service_found);

	// get characteristic value handle and client configuration handle
	dfu_ctrl_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(start_handle, end_handle, dfu_ctrl_point_uuid128);
	dfu_ctrl_point_cccd_handle  = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(start_handle, end_handle, dfu_ctrl_point_uuid128);
	dfu_data_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(start_handle, end_handle, dfU_data_point_uuid128);
	dfu_buttonless_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(start_handle, end_handle, dfu_buttonless_uuid128);
	dfu_buttonless_cccd_handle  = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(start_handle, end_handle, dfu_buttonless_uuid128);

	log_info("dfu_ctrl_point_value_handle 	0x%02x", dfu_ctrl_point_value_handle);
	log_info("dfu_ctrl_point_cccd_handle 	0x%02x", dfu_ctrl_point_cccd_handle);
	log_info("dfu_data_point_value_handle 	0x%02x", dfu_data_point_value_handle);
	log_info("dfu_buttonless_value_handle 	0x%02x", dfu_buttonless_value_handle);
	log_info("dfu_buttonless_cccd_handle 	0x%02x", dfu_buttonless_cccd_handle);
	
	// register service with ATT Server
	nordic_dfu_service.start_handle   = start_handle;
	nordic_dfu_service.end_handle     = end_handle;
	nordic_dfu_service.read_callback  = &nordic_dfu_service_read_callback;
	nordic_dfu_service.write_callback = &nordic_dfu_service_write_callback;
	att_server_register_service_handler(&nordic_dfu_service);
}

/** 
 * @brief Queue send request. When called, one packet can be send via nordic_dfu_service_send below
 * @param request
 * @param con_handle
 */
void nordic_dfu_service_server_notify_request(void (*cb)(void *context), void *context, hci_con_handle_t con_handle) {
	send_request.callback = cb;
	send_request.context = context;
	att_server_request_to_send_notification(&send_request, con_handle);
}

/** 
 * @brief Queue send request. When called, one packet can be send via nordic_dfu_service_send below
 * @param request
 * @param con_handle
 */
void nordic_dfu_service_server_indicate_request(void (*cb)(void *context), void *context, hci_con_handle_t con_handle) {
	send_request.callback = cb;
	send_request.context = context;
	att_server_request_to_send_indication(&send_request, con_handle);
}

