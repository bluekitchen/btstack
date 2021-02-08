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

#define BTSTACK_FILE__ "classic_test.c"

/*
 * classic_test.c : minimal setup for SDP client over USB or UART
 */


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ble/sm.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "bluetooth_sdp.h"
#include "classic/rfcomm.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "classic/spp_server.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "btstack_stdin.h"

static void show_usage(void);

static bd_addr_t remote;
static bd_addr_t remote_rfcomm;
static const char * remote_addr_string = "00:1B:DC:08:E2:72";

static uint8_t rfcomm_channel_nr = 1;

static int gap_discoverable = 0;
static int gap_connectable = 0;
// static int gap_pagable = 0;
static int gap_bondable = 0;
static int gap_dedicated_bonding_mode = 0;
static int gap_mitm_protection = 0;
static uint8_t gap_auth_req = 0;
static char * gap_io_capabilities;

static int ui_passkey = 0;
static int ui_digits_for_passkey = 0;
static int ui_chars_for_pin = 0;
static uint8_t ui_pin[17];
static int ui_pin_offset = 0;

static uint16_t handle;
static uint16_t local_cid;

// SPP / RFCOMM
#define RFCOMM_SERVER_CHANNEL 1
#define HEARTBEAT_PERIOD_MS 1000
static uint16_t  rfcomm_channel_id;
static uint32_t  spp_service_buffer[150/4];    // implicit alignment to 4-byte memory address
static uint32_t  dummy_service_buffer[150/4];  // implicit alignment to 4-byte memory address
static uint8_t   dummy_uuid128[] = { 1,1,1,1, 1,1,1,1,  1,1,1,1, 1,1,1,1, 1,1,1,1};
static uint16_t  mtu;

static btstack_packet_callback_registration_t hci_event_callback_registration;

// GAP INQUIRY

#define MAX_DEVICES 10
enum DEVICE_STATE { REMOTE_NAME_REQUEST, REMOTE_NAME_INQUIRED, REMOTE_NAME_FETCHED };
struct device {
    bd_addr_t  address;
    uint16_t   clockOffset;
    uint32_t   classOfDevice;
    uint8_t    pageScanRepetitionMode;
    uint8_t    rssi;
    enum DEVICE_STATE  state; 
};

#define INQUIRY_INTERVAL 5
struct device devices[MAX_DEVICES];
int deviceCount = 0;


enum STATE {INIT, W4_INQUIRY_MODE_COMPLETE, ACTIVE} ;
enum STATE state = INIT;


static int getDeviceIndexForAddress( bd_addr_t addr){
    int j;
    for (j=0; j< deviceCount; j++){
        if (bd_addr_cmp(addr, devices[j].address) == 0){
            return j;
        }
    }
    return -1;
}

static void start_scan(void){
    printf("Starting inquiry scan..\n");
    gap_inquiry_start(INQUIRY_INTERVAL);
}

static int has_more_remote_name_requests(void){
    int i;
    for (i=0;i<deviceCount;i++) {
        if (devices[i].state == REMOTE_NAME_REQUEST) return 1;
    }
    return 0;
}

static void do_next_remote_name_request(void){
    int i;
    for (i=0;i<deviceCount;i++) {
        // remote name request
        if (devices[i].state == REMOTE_NAME_REQUEST){
            devices[i].state = REMOTE_NAME_INQUIRED;
            printf("Get remote name of %s...\n", bd_addr_to_str(devices[i].address));
            hci_send_cmd(&hci_remote_name_request, devices[i].address,
                        devices[i].pageScanRepetitionMode, 0, devices[i].clockOffset | 0x8000);
            return;
        }
    }
}

static void continue_remote_names(void){
    // don't get remote names for testing
    if (has_more_remote_name_requests()){
        do_next_remote_name_request();
        return;
    } 
    // start_scan();
    // accept first device
    if (deviceCount){
        memcpy(remote, devices[0].address, 6);
        printf("Inquiry scan over, using %s for outgoing connections\n", bd_addr_to_str(remote));
    } else {
        printf("Inquiry scan over but no devices found\n" );
    }
}

