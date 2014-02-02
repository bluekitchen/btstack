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
// BLE Peripheral Demo
//
//*****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <termios.h>

#include "btstack-config.h"

#include <btstack/run_loop.h>
#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "sm.h"
#include "att.h"
#include "att_server.h"
#include "gap_le.h"
#include "central_device_db.h"

#define HEARTBEAT_PERIOD_MS 1000

// test profile
#include "profile.h"

typedef enum {
    DISABLE_ADVERTISEMENTS   = 1 << 0,
    SET_ADVERTISEMENT_PARAMS = 1 << 1,
    SET_ADVERTISEMENT_DATA   = 1 << 2,
    SET_SCAN_RESPONSE_DATA   = 1 << 3,
    ENABLE_ADVERTISEMENTS    = 1 << 4,
} todo_t;
static todo_t todos = 0;

///------
static int advertisements_enabled = 0;
static int gap_advertisements = 0;
static int gap_discoverable = 0;
static int gap_connectable = 0;
static int gap_bondable = 0;
static int gap_directed_connectable = 0;
static int gap_privacy = 0;
static int gap_scannable = 0;
static char gap_device_name[20];
static uint16_t gap_appearance = 0;

static bd_addr_t gap_reconnection_address;

static int att_default_value_long = 0;

static uint8_t test_irk[] =  { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };

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

static timer_source_t heartbeat;
static uint8_t counter = 0;
static int update_client = 0;
static int client_configuration = 0;
static uint16_t client_configuration_handle;

static uint16_t handle = 0;

static void app_run();
static void show_usage();
static void update_advertisements();

// -> hier Adresse vom Tester eintragen für Directed Connectable Mode
// #define SKIP_ADVERTISEMENT_PARAMAS_UPDATE

#ifndef SKIP_ADVERTISEMENT_PARAMAS_UPDATE
static bd_addr_t tester_address = {0x00, 0x1B, 0xDC, 0x06, 0x07, 0x5F};
static int tester_address_type = 0;
#endif

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
};

static int advertisement_index = 2;

// att write queue engine

static const char default_value_long[]  = "abcdefghijklmnopqrstuvwxyz";
static const char default_value_short[] = "a";

#define ATT_VALUE_MAX_LEN 26

typedef struct {
    uint16_t handle;
    uint16_t len;
    uint8_t  value[ATT_VALUE_MAX_LEN];
} attribute_t;

#define ATT_NUM_WRITE_QUEUES 2
static attribute_t att_write_queues[ATT_NUM_WRITE_QUEUES];
#define ATT_NUM_ATTRIBUTES 10
static attribute_t att_attributes[ATT_NUM_ATTRIBUTES];

static void att_write_queue_init(){
    int i;
    for (i=0;i<ATT_NUM_WRITE_QUEUES;i++){
        att_write_queues[i].handle = 0;
    }
}

static int att_write_queue_for_handle(uint16_t handle){
    int i;
    for (i=0;i<ATT_NUM_WRITE_QUEUES;i++){
        if (att_write_queues[i].handle == handle){
            return i;
        }
    }
    for (i=0;i<ATT_NUM_WRITE_QUEUES;i++){
        if (att_write_queues[i].handle == 0){
            att_write_queues[i].handle = handle;
            memset(att_write_queues[i].value, 0, ATT_VALUE_MAX_LEN);
            att_write_queues[i].len = 0;
            return i;
        }
    }
    return -1;
}

static void att_attributes_init(){
    int i;
    for (i=0;i<ATT_NUM_ATTRIBUTES;i++){
        att_attributes[i].handle = 0;
    }
}

// handle == 0 finds free attribute
static int att_attribute_for_handle(uint16_t handle){
    int i;
    for (i=0;i<ATT_NUM_ATTRIBUTES;i++){
        if (att_attributes[i].handle == handle) {
            return i;
        }
    }
    return -1;
}


static void  heartbeat_handler(struct timer *ts){
    // restart timer
    run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(ts);

    counter++;
    update_client = 1;
    app_run();
} 

