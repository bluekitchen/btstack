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

#define BTSTACK_FILE__ "ble_peripheral_test.c"

/*
 * ble_peripheral_test.c : Tool for testig BLE peripheral
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ble/att_db_util.h>

#include "btstack_config.h"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "ble/le_device_db.h"
#include "ble/sm.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "btstack_stdin.h"

#ifdef ENABLE_GATT_OVER_CLASSIC
#include "classic/gatt_sdp.h"
#include "classic/sdp_util.h"
static uint8_t gatt_service_buffer[70];
#endif

#define HEARTBEAT_PERIOD_MS 1000

// test profile
#include "gatt_server_test.h"

///------
static int gap_advertisements = 0;
static int gap_discoverable = 0;
static int gap_connectable = 0;
static int gap_privacy = 0;
static int gap_random = 0;
static int gap_bondable = 0;
static int gap_directed_connectable = 0;
static int gap_scannable = 0;
static char gap_device_name[20];
static uint16_t gap_appearance = 0;

static bd_addr_t gap_reconnection_address;

static int att_default_value_long = 0;

// static uint8_t test_irk[] =  { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
static uint8_t test_irk[] =  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static char * sm_io_capabilities = NULL;
static int sm_mitm_protection = 0;
static int sm_have_oob_data = 0;
static uint8_t * sm_oob_data = (uint8_t *) "0123456789012345"; // = { 0x30...0x39, 0x30..0x35}
// static uint8_t sm_oob_data[] = { 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  };
static int sm_min_key_size = 7;

static int master_addr_type;
static bd_addr_t master_address;
static int ui_passkey = 0;
static int ui_digits_for_passkey = 0;

static btstack_timer_source_t heartbeat;
static uint8_t counter = 0;
static int update_client = 0;
static int client_configuration = 0;
static uint16_t client_configuration_handle;
static hci_con_handle_t handle = HCI_CON_HANDLE_INVALID;

static bool dynamic_db;
static uint8_t gatt_database_hash[16];
static btstack_crypto_aes128_cmac_t gatt_aes_cmac_context;

static btstack_packet_callback_registration_t sm_event_callback_registration;

static void app_run(void);
static void show_usage(void);
static void update_advertisements(void);


// static bd_addr_t tester_address = {0x00, 0x1B, 0xDC, 0x06, 0x07, 0x5F};
static bd_addr_t tester_address = {0x00, 0x1B, 0xDC, 0x07, 0x32, 0xef};
static int tester_address_type = 0;

// general discoverable flags
static uint8_t adv_general_discoverable[] = { 2, 01, 02 };

// non discoverable flags
static uint8_t adv_non_discoverable[] = { 2, 01, 00 };


// AD Manufacturer Specific Data - Ericsson, 1, 2, 3, 4
static uint8_t adv_data_1[] = { 7, 0xff, 0x00, 0x00, 1, 2, 3, 4 }; 
// AD Local Name - 'BTstack'
static uint8_t adv_data_2[] = { 8, 0x09, 'B', 'T', 's', 't', 'a', 'c', 'k' }; 
// AD Flags - 2 - General Discoverable mode -- flags are always prepended
static uint8_t adv_data_3[] = {  }; 
// AD Service Data - 0x1812 HID over LE
static uint8_t adv_data_4[] = { 3, 0x16, 0x12, 0x18 }; 
// AD Service Solicitation -  0x1812 HID over LE
static uint8_t adv_data_5[] = { 3, 0x14, 0x12, 0x18 }; 
// AD Services
static uint8_t adv_data_6[] = { 3, 0x03, 0x12, 0x18 }; 
// AD Slave Preferred Connection Interval Range - no min, no max
static uint8_t adv_data_7[] = { 5, 0x12, 0xff, 0xff, 0xff, 0xff }; 
// AD Tx Power Level - +4 dBm
static uint8_t adv_data_8[] = { 2, 0x0a, 4 }; 
// AD OOB
static uint8_t adv_data_9[] = { 2, 0x11, 3 }; 
// AD TK Value
static uint8_t adv_data_0[] = { 17, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 
// AD LE Bluetooth Device Address - 66:55:44:33:22:11  public address
static uint8_t adv_data_le_bd_addr[] = { 8, 0x1b, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x00};
// AD Appearance
static uint8_t adv_data_appearance[] = { 3, 0x19, 0x00, 0x00};
// AD LE Role - Peripheral only
static uint8_t adv_data_le_role[] = {2 , 0x1c, 0x00};
// AD Public Target Address
static uint8_t adv_data_public_target_address[] = { 7, 0x17,  0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
// AD Random Target Address
static uint8_t adv_data_random_target_address[] = { 7, 0x18,  0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
// AD Advertising Interval
static uint8_t adv_data_advertising_interval[] =  { 3, 0x1a, 0x00, 0x80};

static uint8_t adv_data_len;
static uint8_t adv_data[32];

// static uint8_t scan_data_len;
// static uint8_t scan_data[32];

typedef struct {
    uint16_t len;
    uint8_t * data;
} advertisement_t;

#define ADV(a) { sizeof(a), &a[0]}
static advertisement_t advertisements[] = {
    ADV(adv_data_0),
    ADV(adv_data_1),
    ADV(adv_data_2),
    ADV(adv_data_3),
    ADV(adv_data_4),
    ADV(adv_data_5),
    ADV(adv_data_6),
    ADV(adv_data_7),
    ADV(adv_data_8),
    ADV(adv_data_9),
    ADV(adv_data_le_bd_addr),
    ADV(adv_data_appearance),
    ADV(adv_data_le_role),
    ADV(adv_data_public_target_address),
    ADV(adv_data_random_target_address),
    ADV(adv_data_advertising_interval),
};

static int advertisement_index = 2;

// signed write
static uint8_t signed_write_characteristic_value;

// att write queue engine

static const char default_value_long[]  = "0123456789abcdef0123456789abcdef0123456789abcdef!!";
static const char default_value_short[] = "a";

#define ATT_VALUE_MAX_LEN 50

typedef struct {
    uint16_t handle;
    uint16_t len;
    uint8_t  value[ATT_VALUE_MAX_LEN];
} attribute_t;

#define ATT_NUM_WRITE_QUEUES 2
static attribute_t att_write_queues[ATT_NUM_WRITE_QUEUES];
#define ATT_NUM_ATTRIBUTES 10
static attribute_t att_attributes[ATT_NUM_ATTRIBUTES];

static void att_write_queue_init(void){
    int i;
    for (i=0;i<ATT_NUM_WRITE_QUEUES;i++){
        att_write_queues[i].handle = 0;
    }
}

static int att_write_queue_for_handle(uint16_t aHandle){
    int i;
    for (i=0;i<ATT_NUM_WRITE_QUEUES;i++){
        if (att_write_queues[i].handle == aHandle){
            return i;
        }
    }
    for (i=0;i<ATT_NUM_WRITE_QUEUES;i++){
        if (att_write_queues[i].handle == 0){
            att_write_queues[i].handle = aHandle;
            memset(att_write_queues[i].value, 0, ATT_VALUE_MAX_LEN);
            att_write_queues[i].len = 0;
            return i;
        }
    }
    return -1;
}

// handle == 0 finds free attribute
static int att_attribute_for_handle(uint16_t aHandle){
    int i;
    for (i=0;i<ATT_NUM_ATTRIBUTES;i++){
        if (att_attributes[i].handle == aHandle) {
            return i;
        }
    }
    return -1;
}

static void att_setup_attribute(uint16_t attribute_handle, const uint8_t * value, uint16_t len){
    int index = att_attribute_for_handle(attribute_handle);
    if (index < 0){
        index = att_attribute_for_handle(0);
    }
    btstack_assert(index >= 0);
    printf("Setup Attribute %04x, len %u, value: %s\n", attribute_handle, len, value);
    btstack_assert(len <= ATT_VALUE_MAX_LEN);
    att_attributes[index].handle = attribute_handle;
    att_attributes[index].len    = len;
    memcpy(att_attributes[index].value, value, len);
}

static void att_attributes_init(void){
    int i;
    for (i=0;i<ATT_NUM_ATTRIBUTES;i++){
        att_attributes[i].handle = 0;
    }

    // preset some attributes
    att_setup_attribute(ATT_CHARACTERISTIC_FFF7_01_USER_DESCRIPTION_HANDLE, (const uint8_t *) default_value_long, strlen(default_value_long));
}


static void  heartbeat_handler(struct btstack_timer_source *ts){
    // restart timer
    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);

    counter++;
    update_client = 1;
    app_run();
} 

static void app_run(void){
    if (!update_client) return;
    if (!att_server_can_send_packet_now(handle)) return;

    int result = -1;
    switch (client_configuration){
        case 0x01:
            printf("Notify value %u\n", counter);
            result = att_server_notify(handle, client_configuration_handle - 1, &counter, 1);
            break;
        case 0x02:
            printf("Indicate value %u\n", counter);
            result = att_server_indicate(handle, client_configuration_handle - 1, &counter, 1);
            break;
        default:
            return;
    }        
    if (result){
        printf("Error 0x%02x\n", result);
        return;        
    }
    update_client = 0;
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){

    UNUSED(con_handle);
    printf("READ Callback, handle %04x, offset %u, buffer size %u, buffer %p\n", attribute_handle, offset, buffer_size, buffer);

    if (dynamic_db){
        // support Database Hash
        if (attribute_handle == 3){
            uint8_t hash_buffer[16];
            reverse_128(gatt_database_hash, hash_buffer);
            return att_read_callback_handle_blob(hash_buffer, 16, offset, buffer, buffer_size);
        }
    }

    uint16_t  att_value_len;
    const uint16_t long_characteristic_len = 51;

    switch (attribute_handle){
        case ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_VALUE_HANDLE:
            att_value_len = strlen(gap_device_name);
            if (buffer) {
                memcpy(buffer, gap_device_name, att_value_len);
            }
            return att_value_len; 
        case ATT_CHARACTERISTIC_GAP_APPEARANCE_01_VALUE_HANDLE:
            if (buffer){
                little_endian_store_16(buffer, 0, gap_appearance);
            }
            return 2;
        case ATT_CHARACTERISTIC_GAP_PERIPHERAL_PRIVACY_FLAG_01_VALUE_HANDLE:
            if (buffer){
                buffer[0] = gap_privacy;
            }
            return 1;
        case ATT_CHARACTERISTIC_GAP_RECONNECTION_ADDRESS_01_VALUE_HANDLE:
            if (buffer) {
                reverse_bd_addr(gap_reconnection_address, buffer);
            }
            return 6;
        case ATT_CHARACTERISTIC_FFF2_01_VALUE_HANDLE:
            // Value with size mtu - 1, buffer has size mtu - 1
            if (buffer){
                memset(buffer, 'x', buffer_size);
            }
            return buffer_size;
        case ATT_CHARACTERISTIC_FFF1_01_CLIENT_CONFIGURATION_HANDLE:
            if (buffer){
                little_endian_store_16(buffer, 0, client_configuration);
            }
            return 2;
        case ATT_CHARACTERISTIC_FFF8_01_VALUE_HANDLE:
            // access only for BR/EDR, return application error (0x80-0x9f)
            if (gap_get_connection_type(con_handle) != GAP_CONNECTION_ACL) return ATT_READ_ERROR_CODE_OFFSET + 0x80;
            if (buffer) {
                buffer[0] = 'A';
            }
            return 1;
        case ATT_CHARACTERISTIC_FFF9_01_VALUE_HANDLE:
            if (gap_get_connection_type(con_handle) != GAP_CONNECTION_LE) return ATT_READ_ERROR_CODE_OFFSET + 0x80;
            if (buffer) {
                buffer[0] = 'A';
            }
            return 1;
        case ATT_CHARACTERISTIC_FFFB_01_VALUE_HANDLE:
            if (buffer){
                buffer[0] = signed_write_characteristic_value;
            }
            return 1;
        default:
            break;
    }

    uint16_t uuid16 = att_uuid_for_handle(attribute_handle);
    if (uuid16){
        printf("- Resolved to UUID %04x\n", uuid16);
        switch (uuid16){
            case 0x2902:
                if (buffer) {
                    buffer[0] = client_configuration;
                }
                return 1;
            default:
                break;
        }
    }
    
    // find attribute
    int index = att_attribute_for_handle(attribute_handle);
    uint8_t * att_value;
    if (index < 0){
        // not written before
        printf("- Attribute not written before\n");
        if (att_default_value_long){
            att_value = (uint8_t*) default_value_long;
            att_value_len  = strlen(default_value_long);
        } else {
            att_value = (uint8_t*) default_value_short;
            att_value_len  = strlen(default_value_short);
        }
    } else {
        printf("- Found existing attribute\n");
        att_value     = att_attributes[index].value;
        att_value_len = att_attributes[index].len;
    }
    printf("- Attribute len %u, data: ", att_value_len);
    printf_hexdump(att_value, att_value_len);

    // assert offset <= att_value_len
    if (offset > att_value_len) {
        return 0;
    }
    uint16_t bytes_to_copy = att_value_len - offset;
    if (!buffer) return bytes_to_copy;
    if (bytes_to_copy > buffer_size){
        bytes_to_copy = buffer_size;
    }
    memcpy(buffer, &att_value[offset], bytes_to_copy);
    return bytes_to_copy;
}

// write requests
static void att_write_update_attribute(hci_con_handle_t con_handle, uint16_t attribute_handle, const uint8_t * value_data, uint16_t value_len){
    switch (attribute_handle){
        case ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_VALUE_HANDLE:
            memcpy(gap_device_name, value_data, value_len);
            gap_device_name[value_len]=0;
            printf("Setting device name to '%s'\n", gap_device_name);
            break;
        case ATT_CHARACTERISTIC_GAP_APPEARANCE_01_VALUE_HANDLE:
            gap_appearance = little_endian_read_16(value_data, 0);
            printf("Setting appearance to 0x%04x'\n", gap_appearance);
            break;
        case ATT_CHARACTERISTIC_GAP_PERIPHERAL_PRIVACY_FLAG_01_VALUE_HANDLE:
            gap_privacy = value_data[0];
            printf("Setting privacy to 0x%04x'\n", gap_privacy);
            update_advertisements();
            break;
        case ATT_CHARACTERISTIC_GAP_RECONNECTION_ADDRESS_01_VALUE_HANDLE:
            reverse_bd_addr(value_data, gap_reconnection_address);
            printf("Setting Reconnection Address to %s\n", bd_addr_to_str(gap_reconnection_address));
            break;
        case ATT_CHARACTERISTIC_FFFB_01_VALUE_HANDLE:
            signed_write_characteristic_value = value_data[0];
            break;
        default:
            break;
    }
}

static int att_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){

    printf("WRITE Callback, handle %04x, mode %u, offset %u, data: ", attribute_handle, transaction_mode, offset);
    printf_hexdump(buffer, buffer_size);

    // lookup UUID for attribute handle
    uint16_t uuid16 = att_uuid_for_handle(attribute_handle);
    if (uuid16){
        printf("- Resolved to UUID %04x\n", uuid16);
        switch (uuid16){
            case GATT_CLIENT_CHARACTERISTICS_CONFIGURATION:
                handle = con_handle;
                client_configuration_handle = attribute_handle;
                client_configuration = buffer[0];
                printf("- Client Configuration set to %u for handle %04x\n", client_configuration, client_configuration_handle);
                return 0;   // ok
            default:
                break;
        }
    }

    // check transaction mode
    int attributes_index;
    int writes_index;
    switch (transaction_mode){
        case ATT_TRANSACTION_MODE_NONE:
            attributes_index = att_attribute_for_handle(attribute_handle);
            if (attributes_index < 0){
                attributes_index = att_attribute_for_handle(0);
                if (attributes_index < 0) return 0;    // ok, but we couldn't store it (our fault)
                printf("- Setup new attribute buffer for UUID %04x, long %u, index %u\n", uuid16, att_default_value_long, attributes_index);
                att_attributes[attributes_index].handle = attribute_handle;
                // not written before
                uint8_t * att_value;
                uint16_t att_value_len;
                if (att_default_value_long){
                    att_value = (uint8_t*) default_value_long;
                    att_value_len  = strlen(default_value_long);
                } else {
                    att_value = (uint8_t*) default_value_short;
                    att_value_len  = strlen(default_value_short);
                }
                att_attributes[attributes_index].len = att_value_len;
            }
            if (buffer_size > att_attributes[attributes_index].len) return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
            att_attributes[attributes_index].len = buffer_size;
            memcpy(att_attributes[attributes_index].value, buffer, buffer_size);
            // commit changes
            att_write_update_attribute(con_handle, attribute_handle, buffer, buffer_size);
            break;
        case ATT_TRANSACTION_MODE_ACTIVE:
            writes_index = att_write_queue_for_handle(attribute_handle);
            if (writes_index < 0)                            return ATT_ERROR_PREPARE_QUEUE_FULL;
            if (offset > att_write_queues[writes_index].len) return ATT_ERROR_INVALID_OFFSET;
            if (buffer_size + offset > ATT_VALUE_MAX_LEN)    return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
            // check offset for known attributes
            switch (attribute_handle){
                case ATT_CHARACTERISTIC_GAP_APPEARANCE_01_VALUE_HANDLE:
                    if (offset > 1) return ATT_ERROR_INVALID_OFFSET;
                    if (offset + buffer_size > 2) return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
                    break;
                default:
                    break;
            }
            att_write_queues[writes_index].len = buffer_size + offset;
            memcpy(&(att_write_queues[writes_index].value[offset]), buffer, buffer_size);
            break;
        case ATT_TRANSACTION_MODE_EXECUTE:
            for (writes_index=0 ; writes_index<ATT_NUM_WRITE_QUEUES ; writes_index++){
                uint16_t aHandle = att_write_queues[writes_index].handle;
                if (aHandle == 0) continue;
                attributes_index = att_attribute_for_handle(aHandle);
                if (attributes_index < 0){
                    attributes_index = att_attribute_for_handle(0);
                    if (attributes_index < 0) continue;
                    att_attributes[attributes_index].handle = aHandle;
                }
                att_attributes[attributes_index].len = att_write_queues[writes_index].len;
                memcpy(att_attributes[attributes_index].value, att_write_queues[writes_index].value, att_write_queues[writes_index].len);
                // commit changes
                att_write_update_attribute(con_handle, attribute_handle, buffer, buffer_size);
            }
            att_write_queue_init();
            break;
        case ATT_TRANSACTION_MODE_CANCEL:
            att_write_queue_init();
            break;
    }
    return 0;
}

static uint8_t gap_adv_type(void){
    if (gap_scannable) return 0x02;
    if (gap_directed_connectable) return 0x01;
    if (!gap_connectable) return 0x03;
    return 0x00;
}

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_address;
    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("SM Init completed\n");
                        update_advertisements();
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    handle = HCI_CON_HANDLE_INVALID;
                    att_attributes_init();
                    att_write_queue_init();
                    break;
                    
                case SM_EVENT_JUST_WORKS_REQUEST:
                    printf("SM_EVENT_JUST_WORKS_REQUEST\n");
                    sm_just_works_confirm(little_endian_read_16(packet, 2));
                    break;

                case SM_EVENT_PASSKEY_INPUT_NUMBER:
                    // display number
                    master_addr_type = packet[4];
                    reverse_bd_addr(&packet[5], event_address);
                    printf("\nGAP Bonding %s (%u): Enter 6 digit passkey: '", bd_addr_to_str(master_address), master_addr_type);
                    fflush(stdout);
                    ui_passkey = 0;
                    ui_digits_for_passkey = 6;
                    break;

                case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
                    // display number
                    printf("\nGAP Bonding %s (%u): Display Passkey '%06u\n", bd_addr_to_str(master_address), master_addr_type, little_endian_read_32(packet, 11));
                    break;

                case SM_EVENT_PASSKEY_DISPLAY_CANCEL: 
                    printf("\nGAP Bonding %s (%u): Display cancel\n", bd_addr_to_str(master_address), master_addr_type);
                    break;

                case SM_EVENT_AUTHORIZATION_REQUEST:
                    // auto-authorize connection if requested
                    sm_authorization_grant(little_endian_read_16(packet, 2));
                    break;

                case ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE:
                    printf("ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE status %u\n", packet[2]);
                    break;

                default:
                    break;
            }
    }
}

void show_usage(void){
    printf("\n--- CLI for LE Peripheral ---\n");
    printf("GAP: discoverable %u, connectable %u, bondable %u, directed connectable %u, random addr %u, ads enabled %u, adv type %u \n",
        gap_discoverable, gap_connectable, gap_bondable, gap_directed_connectable, gap_random, gap_advertisements, gap_adv_type());
    printf("ADV: "); printf_hexdump(adv_data, adv_data_len);
    printf("SM: %s, MITM protection %u, OOB data %u, key range [%u..16]\n",
        sm_io_capabilities, sm_mitm_protection, sm_have_oob_data, sm_min_key_size);
    printf("Default value: '%s'\n", att_default_value_long ? default_value_long : default_value_short);
    printf("Privacy %u\n", gap_privacy);
    printf("Device name %s\n", gap_device_name);
    printf("---\n");
    printf("a/A - advertisements off/on\n");
    printf("b/B - bondable off/on\n");
    printf("c/C - connectable off\n");
    printf("d/D - discoverable off\n");
    printf("r/R - random address off/on\n");
    printf("x/X - directed connectable off\n");
    printf("y/Y - scannable on/off\n");
    printf("---\n");
    printf("1   - AD Manufacturer Data    | 7 - AD Slave Preferred... | = - AD Public Target Address\n");
    printf("2   - AD Local Name           | 8 - AD Tx Power Level     | / - AD Random Target Address\n");
    printf("3   - AD flags                | 9 - AD SM OOB             | # - AD Advertising interval\n");
    printf("4   - AD Service Data         | 0 - AD SM TK              | & - AD LE Role\n");
    printf("5   - AD Service Solicitation | + - AD LE BD_ADDR\n");
    printf("6   - AD Services             | - - AD Appearance\n");
    printf("---\n");
    printf("l/L - default attribute value '%s'/'%s'\n", default_value_short, default_value_long);
    printf("p/P - privacy flag off\n");
    printf("s   - send security request\n");
    printf("z   - send Connection Parameter Update Request\n");
    printf("t   - terminate connection\n");
    printf("j   - create L2CAP LE connection to %s\n", bd_addr_to_str(tester_address));
    printf("---\n");
    printf("e   - IO_CAPABILITY_DISPLAY_ONLY\n");
    printf("f   - IO_CAPABILITY_DISPLAY_YES_NO\n");
    printf("g   - IO_CAPABILITY_NO_INPUT_NO_OUTPUT\n");
    printf("h   - IO_CAPABILITY_KEYBOARD_ONLY\n");
    printf("i   - IO_CAPABILITY_KEYBOARD_DISPLAY\n");
    printf("o/O - OOB data off/on ('%s')\n", sm_oob_data);
    printf("m/M - MITM protection off\n");
    printf("k/k - encryption key range [7..16]/[16..16]\n");
    printf("n   - setup dynamic test db\n");
    printf("N   - add Characteristic to test db\n");
    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

void update_advertisements(void){

    // update adv data
    memset(adv_data, 0, 32);
    adv_data_len = 0;
    
    // add "Flags"
    if (gap_discoverable){
        memcpy(adv_data, adv_general_discoverable, sizeof(adv_general_discoverable));
        adv_data_len += sizeof(adv_general_discoverable);
    } else {
        memcpy(adv_data, adv_non_discoverable, sizeof(adv_non_discoverable));
        adv_data_len += sizeof(adv_non_discoverable);
    }

    // append selected adv data
    memcpy(&adv_data[adv_data_len], advertisements[advertisement_index].data, advertisements[advertisement_index].len);
    adv_data_len += advertisements[advertisement_index].len;

    // set as adv + scan response data
    gap_advertisements_set_data(adv_data_len, adv_data);
    gap_scan_response_set_data(adv_data_len, adv_data);

    // update advertisement params
    uint8_t adv_type = gap_adv_type();
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    uint16_t adv_int_min = 0x800;
    uint16_t adv_int_max = 0x800;
    uint8_t filter_policy = 0;
    uint8_t channel_map = 0x07;
    switch (adv_type){
        case 0:
        case 2:
        case 3:
            gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, channel_map, filter_policy);
            break;
        case 1:
        case 4:
            gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, tester_address_type, tester_address, channel_map, filter_policy);
            break;
    }
}

static void update_auth_req(void){
    uint8_t auth_req = 0;
    if (sm_mitm_protection){
        auth_req |= SM_AUTHREQ_MITM_PROTECTION;
    }
    if (gap_bondable){
        auth_req |= SM_AUTHREQ_BONDING;
    }
    sm_set_authentication_requirements(auth_req);
}

static void gatt_hash_calculated(void * arg){
    printf("GATT Hash calculated: ");
    printf_hexdump(gatt_database_hash, 16);
}

static void stdin_process(char c){

    // passkey input
    if (ui_digits_for_passkey){
        if (c < '0' || c > '9') return;
        printf("%c", c);
        fflush(stdout);
        ui_passkey = ui_passkey * 10 + c - '0';
        ui_digits_for_passkey--;
        if (ui_digits_for_passkey == 0){
            printf("\nSending Passkey '%06x'\n", ui_passkey);
            sm_passkey_input(handle, ui_passkey);
        }
        return;
    }

    switch (c){
        case 'a':
            gap_advertisements = 0;
            gap_advertisements_enable(gap_advertisements);
            show_usage();
            break;
        case 'A':
            gap_advertisements = 1;
            gap_advertisements_enable(gap_advertisements);
            show_usage();
            break;
        case 'b':
            gap_bondable = 0;
            sm_set_authentication_requirements(SM_AUTHREQ_NO_BONDING);
            show_usage();
            break;
        case 'B':
            gap_bondable = 1;
            sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
            show_usage();
            break;
        case 'c':
            gap_connectable = 0;
            update_advertisements();
            break;
        case 'C':
            gap_connectable = 1;
            update_advertisements();
            break;
        case 'd':
            gap_discoverable = 0;
            update_advertisements();
            break;
        case 'D':
            gap_discoverable = 1;
            update_advertisements();
            break;
        case 'r':
            gap_random = 0;
            update_advertisements();
            break;
        case 'R':
            gap_random = 1;
            update_advertisements();
            break;
        case 'x':
            gap_directed_connectable = 0;
            update_advertisements();
            break;
        case 'X':
            gap_directed_connectable = 1;
            update_advertisements();
            break;
        case 'y':
            gap_scannable = 0;
            update_advertisements();
            break;
        case 'Y':
            gap_scannable = 1;
            update_advertisements();
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
            advertisement_index = c - '0';
            update_advertisements();
            break;
        case '+':
            advertisement_index = 10;
            update_advertisements();
            break;
        case '-':
            advertisement_index = 11;
            update_advertisements();
            break;
        case '&':
            advertisement_index = 12;
            update_advertisements();
            break;
        case '=':
            advertisement_index = 13;
            update_advertisements();
            break;
        case '/':
            advertisement_index = 14;
            update_advertisements();
            break;
        case '#':
            advertisement_index = 15;
            update_advertisements();
            break;
        case 's':
            printf("SM: sending security request\n");
            sm_send_security_request(handle);
            break;
        case 'e':
            sm_io_capabilities = "IO_CAPABILITY_DISPLAY_ONLY";
            sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
            show_usage();
            break;
        case 'f':
            sm_io_capabilities = "IO_CAPABILITY_DISPLAY_YES_NO";
            sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_YES_NO);
            show_usage();
            break;
        case 'g':
            sm_io_capabilities = "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
            sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
            show_usage();
            break;
        case 'h':
            sm_io_capabilities = "IO_CAPABILITY_KEYBOARD_ONLY";
            sm_set_io_capabilities(IO_CAPABILITY_KEYBOARD_ONLY);
            show_usage();
            break;
        case 'i':
            sm_io_capabilities = "IO_CAPABILITY_KEYBOARD_DISPLAY";
            sm_set_io_capabilities(IO_CAPABILITY_KEYBOARD_DISPLAY);
            show_usage();
            break;
        case 't':
            printf("Terminating connection\n");
            hci_send_cmd(&hci_disconnect, handle, 0x13);
            break;
        case 'z':
            printf("Sending l2cap connection update parameter request\n");
            gap_request_connection_parameter_update(handle, 50, 120, 0, 550);
            break;
        case 'l':
            att_default_value_long = 0;
            show_usage();
            break;
        case 'L':
            att_default_value_long = 1;
            show_usage();
            break;
        case 'p':
            gap_privacy = 0;
            show_usage();
            break;
        case 'P':
            gap_privacy = 1;
            show_usage();
            break;
        case 'o':
            sm_have_oob_data = 0;
            show_usage();
            break;
        case 'O':
            sm_have_oob_data = 1;
            show_usage();
            break;
        case 'k':
            sm_min_key_size = 7;
            sm_set_encryption_key_size_range(7, 16);
            show_usage();
            break;
        case 'K':
            sm_min_key_size = 16;
            sm_set_encryption_key_size_range(16, 16);
            show_usage();
            break;
        case 'm':
            sm_mitm_protection = 0;
            update_auth_req();
            show_usage();
            break;
        case 'M':
            sm_mitm_protection = 1;
            update_auth_req();
            show_usage();
            break;
        case 'j':
            printf("Create L2CAP Connection to %s\n", bd_addr_to_str(tester_address));
            hci_send_cmd(&hci_le_create_connection, 
                1000,      // scan interval: 625 ms
                1000,      // scan interval: 625 ms
                0,         // don't use whitelist
                0,         // peer address type: public
                tester_address,      // remote bd addr
                tester_address_type, // random or public
                80,        // conn interval min
                80,        // conn interval max (3200 * 0.625)
                0,         // conn latency
                2000,      // supervision timeout
                0,         // min ce length
                1000       // max ce length
                );
            break;
        case 'n':
            printf("Switch to dynamic database\n");
            dynamic_db = true;
            att_set_db(att_db_util_get_address());
            att_db_util_add_service_uuid16(ORG_BLUETOOTH_SERVICE_GENERIC_ATTRIBUTE);
            att_db_util_add_characteristic_uuid16(0x2b2a, ATT_PROPERTY_READ | ATT_PROPERTY_DYNAMIC, ATT_SECURITY_NONE, ATT_SECURITY_NONE, NULL, 0);
            att_db_util_hash_calc(&gatt_aes_cmac_context, gatt_database_hash, &gatt_hash_calculated, NULL);
            break;
        case 'N':
            printf("Adding Characteristic to dynamic database\n");
            att_db_util_add_service_uuid16(0xfff0);
            att_db_util_add_characteristic_uuid16(0xfff1, ATT_PROPERTY_READ | ATT_PROPERTY_DYNAMIC, ATT_SECURITY_NONE, ATT_SECURITY_NONE, NULL, 0);
            att_db_util_hash_calc(&gatt_aes_cmac_context, gatt_database_hash, &gatt_hash_calculated, NULL);
            break;
        default:
            show_usage();
            break;

    }
}

static int get_oob_data_callback(uint8_t address_type, bd_addr_t addr, uint8_t * oob_data){
    UNUSED(address_type);
    (void)addr;
    if(!sm_have_oob_data) return 0;
    memcpy(oob_data, sm_oob_data, 16);
    return 1;
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void) argv;
    (void) argc;

    printf("BTstack LE Peripheral starting up...\n");

    // set up l2cap_le
    l2cap_init();
    
    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING | SM_AUTHREQ_MITM_PROTECTION);

    // register for SM events
    sm_event_callback_registration.callback = &app_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

#ifdef ENABLE_GATT_OVER_CLASSIC
    // configure Classic GAP
    gap_set_local_name("GATT Counter BR/EDR 00:00:00:00:00:00");
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_set_security_level(LEVEL_0);
    gap_discoverable_control(1);
#endif

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    
    att_write_queue_init();
    att_attributes_init();
    att_server_register_packet_handler(app_packet_handler);

    // prepare for dynamic db construction
    att_db_util_init();

#ifdef ENABLE_GATT_OVER_CLASSIC
    // init SDP, create record for GATT and register with SDP
    sdp_init();
    memset(gatt_service_buffer, 0, sizeof(gatt_service_buffer));
    gatt_create_sdp_record(gatt_service_buffer, 0x10001, ATT_SERVICE_GATT_SERVICE_START_HANDLE, ATT_SERVICE_GATT_SERVICE_END_HANDLE);
    sdp_register_service(gatt_service_buffer);
    printf("SDP service record size: %u\n", de_get_len(gatt_service_buffer));
#endif

    btstack_stdin_setup(stdin_process);

    gap_random_address_set_update_period(5000);
    gap_random_address_set_mode(GAP_RANDOM_ADDRESS_TYPE_OFF);
    strcpy(gap_device_name, "BTstack");
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_io_capabilities =  "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
    sm_set_authentication_requirements(0);
    sm_register_oob_data_callback(get_oob_data_callback);
    sm_set_encryption_key_size_range(sm_min_key_size, 16);
    sm_test_set_irk(test_irk);

    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);

    // report gatt service handle range for SDP tests
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_GENERIC_ATTRIBUTE, &start_handle, &end_handle);
    btstack_assert(service_found != 0);
    printf("GATT Service range: 0x%04x - 0x%04x\n", start_handle, end_handle);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}