static void inquiry_packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
    UNUSED(size);

    bd_addr_t addr;
    int i;
    int numResponses;
    int index;

    // printf("packet_handler: pt: 0x%02x, packet[0]: 0x%02x\n", packet_type, packet[0]);
    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event = packet[0];

    switch(event){
        case HCI_EVENT_INQUIRY_RESULT:
        case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:{
            numResponses = hci_event_inquiry_result_get_num_responses(packet);
            int offset = 3;
            for (i=0; i<numResponses && deviceCount < MAX_DEVICES;i++){
                reverse_bd_addr(&packet[offset], addr);
                offset += 6;
                index = getDeviceIndexForAddress(addr);
                if (index >= 0) continue;   // already in our list
                memcpy(devices[deviceCount].address, addr, 6);

                devices[deviceCount].pageScanRepetitionMode = packet[offset];
                offset += 1;

                if (event == HCI_EVENT_INQUIRY_RESULT){
                    offset += 2; // Reserved + Reserved
                    devices[deviceCount].classOfDevice = little_endian_read_24(packet, offset);
                    offset += 3;
                    devices[deviceCount].clockOffset =   little_endian_read_16(packet, offset) & 0x7fff;
                    offset += 2;
                    devices[deviceCount].rssi  = 0;
                } else {
                    offset += 1; // Reserved
                    devices[deviceCount].classOfDevice = little_endian_read_24(packet, offset);
                    offset += 3;
                    devices[deviceCount].clockOffset =   little_endian_read_16(packet, offset) & 0x7fff;
                    offset += 2;
                    devices[deviceCount].rssi  = packet[offset];
                    offset += 1;
                }
                devices[deviceCount].state = REMOTE_NAME_REQUEST;
                printf("Device found: %s with COD: 0x%06x, pageScan %d, clock offset 0x%04x, rssi 0x%02x\n", bd_addr_to_str(addr),
                        devices[deviceCount].classOfDevice, devices[deviceCount].pageScanRepetitionMode,
                        devices[deviceCount].clockOffset, devices[deviceCount].rssi);
                deviceCount++;
            }

            break;
        }
        case HCI_EVENT_INQUIRY_COMPLETE:
            for (i=0;i<deviceCount;i++) {
                // retry remote name request
                if (devices[i].state == REMOTE_NAME_INQUIRED)
                    devices[i].state = REMOTE_NAME_REQUEST;
            }
            continue_remote_names();
            break;

        case DAEMON_EVENT_REMOTE_NAME_CACHED:
            reverse_bd_addr(&packet[3], addr);
            printf("Cached remote name for %s: '%s'\n", bd_addr_to_str(addr), &packet[9]);
            break;

        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            reverse_bd_addr(&packet[3], addr);
            index = getDeviceIndexForAddress(addr);
            if (index >= 0) {
                if (packet[2] == 0) {
                    printf("Name: '%s'\n", &packet[9]);
                    devices[index].state = REMOTE_NAME_FETCHED;
                } else {
                    printf("Failed to get name: page timeout\n");
                }
            }
            continue_remote_names();
            break;

        default:
            break;
    }
}
// GAP INQUIRY END


