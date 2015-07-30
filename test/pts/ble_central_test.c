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

// *****************************************************************************
//
// BLE Central PTS Test
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
#include "le_device_db.h"
#include "stdin_support.h"
#include "ad_parser.h"

// test profile
#include "profile.h"

// Non standard IXIT
#define PTS_USES_RECONNECTION_ADDRESS_FOR_ITSELF

typedef enum {
    CENTRAL_IDLE,
    CENTRAL_W4_NAME_QUERY_COMPLETE,
    CENTRAL_W4_NAME_VALUE,
    CENTRAL_W4_RECONNECTION_ADDRESS_QUERY_COMPLETE,
    CENTRAL_W4_PERIPHERAL_PRIVACY_FLAG_QUERY_COMPLETE,
} central_state_t;

typedef struct advertising_report {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} advertising_report_t;

static uint8_t test_irk[] =  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static int gap_privacy = 0;
/* static */ int gap_bondable = 0;
static char gap_device_name[20];
static int gap_connectable = 0;

static char * sm_io_capabilities = NULL;
static int sm_mitm_protection = 0;
static int sm_have_oob_data = 0;
static uint8_t * sm_oob_data = (uint8_t *) "0123456789012345"; // = { 0x30...0x39, 0x30..0x35}
static int sm_min_key_size = 7;

static int peer_addr_type;
static bd_addr_t peer_address;
static int ui_passkey = 0;
static int ui_digits_for_passkey = 0;

static uint16_t handle = 0;
static uint16_t gc_id;

static bd_addr_t public_pts_address = {0x00, 0x1B, 0xDC, 0x07, 0x32, 0xef};
static int       public_pts_address_type = 0;
static bd_addr_t current_pts_address;
static int       current_pts_address_type;
static int       reconnection_address_set = 0;
static bd_addr_t our_private_address;

static central_state_t central_state = CENTRAL_IDLE;
static le_characteristic_t gap_name_characteristic;
static le_characteristic_t gap_reconnection_address_characteristic;
static le_characteristic_t gap_peripheral_privacy_flag_characteristic;

static void show_usage();
///

static void printUUID(uint8_t * uuid128, uint16_t uuid16){
    if (uuid16){
        printf("%04x",uuid16);
    } else {
        printUUID128(uuid128);
    }
}

void dump_characteristic(le_characteristic_t * characteristic){
    printf("    * characteristic: [0x%04x-0x%04x-0x%04x], properties 0x%02x, uuid ",
                            characteristic->start_handle, characteristic->value_handle, characteristic->end_handle, characteristic->properties);
    printUUID(characteristic->uuid128, characteristic->uuid16);
    printf("\n");
}

void dump_service(le_service_t * service){
    printf("    * service: [0x%04x-0x%04x], uuid ", service->start_group_handle, service->end_group_handle);
    printUUID(service->uuid128, service->uuid16);
    printf("\n");
}

const char * ad_event_types[] = {
    "Connectable undirected advertising",
    "Connectable directed advertising",
    "Scannable undirected advertising",
    "Non connectable undirected advertising",
    "Scan Response"
};

static void handle_advertising_event(uint8_t * packet, int size){
    // filter PTS
    bd_addr_t addr;
    bt_flip_addr(addr, &packet[4]);
    if (memcmp(addr, public_pts_address, 6)) return;
    printf("Advertisement: %s, ", ad_event_types[packet[2]]);
    int adv_size = packet[11];
    uint8_t * adv_data = &packet[12];

    // check flags
    ad_context_t context;
    for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type = ad_iterator_get_data_type(&context);
        // uint8_t size      = ad_iterator_get_data_len(&context);
        uint8_t * data    = ad_iterator_get_data(&context);
        switch (data_type){
            case 1: // AD_FLAGS
                if (*data & 1) printf("LE Limited Discoverable Mode, ");
                if (*data & 2) printf("LE General Discoverable Mode, ");
                break;
            default:
                break;
        }
    }

    // dump data
    printf("Data: ");
    printf_hexdump(adv_data, adv_size);
}

static uint8_t gap_adv_type(void){
    // if (gap_scannable) return 0x02;
    // if (gap_directed_connectable) return 0x01;
    if (!gap_connectable) return 0x03;
    return 0x00;
}

