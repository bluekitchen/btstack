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

/**
 * @title Device Information Service Server
 * 
 */

#ifndef DEVICE_INFORMATION_SERVICE_SERVER_H
#define DEVICE_INFORMATION_SERVICE_SERVER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @text Th eDevice Information Service allows to query manufacturer and/or 
 * vendor information about a device.
 * 
 * To use with your application, add `#import <device_information_service.gatt>` to your .gatt file. 
 *
 * *Note*: instead of calling all setters, you can create a local copy of the .gatt file and remove
 * all Characteristics that are not relevant for your application and define all fixed values in the .gatt file.
 */

/**
 * @brief Init Device Information Service Server with ATT DB
 * @param battery_value in range 0-100
 */
void device_information_service_server_init(void);

/**
 * @brief Set Manufacturer Name
 * @param manufacturer_name
 */
void device_information_service_server_set_manufacturer_name(const char * manufacturer_name);

/**
 * @brief Set Model Number
 * @param model_number
 */
void device_information_service_server_set_model_number(const char * model_number);

/**
 * @brief Set Serial Number
 * @param serial_number
 */
void device_information_service_server_set_serial_number(const char * serial_number);

/**
 * @brief Set Hardware Revision
 * @param hardware_revision
 */
void device_information_service_server_set_hardware_revision(const char * hardware_revision);

/**
 * @brief Set Firmware Revision
 * @param firmware_revision
 */
void device_information_service_server_set_firmware_revision(const char * firmware_revision);

/**
 * @brief Set Software Revision
 * @param software_revision
 */
void device_information_service_server_set_software_revision(const char * software_revision);

/**
 * @brief Set System ID
 * @param manufacturer_identifier uint40
 * @param organizationally_unique_identifier uint24
 */
void device_information_service_server_set_system_id(uint64_t manufacturer_identifier, uint32_t organizationally_unique_identifier);

/**
 * @brief Set IEEE 11073-20601 regulatory certification data list
 * @note: format duint16. duint16 is two uint16 values concatenated together.
 * @param value_a
 * @param value_b
 */
void device_information_service_server_set_ieee_regulatory_certification(uint16_t value_a, uint16_t value_b);

/**
 * @brief Set PnP ID
 * @param vendor_source_id
 * @param vendor_id
 * @param product_id
 * @param product_version
 */
void device_information_service_server_set_pnp_id(uint8_t vendor_source_id, uint16_t vendor_id, uint16_t product_id, uint16_t product_version);

/**
 * @brief Set UDI for medical devices
 * @param label
 * @param device_id
 * @param issuer
 * @param authority
 */
void device_information_service_server_set_udi_for_medical_devices(const char * label, const char * device_id, const char * issuer, const char * authority);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