static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    UNUSED(channel);
    UNUSED(packet_type);

    uint16_t psm;
    uint32_t passkey;

    if (packet_type == UCD_DATA_PACKET){
        printf("UCD Data for PSM %04x received, size %u\n", little_endian_read_16(packet, 0), size - 2);
    }

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (packet[0]) {
        case HCI_EVENT_INQUIRY_RESULT:
        case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
        case HCI_EVENT_INQUIRY_COMPLETE:
        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            inquiry_packet_handler(packet_type, packet, size);
            break;

        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                printf("BTstack Bluetooth Classic Test Ready\n");
                hci_send_cmd(&hci_write_inquiry_mode, 0x01); // with RSSI
                show_usage();
            }
            break;
        case GAP_EVENT_DEDICATED_BONDING_COMPLETED:
            printf("GAP Dedicated Bonding Complete, status %u\n", packet[2]);
            break;

        case HCI_EVENT_CONNECTION_COMPLETE:
            if (!packet[2]){
                handle = little_endian_read_16(packet, 3);
                reverse_bd_addr(&packet[5], remote);
                printf("HCI_EVENT_CONNECTION_COMPLETE: handle 0x%04x\n", handle);
            }
            break;

        case HCI_EVENT_USER_PASSKEY_REQUEST:
            reverse_bd_addr(&packet[2], remote);
            printf("GAP User Passkey Request for %s\nPasskey:", bd_addr_to_str(remote));
            fflush(stdout);
            ui_digits_for_passkey = 6;
            break;

        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            reverse_bd_addr(&packet[2], remote);
            passkey = little_endian_read_32(packet, 8);
            printf("GAP User Confirmation Request for %s, number '%06u'\n", bd_addr_to_str(remote),passkey);
            break;

        case HCI_EVENT_PIN_CODE_REQUEST:
            hci_event_pin_code_request_get_bd_addr(packet, remote);
            printf("GAP Legacy PIN Request for %s (press ENTER to send)\nPasskey:", bd_addr_to_str(remote));
            fflush(stdout);
            ui_chars_for_pin = 1;
            break;

        case L2CAP_EVENT_CHANNEL_OPENED:
            // inform about new l2cap connection
            reverse_bd_addr(&packet[3], remote);
            psm = little_endian_read_16(packet, 11); 
            local_cid = little_endian_read_16(packet, 13); 
            handle = little_endian_read_16(packet, 9);
            if (packet[2] == 0) {
                printf("L2CAP Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                       bd_addr_to_str(remote), handle, psm, local_cid,  little_endian_read_16(packet, 15));
            } else {
                printf("L2CAP connection to device %s failed. status code %u\n", bd_addr_to_str(remote), packet[2]);
            }
            break;

        case L2CAP_EVENT_INCOMING_CONNECTION: {
            // data: event (8), len(8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16) 
            psm = little_endian_read_16(packet, 10);
            // uint16_t l2cap_cid  = little_endian_read_16(packet, 12);
            printf("L2CAP incoming connection request on PSM %u\n", psm); 
            // l2cap_accept_connection(l2cap_cid);
            break;
        }

        case RFCOMM_EVENT_INCOMING_CONNECTION:
            // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
            rfcomm_event_incoming_connection_get_bd_addr(packet, remote); 
            rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
            rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
            printf("RFCOMM channel %u requested for %s\n\r", rfcomm_channel_nr, bd_addr_to_str(remote));
            rfcomm_accept_connection(rfcomm_channel_id);
            break;
            
        case RFCOMM_EVENT_CHANNEL_OPENED:
            // data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
            if (rfcomm_event_channel_opened_get_status(packet)) {
                printf("RFCOMM channel open failed, status %u\n\r", rfcomm_event_channel_opened_get_status(packet));
            } else {
                rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                if (mtu > 60){
                    printf("BTstack libusb hack: using reduced MTU for sending instead of %u\n", mtu);
                    mtu = 60;
                }
                printf("\n\rRFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n\r", rfcomm_channel_id, mtu);
            }
            break;
            
        case RFCOMM_EVENT_CHANNEL_CLOSED:
            rfcomm_channel_id = 0;
            break;

        default:
            break;
    }
}

static void update_auth_req(void){
    gap_set_bondable_mode(gap_bondable);
    gap_auth_req = 0;
    if (gap_mitm_protection){
        gap_auth_req |= 1;  // MITM Flag
    }
    if (gap_dedicated_bonding_mode){
        gap_auth_req |= 2;  // Dedicated bonding
    } else if (gap_bondable){
        gap_auth_req |= 4;  // General bonding
    }
    printf("Authentication Requirements: %u\n", gap_auth_req);
    gap_ssp_set_authentication_requirement(gap_auth_req);
}

static void handle_found_service(const char * name, uint8_t port){
    printf("SDP: Service name: '%s', RFCOMM port %u\n", name, port);
    rfcomm_channel_nr = port;
}

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(packet_type);
    UNUSED(size);

    switch (packet[0]){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            handle_found_service(sdp_event_query_rfcomm_service_get_name(packet),
                                 sdp_event_query_rfcomm_service_get_rfcomm_channel(packet));
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            printf("SDP SPP Query complete\n");
            break;
        default: 
            break;
    }
}

static void send_ucd_packet(void){
    l2cap_reserve_packet_buffer();
    int ucd_size = 50;
    uint8_t * ucd_buffer = l2cap_get_outgoing_buffer();
    little_endian_store_16(ucd_buffer, 0, 0x2211);
    int i; 
    for (i=2; i< ucd_size ; i++){
        ucd_buffer[i] = i;
    }
    l2cap_send_prepared_connectionless(handle, L2CAP_CID_CONNECTIONLESS_CHANNEL, ucd_size);
}

