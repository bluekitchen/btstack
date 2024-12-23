/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hid_host_demo.c"

/*
 * hid_host_demo.c
 */

/* EXAMPLE_START(hid_host_demo): HID Host Classic
 *
 * @text This example implements a HID Host. For now, it connects to a fixed device.
 * It will connect in Report protocol mode if this mode is supported by the HID Device,
 * otherwise it will fall back to BOOT protocol mode. 
 */

#include <inttypes.h>
#include <stdio.h>

#include "btstack_config.h"
#include "btstack.h"

#define MAX_ATTRIBUTE_VALUE_SIZE 300

static const char * remote_addr_string = "00:1A:7D:DA:71:01";

static bd_addr_t remote_addr;

static btstack_packet_callback_registration_t hci_event_callback_registration;

// Simplified US Keyboard with Shift modifier

#define CHAR_ILLEGAL     0xff
#define CHAR_RETURN     '\n'
#define CHAR_ESCAPE      27
#define CHAR_TAB         '\t'
#define CHAR_BACKSPACE   0x7f

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

// SDP
static uint8_t hid_descriptor_storage[MAX_ATTRIBUTE_VALUE_SIZE];

// App
static enum {
    APP_IDLE,
    APP_CONNECTED
} app_state = APP_IDLE;

static uint16_t hid_host_cid = 0;
static bool     hid_host_descriptor_available = false;
static hid_protocol_mode_t hid_host_report_mode = HID_PROTOCOL_MODE_REPORT_WITH_FALLBACK_TO_BOOT;

/* @section Main application configuration
 *
 * @text In the application configuration, L2CAP and HID host are initialized, and the link policies 
 * are set to allow sniff mode and role change. 
 */

/* LISTING_START(PanuSetup): Panu setup */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void hid_host_setup(void){

    // Initialize L2CAP
    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    // Initialize HID Host
    hid_host_init(hid_descriptor_storage, sizeof(hid_descriptor_storage));
    hid_host_register_packet_handler(packet_handler);

    // Allow sniff mode requests by HID device and support role switch
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // try to become master on incoming connections
    hci_set_master_slave_policy(HCI_ROLE_MASTER);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // make discoverable to allow HID device to initiate connection
    gap_discoverable_control(1);

    // Disable stdout buffering
    setvbuf(stdin, NULL, _IONBF, 0);
}
/* LISTING_END */

/*
 * @section HID Report Handler
 * 
 * @text Use BTstack's compact HID Parser to process incoming HID Report in Report protocol mode. 
 * Iterate over all fields and process fields with usage page = 0x07 / Keyboard
 * Check if SHIFT is down and process first character (don't handle multiple key presses)
 * 
 */

#define NUM_KEYS 6
static uint8_t last_keys[NUM_KEYS];
static bool hid_host_caps_lock;

static uint16_t hid_host_led_report_id;
static uint8_t  hid_host_led_report_len;
static uint8_t  hid_host_led_caps_lock_bit;

static void hid_host_set_leds(void){
    if (hid_host_led_report_len == 0) return;

    uint8_t output_report[8];

    uint8_t caps_lock_report_offset = hid_host_led_caps_lock_bit >> 3;
    if (caps_lock_report_offset >= sizeof(output_report)) return;

    memset(output_report, 0, sizeof(output_report));
    if (hid_host_caps_lock){
        output_report[caps_lock_report_offset] = 1 << (hid_host_led_caps_lock_bit & 0x07);
    }
    hid_host_send_set_report(hid_host_cid, HID_REPORT_TYPE_OUTPUT, hid_host_led_report_id, output_report, hid_host_led_report_len);
}

static void hid_host_demo_lookup_caps_lock_led(void){
    btstack_hid_usage_iterator_t iterator;
    const uint8_t *hid_descriptor = hid_descriptor_storage_get_descriptor_data(hid_host_cid);
    const uint16_t hid_descriptor_len = hid_descriptor_storage_get_descriptor_len(hid_host_cid);
    btstack_hid_usage_iterator_init(&iterator, hid_descriptor, hid_descriptor_len, HID_REPORT_TYPE_OUTPUT);
    while (btstack_hid_usage_iterator_has_more(&iterator)){
        btstack_hid_usage_item_t item;
        btstack_hid_usage_iterator_get_item(&iterator, &item);
        if (item.usage_page == HID_USAGE_PAGE_LED && item.usage == HID_USAGE_LED_CAPS_LOCK){
            hid_host_led_report_id     = item.report_id;
            hid_host_led_report_len    = btstack_hid_get_report_size_for_id(hid_host_led_report_id, HID_REPORT_TYPE_OUTPUT, hid_descriptor, hid_descriptor_len);
            hid_host_led_caps_lock_bit = (uint8_t) item.bit_pos;
            printf("Found CAPS LOCK in Output Report with ID 0x%04x at bit %3u\n", hid_host_led_report_id, hid_host_led_caps_lock_bit);
        }
    }
}

