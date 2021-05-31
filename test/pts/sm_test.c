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

#define BTSTACK_FILE__ "sm_test.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack_debug.h>

#include "btstack_config.h"

#include "ad_parser.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "ble/le_device_db.h"
#include "ble/sm.h"
#include "btstack_event.h"
#include "gap.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "btstack_stdin.h"

// Non standard IXIT
#define PTS_USES_RECONNECTION_ADDRESS_FOR_ITSELF
#define PTS_UUID128_REPRESENTATION

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
static int gap_bondable = 1;
static int gap_connectable = 0;

static char * sm_io_capabilities = NULL;
static bool sm_le_secure_connections = 0;
static int sm_mitm_protection = 0;
static int sm_have_oob_data = 0;
static uint8_t * sm_oob_data_A = (uint8_t *) "0123456789012345"; // = { 0x30...0x39, 0x30..0x35}
static uint8_t * sm_oob_data_B = (uint8_t *) "3333333333333333"; // = { 0x30...0x39, 0x30..0x35}
static int sm_min_key_size = 7;

static int ui_passkey = 0;
static int ui_digits_for_passkey = 0;
static uint16_t handle = 0;

static bd_addr_t public_pts_address = {0x00, 0x1B, 0xDC, 0x08, 0xe2, 0x5c};
static int       public_pts_address_type = 0;
static bd_addr_t current_pts_address;
static int       current_pts_address_type;
static int       reconnection_address_set = 0;

static uint8_t signed_write_value[] = { 0x12 };
static int le_device_db_index;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static void show_usage(void);
///

const char * ad_event_types[] = {
    "Connectable undirected advertising",
    "Connectable directed advertising",
    "Scannable undirected advertising",
    "Non connectable undirected advertising",
    "Scan Response"
};

static void handle_advertising_event(uint8_t * packet, int size){
    UNUSED(size);
    // filter PTS
    bd_addr_t addr;
    gap_event_advertising_report_get_address(packet, addr);
    uint8_t addr_type = gap_event_advertising_report_get_address_type(packet);
    // always request address resolution
    sm_address_resolution_lookup(addr_type, addr);

    // ignore advertisement from devices other than pts
    // if (memcmp(addr, current_pts_address, 6)) return;
    uint8_t adv_event_type = gap_event_advertising_report_get_advertising_event_type(packet);
    printf("Advertisement: %s - %s, ", bd_addr_to_str(addr), ad_event_types[adv_event_type]);
    int adv_size = gap_event_advertising_report_get_data_length(packet);
    const uint8_t * adv_data = gap_event_advertising_report_get_data(packet);

    // check flags
    ad_context_t context;
    for (ad_iterator_init(&context, adv_size, (uint8_t *)adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type = ad_iterator_get_data_type(&context);
        // uint8_t size      = ad_iterator_get_data_len(&context);
        const uint8_t * data = ad_iterator_get_data(&context);
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

static void update_advertisement_params(void){
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
        default:
            btstack_assert(false);
            break;
    }
}

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    uint16_t aHandle;
    bd_addr_t event_address;

    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        // add bonded device with IRK 0x00112233..FF for gap-conn-prda-bv-2
                        uint8_t pts_irk[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
                        le_device_db_add(public_pts_address_type, public_pts_address, pts_irk);
                        // ready
                        printf("SM Test ready\n");
                        show_usage();
                    }
                    break;

                case HCI_EVENT_CONNECTION_COMPLETE:
                    if (hci_event_connection_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
                        handle = hci_event_connection_complete_get_connection_handle(packet);
                        printf("BR/EDR Connection complete, handle 0x%04x\n", handle);
                    }
                    break;

                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                            printf("Connection complete, handle 0x%04x\n", handle);
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    aHandle = little_endian_read_16(packet, 3);
                    printf("Disconnected from handle 0x%04x\n", aHandle);
                    break;
                    
                case SM_EVENT_PASSKEY_INPUT_NUMBER: 
                    // store peer address for input
                    printf("\nGAP Bonding: Enter 6 digit passkey: '");
                    fflush(stdout);
                    ui_passkey = 0;
                    ui_digits_for_passkey = 6;
                    break;

                case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
                    printf("\nGAP Bonding: Display Passkey '%06u\n", little_endian_read_32(packet, 11));
                    break;

                case SM_EVENT_PASSKEY_DISPLAY_CANCEL: 
                    printf("\nGAP Bonding: Display cancel\n");
                    break;

                case SM_EVENT_JUST_WORKS_REQUEST:
                    // auto-authorize connection if requested
                    sm_just_works_confirm(little_endian_read_16(packet, 2));
                    printf("Just Works request confirmed\n");
                    break;

                case SM_EVENT_AUTHORIZATION_REQUEST:
                    // auto-authorize connection if requested
                    sm_authorization_grant(little_endian_read_16(packet, 2));
                    break;

                case GAP_EVENT_ADVERTISING_REPORT:
                    handle_advertising_event(packet, size);
                    break;

                case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
                    reverse_bd_addr(&packet[5], event_address);
                    // skip already detected pts
                    if (memcmp(event_address, current_pts_address, 6) == 0) break;
                    memcpy(current_pts_address, event_address, 6);
                    current_pts_address_type =  packet[4];
                    le_device_db_index       =  little_endian_read_16(packet, 11);
                    printf("Address resolving succeeded: resolvable address %s, addr type %u\n",
                        bd_addr_to_str(current_pts_address), current_pts_address_type);
                    break;
                default:
                    break;
            }
    }
}

static void use_public_pts_address(void){
    memcpy(current_pts_address, public_pts_address, 6);
    current_pts_address_type = public_pts_address_type;                    
}

static uint16_t value_handle = 1;
static uint16_t attribute_size = 1;

static int num_rows = 0;
static int num_lines = 0;
static const char * rows[100];
static const char * lines[100];
static const int width = 70;

static void reset_screen(void){
    // free memory
    int i = 0;
    for (i=0;i<num_rows;i++) {
        free((void*)rows[i]);
        rows[i] = NULL;
    }
    num_rows = 0;
    for (i=0;i<num_lines;i++) {
        free((void*)lines[i]);
        lines[i] = NULL;
    }
    num_lines = 0;
}

static void print_line(const char * format, ...){
    va_list argptr;
    va_start(argptr, format);
    char * line = malloc(80);
    vsnprintf(line, 80, format, argptr);
    va_end(argptr);
    lines[num_lines] = line;
    num_lines++;
}

static void printf_row(const char * format, ...){
    va_list argptr;
    va_start(argptr, format);
    char * row = malloc(80);
    vsnprintf(row, 80, format, argptr);
    va_end(argptr);
    rows[num_rows] = row;
    num_rows++;
}

static void print_screen(void){

    // clear screen
    printf("\e[1;1H\e[2J");

    // full lines on top
    int i;
    for (i=0;i<num_lines;i++){
        printf("%s\n", lines[i]);
    }
    printf("\n");

    // two columns
    int second_half = (num_rows + 1) / 2;
    for (i=0;i<second_half;i++){
        int pos = strlen(rows[i]);
        printf("%s", rows[i]);
        while (pos < width){
            printf(" ");
            pos++;
        }
        if (i + second_half < num_rows){
            printf("|  %s", rows[i+second_half]);
        }
        printf("\n");
    }
    printf("\n");
}

static void show_usage(void){
    uint8_t iut_address_type;
    bd_addr_t      iut_address;
    gap_le_get_own_address(&iut_address_type, iut_address);

    reset_screen();

    print_line("--- CLI for SM Tests ---");
    print_line("PTS: addr type %u, addr %s", current_pts_address_type, bd_addr_to_str(current_pts_address));
    print_line("IUT: addr type %u, addr %s", iut_address_type, bd_addr_to_str(iut_address));
    print_line("--------------------------");
    print_line("GAP: connectable %u, bondable %u", gap_connectable, gap_bondable);
    print_line("SM: Secure Connections %u", (int) sm_le_secure_connections);
    print_line("SM: %s, MITM protection %u", sm_io_capabilities, sm_mitm_protection);
    print_line("SM: key range [%u..16], OOB data: %s", sm_min_key_size, 
        sm_have_oob_data ? (sm_have_oob_data == 1 ? (const char*) sm_oob_data_A : (const char*) sm_oob_data_B) : "None");
    print_line("Privacy %u", gap_privacy);
    printf_row("c/C - connectable off");
    printf_row("d/D - bondable off/on");
    printf_row("---");
    printf_row("1   - enable privacy using random non-resolvable private address");
    printf_row("2   - clear Peripheral Privacy Flag on PTS");
    printf_row("3   - set Peripheral Privacy Flag on PTS");
    printf_row("9   - create HCI Classic connection to addr %s", bd_addr_to_str(public_pts_address));
    printf_row("a   - enable Advertisements");
    printf_row("b   - start bonding");
    printf_row("t   - terminate connection, stop connecting");
    printf_row("P   - direct connect to PTS");
    printf_row("---");
    printf_row("---");
    printf_row("4   - IO_CAPABILITY_DISPLAY_ONLY");
    printf_row("5   - IO_CAPABILITY_DISPLAY_YES_NO");
    printf_row("6   - IO_CAPABILITY_NO_INPUT_NO_OUTPUT");
    printf_row("7   - IO_CAPABILITY_KEYBOARD_ONLY");
    printf_row("8   - IO_CAPABILITY_KEYBOARD_DISPLAY");
    printf_row("m/M - MITM protection off/on");
    printf_row("s/S - Secure Connections off/on");
    printf_row("x/X - encryption key range [7..16]/[16..16]");
    printf_row("y/Y - OOB data off/on/toggle A/B");
    printf_row("---");
    printf_row("Ctrl-c - exit");

    print_screen();
}

static void update_auth_req(void){
    uint8_t auth_req = 0;
    if (sm_mitm_protection){
        auth_req |= SM_AUTHREQ_MITM_PROTECTION;
    }
    if (sm_le_secure_connections){
        auth_req |= SM_AUTHREQ_SECURE_CONNECTION;
    }
    if (gap_bondable){
        auth_req |= SM_AUTHREQ_BONDING;
    }
    sm_set_authentication_requirements(auth_req);
}

static int hexForChar(char c){
    if (c >= '0' && c <= '9'){
        return c - '0';
    } 
    if (c >= 'a' && c <= 'f'){
        return c - 'a' + 10;
    }      
    if (c >= 'A' && c <= 'F'){
        return c - 'A' + 10;
    } 
    return -1;
} 

static int ui_process_digits_for_passkey(char buffer){
    if (buffer < '0' || buffer > '9') {
        return 0;
    }
    printf("%c", buffer);
    fflush(stdout);
    ui_passkey = ui_passkey * 10 + buffer - '0';
    ui_digits_for_passkey--;
    if (ui_digits_for_passkey == 0){
        printf("\nSending Passkey %u (0x%x)\n", ui_passkey, ui_passkey);
        sm_passkey_input(handle, ui_passkey);
    }
    return 0;
}

static void ui_process_command(char buffer){
    int res;
    switch (buffer){
        case '1':
            printf("Enabling non-resolvable private address\n");
            gap_random_address_set_mode(GAP_RANDOM_ADDRESS_NON_RESOLVABLE);
            gap_privacy = 1;
            update_advertisement_params();
            show_usage();
            break;
        case '4':
            sm_io_capabilities = "IO_CAPABILITY_DISPLAY_ONLY";
            sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
            show_usage();
            break;
        case '5':
            sm_io_capabilities = "IO_CAPABILITY_DISPLAY_YES_NO";
            sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_YES_NO);
            show_usage();
            break;
        case '6':
            sm_io_capabilities = "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
            sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
            show_usage();
            break;
        case '7':
            sm_io_capabilities = "IO_CAPABILITY_KEYBOARD_ONLY";
            sm_set_io_capabilities(IO_CAPABILITY_KEYBOARD_ONLY);
            show_usage();
            break;
        case '8':
            sm_io_capabilities = "IO_CAPABILITY_KEYBOARD_DISPLAY";
            sm_set_io_capabilities(IO_CAPABILITY_KEYBOARD_DISPLAY);
            show_usage();
            break;
        case '9':
            printf("Creating HCI Classic Connection to %s\n", bd_addr_to_str(public_pts_address));
            hci_send_cmd(&hci_create_connection, public_pts_address, hci_usable_acl_packet_types(), 0, 0, 0, 1);
            break;
        case 'a':
            gap_advertisements_enable(1);
            show_usage();
            break;
        case 'b':
            sm_request_pairing(handle);
            break;
        case 'c':
            gap_connectable = 0;
            update_advertisement_params();
            gap_connectable_control(gap_connectable);
            show_usage();
            break;
        case 'C':
            gap_connectable = 1;
            update_advertisement_params();
            gap_connectable_control(gap_connectable);
            show_usage();
            break;
        case 'd':
            gap_bondable = 0;
            update_auth_req();
            show_usage();
            break;
        case 'D':
            gap_bondable = 1;
            update_auth_req();
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
        case 'p':
            res = gap_auto_connection_start(current_pts_address_type, current_pts_address);
            printf("Auto Connection Establishment to type %u, addr %s -> %x\n", current_pts_address_type, bd_addr_to_str(current_pts_address), res);
            break;
        case 's':
            printf("Disable LE Secure Connections\n");
            sm_le_secure_connections = false;
            update_auth_req();
            break;
        case 'S':
            printf("Enable LE Secure Connections\n");
            sm_le_secure_connections = true;
            update_auth_req();
            break;
        case 't':
            printf("Terminating connection\n");
            hci_send_cmd(&hci_disconnect, handle, 0x13);
            gap_auto_connection_stop_all();
            gap_connect_cancel();
            break;
        case 'x':
            sm_min_key_size = 7;
            sm_set_encryption_key_size_range(7, 16);
            show_usage();
            break;
        case 'X':
            sm_min_key_size = 16;
            sm_set_encryption_key_size_range(16, 16);
            show_usage();
            break; 
       case 'y':
            sm_have_oob_data = 0;
            show_usage();
            break;
        case 'Y':
            if (sm_have_oob_data){
                sm_have_oob_data = 3 - sm_have_oob_data;
            } else {
                sm_have_oob_data = 1;
            }
            show_usage();
            break;
        default:
            show_usage();
            break;
    }
}

static void stdin_process(char c){
    if (ui_digits_for_passkey){
        ui_process_digits_for_passkey(c);
        return;
    }

    ui_process_command(c);
}

static int get_oob_data_callback(uint8_t addres_type, bd_addr_t addr, uint8_t * oob_data){
    UNUSED(addres_type);
    (void)addr;
    switch(sm_have_oob_data){
        case 1:
            memcpy(oob_data, sm_oob_data_A, 16);
            return 1;
        case 2:
            memcpy(oob_data, sm_oob_data_B, 16);
            return 1;
        default:
            return 0;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;
    printf("BTstack LE Peripheral starting up...\n");

    memset(rows, 0, sizeof(char *) * 100);
    memset(lines, 0, sizeof(char *) * 100);

    // register for HCI Events
    hci_event_callback_registration.callback = &app_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for SM events
    sm_event_callback_registration.callback = &app_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // set up l2cap_le
    l2cap_init();
    
    // Setup SM: Display only
    sm_init();
    sm_register_oob_data_callback(get_oob_data_callback);
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_io_capabilities =  "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

    sm_set_encryption_key_size_range(sm_min_key_size, 16);
    sm_test_set_irk(test_irk);
    sm_test_use_fixed_local_csrk();

    // Setup LE Device DB
    le_device_db_init();

    // set adv params
    update_advertisement_params();

    memcpy(current_pts_address, public_pts_address, 6);
    current_pts_address_type = public_pts_address_type;

    // classic discoverable / connectable
    gap_connectable_control(0);
    gap_discoverable_control(1);

    // allow foor terminal input
    btstack_stdin_setup(stdin_process);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}