static void show_usage(void){

    printf("\n--- Bluetooth Classic Test Console ---\n");
    printf("GAP: discoverable %u, connectable %u, bondable %u, MITM %u, dedicated bonding %u, auth_req 0x0%u, %s\n",
        gap_discoverable, gap_connectable, gap_bondable, gap_mitm_protection, gap_dedicated_bonding_mode, gap_auth_req, gap_io_capabilities);
    printf("---\n");
    printf("b/B - bondable off/on\n");
    printf("c/C - connectable off/on\n");
    printf("d/D - discoverable off/on\n");
    printf("</> - dedicated bonding off/on\n");
    printf("m/M - MITM protection off/on\n");
    // printf("a/A - pageable off/on\n");
    printf("---\n");
    printf("e - IO_CAPABILITY_DISPLAY_ONLY\n");
    printf("f - IO_CAPABILITY_DISPLAY_YES_NO\n");
    printf("g - IO_CAPABILITY_NO_INPUT_NO_OUTPUT\n");
    printf("h - IO_CAPABILITY_KEYBOARD_ONLY\n");
    printf("---\n");
    printf("i - perform inquiry and remote name request\n");
    printf("j - perform dedicated bonding to %s, MITM = %u\n", bd_addr_to_str(remote), gap_mitm_protection);
    printf("z - perform dedicated bonding to %s using legacy pairing\n", bd_addr_to_str(remote));
    printf("t - terminate HCI connection with handle 0x%04x\n", handle);
    printf("y - disable SSP\n");
    printf("---\n");
    printf("k - query %s for RFCOMM channel\n", bd_addr_to_str(remote_rfcomm));
    printf("l - create RFCOMM connection to %s using channel #%u\n",  bd_addr_to_str(remote_rfcomm), rfcomm_channel_nr);
    printf("n - send RFCOMM data\n");
    printf("u - send RFCOMM Remote Line Status Indication indicating Framing Error\n");
    printf("v - send RFCOMM Remote Port Negotiation to select 115200 baud\n");
    printf("w - query RFCOMM Remote Port Negotiation\n");
    printf("o - close RFCOMM connection\n");
    printf("---\n");
    // printf("p - create L2CAP channel to SDP at addr %s\n", bd_addr_to_str(remote));
    printf("p - create HCI connection to addr %s\n", bd_addr_to_str(remote));
    printf("Q - close HCI connection\n");
    printf("q - send L2CAP data\n");
    printf("r - send L2CAP ECHO request\n");
    printf("U - send UCD data\n");
    printf("s - close L2CAP channel\n");
    printf("x - require SSP for outgoing SDP L2CAP channel\n");
    printf("+ - initate SSP on current connection\n");
    printf("* - send SSP User Confirm YES\n");
    printf("= - delete link key\n");
    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char c){
    // passkey input
    if (ui_digits_for_passkey){
        printf("%c", c);
        fflush(stdout);
        ui_passkey = ui_passkey * 10 + c - '0';
        ui_digits_for_passkey--;
        if (ui_digits_for_passkey == 0){
            printf("\nSending Passkey '%06u'\n", ui_passkey);
            hci_send_cmd(&hci_user_passkey_request_reply, remote, ui_passkey);
        }
    }
    if (ui_chars_for_pin){
        printf("%c", c);
        fflush(stdout);
        if (c == '\n'){
            printf("\nSending Pin '%s'\n", ui_pin);
            ui_pin[ui_pin_offset] = 0;
            gap_pin_code_response(remote, (const char*) ui_pin);
        } else {
            ui_pin[ui_pin_offset++] = c;
        }
    }

    switch (c){
        case 'c':
            gap_connectable = 0;
            gap_connectable_control(0);
            show_usage();
            break;
        case 'C':
            gap_connectable = 1;
            gap_connectable_control(1);
            show_usage();
            break;
        case 'd':
            gap_discoverable = 0;
            gap_discoverable_control(0);
            show_usage();
            break;
        case 'D':
            gap_discoverable = 1;
            gap_discoverable_control(1);
            show_usage();
            break;
        case 'b':
            gap_bondable = 0;
            update_auth_req();
            show_usage();
            break;
        case 'B':
            gap_bondable = 1;
            update_auth_req();
            show_usage();
            break;
        case 'm':
            gap_mitm_protection = 0;
            update_auth_req();
            show_usage();
            break;
        case 'M':
            gap_mitm_protection = 1;
            update_auth_req();
            show_usage();
            break;

        case '<':
            gap_dedicated_bonding_mode = 0;
            update_auth_req();
            show_usage();
            break;
        case '>':
            gap_dedicated_bonding_mode = 1;
            update_auth_req();
            show_usage();
            break;

        case 'e':
            gap_io_capabilities = "IO_CAPABILITY_DISPLAY_ONLY";
            gap_ssp_set_io_capability(IO_CAPABILITY_DISPLAY_ONLY);
            show_usage();
            break;
        case 'f':
            gap_io_capabilities = "IO_CAPABILITY_DISPLAY_YES_NO";
            gap_ssp_set_io_capability(IO_CAPABILITY_DISPLAY_YES_NO);
            show_usage();
            break;
        case 'g':
            gap_io_capabilities = "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
            gap_ssp_set_io_capability(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
            show_usage();
            break;
        case 'h':
            gap_io_capabilities = "IO_CAPABILITY_KEYBOARD_ONLY";
            gap_ssp_set_io_capability(IO_CAPABILITY_KEYBOARD_ONLY);
            show_usage();
            break;

        case 'i':
            start_scan();
            break;

        case 'j':
            printf("Start dedicated bonding to %s using MITM %u\n", bd_addr_to_str(remote), gap_mitm_protection);
            gap_dedicated_bonding(remote, gap_mitm_protection);
            break;

        case 'z':
            printf("Start dedicated bonding to %s using legacy pairing\n", bd_addr_to_str(remote));
            gap_dedicated_bonding(remote, gap_mitm_protection);
            break;

        case 'y':
            printf("Disabling SSP for this session\n");
            hci_send_cmd(&hci_write_simple_pairing_mode, 0);
            break;

        case 'k':
            printf("Start SDP query for SPP service\n");
            sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, remote_rfcomm, 0x1101);
            break;

        case 't':
            printf("Terminate connection with handle 0x%04x\n", handle);
            hci_send_cmd(&hci_disconnect, handle, 0x13);  // remote closed connection
            break;

        case 'p':
            printf("Creating HCI Connection to %s\n", bd_addr_to_str(remote));
            hci_send_cmd(&hci_create_connection, remote, hci_usable_acl_packet_types(), 0, 0, 0, 1);
            break;
            // printf("Creating L2CAP Connection to %s, PSM SDP\n", bd_addr_to_str(remote));
            // l2cap_create_channel(packet_handler, remote, BLUETOOTH_PROTOCOL_SDP, 100);
            // break;
        // case 'u':
        //     printf("Creating L2CAP Connection to %s, PSM 3\n", bd_addr_to_str(remote));
        //     l2cap_create_channel(packet_handler, remote, 3, 100);
        //     break;
        case 'q':
            printf("Send L2CAP Data\n");
            l2cap_send(local_cid, (uint8_t *) "0123456789", 10);
       break;
        case 'r':
            printf("Send L2CAP ECHO Request\n");
            l2cap_send_echo_request(handle, (uint8_t *)  "Hello World!", 13);
            break;
        case 's':
            printf("L2CAP Channel Closed\n");
            l2cap_disconnect(local_cid, 0);
            break;
        case 'x':
            printf("Outgoing L2CAP Channels to SDP will also require SSP\n");
            l2cap_require_security_level_2_for_outgoing_sdp();
            break;

        case 'l':
            printf("Creating RFCOMM Channel to %s #%u\n", bd_addr_to_str(remote_rfcomm), rfcomm_channel_nr);
            rfcomm_create_channel(packet_handler, remote_rfcomm, rfcomm_channel_nr, NULL);
            break;
        case 'n':
            printf("Send RFCOMM Data\n");   // mtu < 60 
            rfcomm_send(rfcomm_channel_id, (uint8_t *) "012345678901234567890123456789012345678901234567890123456789", mtu);
            break;
        case 'u':
            printf("Sending RLS indicating framing error\n");   // mtu < 60 
            rfcomm_send_local_line_status(rfcomm_channel_id, 9);
            break;
        case 'v':
            printf("Sending RPN CMD to select 115200 baud\n");   // mtu < 60 
            rfcomm_send_port_configuration(rfcomm_channel_id, RPN_BAUD_115200, RPN_DATA_BITS_8, RPN_STOP_BITS_1_0, RPN_PARITY_NONE, 0);
            break;
        case 'w':
            printf("Sending RPN REQ to query remote port settings\n");   // mtu < 60 
            rfcomm_query_port_configuration(rfcomm_channel_id);
            break;
        case 'o':
            printf("RFCOMM Channel Closed\n");
            rfcomm_disconnect(rfcomm_channel_id);
            rfcomm_channel_id = 0;
            break;

        case '+':
            printf("Initiate SSP on current connection\n");
            gap_request_security_level(handle, LEVEL_2);
            break;

        case '*':
            printf("Sending SSP User Confirmation for %s\n", bd_addr_to_str(remote));
            hci_send_cmd(&hci_user_confirmation_request_reply, remote);
            break;

        case '=':
            printf("Deleting Link Key for %s\n", bd_addr_to_str(remote));
            gap_drop_link_key_for_bd_addr(remote);
            break;

        case 'U':
            printf("Sending UCD data on handle 0x%04x\n", handle);
            send_ucd_packet();
            break;

        case 'Q':
            printf("Closing HCI Connection to handle 0x%04x\n", handle);
            gap_disconnect(handle);
            break;

        default:
            show_usage();
            break;

    }
}