static void hid_host_handle_interrupt_report(const uint8_t * report, uint16_t report_len){
    // check if HID Input Report
    if (report_len < 1) return;
    if (*report != 0xa1) return; 
    
    report++;
    report_len--;
    
    btstack_hid_parser_t parser;
    btstack_hid_parser_init(&parser, 
        hid_descriptor_storage_get_descriptor_data(hid_host_cid), 
        hid_descriptor_storage_get_descriptor_len(hid_host_cid), 
        HID_REPORT_TYPE_INPUT, report, report_len);

    bool shift = hid_host_caps_lock;
    uint8_t new_keys[NUM_KEYS];
    memset(new_keys, 0, sizeof(new_keys));
    int     new_keys_count = 0;

    while (btstack_hid_parser_has_more(&parser)){
        uint16_t usage_page;
        uint16_t usage;
        int32_t  value;
        btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);
        if (usage_page != 0x07) continue;   
        switch (usage){
            case HID_USAGE_KEY_KEYBOARD_CAPS_LOCK:
                // Toggle Caps Lock
                hid_host_caps_lock = !hid_host_caps_lock;
                // update LEDs
                hid_host_set_leds();
                break;
            case HID_USAGE_KEY_KEYBOARD_LEFTSHIFT:
            case HID_USAGE_KEY_KEYBOARD_RIGHTSHIFT:
                if (value){
                    shift = 1;
                }
                continue;
            case HID_USAGE_KEY_RESERVED:
                continue;
            default:
                break;
        }
        if (usage >= sizeof(keytable_us_none)) continue;

        // store new keys
        new_keys[new_keys_count++] = (uint8_t) usage;

        // check if usage was used last time (and ignore in that case)
        int i;
        for (i=0;i<NUM_KEYS;i++){
            if (usage == last_keys[i]){
                usage = 0;
            }
        }
        if (usage == 0) continue;

        uint8_t key;
        if (shift){
            key = keytable_us_shift[usage];
        } else {
            key = keytable_us_none[usage];
        }
        if (key == CHAR_ILLEGAL) continue;
        if (key == CHAR_BACKSPACE){ 
            printf("\b \b");    // go back one char, print space, go back one char again
            continue;
        }
        printf("%c", key);
        fflush(stdout);
    }
    memcpy(last_keys, new_keys, NUM_KEYS);
}

/*
 * @section Packet Handler
 * 
 * @text The packet handler responds to various HID events.
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    /* LISTING_PAUSE */
    UNUSED(channel);
    UNUSED(size);

    uint8_t   event;
    bd_addr_t event_addr;
    uint8_t   status;

    /* LISTING_RESUME */
    switch (packet_type) {
		case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);
            
            switch (event) {            
#ifndef HAVE_BTSTACK_STDIN
                /* @text When BTSTACK_EVENT_STATE with state HCI_STATE_WORKING
                 * is received and the example is started in client mode, the remote SDP HID query is started.
                 */
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        status = hid_host_connect(remote_addr, hid_host_report_mode, &hid_host_cid);
                        if (status != ERROR_CODE_SUCCESS){
                            printf("HID host connect failed, status 0x%02x.\n", status);
                        }
                    }
                    break;