static void app_run(){
    if (!update_client) return;
    if (!att_server_can_send()) return;

    int result = -1;
    switch (client_configuration){
        case 0x01:
            printf("Notify value %u\n", counter);
            result = att_server_notify(client_configuration_handle - 1, &counter, 1);
            break;
        case 0x02:
            printf("Indicate value %u\n", counter);
            result = att_server_indicate(client_configuration_handle - 1, &counter, 1);
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
static uint16_t att_read_callback(uint16_t handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){

    printf("READ Callback, handle %04x, offset %u, buffer size %u\n", handle, offset, buffer_size);
    uint16_t  att_value_len;

    uint16_t uuid16 = att_uuid_for_handle(handle);
    switch (uuid16){
        case 0x2902:
            if (buffer) {
                buffer[0] = client_configuration;
            }
            return 1;
        case 0x2a00:
            att_value_len = strlen(gap_device_name);
            if (buffer) {
                memcpy(buffer, gap_device_name, att_value_len);
            }
            return att_value_len;        
        case 0x2a01:
            if (buffer){
                bt_store_16(buffer, 0, gap_appearance);
            }
            return 2;
        case 0x2A03:
            if (buffer) {
                bt_flip_addr(buffer, gap_reconnection_address);
            }
            return 6;

        // handle device name
        // handle appearance
        default:
            break;
    }

    // find attribute
    int index = att_attribute_for_handle(handle);
    uint8_t * att_value;
    if (index < 0){
        // not written before
        if (att_default_value_long){
            att_value = (uint8_t*) default_value_long;
            att_value_len  = strlen(default_value_long);
        } else {
            att_value = (uint8_t*) default_value_short;
            att_value_len  = strlen(default_value_short);
        }
    } else {
        att_value     = att_attributes[index].value;
        att_value_len = att_attributes[index].len;
    }
    printf("Attribute len %u, data: ", att_value_len);
    hexdump(att_value, att_value_len);

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
static int att_write_callback(uint16_t handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size, signature_t * signature){
    printf("WRITE Callback, handle %04x, mode %u, offset %u, data: ", handle, transaction_mode, offset);
    hexdump(buffer, buffer_size);

    uint16_t uuid16 = att_uuid_for_handle(handle);
    switch (uuid16){
        case 0x2902:
            client_configuration = buffer[0];
            client_configuration_handle = handle;
            printf("Client Configuration set to %u for handle %04x\n", client_configuration, handle);
            return 0;   // ok
        case 0x2a00:
            memcpy(gap_device_name, buffer, buffer_size);
            gap_device_name[buffer_size]=0;
            printf("Setting device name to '%s'\n", gap_device_name);
            return 0;
        case 0x2a01:
            gap_appearance = READ_BT_16(buffer, 0);
            printf("Setting appearance to 0x%04x'\n", gap_appearance);
            return 0;
        case 0x2A03:
            bt_flip_addr(gap_reconnection_address, buffer);
            printf("Setting Reconnection Address to %s\n", bd_addr_to_str(gap_reconnection_address));
            return 0;
        // handle device name
        // handle appearance
    }

    // check transaction mode
    int attributes_index;
    int writes_index;
    switch (transaction_mode){
        case ATT_TRANSACTION_MODE_NONE:
            attributes_index = att_attribute_for_handle(handle);
            if (attributes_index < 0){
                attributes_index = att_attribute_for_handle(0);
                if (attributes_index < 0) return 0;    // ok, but we couldn't store it (our fault)
                att_attributes[attributes_index].handle = handle;
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
            break;
        case ATT_TRANSACTION_MODE_ACTIVE:
            writes_index = att_write_queue_for_handle(handle);
            if (writes_index < 0)                            return ATT_ERROR_PREPARE_QUEUE_FULL;
            if (offset > att_write_queues[writes_index].len) return ATT_ERROR_INVALID_OFFSET;
            if (buffer_size + offset > ATT_VALUE_MAX_LEN)    return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
            att_write_queues[writes_index].len = buffer_size + offset;
            memcpy(&(att_write_queues[writes_index].value[offset]), buffer, buffer_size);
            break;
        case ATT_TRANSACTION_MODE_EXECUTE:
            for (writes_index=0 ; writes_index<ATT_NUM_WRITE_QUEUES ; writes_index++){
                handle = att_write_queues[writes_index].handle;
                if (handle == 0) continue;
                attributes_index = att_attribute_for_handle(handle);
                if (attributes_index < 0){
                    attributes_index = att_attribute_for_handle(0);
                    if (attributes_index < 0) continue;
                    att_attributes[attributes_index].handle = handle;
                }
                att_attributes[attributes_index].len = att_write_queues[writes_index].len;
                memcpy(att_attributes[attributes_index].value, att_write_queues[writes_index].value, att_write_queues[writes_index].len);
            }
            att_write_queue_init();
            break;
        case ATT_TRANSACTION_MODE_CANCEL:
            att_write_queue_init();
            break;
    }
    return 0;
}

static uint8_t gap_adv_type(){
    if (gap_scannable) return 0x02;
    if (gap_directed_connectable) return 0x01;
    if (!gap_connectable) return 0x03;
    return 0x00;
}

static void gap_run(){
    if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;

    if (todos & DISABLE_ADVERTISEMENTS){
        todos &= ~DISABLE_ADVERTISEMENTS;
        printf("GAP_RUN: disable advertisements\n");
        advertisements_enabled = 0;
        hci_send_cmd(&hci_le_set_advertise_enable, 0);
        return;
    }    

    if (todos & SET_ADVERTISEMENT_DATA){
        printf("GAP_RUN: set advertisement data\n");
        todos &= ~SET_ADVERTISEMENT_DATA;
        hci_send_cmd(&hci_le_set_advertising_data, adv_data_len, adv_data);
        return;
    }    

    if (todos & SET_ADVERTISEMENT_PARAMS){
        todos &= ~SET_ADVERTISEMENT_PARAMS;
        uint8_t adv_type = gap_adv_type();
        bd_addr_t null_addr;
        memset(null_addr, 0, 6);
        uint16_t adv_int_min = 0x800;
        uint16_t adv_int_max = 0x800;
        switch (adv_type){
            case 0:
            case 2:
            case 3:
                hci_send_cmd(&hci_le_set_advertising_parameters, adv_int_min, adv_int_max, adv_type, gap_privacy, 0, &null_addr, 0x07, 0x00);
                break;
            case 1:
            case 4:
                hci_send_cmd(&hci_le_set_advertising_parameters, adv_int_min, adv_int_max, adv_type, gap_privacy, tester_address_type, &tester_address, 0x07, 0x00);
                break;
        }
        return;
    }    

    if (todos & SET_SCAN_RESPONSE_DATA){
        printf("GAP_RUN: set scan response data\n");
        todos &= ~SET_SCAN_RESPONSE_DATA;
        hci_send_cmd(&hci_le_set_scan_response_data, adv_data_len, adv_data);
        // hack for menu
        if ((todos & ENABLE_ADVERTISEMENTS) == 0) show_usage();
        return;
    }    

    if (todos & ENABLE_ADVERTISEMENTS){
        printf("GAP_RUN: enable advertisements\n");
        todos &= ~ENABLE_ADVERTISEMENTS;
        advertisements_enabled = 1;
        hci_send_cmd(&hci_le_set_advertise_enable, 1);
        show_usage();
        return;
    }
}

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    
    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (packet[2] == HCI_STATE_WORKING) {
                        printf("SM Init completed\n");
                        todos = SET_ADVERTISEMENT_PARAMS | SET_ADVERTISEMENT_DATA | SET_SCAN_RESPONSE_DATA | ENABLE_ADVERTISEMENTS;
                        update_advertisements();
                        gap_run();
                    }
                    break;
                
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            advertisements_enabled = 0;
                            handle = READ_BT_16(packet, 4);
                            printf("Connection handle 0x%04x\n", handle);
                            // request connection parameter update - test parameters
                            // l2cap_le_request_connection_parameter_update(READ_BT_16(packet, 4), 20, 1000, 100, 100);
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    if (!advertisements_enabled == 0 && gap_discoverable){
                        todos = ENABLE_ADVERTISEMENTS;
                    }
                    att_attributes_init();
                    att_write_queue_init();
                    break;
                    
                case SM_PASSKEY_INPUT_NUMBER: {
                    // display number
                    sm_event_t * event = (sm_event_t *) packet;
                    memcpy(master_address, event->address, 6);
                    master_addr_type = event->addr_type;
                    printf("\nGAP Bonding %s (%u): Enter 6 digit passkey: '", bd_addr_to_str(master_address), master_addr_type);
                    fflush(stdout);
                    ui_passkey = 0;
                    ui_digits_for_passkey = 6;
                    break;
                }

                case SM_PASSKEY_DISPLAY_NUMBER: {
                    // display number
                    sm_event_t * event = (sm_event_t *) packet;
                    printf("\nGAP Bonding %s (%u): Display Passkey '%06u\n", bd_addr_to_str(master_address), master_addr_type, event->passkey);
                    break;
                }

                case SM_PASSKEY_DISPLAY_CANCEL: 
                    printf("\nGAP Bonding %s (%u): Display cancel\n", bd_addr_to_str(master_address), master_addr_type);
                    break;

                case SM_AUTHORIZATION_REQUEST: {
                    // auto-authorize connection if requested
                    sm_event_t * event = (sm_event_t *) packet;
                    sm_authorization_grant(event->addr_type, event->address);
                    break;
                }
                case ATT_HANDLE_VALUE_INDICATION_COMPLETE:
                    printf("ATT_HANDLE_VALUE_INDICATION_COMPLETE status %u\n", packet[2]);
                    break;

                default:
                    break;
            }
    }
    gap_run();
}

void show_usage(){
    printf("\n--- CLI for LE Peripheral ---\n");
    printf("GAP: discoverable %u, connectable %u, bondable %u, directed connectable %u, privacy %u, ads enabled %u, adv type %u \n",
        gap_discoverable, gap_connectable, gap_bondable, gap_directed_connectable, gap_privacy, gap_advertisements, gap_adv_type());
    printf("ADV: "); hexdump(adv_data, adv_data_len);
    printf("SM: %s, MITM protection %u, OOB data %u, key range [%u..16]\n",
        sm_io_capabilities, sm_mitm_protection, sm_have_oob_data, sm_min_key_size);
    printf("Default value: '%s'\n", att_default_value_long ? default_value_long : default_value_short);
    printf("---\n");
    printf("a/A - advertisements off/on\n");
    printf("b/B - bondable off/on\n");
    printf("c/C - connectable off\n");
    printf("d/D - discoverable off\n");
    printf("p/P - privacy off\n");
    printf("x/X - directed connectable off\n");
    printf("y/Y - scannable on/off\n");
    printf("---\n");
    printf("1   - AD Manufacturer Data    | 6 - AD Services\n");
    printf("2   - AD Local Name           | 7 - AD Slave Preferred Connection Interval Range\n");
    printf("3   - AD flags                | 8 - AD Tx Power Level\n");
    printf("4   - AD Service Data         | 9 - AD SM OOB\n");
    printf("5   - AD Service Solicitation | 0 - AD SM TK\n");
    printf("---\n");
    printf("l/L - default attribute value '%s'/'%s'\n", default_value_short, default_value_long);
    printf("s   - send security request\n");
    printf("z   - send Connection Parameter Update Request\n");
    printf("t   - terminate connection\n");
    printf("---\n");
    printf("e   - IO_CAPABILITY_DISPLAY_ONLY\n");
    printf("f   - IO_CAPABILITY_DISPLAY_YES_NO\n");
    printf("g   - IO_CAPABILITY_NO_INPUT_NO_OUTPUT\n");
    printf("h   - IO_CAPABILITY_KEYBOARD_ONLY\n");
    printf("i   - IO_CAPABILITY_KEYBOARD_DISPLAY\n");
    printf("o/O - OOB data off/on ('%s')\n", sm_oob_data);
    printf("m/M - MITM protection off\n");
    printf("k/k - encryption key range [7..16]/[16..16]\n");
    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

void update_advertisements(){

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

    todos = SET_ADVERTISEMENT_PARAMS | SET_ADVERTISEMENT_DATA | SET_SCAN_RESPONSE_DATA;

    // disable advertisements, if thez are active
    if (advertisements_enabled){
        todos |= DISABLE_ADVERTISEMENTS;
    }

    // enable advertisements, if they should be on
    if (gap_advertisements) {
        todos |= ENABLE_ADVERTISEMENTS;
    }

    gap_run();
}

void update_auth_req(){
    uint8_t auth_req = 0;
    if (sm_mitm_protection){
        auth_req |= SM_AUTHREQ_MITM_PROTECTION;
    }
    if (gap_bondable){
        auth_req |= SM_AUTHREQ_BONDING;
    }
    sm_set_authentication_requirements(auth_req);
}

int  stdin_process(struct data_source *ds){
    char buffer;
    read(ds->fd, &buffer, 1);

    // passkey input
    if (ui_digits_for_passkey){
        if (buffer < '0' || buffer > '9') return 0;
        printf("%c", buffer);
        fflush(stdout);
        ui_passkey = ui_passkey * 10 + buffer - '0';
        ui_digits_for_passkey--;
        if (ui_digits_for_passkey == 0){
            printf("\nSending Passkey '%06x'\n", ui_passkey);
            sm_passkey_input(master_addr_type, master_address, ui_passkey);
        }
        return 0;
    }

    switch (buffer){
        case 'a':
            gap_advertisements = 0;
            update_advertisements();
            show_usage();
            break;
        case 'A':
            gap_advertisements = 1;
            update_advertisements();
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
        case 'p':
            gap_privacy = 0;
            update_advertisements();
            break;
        case 'P':
            gap_privacy = 1;
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
            advertisement_index = buffer - '0';
            update_advertisements();
            break;
        case 's':
            printf("SM: sending security request\n");
            sm_send_security_request();
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
            l2cap_le_request_connection_parameter_update(handle, 50, 120, 0, 550);
            break;
        case 'l':
            att_default_value_long = 0;
            show_usage();
            break;
        case 'L':
            att_default_value_long = 1;
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
        default:
            show_usage();
            break;

    }
    return 0;
}

static int get_oob_data_callback(uint8_t addres_type, bd_addr_t * addr, uint8_t * oob_data){
    if(!sm_have_oob_data) return 0;
    memcpy(oob_data, sm_oob_data, 16);
    return 1;
}

static data_source_t stdin_source;
void setup_cli(){

    struct termios term = {0};
    if (tcgetattr(0, &term) < 0)
            perror("tcsetattr()");
    term.c_lflag &= ~ICANON;
    term.c_lflag &= ~ECHO;
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &term) < 0)
            perror("tcsetattr ICANON");

    stdin_source.fd = 0; // stdin
    stdin_source.process = &stdin_process;
    run_loop_add_data_source(&stdin_source);
}

void setup(void){
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
        
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);

    // init HCI
    hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t  * config    = NULL;
    bt_control_t       * control   = NULL;
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, config, control, remote_db);

    // set up l2cap_le
    l2cap_init();
    
    // setup central device db
    central_device_db_init();

    // setup SM: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING | SM_AUTHREQ_MITM_PROTECTION); 

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    
    att_write_queue_init();
    att_attributes_init();
    att_server_register_packet_handler(app_packet_handler);

    att_dump_attributes();
}

int main(void)
{
    printf("BTstack LE Peripheral starting up...\n");
    setup();

    setup_cli();

    gap_random_address_set_update_period(300000);
    gap_random_address_set_mode(GAP_RANDOM_ADDRESS_RESOLVABLE);
    strcpy(gap_device_name, "BTstack");
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_io_capabilities =  "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
    sm_set_authentication_requirements(0);
    sm_register_oob_data_callback(get_oob_data_callback);
    sm_set_encryption_key_size_range(sm_min_key_size, 16);
    sm_test_set_irk(test_irk);

    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(&heartbeat);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    // go!
    run_loop_execute(); 
    
    // happy compiler!
    return 0;
}
