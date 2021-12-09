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

#define BTSTACK_FILE__ "bond_management_service_server.c"

#include "bluetooth.h"
#include "btstack_defines.h"
#include "hci.h"
#include "gap.h"
#include "btstack_util.h"
#include "btstack_debug.h"

#include "bluetooth_gatt.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "ble/le_device_db.h"

#include "ble/gatt-service/bond_management_service_server.h"

// characteristic:  Control Point
static uint16_t bm_control_point_value_handle;

static const char * bm_authorization_string;

// characteristic: Feature
static uint16_t bm_supported_features_value_handle;
static uint32_t bm_supported_features;

static att_service_handler_t bond_management_service;

#ifdef ENABLE_CLASSIC
static void bond_management_delete_bonding_information_classic(hci_connection_t * connection, bool delete_own_bonding, bool delete_all_bonding_but_active){
    bd_addr_t entry_address;
    link_key_t link_key;
    link_key_type_t type;
    btstack_link_key_iterator_t it;

    log_info("BMS Classic: delete bonding %s - own %d, other %d", bd_addr_to_str(connection->address), delete_own_bonding?1:0, delete_all_bonding_but_active?1:0);

    int ok = gap_link_key_iterator_init(&it);
    if (!ok) {
        log_error("BMS: could not initialize iterator");
        return;
    }

    while (gap_link_key_iterator_get_next(&it, entry_address, link_key, &type)){
        if (memcmp(connection->address, entry_address, 6) == 0){
            if (delete_own_bonding){
                gap_drop_link_key_for_bd_addr(entry_address);
            }
        } else {
            if (delete_all_bonding_but_active){
                gap_drop_link_key_for_bd_addr(entry_address); 
            }
        }
    }
    gap_link_key_iterator_done(&it); 

}
#endif

static void bond_management_delete_bonding_information_le(hci_connection_t * connection, bool delete_own_bonding, bool delete_all_bonding_but_active){
    bd_addr_t entry_address;
    bd_addr_type_t device_address_type = connection->address_type;

    log_info("BMS LE: delete bonding %s - own %d, other %d",  bd_addr_to_str(connection->address), delete_own_bonding?1:0, delete_all_bonding_but_active?1:0);

    uint16_t i;
    for (i=0; i < le_device_db_max_count(); i++){
        int entry_address_type = (int) BD_ADDR_TYPE_UNKNOWN;
        le_device_db_info(i, &entry_address_type, entry_address, NULL);
        // skip unused entries
        
        if (entry_address_type == (int) BD_ADDR_TYPE_UNKNOWN) continue;
        
        if ((entry_address_type == (int) device_address_type) && (memcmp(entry_address, connection->address, 6) == 0)){
            if (delete_own_bonding){   
                gap_delete_bonding((bd_addr_type_t)entry_address_type, entry_address);
            }
        } else {
            if (delete_all_bonding_but_active){
                gap_delete_bonding((bd_addr_type_t)entry_address_type, entry_address);
            }
        }
    }
}

static uint16_t bond_management_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(attribute_handle);
    UNUSED(offset);
    UNUSED(buffer_size);
    
    if (attribute_handle == bm_supported_features_value_handle){

#if 0
    
        // According to BMS Spec, 3.2.1 Bond Management Feature Characteristic Behavior, only relevant bits should be sent
        uint16_t relevant_octets = 0;

        // The server shall only include the number of octets needed for returning the highest set feature bit
        if (bm_supported_features > 0xFFFF){
            relevant_octets = 3;
        } else if (bm_supported_features > 0xFF) {
            relevant_octets = 2;
        } else if (bm_supported_features > 0x00){
            relevant_octets = 1;
        }
#else
        // however PTS 8.0.3 expects 3 bytes
        uint16_t relevant_octets = 3;
#endif

        uint8_t feature_buffer[3];
        if (buffer != NULL){
            little_endian_store_24(feature_buffer, 0, bm_supported_features);
            (void) memcpy(buffer, feature_buffer, relevant_octets);
        } 
        return relevant_octets;
    }

    return 0;
}

#include <stdio.h>

