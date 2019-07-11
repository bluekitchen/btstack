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

#define __BTSTACK_FILE__ "hid_keyboard_demo.c"
 
// *****************************************************************************
/* EXAMPLE_START(hid_keyboard_demo): HID Keyboard (Server) Demo
 *
 * @text This HID Device example demonstrates how to implement
 * an HID keyboard. Without a HAVE_BTSTACK_STDIN, a fixed demo text is sent
 * If HAVE_BTSTACK_STDIN is defined, you can type from the terminal
 *
 * @text Status: Basic implementation. HID Request from Host are not answered yet. Works with iOS.
 */
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack.h"

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

// #define REPORT_ID_DECLARED 
// to enable demo text on POSIX systems
// #undef HAVE_BTSTACK_STDIN

// from USB HID Specification 1.1, Appendix B.1
const uint8_t hid_descriptor_keyboard_boot_mode[] = {
    0x05, 0x01,                    // Usage Page (Generic Desktop)
    0x09, 0x06,                    // Usage (Keyboard)
    0xa1, 0x01,                    // Collection (Application)

#ifdef REPORT_ID_DECLARED
    0x85, 0x01,                   // Report ID 1
#endif
    // Modifier byte

    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x08,                    //   Report Count (8)
    0x05, 0x07,                    //   Usage Page (Key codes)
    0x19, 0xe0,                    //   Usage Minimum (Keyboard LeftControl)
    0x29, 0xe7,                    //   Usage Maxium (Keyboard Right GUI)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)

    // Reserved byte

    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x08,                    //   Report Count (8)
    0x81, 0x03,                    //   Input (Constant, Variable, Absolute)

    // LED report + padding

    0x95, 0x05,                    //   Report Count (5)
    0x75, 0x01,                    //   Report Size (1)
    0x05, 0x08,                    //   Usage Page (LEDs)
    0x19, 0x01,                    //   Usage Minimum (Num Lock)
    0x29, 0x05,                    //   Usage Maxium (Kana)
    0x91, 0x02,                    //   Output (Data, Variable, Absolute)

    0x95, 0x01,                    //   Report Count (1)
    0x75, 0x03,                    //   Report Size (3)
    0x91, 0x03,                    //   Output (Constant, Variable, Absolute)

    // Keycodes

    0x95, 0x06,                    //   Report Count (6)
    0x75, 0x08,                    //   Report Size (8)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0xff,                    //   Logical Maximum (1)
    0x05, 0x07,                    //   Usage Page (Key codes)
    0x19, 0x00,                    //   Usage Minimum (Reserved (no event indicated))
    0x29, 0xff,                    //   Usage Maxium (Reserved)
    0x81, 0x00,                    //   Input (Data, Array)

    0xc0,                          // End collection  

};


// 
#define CHAR_ILLEGAL     0xff
#define CHAR_RETURN     '\n'
#define CHAR_ESCAPE      27
#define CHAR_TAB         '\t'
#define CHAR_BACKSPACE   0x7f

// Simplified US Keyboard with Shift modifier

/**
 * English (US)
 */
static const uint8_t keytable_us_none [] = {
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /*   0-3 */
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',                   /*  4-13 */
    'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',                   /* 14-23 */
    'u', 'v', 'w', 'x', 'y', 'z',                                       /* 24-29 */
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',                   /* 30-39 */
    CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ',            /* 40-44 */
    '-', '=', '[', ']', '\\', CHAR_ILLEGAL, ';', '\'', 0x60, ',',       /* 45-54 */
    '.', '/', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,   /* 55-60 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 61-64 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 65-68 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 69-72 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 73-76 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 77-80 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 81-84 */
    '*', '-', '+', '\n', '1', '2', '3', '4', '5',                       /* 85-97 */
    '6', '7', '8', '9', '0', '.', 0xa7,                                 /* 97-100 */
}; 

static const uint8_t keytable_us_shift[] = {
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /*  0-3  */
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',                   /*  4-13 */
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',                   /* 14-23 */
    'U', 'V', 'W', 'X', 'Y', 'Z',                                       /* 24-29 */
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',                   /* 30-39 */
    CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ',            /* 40-44 */
    '_', '+', '{', '}', '|', CHAR_ILLEGAL, ':', '"', 0x7E, '<',         /* 45-54 */
    '>', '?', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,   /* 55-60 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 61-64 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 65-68 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 69-72 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 73-76 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 77-80 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 81-84 */
    '*', '-', '+', '\n', '1', '2', '3', '4', '5',                       /* 85-97 */
    '6', '7', '8', '9', '0', '.', 0xb1,                                 /* 97-100 */
}; 

// STATE

static uint8_t hid_service_buffer[250];
static uint8_t device_id_sdp_service_buffer[100];
static const char hid_device_name[] = "BTstack HID Keyboard";
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t hid_cid;
static bd_addr_t device_addr;
static int     report_data_ready = 1;
static uint8_t report_data[20];
static uint8_t hid_boot_device = 1;
static int send_mouse_on_interrupt_channel = 0;
static int send_keyboard_on_interrupt_channel = 0;

#ifdef HAVE_BTSTACK_STDIN
static const char * device_addr_string = "BC:EC:5D:E6:15:03";
#endif

static enum {
    APP_BOOTING,
    APP_NOT_CONNECTED,
    APP_CONNECTING,
    APP_CONNECTED
} app_state = APP_BOOTING;

// static uint8_t modifier = 0;
// static uint8_t keycode = 0;
// static uint8_t report[] = { /* 0xa1, */ modifier, 0, 0, keycode, 0, 0, 0, 0, 0};

// HID Keyboard lookup
static int lookup_keycode(uint8_t character, const uint8_t * table, int size, uint8_t * keycode){
    int i;
    for (i=0;i<size;i++){
        if (table[i] != character) continue;
        *keycode = i;
        return 1;
    }
    return 0;
}

static int keycode_and_modifer_us_for_character(uint8_t character, uint8_t * keycode, uint8_t * modifier){
    int found;
    found = lookup_keycode(character, keytable_us_none, sizeof(keytable_us_none), keycode);
    if (found) {
        *modifier = 0;  // none
        return 1;
    }
    found = lookup_keycode(character, keytable_us_shift, sizeof(keytable_us_shift), keycode);
    if (found) {
        *modifier = 2;  // shift
        return 1;
    }
    return 0;
}

// HID Report sending
static int send_keycode;
static int send_modifier;

static void send_key(int modifier, int keycode){
    send_keycode = keycode;
    send_modifier = modifier;
    hid_device_request_can_send_now_event(hid_cid);
}

static void send_keyboard_report_on_interrupt_channel(int modifier, int keycode){
    uint8_t report[] = {0xa1, 
        0x01, 
        modifier, 0, 0, keycode, 0, 0, 0, 0};
    hid_device_send_interrupt_message(hid_cid, &report[0], sizeof(report));
}

static void send_mouse_report_on_interrupt_channel(uint8_t buttons, int8_t dx, int8_t dy){
    uint8_t report[] = {0xa1, 
        0x02, 
        buttons, dx, dy, 0};
    hid_device_send_interrupt_message(hid_cid, &report[0], sizeof(report));
}

// HID Report sending
static int prepare_mouse_report(hid_report_type_t report_type, uint8_t buttons, int8_t dx, int8_t dy, uint8_t * buffer, int buffer_size){
    UNUSED(report_type);
    if (buffer_size < 3) return 0;
    int pos = 0;
    buffer[pos++] = buttons;
    buffer[pos++] = (uint8_t) dx;
    buffer[pos++] = (uint8_t) dy;
    // buffer[pos++] = 0;
    
    return pos;
}

static int prepare_keyboard_report(hid_report_type_t report_type, int modifier, int keycode, uint8_t * buffer, int buffer_size){
    if (buffer_size < 8) return 0;
    int pos = 0;
    
    switch (report_type){
        case HID_REPORT_TYPE_INPUT:
            buffer[pos++] = (uint8_t) modifier;
            buffer[pos++] = 0;
            buffer[pos++] = (uint8_t) keycode;
            buffer[pos++] = 0;
            buffer[pos++] = 0;
            buffer[pos++] = 0;
            buffer[pos++] = 0;
            buffer[pos++] = 0;
            break;
        case HID_REPORT_TYPE_OUTPUT:
            printf("report type OUTPUT \n");
            buffer[pos++] = 0x00;
            break;
        default:
            break;
    }
    return pos;
}

static int hid_get_report_callback(uint16_t cid, hid_report_type_t report_type, uint16_t report_id, int * out_report_size, uint8_t * out_report){
    UNUSED(report_id);
    if (!report_data_ready){
        printf("report_data_ready not ready\n");
        return 0;
    }
    int report_size = 0;
    if (hid_device_in_boot_protocol_mode(cid)){
        switch (report_id){
            case HID_BOOT_MODE_KEYBOARD_ID:
                report_size = prepare_keyboard_report(report_type, send_modifier, send_keycode, &report_data[0], sizeof(report_data));
                break;
            case HID_BOOT_MODE_MOUSE_ID:
                report_size = prepare_mouse_report(report_type, 0, 0, 0, &report_data[0], sizeof(report_data));
            default:
                break;
        }
    } else {
        report_size = prepare_keyboard_report(report_type, send_modifier, send_keycode, &report_data[0], sizeof(report_data));
    } 
    memcpy(out_report, report_data, report_size); 
    *out_report_size = report_size;
    return 1;
}