static void sdp_create_dummy_service(uint8_t *service, const char *name){
    
    uint8_t* attribute;
    de_create_sequence(service);
    
    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, 0x10002);
    
    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        de_add_uuid128(attribute, &dummy_uuid128[0] );
    }
    de_pop_sequence(service, attribute);
    
    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
        }
        de_pop_sequence(attribute, l2cpProtocol);
    }
    de_pop_sequence(service, attribute);
    
    // 0x0005 "Public Browse Group"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BROWSE_GROUP_LIST); // public browse group
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT);
    }
    de_pop_sequence(service, attribute);
    
    // 0x0006
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_LANGUAGE_BASE_ATTRIBUTE_ID_LIST);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x656e);
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x006a);
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x0100);
    }
    de_pop_sequence(service, attribute);
    
    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t *sppProfile = de_push_sequence(attribute);
        {
            de_add_number(sppProfile,  DE_UINT, DE_SIZE_16, 0x0100);
        }
        de_pop_sequence(attribute, sppProfile);
    }
    de_pop_sequence(service, attribute);
    
    // 0x0100 "ServiceName"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void) argv;
    (void) argc;
    
    printf("Starting up..\n");

    hci_disable_l2cap_timeout_check();

    gap_set_class_of_device(0x220404);
    gap_ssp_set_io_capability(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_io_capabilities =  "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
    gap_ssp_set_authentication_requirement(0);
    gap_ssp_set_auto_accept(0);

    update_auth_req();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    l2cap_register_fixed_channel(&packet_handler, L2CAP_CID_CONNECTIONLESS_CHANNEL);

    rfcomm_init();
    rfcomm_register_service(packet_handler, RFCOMM_SERVER_CHANNEL, 150);  // reserved channel, mtu=100

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record((uint8_t*) spp_service_buffer, 0x10001, RFCOMM_SERVER_CHANNEL, "SPP Counter");
    de_dump_data_element((uint8_t*) spp_service_buffer);
    printf("SDP service record size: %u\n\r", de_get_len((uint8_t*)spp_service_buffer));
    sdp_register_service((uint8_t*)spp_service_buffer);
    memset(dummy_service_buffer, 0, sizeof(dummy_service_buffer));
    sdp_create_dummy_service((uint8_t*)dummy_service_buffer, "UUID128 Test");
    de_dump_data_element((uint8_t*)dummy_service_buffer);
    printf("Dummy service record size: %u\n\r", de_get_len((uint8_t*)dummy_service_buffer));
    sdp_register_service((uint8_t*)dummy_service_buffer);
    
    gap_discoverable_control(0);
    gap_connectable_control(0);

    // parse human readable Bluetooth address
    sscanf_bd_addr(remote_addr_string, remote);
    sscanf_bd_addr(remote_addr_string, remote_rfcomm);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);

    return 0;
}

// Notes:
// UCD Test 1: C
// UCD Test 2: p, U, Q
// UCD Test 3: =, p, +, *, U, Q