static void update_advertisment_params(void){
    uint8_t adv_type = gap_adv_type();
    printf("GAP: Connectable = %u -> advertising_type %u (%s)\n", gap_connectable, adv_type, ad_event_types[adv_type]);
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    uint16_t adv_int_min = 0x800;
    uint16_t adv_int_max = 0x800;
    switch (adv_type){
        case 0:
        case 2:
        case 3:
            gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
            break;
        case 1:
        case 4:
            gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, public_pts_address_type, public_pts_address, 0x07, 0x00);
            break;
        }
}

static void gap_run(void){
    if (!hci_can_send_command_packet_now()) return;
}

void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (packet[2] == HCI_STATE_WORKING) {
                        printf("SM Init completed\n");
                        show_usage();
                        gap_run();
                    }
                    break;
                
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            handle = READ_BT_16(packet, 4);
                            printf("Connection complete, handle 0x%04x\n", handle);
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    break;
                    
                case SM_PASSKEY_INPUT_NUMBER: {
                    // display number
                    sm_event_t * event = (sm_event_t *) packet;
                    memcpy(peer_address, event->address, 6);
                    peer_addr_type = event->addr_type;
                    printf("\nGAP Bonding %s (%u): Enter 6 digit passkey: '", bd_addr_to_str(peer_address), peer_addr_type);
                    fflush(stdout);
                    ui_passkey = 0;
                    ui_digits_for_passkey = 6;
                    break;
                }

                case SM_PASSKEY_DISPLAY_NUMBER: {
                    // display number
                    sm_event_t * event = (sm_event_t *) packet;
                    printf("\nGAP Bonding %s (%u): Display Passkey '%06u\n", bd_addr_to_str(peer_address), peer_addr_type, event->passkey);
                    break;
                }

                case SM_PASSKEY_DISPLAY_CANCEL: 
                    printf("\nGAP Bonding %s (%u): Display cancel\n", bd_addr_to_str(peer_address), peer_addr_type);
                    break;

                case SM_AUTHORIZATION_REQUEST: {
                    // auto-authorize connection if requested
                    sm_event_t * event = (sm_event_t *) packet;
                    sm_authorization_grant(event->addr_type, event->address);
                    break;
                }

                case GAP_LE_ADVERTISING_REPORT:
                    handle_advertising_event(packet, size);
                    break;
                default:
                    break;
            }
    }
    gap_run();
}

void use_public_pts_address(void){
    memcpy(current_pts_address, public_pts_address, 6);
    current_pts_address_type = public_pts_address_type;                    
}

