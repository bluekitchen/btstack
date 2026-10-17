/*
 * Copyright (C) 2017 BlueKitchen GmbH
 * Copyright (C) 2023 Benjamin Aigner <aignerb@technikum-wien.at> / <beni@asterics-foundation.org>
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

#define BTSTACK_FILE__ "hog_mouse_kbd_joystick.c"

// *****************************************************************************
/* EXAMPLE_START(hog_mouse_kbd_joystick): HID Mouse+KBD+Joystick LE
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "hog_mouse_kbd_joystick.h"

#include "btstack.h"

#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "ble/gatt-service/hids_device.h"

#ifndef HAVE_BTSTACK_STDIN
    #error "This examples requires STDIN to control various HID reports"
#endif

static const uint8_t hidReportMap[] = {
    0x05, 0x01,  // Usage Pg (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection: (Application)
    0x85, 0x01,  // Report Id (1)
    //
    0x05, 0x07,  //   Usage Pg (Key Codes)
    0x19, 0xE0,  //   Usage Min (224)
    0x29, 0xE7,  //   Usage Max (231)
    0x15, 0x00,  //   Log Min (0)
    0x25, 0x01,  //   Log Max (1)
    //
    //   Modifier byte
    0x75, 0x01,  //   Report Size (1)
    0x95, 0x08,  //   Report Count (8)
    0x81, 0x02,  //   Input: (Data, Variable, Absolute)
    //
    //   Reserved byte
    //
    //   Key arrays (6 bytes)
    0x95, 0x06,  //   Report Count (6)
    0x75, 0x08,  //   Report Size (8)
    0x15, 0x00,  //   Log Min (0)
    0x25, 0x65,  //   Log Max (101)
    0x05, 0x07,  //   Usage Pg (Key Codes)
    0x19, 0x00,  //   Usage Min (0)
    0x29, 0x65,  //   Usage Max (101)
    0x81, 0x00,  //   Input: (Data, Array)
    //
    0xC0,        // End Collection
    //
    0x05, 0x0C,   // Usage Pg (Consumer Devices)
    0x09, 0x01,   // Usage (Consumer Control)
    0xA1, 0x01,   // Collection (Application)
    0x85, 0x02,   // Report Id (2)
    0x05, 0x0C,   //   Usage Pg (Consumer Devices)
    0x09, 0x86,   //   Usage (Channel)
    0x15, 0xFF,   //   Logical Min (-1)
    0x25, 0x01,   //   Logical Max (1)
    0x75, 0x02,   //   Report Size (2)
    0x95, 0x01,   //   Report Count (1)
    0x81, 0x46,   //   Input (Data, Var, Rel, Null)
    0x09, 0xE9,   //   Usage (Volume Up)
    0x09, 0xEA,   //   Usage (Volume Down)
    0x15, 0x00,   //   Logical Min (0)
    0x75, 0x01,   //   Report Size (1)
    0x95, 0x02,   //   Report Count (2)
    0x81, 0x02,   //   Input (Data, Var, Abs)
    0x09, 0xE2,   //   Usage (Mute)
    0x09, 0x30,   //   Usage (Power)
    0x09, 0x83,   //   Usage (Recall Last)
    0x09, 0x81,   //   Usage (Assign Selection)
    0x09, 0xB0,   //   Usage (Play)
    0x09, 0xB1,   //   Usage (Pause)
    0x09, 0xB2,   //   Usage (Record)
    0x09, 0xB3,   //   Usage (Fast Forward)
    0x09, 0xB4,   //   Usage (Rewind)
    0x09, 0xB5,   //   Usage (Scan Next)
    0x09, 0xB6,   //   Usage (Scan Prev)
    0x09, 0xB7,   //   Usage (Stop)
    0x15, 0x01,   //   Logical Min (1)
    0x25, 0x0C,   //   Logical Max (12)
    0x75, 0x04,   //   Report Size (4)
    0x95, 0x01,   //   Report Count (1)
    0x81, 0x00,   //   Input (Data, Ary, Abs)
    0xC0,            // End Collection
    
    
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x02,  // Usage (Mouse)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x03,  // Report Id (3)
    0x09, 0x01,  //   Usage (Pointer)
    0xA1, 0x00,  //   Collection (Physical)
    0x05, 0x09,  //     Usage Page (Buttons)
    0x19, 0x01,  //     Usage Minimum (01) - Button 1
    0x29, 0x03,  //     Usage Maximum (03) - Button 3
    0x15, 0x00,  //     Logical Minimum (0)
    0x25, 0x01,  //     Logical Maximum (1)
    0x75, 0x01,  //     Report Size (1)
    0x95, 0x03,  //     Report Count (3)
    0x81, 0x02,  //     Input (Data, Variable, Absolute) - Button states
    0x75, 0x05,  //     Report Size (5)
    0x95, 0x01,  //     Report Count (1)
    0x81, 0x01,  //     Input (Constant) - Padding or Reserved bits
    0x05, 0x01,  //     Usage Page (Generic Desktop)
    0x09, 0x30,  //     Usage (X)
    0x09, 0x31,  //     Usage (Y)
    0x09, 0x38,  //     Usage (Wheel)
    0x15, 0x81,  //     Logical Minimum (-127)
    0x25, 0x7F,  //     Logical Maximum (127)
    0x75, 0x08,  //     Report Size (8)
    0x95, 0x03,  //     Report Count (3)
    0x81, 0x06,  //     Input (Data, Variable, Relative) - X & Y coordinate
    0xC0,        //   End Collection
    0xC0,        // End Collection

    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x05,  // Usage (Gamepad)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x04,  // Report Id (4)
    /* 8 bit X, Y, Z, Rz, Rx, Ry (min -127, max 127 ) */ 
    /* implemented like Gamepad from tinyUSB */
      0x05, 0x01,  // Usage Page (Generic Desktop)
      0x09, 0x30,  // Usage (desktop X)
      0x09, 0x31,  // Usage (desktop Y)
      0x09, 0x32,  // Usage (desktop Z)
      0x09, 0x35,  // Usage (desktop RZ)
      0x09, 0x33,  // Usage (desktop RX)
      0x09, 0x34,  // Usage (desktop RY)
      0x15, 0x81,  // Logical Minimum (-127)
      0x25, 0x7F,  // Logical Maximum (127)
      0x95, 0x06,  // Report Count (6)
      0x75, 0x08,  // Report Size (8)
      0x81, 0x02,  // Input: (Data, Variable, Absolute)
    /* 8 bit DPad/Hat Button Map  */
      0x05, 0x01,  // Usage Page (Generic Desktop)
      0x09, 0x39,  // Usage (hat switch)
      0x15, 0x01,   //     Logical Min (1)
      0x25, 0x08,   //     Logical Max (8)
      
      0x35, 0x00,   // Physical minimum (0)
      0x46, 0x3B, 0x01, // Physical maximum (315, size 2)
      //0x46, 0x00,   // Physical maximum (315, size 2)
      0x95, 0x01,  // Report Count (1)
      0x75, 0x08,  // Report Size (8)
      0x81, 0x02,  // Input: (Data, Variable, Absolute)
    /* 16 bit Button Map */
      0x05, 0x09,  // Usage Page (button)
      0x19, 0x01,  //     Usage Minimum (01) - Button 1
      0x29, 0x20,  //     Usage Maximum (32) - Button 32
      0x15, 0x00,   //     Logical Min (0)
      0x25, 0x01,   //     Logical Max (1)
      0x95, 0x20,  // Report Count (32)
      0x75, 0x01,  // Report Size (1)
      0x81, 0x02,  // Input: (Data, Variable, Absolute)
    0xC0,            // End Collection
};

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t l2cap_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static uint8_t battery = 100;
static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static uint8_t protocol_mode = 1;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

hids_device_report_t *dev_report_storage;

//buffers for the different HID reports
//sizes are determined by the HID report map (look for report count & size)
uint8_t mouseReport[4];
uint8_t kbdReport[7];
uint8_t consumerReport[1];
uint8_t joystickReport[11];
//currently used report ID, determines the kind of report to be sent after an ATT notification
volatile uint8_t lastReportID;

//report IDs for the different HID reports.
//must correspond to the HID report map!
#define REPORT_ID_MOUSE    3
#define REPORT_ID_KBD      1
#define REPORT_ID_CONSUMER 2
#define REPORT_ID_JOYSTICK 4

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    // Name
    0x0a, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'K', 'B', 'D', ' ', 'M', ' ', 'J', 'O', 'Y',
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE & 0xff, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE >> 8,
    // Appearance HID - Mouse (Category 15, Sub-Category 2)
    0x03, BLUETOOTH_DATA_TYPE_APPEARANCE, 0xC0, 0x03,
};
const uint8_t adv_data_len = sizeof(adv_data);

static void hog_composite_setup(void){

    // setup l2cap and
    l2cap_init();

    // setup SM: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);

    // setup battery service
    battery_service_server_init(battery);

    // setup device information service
    device_information_service_server_init();

    // setup HID Device service
    dev_report_storage = (hids_device_report_t *)malloc(sizeof(hids_device_report_t)*6);
    hids_device_init_with_storage(0, hidReportMap, sizeof(hidReportMap),6,dev_report_storage);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    // register for events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for connection parameter updates
    l2cap_event_callback_registration.callback = &packet_handler;
    l2cap_add_event_handler(&l2cap_event_callback_registration);

    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    hids_device_register_packet_handler(packet_handler);
}

// HID Report sending
static void send_report(void){
  switch(lastReportID)
  {
    case REPORT_ID_KBD: // keyboard
      if(protocol_mode == 0) hids_device_send_boot_keyboard_input_report(con_handle, kbdReport, sizeof(kbdReport));
      if(protocol_mode == 1) hids_device_send_input_report_for_id(con_handle,lastReportID,kbdReport, sizeof(kbdReport));
      printf("Keyboard - %02x (modifiers) %02x / %02x / %02x / %02x / %02x / %02x (keys) - mode: %d / id: %d\n", kbdReport[0], \
        kbdReport[1], kbdReport[2], kbdReport[3], kbdReport[4], kbdReport[5], kbdReport[6],protocol_mode,lastReportID);
    break;
    
    case REPORT_ID_CONSUMER: // consumer
      if(protocol_mode == 0) printf("Cannot send consumer keys in boot mode");
      if(protocol_mode == 1) hids_device_send_input_report_for_id(con_handle,lastReportID,consumerReport, sizeof(consumerReport));
      printf("Consumer key: %02x / id: %d\n",consumerReport[0],lastReportID);
    break;
    
    case REPORT_ID_MOUSE: // mouse
      if(protocol_mode == 0) hids_device_send_boot_mouse_input_report(con_handle, mouseReport, sizeof(mouseReport));
      if(protocol_mode == 1) hids_device_send_input_report_for_id(con_handle,lastReportID,mouseReport, sizeof(mouseReport));
      printf("Mouse: %d/%d - buttons: %02x - mode: %d / id: %d\n", mouseReport[1], mouseReport[2], mouseReport[0],protocol_mode,lastReportID);
    break;
    
    case REPORT_ID_JOYSTICK: // joystick
      if(protocol_mode == 0) printf("Cannot send joystick in boot mode");
      if(protocol_mode == 1) hids_device_send_input_report_for_id(con_handle,lastReportID,joystickReport, sizeof(joystickReport));
      printf("Joystick report: %02x - %02x - %02x - %02x - %02x - %02x - %02x - %02x - %02x - %02x - %02x / id: %d", \
        joystickReport[0], joystickReport[1], joystickReport[2], joystickReport[3], joystickReport[4], joystickReport[5], \
        joystickReport[6], joystickReport[7], joystickReport[8], joystickReport[9], joystickReport[10],lastReportID);
    break;
    
    default:
      printf("lastReportID == 0, don't know which report to send\n");
   }
   
   lastReportID = 0;
}

// Demo Application
static const int MOUSE_SPEED = 30;

// On systems with STDIN, we can directly type on the console