static int bond_management_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer_size);
    
    if (transaction_mode != ATT_TRANSACTION_MODE_NONE){
        return 0;
    } 

    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    btstack_assert(connection != NULL);

    if (attribute_handle == bm_control_point_value_handle){
        if (buffer_size == 0){
            return BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED;
        }

        uint8_t remote_cmd = buffer[0];
        // check if command/auth is supported
        if (remote_cmd > BOND_MANAGEMENT_CMD_DELETE_ALL_BUT_ACTIVE_BOND_LE) {
            return BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED;
        }
        uint16_t authorisation_code_size = buffer_size - 1;
        if (authorisation_code_size > 511){
            return BOND_MANAGEMENT_OPERATION_FAILED;
        }
        
        uint32_t requested_feature_mask_without_auth = 1UL << (2*(remote_cmd-1));
        uint32_t requested_feature_mask_with_auth    = 1UL << (2*(remote_cmd-1) + 1);
        bool locally_supported_with_auth    = (bm_supported_features & requested_feature_mask_with_auth) != 0;
        bool locally_supported_without_auth = (bm_supported_features & requested_feature_mask_without_auth) != 0;

        bool remote_auth_provided = authorisation_code_size > 0;
        
        // log_info("cmd 0x%02X, features 0x%03X, auth_provided %d, LA %d, LW %d", remote_cmd, bm_supported_features, remote_auth_provided?1:0, locally_supported_with_auth?1:0, locally_supported_without_auth?1:0);
        if (remote_auth_provided){
            if (locally_supported_with_auth){
                if (!bm_authorization_string){
                    return ATT_ERROR_INSUFFICIENT_AUTHORIZATION;
                }
                if (strlen(bm_authorization_string) != authorisation_code_size){
                    return ATT_ERROR_INSUFFICIENT_AUTHORIZATION;
                }
                if (memcmp(bm_authorization_string, (const char *)&buffer[1], authorisation_code_size) != 0){
                    return ATT_ERROR_INSUFFICIENT_AUTHORIZATION;
                }
            } else {
                return BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED;
            }
        } else {
            if (!locally_supported_without_auth){
                if (locally_supported_with_auth){
                    return ATT_ERROR_INSUFFICIENT_AUTHORIZATION;
                } else {
                    return BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED;
                }
            } 
        }

        switch (remote_cmd){
#ifdef ENABLE_CLASSIC
            case BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE:
                bond_management_delete_bonding_information_classic(connection, true, false);
                bond_management_delete_bonding_information_le(connection, true, false);
                break;
            case BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC:
                bond_management_delete_bonding_information_classic(connection, true, false);
                break;
            case BOND_MANAGEMENT_CMD_DELETE_ALL_BONDS_CLASSIC_AND_LE:
                bond_management_delete_bonding_information_classic(connection, true, true);
                bond_management_delete_bonding_information_le(connection, true, true);
                break;
            case BOND_MANAGEMENT_CMD_DELETE_ALL_BONDS_CLASSIC:
                bond_management_delete_bonding_information_classic(connection, true, true);
                break;
            case BOND_MANAGEMENT_CMD_DELETE_ALL_BUT_ACTIVE_BOND_CLASSIC_AND_LE:
                bond_management_delete_bonding_information_classic(connection, false, true);
                bond_management_delete_bonding_information_le(connection, false, true);
                break;
            case BOND_MANAGEMENT_CMD_DELETE_ALL_BUT_ACTIVE_BOND_CLASSIC:
                bond_management_delete_bonding_information_classic(connection, false, true);
                break;
#endif
            case BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_LE:
                bond_management_delete_bonding_information_le(connection, true, false);
                break;
            case BOND_MANAGEMENT_CMD_DELETE_ALL_BONDS_LE:
                bond_management_delete_bonding_information_le(connection, true, true);
                break;
            case BOND_MANAGEMENT_CMD_DELETE_ALL_BUT_ACTIVE_BOND_LE:
                bond_management_delete_bonding_information_le(connection, false, true);
                break;
            default:
                return BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED;
        }

        return 0;
    }
    return 0;
}

// buffer for authorisation conde
void bond_management_service_server_init(uint32_t supported_features){
    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_BOND_MANAGEMENT, &start_handle, &end_handle);
	btstack_assert(service_found != 0);
	UNUSED(service_found);

    bm_control_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BOND_MANAGEMENT_CONTROL_POINT);
    bm_supported_features_value_handle       = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BOND_MANAGEMENT_FEATURE);
    bm_supported_features = supported_features;
    
    log_info("Control Point value handle 0x%02x", bm_control_point_value_handle);
    log_info("Feature       value handle 0x%02x", bm_supported_features_value_handle);
    // register service with ATT Server
    bond_management_service.start_handle   = start_handle;
    bond_management_service.end_handle     = end_handle;
    bond_management_service.read_callback  = &bond_management_service_read_callback;
    bond_management_service.write_callback = &bond_management_service_write_callback;
    
    att_server_register_service_handler(&bond_management_service);
}

void bond_management_service_server_set_authorisation_string(const char * authorisation_string){
    bm_authorization_string = authorisation_string;
}
