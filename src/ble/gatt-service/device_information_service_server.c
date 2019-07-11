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

#define BTSTACK_FILE__ "device_information_service_server.c"

/**
 * Implementation of the Device Information Service Server 
 *
 * To use with your application, add '#import <device_information_sevice.gatt' to your .gatt file
 * and call all functions below. All strings and blobs need to stay valid after calling the functions.
 *
 * @note: instead of calling all setters, you can create a local copy of the .gatt file and remove
 * all Characteristics that are not relevant for your application.
 */


#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"

#include "ble/gatt-service/device_information_service_server.h"

typedef enum {
	MANUFACTURER_NAME = 0,
	MODEL_NUMBER,
	SERIAL_NUMBER,
	HARDWARE_REVISION,
	FIRMWARE_REVISION,
	SOFTWARE_REVISION,
	SYSTEM_ID,
	IEEE_REGULATORY_CERTIFICATION,
	PNP_ID,
	NUM_INFORMATION_FIELDS
} device_information_field_id_t;

typedef struct {
	uint8_t * data;
	uint16_t  len;
	uint16_t  value_handle;
} device_information_field_t;

const uint16_t device_information_characteristic_uuids[] = {
	ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING,
	ORG_BLUETOOTH_CHARACTERISTIC_MODEL_NUMBER_STRING,
	ORG_BLUETOOTH_CHARACTERISTIC_SERIAL_NUMBER_STRING,
	ORG_BLUETOOTH_CHARACTERISTIC_HARDWARE_REVISION_STRING,
	ORG_BLUETOOTH_CHARACTERISTIC_FIRMWARE_REVISION_STRING,
	ORG_BLUETOOTH_CHARACTERISTIC_SOFTWARE_REVISION_STRING,
	ORG_BLUETOOTH_CHARACTERISTIC_SYSTEM_ID,
	ORG_BLUETOOTH_CHARACTERISTIC_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST,
	ORG_BLUETOOTH_CHARACTERISTIC_PNP_ID
};

static device_information_field_t device_information_fields[NUM_INFORMATION_FIELDS];

static uint8_t device_information_system_id[8];
static uint8_t device_information_ieee_regulatory_certification[4];
static uint8_t device_information_pnp_id[7];

static att_service_handler_t       device_information_service;

static void set_string(device_information_field_id_t field_id, const char * text){
	device_information_fields[field_id].data = (uint8_t*) text;
	device_information_fields[field_id].len  = strlen(text);
}

static uint16_t device_information_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	UNUSED(con_handle);	// ok: info same for all devices
	int i;
	for (i=0;i<NUM_INFORMATION_FIELDS;i++){
		if (device_information_fields[i].value_handle != attribute_handle) continue;
		if (buffer == 0){
			return device_information_fields[i].len;
		}
		int bytes_to_copy = btstack_min(device_information_fields[i].len - offset, buffer_size);
		memcpy(buffer, &device_information_fields[i].data[offset], bytes_to_copy);
		return bytes_to_copy;
	}
	return 0;
}

void device_information_service_server_init(void){

	// get service handle range
	uint16_t start_handle;
	uint16_t end_handle;
	int service_found = gatt_server_get_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_DEVICE_INFORMATION, &start_handle, &end_handle);
	if (!service_found) return;

	// set length for fixed size characateristics
	device_information_fields[SYSTEM_ID].data = device_information_system_id;
	device_information_fields[SYSTEM_ID].len = 8;
	device_information_fields[IEEE_REGULATORY_CERTIFICATION].data = device_information_ieee_regulatory_certification;
	device_information_fields[IEEE_REGULATORY_CERTIFICATION].len = 4;
	device_information_fields[PNP_ID].data = device_information_pnp_id;
	device_information_fields[PNP_ID].len = 7;

	// get characteristic value handles
	int i;
	for (i=0;i<NUM_INFORMATION_FIELDS;i++){
		device_information_fields[i].value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, device_information_characteristic_uuids[i]);
	}

	// register service with ATT Server
	device_information_service.start_handle   = start_handle;
	device_information_service.end_handle     = end_handle;
	device_information_service.read_callback  = &device_information_service_read_callback;
	device_information_service.write_callback = NULL;
	att_server_register_service_handler(&device_information_service);
}

/**
 * @brief Set Manufacturer Name
 * @param manufacturer_name
 */
void device_information_service_server_set_manufacturer_name(const char * manufacturer_name){
	set_string(MANUFACTURER_NAME, manufacturer_name);
}

/**
 * @brief Set Model Number
 * @param model_number
 */
void device_information_service_server_set_model_number(const char * model_number){
	set_string(MODEL_NUMBER, model_number);	
}

/**
 * @brief Set Serial Number
 * @param serial_number
 */
void device_information_service_server_set_serial_number(const char * serial_number){
	set_string(SERIAL_NUMBER, serial_number);	
}

/**
 * @brief Set Hardware Revision
 * @param hardware_revision
 */
void device_information_service_server_set_hardware_revision(const char * hardware_revision){
	set_string(HARDWARE_REVISION, hardware_revision);	
}

/**
 * @brief Set Firmware Revision
 * @param firmware_revision
 */
void device_information_service_server_set_firmware_revision(const char * firmware_revision){
	set_string(FIRMWARE_REVISION, firmware_revision);	
}

/**
 * @brief Set Software Revision
 * @param software_revision
 */
void device_information_service_server_set_software_revision(const char * software_revision){
	set_string(SOFTWARE_REVISION, software_revision);	
}

/**
 * @brief Set System ID
 * @param manufacturer_identifier uint40
 * @param organizationally_unique_identifier uint24
 */
void device_information_service_server_set_system_id(uint64_t manufacturer_identifier, uint32_t organizationally_unique_identifier){
	little_endian_store_32(device_information_system_id, 0, manufacturer_identifier);
	device_information_system_id[4] = manufacturer_identifier >> 32;
	little_endian_store_16(device_information_system_id, 5, organizationally_unique_identifier);
	device_information_system_id[7] = organizationally_unique_identifier >> 16;
}

/**
 * @brief Set IEEE 11073-20601 regulatory certification data list
 * @note: format duint16. duint16 is two uint16 values concatenated together.
 * @param ieee_regulatory_certification
 */
void device_information_service_server_set_ieee_regulatory_certification(uint16_t value_a, uint16_t value_b){
	little_endian_store_16(device_information_ieee_regulatory_certification, 0, value_a);
	little_endian_store_16(device_information_ieee_regulatory_certification, 2, value_b);
}

/**
 * @brief Set PNP ID
 * @param vendor_source_id
 * @param vendor_id
 * @param product_id
 */
void device_information_service_server_set_pnp_id(uint8_t vendor_source_id, uint16_t vendor_id, uint16_t product_id, uint16_t product_version){
	device_information_pnp_id[0] = vendor_source_id;
	little_endian_store_16(device_information_pnp_id, 1, vendor_id);
	little_endian_store_16(device_information_pnp_id, 3, product_id);
	little_endian_store_16(device_information_pnp_id, 5, product_version);
}