void handle_gatt_client_event(le_event_t * event){
    le_characteristic_value_event_t * value;
    uint8_t address_type;
    uint8_t privacy_flag = 0;
    bd_addr_t flipped_address;
    switch(event->type){
        case GATT_SERVICE_QUERY_RESULT:
            // service = ((le_service_event_t *) event)->service;
            // dump_service(&service);
            break;
        case GATT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            value = (le_characteristic_value_event_t *) event;
            switch (central_state){
                case CENTRAL_W4_NAME_VALUE:
                    central_state = CENTRAL_IDLE;
                    value->blob[value->blob_length] = 0;
                    printf("GAP Service: Device Name: %s\n", value->blob);
                    break;
                default:
                    break;
            }
            // printf("\ntest client - CHARACTERISTIC for SERVICE ");
            // printUUID128(service.uuid128); printf("\n");
            break;
        case GATT_QUERY_COMPLETE:
            switch (central_state){
                case CENTRAL_W4_NAME_QUERY_COMPLETE:
                    central_state = CENTRAL_W4_NAME_VALUE;
                    gatt_client_read_value_of_characteristic(gc_id, handle, &gap_name_characteristic);
                    break;
                case CENTRAL_W4_RECONNECTION_ADDRESS_QUERY_COMPLETE:
                    central_state = CENTRAL_IDLE;
                    hci_le_advertisement_address(&address_type, our_private_address);
                    printf("Our private address: %s\n", bd_addr_to_str(our_private_address));
                    bt_flip_addr(flipped_address, our_private_address);
                    gatt_client_write_value_of_characteristic(gc_id, handle, gap_reconnection_address_characteristic.value_handle, 6, flipped_address);
                    reconnection_address_set = 1;
#ifdef PTS_USES_RECONNECTION_ADDRESS_FOR_ITSELF
                    memcpy(current_pts_address, our_private_address, 6);
                    current_pts_address_type = 1;
#endif
                    break;
                case CENTRAL_W4_PERIPHERAL_PRIVACY_FLAG_QUERY_COMPLETE:
                    central_state = CENTRAL_IDLE;
                    gatt_client_write_value_of_characteristic(gc_id, handle, gap_peripheral_privacy_flag_characteristic.value_handle, 1, &privacy_flag);
                    use_public_pts_address();
                    printf("Peripheral Privacy Flag set to FALSE, connecting to public PTS address again\n");
                    break;
                default:
                    break;
            }
            break;
        case GATT_CHARACTERISTIC_QUERY_RESULT:
            switch (central_state) {
                case CENTRAL_W4_NAME_QUERY_COMPLETE:
                    gap_name_characteristic = ((le_characteristic_event_t *) event)->characteristic;
                    printf("GAP Name Characteristic found, value handle: 0x04%x\n", gap_name_characteristic.value_handle);
                    break;
                case CENTRAL_W4_RECONNECTION_ADDRESS_QUERY_COMPLETE:
                    gap_reconnection_address_characteristic = ((le_characteristic_event_t *) event)->characteristic;
                    printf("GAP Reconnection Address Characteristic found, value handle: 0x04%x\n", gap_reconnection_address_characteristic.value_handle);
                    break;
                case CENTRAL_W4_PERIPHERAL_PRIVACY_FLAG_QUERY_COMPLETE:
                    gap_peripheral_privacy_flag_characteristic = ((le_characteristic_event_t *) event)->characteristic;
                    printf("GAP Peripheral Privacy Flag Characteristic found, value handle: 0x04%x\n", gap_peripheral_privacy_flag_characteristic.value_handle);
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

uint16_t value_handle = 1;
uint16_t attribute_size = 1;
int scanning_active = 0;

void show_usage(void){
    printf("\e[1;1H\e[2J");
    printf("--- CLI for LE Central ---\n");
    printf("GAP: connectable %u\n", gap_connectable);
    printf("SM: %s, MITM protection %u, OOB data %u, key range [%u..16]\n",
        sm_io_capabilities, sm_mitm_protection, sm_have_oob_data, sm_min_key_size);
    printf("PTS: addr type %u, addr %s\n", current_pts_address_type, current_pts_address);
    printf("Privacy %u\n", gap_privacy);
    printf("Device name: %s\n", gap_device_name);
    printf("Value Handle: %x\n", value_handle);
    printf("Attribute Size: %u\n", attribute_size);
    printf("---\n");
    printf("c/C - connectable off\n");
    printf("---\n");
    printf("1   - enable privacy using random non-resolvable private address\n");
    printf("2   - clear Peripheral Privacy Flag on PTS\n");
    printf("s/S - passive/active scanning\n");
    printf("a   - enable Advertisements\n");
    printf("n   - query GAP Device Name\n");
    printf("o   - set GAP Reconnection Address\n");
    printf("t   - terminate connection, stop connecting\n");
    printf("p   - auto connect to PTS\n");
    printf("P   - direct connect to PTS\n");
    printf("z   - Update L2CAP Connection Parameters\n");
    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

int  stdin_process(struct data_source *ds){
    char buffer;
    read(ds->fd, &buffer, 1);
    int res;

    // passkey input
    if (ui_digits_for_passkey){
        if (buffer < '0' || buffer > '9') return 0;
        printf("%c", buffer);
        fflush(stdout);
        ui_passkey = ui_passkey * 10 + buffer - '0';
        ui_digits_for_passkey--;
        if (ui_digits_for_passkey == 0){
            printf("\nSending Passkey '%06x'\n", ui_passkey);
            sm_passkey_input(peer_addr_type, peer_address, ui_passkey);
        }
        return 0;
    }

    switch (buffer){
        case '1':
            printf("Enabling non-resolvable private address\n");
            gap_random_address_set_mode(GAP_RANDOM_ADDRESS_NON_RESOLVABLE);
            update_advertisment_params(); 
            gap_privacy = 1;
            break;
        case 'a':
            hci_send_cmd(&hci_le_set_advertise_enable, 1);
            break;
        case 'c':
            gap_connectable = 1;
            update_advertisment_params();
            break;
        case 'C':
            gap_connectable = 0;
            update_advertisment_params();
            break;
        case 'n':
            central_state = CENTRAL_W4_NAME_QUERY_COMPLETE;
            gatt_client_discover_characteristics_for_handle_range_by_uuid16(gc_id, handle, 1, 0xffff, GAP_DEVICE_NAME_UUID);
            break;
        case '2':
            central_state = CENTRAL_W4_PERIPHERAL_PRIVACY_FLAG_QUERY_COMPLETE;
            gatt_client_discover_characteristics_for_handle_range_by_uuid16(gc_id, handle, 1, 0xffff, GAP_PERIPHERAL_PRIVACY_FLAG);
            break;
        case 'o':
            central_state = CENTRAL_W4_RECONNECTION_ADDRESS_QUERY_COMPLETE;
            gatt_client_discover_characteristics_for_handle_range_by_uuid16(gc_id, handle, 1, 0xffff, GAP_RECONNECTION_ADDRESS_UUID);
            break;
        case 'p':
            res = gap_auto_connection_start(current_pts_address_type, current_pts_address);
            printf("Auto Connection Establishment to type %u, addr %s -> %x\n", current_pts_address_type, bd_addr_to_str(current_pts_address), res);
            break;
        case 'P':
            le_central_connect(current_pts_address, current_pts_address_type);
            printf("Direct Connection Establishment to type %u, addr %s\n", current_pts_address_type, bd_addr_to_str(current_pts_address));
            break;
        case 's':
            if (scanning_active){
                le_central_stop_scan();
                scanning_active = 0;
                break;
            }
            printf("Start passive scanning\n");
            le_central_set_scan_parameters(0, 48, 48);
            le_central_start_scan();
            scanning_active = 1;
            break;
        case 'S':
            if (scanning_active){
                printf("Stop scanning\n");
                le_central_stop_scan();
                scanning_active = 0;
                break;
            }
            printf("Start active scanning\n");
            le_central_set_scan_parameters(1, 48, 48);
            le_central_start_scan();
            scanning_active = 1;
            break;
        case 't':
            printf("Terminating connection\n");
            hci_send_cmd(&hci_disconnect, handle, 0x13);
            gap_auto_connection_stop_all();
            le_central_connect_cancel();
            break;
        case 'z':
            printf("Updating l2cap connection parameters\n");
            gap_update_connection_parameters(handle, 50, 120, 0, 550);
            break;
        default:
            show_usage();
            break;

    }
    return 0;
}

static int get_oob_data_callback(uint8_t addres_type, bd_addr_t addr, uint8_t * oob_data){
    if(!sm_have_oob_data) return 0;
    memcpy(oob_data, sm_oob_data, 16);
    return 1;
}


// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(uint16_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){

    printf("READ Callback, handle %04x, offset %u, buffer size %u\n", handle, offset, buffer_size);
    uint16_t  att_value_len;

    uint16_t uuid16 = att_uuid_for_handle(handle);
    switch (uuid16){
        case 0x2a00:
            att_value_len = strlen(gap_device_name);
            if (buffer) {
                memcpy(buffer, gap_device_name, att_value_len);
            }
            return att_value_len;        
        default:
            break;
    }
    return 0;
}


void setup(void){

}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    
    printf("BTstack LE Peripheral starting up...\n");

    strcpy(gap_device_name, "BTstack");

    // set up l2cap_le
    l2cap_init();
    
    // Setup SM: Display only
    sm_init();
    sm_register_packet_handler(app_packet_handler);
    sm_register_oob_data_callback(get_oob_data_callback);
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_io_capabilities =  "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
    sm_set_authentication_requirements(0);
    sm_set_encryption_key_size_range(sm_min_key_size, 16);
    sm_test_set_irk(test_irk);

    // setup GATT Client
    gatt_client_init();
    gc_id = gatt_client_register_packet_handler(handle_gatt_client_event);;

    // Setup ATT/GATT Server
    att_server_init(profile_data, att_read_callback, NULL);    
    att_server_register_packet_handler(app_packet_handler);

    // Setup LE Device DB
    le_device_db_init();

    // set adv params
    update_advertisment_params();

    memcpy(current_pts_address, public_pts_address, 6);
    current_pts_address_type = public_pts_address_type;

    // allow foor terminal input
    btstack_stdin_setup(stdin_process);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}