#endif
                /* LISTING_PAUSE */
                case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
					break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                /* LISTING_RESUME */
                case HCI_EVENT_HID_META:
                    switch (hci_event_hid_meta_get_subevent_code(packet)){

                        case HID_SUBEVENT_INCOMING_CONNECTION:
                            // There is an incoming connection: we can accept it or decline it.
                            // The hid_host_report_mode in the hid_host_accept_connection function 
                            // allows the application to request a protocol mode. 
                            // For available protocol modes, see hid_protocol_mode_t in btstack_hid.h file. 
                            hid_host_accept_connection(hid_subevent_incoming_connection_get_hid_cid(packet), hid_host_report_mode);
                            break;
                        
                        case HID_SUBEVENT_CONNECTION_OPENED:
                            // The status field of this event indicates if the control and interrupt
                            // connections were opened successfully.
                            status = hid_subevent_connection_opened_get_status(packet);
                            if (status != ERROR_CODE_SUCCESS) {
                                printf("Connection failed, status 0x%02x\n", status);
                                app_state = APP_IDLE;
                                hid_host_cid = 0;
                                return;
                            }
                            app_state = APP_CONNECTED;
                            hid_host_descriptor_available = false;
                            hid_host_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                            hid_host_caps_lock = false;
                            hid_host_led_report_len = 0;
                            printf("HID Host connected.\n");
                            break;

                        case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
                            // This event will follows HID_SUBEVENT_CONNECTION_OPENED event. 
                            // For incoming connections, i.e. HID Device initiating the connection,
                            // the HID_SUBEVENT_DESCRIPTOR_AVAILABLE is delayed, and some HID  
                            // reports may be received via HID_SUBEVENT_REPORT event. It is up to 
                            // the application if these reports should be buffered or ignored until 
                            // the HID descriptor is available.
                            status = hid_subevent_descriptor_available_get_status(packet);
                            if (status == ERROR_CODE_SUCCESS){
                                hid_host_descriptor_available = true;
                                printf("HID Descriptor available, please start typing.\n");
                                hid_host_demo_lookup_caps_lock_led();
                            } else {
                                printf("Cannot handle input report, HID Descriptor is not available, status 0x%02x\n", status);
                            }
                            break;

                        case HID_SUBEVENT_REPORT:
                            // Handle input report.
                            if (hid_host_descriptor_available){
                                hid_host_handle_interrupt_report(hid_subevent_report_get_report(packet), hid_subevent_report_get_report_len(packet));
                            } else {
                                printf_hexdump(hid_subevent_report_get_report(packet), hid_subevent_report_get_report_len(packet));
                            }
                            break;

                        case HID_SUBEVENT_SET_PROTOCOL_RESPONSE:
                            // For incoming connections, the library will set the protocol mode of the
                            // HID Device as requested in the call to hid_host_accept_connection. The event 
                            // reports the result. For connections initiated by calling hid_host_connect, 
                            // this event will occur only if the established report mode is boot mode.
                            status = hid_subevent_set_protocol_response_get_handshake_status(packet);
                            if (status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                                printf("Error set protocol, status 0x%02x\n", status);
                                break;
                            }
                            switch ((hid_protocol_mode_t)hid_subevent_set_protocol_response_get_protocol_mode(packet)){
                                case HID_PROTOCOL_MODE_BOOT:
                                    printf("Protocol mode set: BOOT.\n");
                                    break;  
                                case HID_PROTOCOL_MODE_REPORT:
                                    printf("Protocol mode set: REPORT.\n");
                                    break;
                                default:
                                    printf("Unknown protocol mode.\n");
                                    break; 
                            }
                            break;

                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            // The connection was closed.
                            hid_host_cid = 0;
                            hid_host_descriptor_available = false;
                            printf("HID Host disconnected.\n");
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
/* LISTING_END */

#ifdef HAVE_BTSTACK_STDIN
static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth HID Host Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - Connect to %s in report mode, with fallback to boot mode.\n", remote_addr_string);
    printf("C      - Disconnect\n");
    
    printf("\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (cmd){
        case 'c':
            printf("Connect to %s in report mode, with fallback to boot mode.\n", remote_addr_string);
            status = hid_host_connect(remote_addr, hid_host_report_mode, &hid_host_cid);
            break;
        case 'C':
            printf("Disconnect...\n");
            hid_host_disconnect(hid_host_cid);
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
    if (status != ERROR_CODE_SUCCESS){
        printf("HID host cmd \'%c\' failed, status 0x%02x\n", cmd, status);
    }
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    (void)argc;
    (void)argv;

    hid_host_setup();

    // parse human readable Bluetooth address
    sscanf_bd_addr(remote_addr_string, remote_addr);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif

    // Turn on the device 
    hci_power_control(HCI_POWER_ON);
    return 0;
}

/* EXAMPLE_END */
