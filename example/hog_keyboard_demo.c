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

#define BTSTACK_FILE__ "hog_keyboard_demo.c"

// *****************************************************************************
/* EXAMPLE_START(hog_keyboard_demo): HID Keyboard LE
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "hog_keyboard_demo.h"

#include "btstack.h"

#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "ble/gatt-service/hids_device.h"

// from USB HID Specification 1.1, Appendix B.1
const uint8_t hid_descriptor_keyboard_boot_mode[] = {

    0x05, 0x01,                    // Usage Page (Generic Desktop)
    0x09, 0x06,                    // Usage (Keyboard)
    0xa1, 0x01,                    // Collection (Application)

    0x85,  0x01,                   // Report ID 1

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

// static btstack_timer_source_t heartbeat;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static uint8_t battery = 100;
static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static uint8_t protocol_mode = 1;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    // Name
    0x0d, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'H', 'I', 'D', ' ', 'K', 'e', 'y', 'b', 'o', 'a', 'r', 'd',
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE & 0xff, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE >> 8,
    // Appearance HID - Keyboard (Category 15, Sub-Category 1)
    0x03, BLUETOOTH_DATA_TYPE_APPEARANCE, 0xC1, 0x03,
};
const uint8_t adv_data_len = sizeof(adv_data);

static void le_keyboard_setup(void){

    l2cap_init();

    // setup SM: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);

    // setup battery service
    battery_service_server_init(battery);

    // setup device information service
    device_information_service_server_init();

    // setup HID Device service
    hids_device_init(0, hid_descriptor_keyboard_boot_mode, sizeof(hid_descriptor_keyboard_boot_mode));

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // register for HIDS
    hids_device_register_packet_handler(packet_handler);
}

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
static void send_report(int modifier, int keycode){
    uint8_t report[] = {  modifier, 0, keycode, 0, 0, 0, 0, 0};
    switch (protocol_mode){
        case 0:
            hids_device_send_boot_keyboard_input_report(con_handle, report, sizeof(report));
            break;
        case 1:
           hids_device_send_input_report(con_handle, report, sizeof(report));
           break;
        default:
            break;
    }
}

// Demo Application

#ifdef HAVE_BTSTACK_STDIN

// On systems with STDIN, we can directly type on the console
static enum {
    W4_INPUT,
    W4_CAN_SEND_FROM_BUFFER,
    W4_CAN_SEND_KEY_UP,
} state;

// Buffer for 20 characters
static uint8_t ascii_input_storage[20];
static btstack_ring_buffer_t ascii_input_buffer;

static void typing_can_send_now(void){
    switch (state){
        case W4_CAN_SEND_FROM_BUFFER:
            while (1){
                uint8_t c;
                uint32_t num_bytes_read;

                btstack_ring_buffer_read(&ascii_input_buffer, &c, 1, &num_bytes_read);
                if (num_bytes_read == 0){
                    state = W4_INPUT;
                    break;
                }

                uint8_t modifier;
                uint8_t keycode;
                int found = keycode_and_modifer_us_for_character(c, &keycode, &modifier);
                if (!found) continue;

                printf("sending: %c\n", c);

                send_report(modifier, keycode);
                state = W4_CAN_SEND_KEY_UP;
                hids_device_request_can_send_now_event(con_handle);
                break;
            }
            break;
        case W4_CAN_SEND_KEY_UP:
            send_report(0, 0);
            if (btstack_ring_buffer_bytes_available(&ascii_input_buffer)){
                state = W4_CAN_SEND_FROM_BUFFER;
                hids_device_request_can_send_now_event(con_handle);
            } else {
                state = W4_INPUT;
            }
            break;
        default:
            break;
    }
}

static void stdin_process(char character){
    uint8_t c = character;
    btstack_ring_buffer_write(&ascii_input_buffer, &c, 1);
    // start sending
    if (state == W4_INPUT && con_handle != HCI_CON_HANDLE_INVALID){
        state = W4_CAN_SEND_FROM_BUFFER;
        hids_device_request_can_send_now_event(con_handle);
    }
}

#else

// On embedded systems, send constant demo text with fixed period

#define TYPING_PERIOD_MS 50
static const char * demo_text = "\n\nHello World!\n\nThis is the BTstack HID Keyboard Demo running on an Embedded Device.\n\n";

static int demo_pos;
static btstack_timer_source_t typing_timer;

static int send_keycode;
static int send_modifier;
static int send_keyup;

static void send_key(int modifier, int keycode){
    send_keycode = keycode;
    send_modifier = modifier;
    hids_device_request_can_send_now_event(con_handle);
}

static void typing_can_send_now(void){
   send_report(send_modifier, send_keycode);
}

static void typing_timer_handler(btstack_timer_source_t * ts){

    if (send_keyup){
        // just send key up
        send_keyup = 0;
        send_key(0, 0);
    } else {
        // get next character
        uint8_t character = demo_text[demo_pos++];
        if (demo_text[demo_pos] == 0){
            demo_pos = 0;
        }

        // get keycode and send
        uint8_t modifier;
        uint8_t keycode;
        int found = keycode_and_modifer_us_for_character(character, &keycode, &modifier);
        if (found){
            printf("%c\n", character);
            send_key(modifier, keycode);
            send_keyup = 1;
        }
    }

    // set next timer
    btstack_run_loop_set_timer(ts, TYPING_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
}

static void hid_embedded_start_typing(void){
    printf("Start typing..\n");

    demo_pos = 0;
    // set one-shot timer
    typing_timer.process = &typing_timer_handler;
    btstack_run_loop_set_timer(&typing_timer, TYPING_PERIOD_MS);
    btstack_run_loop_add_timer(&typing_timer);
}

#endif

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = HCI_CON_HANDLE_INVALID;
            printf("Disconnected\n");
            break;
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            printf("Display Passkey: %"PRIu32"\n", sm_event_passkey_display_number_get_passkey(packet));
            break;
        case HCI_EVENT_HIDS_META:
            switch (hci_event_hids_meta_get_subevent_code(packet)){
                case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
                    con_handle = hids_subevent_input_report_enable_get_con_handle(packet);
                    printf("Report Characteristic Subscribed %u\n", hids_subevent_input_report_enable_get_enable(packet));
#ifndef HAVE_BTSTACK_STDIN
                    hid_embedded_start_typing();
#endif
                    break;
                case HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE:
                    con_handle = hids_subevent_boot_keyboard_input_report_enable_get_con_handle(packet);
                    printf("Boot Keyboard Characteristic Subscribed %u\n", hids_subevent_boot_keyboard_input_report_enable_get_enable(packet));
                    break;
                case HIDS_SUBEVENT_PROTOCOL_MODE:
                    protocol_mode = hids_subevent_protocol_mode_get_protocol_mode(packet);
                    printf("Protocol Mode: %s mode\n", hids_subevent_protocol_mode_get_protocol_mode(packet) ? "Report" : "Boot");
                    break;
                case HIDS_SUBEVENT_CAN_SEND_NOW:
                    typing_can_send_now();
                    break;
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
}

int btstack_main(void);
int btstack_main(void)
{
    le_keyboard_setup();

#ifdef HAVE_BTSTACK_STDIN
    btstack_ring_buffer_init(&ascii_input_buffer, ascii_input_storage, sizeof(ascii_input_storage));
    btstack_stdin_setup(stdin_process);
#endif

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */
