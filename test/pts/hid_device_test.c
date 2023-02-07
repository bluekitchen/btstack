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

#define BTSTACK_FILE__ "hid_device_test.c"

/*
 * hid_device_test.c : Tool for testig HID device with PTS, see hid_device_test.md for PTS tests command sequences
 */ 

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack.h"

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

#define REPORT_ID_DECLARED 
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

// STATE

static uint8_t hid_service_buffer[300];
static uint8_t device_id_sdp_service_buffer[100];
static const char hid_device_name[] = "BTstack HID Keyboard";
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t hid_cid;
static hci_con_handle_t hid_con_handle = HCI_CON_HANDLE_INVALID;
static bd_addr_t device_addr;
static int     report_data_ready = 1;
static uint8_t report_data[20];
static bool    hid_boot_device = true;
static bool    send_mouse_on_interrupt_channel = false;
static bool    send_keyboard_on_interrupt_channel = false;

static uint16_t host_max_latency = 1600;
static uint16_t host_min_timeout = 3200;

#ifdef HAVE_BTSTACK_STDIN
static const char * device_addr_string = "00:1B:DC:08:E2:5C";
#endif

static enum {
    APP_BOOTING,
    APP_NOT_CONNECTED,
    APP_CONNECTING,
    APP_CONNECTED
} app_state = APP_BOOTING;

static bool virtual_cable_enabled  = false;
static bool virtual_cable_unplugged = false;

// HID Report sending
static int send_keycode;
static int send_modifier;

static void send_keyboard_report_on_interrupt_channel(int modifier, int keycode){
    uint8_t report[] = {0xa1, 
#ifdef REPORT_ID_DECLARED
        0x01, 
#endif
        modifier, 0, 0, keycode, 0, 0, 0, 0};
    hid_device_send_interrupt_message(hid_cid, &report[0], sizeof(report));
}

static void send_mouse_report_on_interrupt_channel(uint8_t buttons, int8_t dx, int8_t dy){
    uint8_t report[] = {0xa1, 
#ifdef REPORT_ID_DECLARED
        0x02, 
#endif
        buttons, dx, dy, 0};
    hid_device_send_interrupt_message(hid_cid, &report[0], sizeof(report));
}

/**
 * @brief Initiate a Virtual Cable unplug from the already pluged remote device. Reconnection requests after unplug will be rejected.
 * @param  hid_cid
 * @return status 
 */
static uint8_t hid_device_send_virtual_cable_unplug(uint16_t hcid){
    if (!virtual_cable_enabled) return ERROR_CODE_COMMAND_DISALLOWED;
    uint8_t report[] = { (HID_MESSAGE_TYPE_HID_CONTROL << 4) | HID_CONTROL_PARAM_VIRTUAL_CABLE_UNPLUG };
    hid_device_send_control_message(hcid, &report[0], sizeof(report));
    return ERROR_CODE_SUCCESS;
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


static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth HID Host Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - Connect to %s...\n", bd_addr_to_str(device_addr));
    printf("D      - Disconnect\n");
    printf("I      - Disconnect from intrrupt channel %s...\n", bd_addr_to_str(device_addr));
    printf("C      - Disconnect from control channel %s...\n", bd_addr_to_str(device_addr));
    printf("\n");
    
    printf("l      - Set limited discoverable mode\n");
    printf("m      - Request can send now (mouse)\n");
    printf("M      - Request can send now (keyboard)\n");
    printf("L      - Reset limited discoverable mode\n");
    printf("u      - Unplug\n");
    printf("w      - Set sniff subrating\n");
    printf("z      - Enter sniff mode\n");
    printf("q      - Set QoS 'Guaranteed'\n");

    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char character){
    switch (character){
        case 'D':
            printf("Disconnect from %s...\n", bd_addr_to_str(device_addr));
            hid_device_disconnect(hid_cid);
            break;
        case 'c':
            if (virtual_cable_unplugged){
                printf("Cannot connect to %s, device has been unplugged...\n", bd_addr_to_str(device_addr));
            } else {
                printf("Connecting %s...\n", bd_addr_to_str(device_addr));
                hid_device_connect(device_addr, &hid_cid);
            }
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
            send_mouse_on_interrupt_channel = true;
            hid_device_request_can_send_now_event(hid_cid);
            break;
        case 'M':
            send_keyboard_on_interrupt_channel = true;
            hid_device_request_can_send_now_event(hid_cid);
            break;
        case 'L':
            printf("reset limited discoverable mode\n");
            gap_discoverable_control(0);
            return;
        case 'u':
            printf("Send unplug request to %s...\n", bd_addr_to_str(device_addr));
            hid_device_send_virtual_cable_unplug(hid_cid);
            break;

        case 'w':
            printf("Set sniff subrating\n");
            gap_sniff_subrating_configure(hid_con_handle, 0, 0, 0);
            break;

        case 'z':{
            printf("Enter sniff mode \n");
            uint16_t sniff_min_interval = 18;
            uint16_t sniff_max_interval = 18;
            uint16_t sniff_attempt = 4;
            uint16_t sniff_timeout = 2;
            gap_sniff_mode_enter(hid_con_handle, sniff_min_interval, sniff_max_interval, sniff_attempt, sniff_timeout);
            break;
        }
        case 'q':
            printf("Set QoS 'Guaranteed'\n");
            gap_qos_set(hid_con_handle, 1, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
            break;

        case '\n':
        case '\r':
            break;

        default:
            show_usage();
            break;
    }

    switch (app_state){
        case APP_BOOTING:
        case APP_CONNECTING:
        case APP_CONNECTED:
            // ignore
            break;

        case APP_NOT_CONNECTED:
            printf("Connecting to %s...\n", bd_addr_to_str(device_addr));
            hid_device_connect(device_addr, &hid_cid);
            break;
    }
}



static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size){
    UNUSED(channel);
    UNUSED(packet_size);
    uint8_t status;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
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
                            virtual_cable_unplugged = false;

                            hid_subevent_connection_opened_get_bd_addr(packet, device_addr);
                            hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                            hid_con_handle = hid_subevent_connection_opened_get_con_handle(packet);
                            printf("HID Connected, please start typing... %s\n", bd_addr_to_str(device_addr));
                            break;
                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            printf("HID Disconnected\n");
                            if (virtual_cable_enabled){
                                virtual_cable_unplugged = true;
                                gap_drop_link_key_for_bd_addr(device_addr);
                                gap_disconnect(hid_con_handle);
                            }
                            hid_con_handle = HCI_CON_HANDLE_INVALID;
                            app_state = APP_NOT_CONNECTED;
                            hid_cid = 0;
                            break;
                        
                        case HID_SUBEVENT_SUSPEND:
                            printf("HID Suspend\n");
                            break;
                        case HID_SUBEVENT_EXIT_SUSPEND:
                            printf("HID Exit Suspend\n");
                            break;
                        case HID_SUBEVENT_VIRTUAL_CABLE_UNPLUG:
                            if (virtual_cable_enabled){
                                printf("HID Device is unplugged.\n");
                                virtual_cable_unplugged = true;
                                gap_drop_link_key_for_bd_addr(device_addr);
                            }
                            break;

                        case HID_SUBEVENT_CAN_SEND_NOW:
                            if (send_mouse_on_interrupt_channel){
                                send_mouse_on_interrupt_channel = false;
                                send_mouse_report_on_interrupt_channel(0,0,0);
                                break;     
                            }
                            if (send_keyboard_on_interrupt_channel){
                                send_keyboard_on_interrupt_channel = false;
                                send_keyboard_report_on_interrupt_channel(send_modifier, send_keycode);
                                send_keycode = 0;
                                send_modifier = 0;
                                break;
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

static int hid_device_connection_filter(bd_addr_t addr, hci_link_type_t link_type){
    UNUSED(link_type);
    if (virtual_cable_enabled && !virtual_cable_unplugged && (memcmp(addr, device_addr, 6) != 0)){
        printf("Virtual Cable: reject incoming connection from %s\n", bd_addr_to_str(addr));
        return 0;
    }
    return 1;
}


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    gap_discoverable_control(1);
    gap_set_class_of_device(0x2540);
    gap_set_local_name("HID Device 00:00:00:00:00:00");
    
    // Allow sniff mode requests by HID device and support role switch
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // L2CAP
    l2cap_init();

    // SDP Server
    sdp_init();
    memset(hid_service_buffer, 0, sizeof(hid_service_buffer));

    uint8_t hid_virtual_cable = 1;
    uint8_t hid_remote_wake = 1;
    uint8_t hid_reconnect_initiate = 1;
    uint8_t hid_normally_connectable = 1;

    if (hid_virtual_cable == 1){
        printf("Virtual Cable enabled, accept only connections from %s\n", device_addr_string);
        virtual_cable_enabled = true;
        virtual_cable_unplugged = false;
    }

    hid_sdp_record_t hid_params = {
        // hid sevice subclass 2540 Keyboard, hid counntry code 33 US
        0x2540, 33, 
        hid_virtual_cable, hid_remote_wake, 
        hid_reconnect_initiate, hid_normally_connectable,
        hid_boot_device, 
        host_max_latency, host_min_timeout, 
        3200,
        hid_descriptor_keyboard_boot_mode,
        sizeof(hid_descriptor_keyboard_boot_mode), 
        hid_device_name
    };

    hid_create_sdp_record(hid_service_buffer, 0x10001, &hid_params);

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
    
    gap_register_classic_connection_filter(&hid_device_connection_filter);

    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);
    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}