static void stdin_process(char character){

    if (con_handle == HCI_CON_HANDLE_INVALID) {
        printf("Mouse not connected, ignoring '%c'\n", character);
        return;
    }

    switch (character){
        case '+': //volume up
            consumerReport[0] = 0x04;
            lastReportID = REPORT_ID_CONSUMER;
            break;
        case '-': //volume down
            consumerReport[0] = 0x08;
            lastReportID = REPORT_ID_CONSUMER;
            break;
        case 'm': //volume mute (see report map, "Usage")
            consumerReport[0] = 1 << 4;
            lastReportID = REPORT_ID_CONSUMER;
            break;
        case 'p': //play  (see report map, "Usage")
            consumerReport[0] = 5 << 4;
            lastReportID = REPORT_ID_CONSUMER;
            break;
        case 'a':
            mouseReport[1] -= MOUSE_SPEED;
            lastReportID = REPORT_ID_MOUSE;
            break;
        case 's':
            mouseReport[2] += MOUSE_SPEED;
            lastReportID = REPORT_ID_MOUSE;
            break;
        case 'd':
            mouseReport[1] += MOUSE_SPEED;
            lastReportID = REPORT_ID_MOUSE;
            break;
        case 'w':
            mouseReport[2] -= MOUSE_SPEED;
            lastReportID = REPORT_ID_MOUSE;
            break;
        case 'l':
            mouseReport[0] |= 1;
            lastReportID = REPORT_ID_MOUSE;
            break;
        case 'r':
            mouseReport[0] |= 2;
            lastReportID = REPORT_ID_MOUSE;
            break;
        case 'y':
            //kbdReport[0] = 2; //shift
            kbdReport[1] = 28; // y key on US layout
            lastReportID = REPORT_ID_KBD;
            break;
        case 'x':
            joystickReport[8] = 0x0F; //set Joystick buttons
            lastReportID = REPORT_ID_JOYSTICK;
            break;
        default:
            return;
    }
    //wait for report to be sent & reset if mouse/kbd/consumer
    //this is used to avoid sticky keys in this demo.
    if(lastReportID == REPORT_ID_KBD)
    {
      //trigger 1. sending
      hids_device_request_can_send_now_event(con_handle);
      //wait for report id to be 0 (report is sent)
      while(lastReportID);
      //reset kbd report
      memset(kbdReport,0,sizeof(kbdReport));
      //set report id again
      lastReportID = REPORT_ID_KBD;
      // trigger 2. sending
      hids_device_request_can_send_now_event(con_handle);
      return;
    }
    if(lastReportID == REPORT_ID_CONSUMER)
    {
      //trigger 1. sending
      hids_device_request_can_send_now_event(con_handle);
      //wait for report id to be 0 (report is sent)
      while(lastReportID);
      //reset kbd report
      memset(consumerReport,0,sizeof(consumerReport));
      //set report id again
      lastReportID = REPORT_ID_CONSUMER;
      // trigger 2. sending
      hids_device_request_can_send_now_event(con_handle);
      return;
    }
    if(lastReportID == REPORT_ID_MOUSE)
    {
      hids_device_request_can_send_now_event(con_handle);
      while(lastReportID);
      memset(mouseReport,0,sizeof(mouseReport));
      lastReportID = REPORT_ID_MOUSE;
      hids_device_request_can_send_now_event(con_handle);
      return;
    }
    hids_device_request_can_send_now_event(con_handle);
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint16_t conn_interval;

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
        case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            printf("L2CAP Connection Parameter Update Complete, response: %x\n", l2cap_event_connection_parameter_update_response_get_result(packet));
            break;
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    // print connection parameters (without using float operations)
                    conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
                    printf("LE Connection Complete:\n");
                    printf("- Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
                    printf("- Connection Latency: %u\n", hci_subevent_le_connection_complete_get_conn_latency(packet));
                    break;
                case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
                    // print connection parameters (without using float operations)
                    conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
                    printf("LE Connection Update:\n");
                    printf("- Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
                    printf("- Connection Latency: %u\n", hci_subevent_le_connection_update_complete_get_conn_latency(packet));
                    break;
                default:
                    break;
            }
            break;  
        case HCI_EVENT_HIDS_META:
            switch (hci_event_hids_meta_get_subevent_code(packet)){
                case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
                    con_handle = hids_subevent_input_report_enable_get_con_handle(packet);
                    printf("Report Characteristic Subscribed %u\n", hids_subevent_input_report_enable_get_enable(packet));

                    // request connection param update via L2CAP following Apple Bluetooth Design Guidelines
                    // gap_request_connection_parameter_update(con_handle, 12, 12, 4, 100);    // 15 ms, 4, 1s

                    // directly update connection params via HCI following Apple Bluetooth Design Guidelines
                    // gap_update_connection_parameters(con_handle, 12, 12, 4, 100);    // 60-75 ms, 4, 1s

                    break;
                case HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE:
                    con_handle = hids_subevent_boot_keyboard_input_report_enable_get_con_handle(packet);
                    printf("Boot Keyboard Characteristic Subscribed %u\n", hids_subevent_boot_keyboard_input_report_enable_get_enable(packet));
                    break;
                case HIDS_SUBEVENT_BOOT_MOUSE_INPUT_REPORT_ENABLE:
                    con_handle = hids_subevent_boot_mouse_input_report_enable_get_con_handle(packet);
                    printf("Boot Mouse Characteristic Subscribed %u\n", hids_subevent_boot_mouse_input_report_enable_get_enable(packet));
                    break;
                case HIDS_SUBEVENT_PROTOCOL_MODE:
                    protocol_mode = hids_subevent_protocol_mode_get_protocol_mode(packet);
                    printf("Protocol Mode: %s mode\n", hids_subevent_protocol_mode_get_protocol_mode(packet) ? "Report" : "Boot");
                    break;
                case HIDS_SUBEVENT_CAN_SEND_NOW:
                    printf("HIDS_SUBEVENT_CAN_SEND_NOW\n");
                    send_report();
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
    hog_composite_setup();

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