static void hid_set_report_callback(uint16_t cid, hid_report_type_t report_type, int report_size, uint8_t * report){
    UNUSED(cid);
    UNUSED(report_type);
    UNUSED(report_size);
    UNUSED(report);
    printf("set report\n");
}

static void hid_report_data_callback(uint16_t cid, hid_report_type_t report_type, uint16_t report_id, int report_size, uint8_t * report){
    UNUSED(cid);
    UNUSED(report_type);
    UNUSED(report_id);
    UNUSED(report_size);
    UNUSED(report);
    printf("do smth with report\n");
}

#ifdef HAVE_BTSTACK_STDIN

// On systems with STDIN, we can directly type on the console
#if 0
static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth HID Host Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - connect \n");
    printf("l      - set limited discoverable mode\n");
    printf("L      - restset limited discoverable mode\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}
#endif

static void stdin_process(char character){
    uint8_t modifier;
    uint8_t keycode;
    int found;
    switch (character){
        case 'D':
            printf("Disconnect from %s...\n", bd_addr_to_str(device_addr));
            hid_device_disconnect(hid_cid);
            break;
        case 'c':
            printf("Connecting %s...\n", bd_addr_to_str(device_addr));
            hid_device_connect(device_addr, &hid_cid);
            return;
        case 'I':
            printf("Disconnect from intrrupt channel %s...\n", bd_addr_to_str(device_addr));
            hid_device_disconnect_interrupt_channel(hid_cid);
            break;
        case 'C':
            printf("Disconnect from control channel %s...\n", bd_addr_to_str(device_addr));
            hid_device_disconnect_control_channel(hid_cid);
            break;
        case 'l':
            printf("set limited discoverable mode\n");
            gap_discoverable_control(1);
            // TODO move into HCI init
            hci_send_cmd(&hci_write_current_iac_lap_two_iacs, 2, GAP_IAC_GENERAL_INQUIRY, GAP_IAC_LIMITED_INQUIRY);
            return;
        case 'm':
            send_mouse_on_interrupt_channel = 1;
            hid_device_request_can_send_now_event(hid_cid);
            break;
        case 'M':
            send_keyboard_on_interrupt_channel = 1;
            hid_device_request_can_send_now_event(hid_cid);
            break;
        case 'L':
            gap_discoverable_control(0);
            printf("reset limited discoverable mode\n");
            return;
        case '\n':
        case '\r':
            break;
        default:
            // show_usage();
            break;
    }

    switch (app_state){
        case APP_BOOTING:
        case APP_CONNECTING:
            // ignore
            break;

        case APP_CONNECTED:
            found = keycode_and_modifer_us_for_character(character, &keycode, &modifier);
            if (found){
                send_key(modifier, keycode);
                return;
            }
            break;
        case APP_NOT_CONNECTED:
            printf("Connecting to %s...\n", bd_addr_to_str(device_addr));
            hid_device_connect(device_addr, &hid_cid);
            break;
    }
}
#else

// On embedded systems, send constant demo text with fixed period

#define TYPING_PERIOD_MS 100
static const char * demo_text = "\n\nHello World!\n\nThis is the BTstack HID Keyboard Demo running on an Embedded Device.\n\n";

static int demo_pos;
static btstack_timer_source_t typing_timer;

static void typing_timer_handler(btstack_timer_source_t * ts){

    // abort if not connected
    if (!hid_cid) return;

    // get next character
    uint8_t character = demo_text[demo_pos++];
    if (demo_text[demo_pos] == 0){
        demo_pos = 0;
    }

    // get keycodeand send
    uint8_t modifier;
    uint8_t keycode;
    int found = keycode_and_modifer_us_for_character(character, &keycode, &modifier);
    if (found){
        send_key(modifier, keycode);
    }

    // set next timer
    btstack_run_loop_set_timer(ts, TYPING_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
}

static void hid_embedded_start_typing(void){
    demo_pos = 0;
    // set one-shot timer
    typing_timer.process = &typing_timer_handler;
    btstack_run_loop_set_timer(&typing_timer, TYPING_PERIOD_MS);
    btstack_run_loop_add_timer(&typing_timer);
}

#endif

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size){
    UNUSED(channel);
    UNUSED(packet_size);
    uint8_t status;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (packet[0]){
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
                    app_state = APP_NOT_CONNECTED;
                    break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // ssp: inform about user confirmation request
                    log_info("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", hci_event_user_confirmation_request_get_numeric_value(packet));
                    log_info("SSP User Confirmation Auto accept\n");                   
                    break; 

                case HCI_EVENT_HID_META:
                    switch (hci_event_hid_meta_get_subevent_code(packet)){
                        case HID_SUBEVENT_CONNECTION_OPENED:
                            status = hid_subevent_connection_opened_get_status(packet);
                            if (status) {
                                // outgoing connection failed
                                printf("Connection failed, status 0x%x\n", status);
                                app_state = APP_NOT_CONNECTED;
                                hid_cid = 0;
                                return;
                            }
                            app_state = APP_CONNECTED;
                            hid_subevent_connection_opened_get_bd_addr(packet, device_addr);
                            hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
#ifdef HAVE_BTSTACK_STDIN                        
                            printf("HID Connected, please start typing... %s\n", bd_addr_to_str(device_addr));
#else                        
                            printf("HID Connected, sending demo text...\n");
                            hid_embedded_start_typing();
#endif
                            break;
                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            printf("HID Disconnected\n");
                            app_state = APP_NOT_CONNECTED;
                            hid_cid = 0;
                            break;
                        
                        case HID_SUBEVENT_SUSPEND:
                            printf("HID Suspend\n");
                            break;
                        case HID_SUBEVENT_EXIT_SUSPEND:
                            printf("HID Exit Suspend\n");
                            break;
                            
                        case HID_SUBEVENT_CAN_SEND_NOW:
                            if (send_mouse_on_interrupt_channel){
                                send_mouse_on_interrupt_channel = 0;
                                printf("send mouse on interrupt channel\n");
                                send_mouse_report_on_interrupt_channel(0,0,0);
                                break;     
                            }
                            if (send_keyboard_on_interrupt_channel){
                                send_keyboard_on_interrupt_channel = 0;
                                send_keyboard_report_on_interrupt_channel(send_modifier, send_keycode);
                                send_keycode = 0;
                                send_modifier = 0;
                            }
                        
                            if (send_keycode){
                                send_keyboard_report_on_interrupt_channel(send_modifier, send_keycode);
                                send_keycode = 0;
                                send_modifier = 0;
                                hid_device_request_can_send_now_event(hid_cid);
                            } else {
                                send_keyboard_report_on_interrupt_channel(0, 0);
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}



/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code. 
 * To run a HID Device service you need to initialize the SDP, and to create and register HID Device record with it. 
 * At the end the Bluetooth stack is started.
 */

/* LISTING_START(MainConfiguration): Setup HID Device */

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    gap_discoverable_control(1);
    gap_set_class_of_device(0x2540);
    gap_set_local_name("HID Keyboard Demo 00:00:00:00:00:00");
    
    // L2CAP
    l2cap_init();

    // SDP Server
    sdp_init();
    memset(hid_service_buffer, 0, sizeof(hid_service_buffer));

    uint8_t hid_reconnect_initiate = 1;
    uint8_t hid_virtual_cable = 1;
    // hid sevice subclass 2540 Keyboard, hid counntry code 33 US, hid virtual cable on, hid reconnect initiate on, hid boot device off 
    hid_create_sdp_record(hid_service_buffer, 0x10001, 0x2540, 33, 
        hid_virtual_cable, hid_reconnect_initiate, hid_boot_device, 
        hid_descriptor_keyboard_boot_mode, sizeof(hid_descriptor_keyboard_boot_mode), hid_device_name);

    printf("HID service record size: %u\n", de_get_len( hid_service_buffer));
    sdp_register_service(hid_service_buffer);

    // See https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers if you don't have a USB Vendor ID and need a Bluetooth Vendor ID
    // device info: BlueKitchen GmbH, product 1, version 1
    device_id_create_sdp_record(device_id_sdp_service_buffer, 0x10003, DEVICE_ID_VENDOR_ID_SOURCE_BLUETOOTH, BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, 1, 1);
    printf("Device ID SDP service record size: %u\n", de_get_len((uint8_t*)device_id_sdp_service_buffer));
    sdp_register_service(device_id_sdp_service_buffer);

    hid_device_init(hid_boot_device, sizeof(hid_descriptor_keyboard_boot_mode), hid_descriptor_keyboard_boot_mode);
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for HID events
    hid_device_register_packet_handler(&packet_handler);
    hid_device_register_report_request_callback(&hid_get_report_callback);
    hid_device_register_set_report_callback(&hid_set_report_callback);
    hid_device_register_report_data_callback(&hid_report_data_callback);

    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);
    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
