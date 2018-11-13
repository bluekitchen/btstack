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

#define __BTSTACK_FILE__ "mesh_provisioning_service_server.c"


#include "bluetooth.h"
#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "l2cap.h"

#include "ble/gatt-service/mesh_provisioning_service_server.h"

typedef struct {
    hci_con_handle_t con_handle;
} mesh_provisioning_t;

static att_service_handler_t mesh_provisioning_service;
static mesh_provisioning_t mesh_provisioning;

static uint16_t mesh_provisioning_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(attribute_handle);
    UNUSED(offset);
    UNUSED(buffer_size);
    
    printf("mesh_provisioning_service_read_callback, not handeled read on handle 0x%02x\n", attribute_handle);
    return 0;
}

static int mesh_provisioning_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer_size);
    
    printf("mesh_provisioning_service_write_callback, not handeled write on handle 0x%02x\n", attribute_handle);
    return 0;
}


void mesh_provisioning_service_server_init(void){
    mesh_provisioning_t * instance = &mesh_provisioning;

    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING, &start_handle, &end_handle);
    if (!service_found) return;

    // // get Heart Rate Mesurement characteristic value handle and client configuration handle
    // instance->measurement_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_mesh_provisioning_MEASUREMENT);
    // instance->measurement_client_configuration_descriptor_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_mesh_provisioning_MEASUREMENT);
    // // get Body Sensor Location characteristic value handle and client configuration handle
    // instance->sensor_location_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BODY_SENSOR_LOCATION);
    // // get Hear Rate Control Point characteristic value handle and client configuration handle
    // instance->control_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_mesh_provisioning_CONTROL_POINT);
    
    // // printf("Measurement     value handle 0x%02x\n", instance->measurement_value_handle);
    // // printf("Client Config   value handle 0x%02x\n", instance->measurement_client_configuration_descriptor_handle);
    // // printf("Sensor location value handle 0x%02x\n", instance->sensor_location_value_handle);
    // // printf("Control Point   value handle 0x%02x\n", instance->control_point_value_handle);
    // // register service with ATT Server
    
    mesh_provisioning_service.start_handle   = start_handle;
    mesh_provisioning_service.end_handle     = end_handle;
    mesh_provisioning_service.read_callback  = &mesh_provisioning_service_read_callback;
    mesh_provisioning_service.write_callback = &mesh_provisioning_service_write_callback;
    
    att_server_register_service_handler(&mesh_provisioning_service);
}
